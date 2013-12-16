#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include "debug.h"
#include "utils.h"
#include "ipmsg.h"
#include "sys_info.h"
#include "comm.h"
#include "user_manage.h"
#include "com_pthread.h"
#include "file_trans.h"

static PFILEINFO sendFileList = NULL;
static pthread_mutex_t sendFileMutex = PTHREAD_MUTEX_INITIALIZER;

#define LOCK_SENDFILE(a)									\
	pthread_mutex_lock(&sendFileMutex);						\
	{                                                       \
        a;                                                  \
	}														\
	pthread_mutex_unlock(&sendFileMutex);

typedef struct _cliInfo_t {
	int connfd;
	struct sockaddr_in address;
} CLIENTINFO, *PCLIENTINFO;

typedef struct request_file_info_t {
    unsigned long packageNo;
    unsigned long fileNo;
    unsigned long offset;
}REQFILEINFO, *PREQFILEINFO;

static int parseRawRequestFileMsg(char *req, PREQFILEINFO fileInfo)
{
    char *StopAt;
    fileInfo->packageNo = strtoul(req, &StopAt, 16);
    if((StopAt <= req) || (*StopAt != ':'))
        return -1;
    req = StopAt + 1;
    fileInfo->fileNo = strtoul(req, &StopAt, 16);
    if(StopAt <= req)
        return -1;
    if(*StopAt == ':')
        fileInfo->offset = strtoul(StopAt + 1, NULL, 16);
    else
        fileInfo->offset = 0;
    return 0;
}

static int getFileNumberOfUsr(PUSER_INFO usr)
{
	int count = 0;
	PFILEINFO file;
	LOCK_SENDFILE(
    	file = sendFileList;
    	while(file)
    	{
    		if(file->usr == usr)
    			count++;
    		file = file->next;
    	}
    );
	return count;
}

static PFILEINFO getFileInfo(PUSER_INFO usr, unsigned long int packageNo, unsigned int fileNo)
{
	PFILEINFO ret = NULL;
	PFILEINFO file;
	LOCK_SENDFILE(
    	file = sendFileList;
    	while(file)
    	{
    		if((file->usr == usr) && (file->packageNo == packageNo) && (file->FileNo == fileNo))
    		{
    			ret = file;
    			break;
    		}
    		file = file->next;
    	}
    );
	return ret;
}

static int uploadFile(int sockfd, PFILEINFO file)
{
    unsigned char buf[4096];
    int len;
    FILE *fp = fopen(file->path, "rb");
    int totSize = 0;
    if(fp == NULL)
        return -1;
    while((len = fread(buf, 1, sizeof(buf), fp)) > 0)
    {
        write(sockfd, buf, len);
        totSize += len;
    }
    fclose(fp);
    PRINTF("file sending over!\n");
    DBUG(PRINTF("totSize = %d\n", totSize));
    return 0;
}

static int uploadFilePack(const char *fullpath, const struct stat *sb, int flag, void *arg)
{
    FILE *tcpOut = fdopen((int)arg, "w");
    unsigned long fileSize;
    char buf[256];
	A_PRINTF("fullpath = %s, flag = %d\n", fullpath, flag);
    if(flag == FTW_INVALID)
        return -1;
    switch(flag)
    {
    case FTW_INVALID:
        return -1;
    case FTW_FILE:
		PRINTF(" This is a file!\n");
        fileSize = sb->st_size;
        sprintf(&buf[5], "%s:%lx:%x:", getFileName(fullpath), fileSize, IPMSG_FILE_REGULAR);
        break;
    case FTW_FOLDER:
		PRINTF(" This is a folder!\n");
        fileSize = 0;
        sprintf(&buf[5], "%s:0:%x:", getFileName(fullpath), IPMSG_FILE_DIR);
        break;
    case FTW_RETPARENT:
		PRINTF(" This is return parent!\n");
        fileSize = 0;
        sprintf(&buf[5], ".:0:%x:", IPMSG_FILE_RETPARENT);
        break;
    }
    sprintf(buf, "%04x", strlen(&buf[5]) + 5);
    buf[4] = ':';
	PRINTF("the buf is [%s]\n", buf);
    fwrite(buf, 1, strlen(buf), tcpOut);
	PRINTF(" start to sending file content...\n");
    while(fileSize > 0)
    {
        char sendBuf[1024];
        int len;
        FILE *fp = fopen(fullpath, "rb");
        if(fp == NULL)
            break;
		PRINTF("fp = %p\n", fp);
        do {
            len = fread(sendBuf, 1, sizeof(sendBuf), fp);
            if(len > 0)
                fwrite(sendBuf, 1, len, tcpOut);
        } while(len > 0);
        fclose(fp);
        break;
    }
    fflush(tcpOut);
    return 0;
}

static int uploadFolder(int sockfd, PFILEINFO file)
{
	int ret;
	A_PRINTF("file->path = %s\n", file->path);
    ret = fileTreeWalk(file->path, uploadFilePack, -1, (void *)sockfd);
	A_PRINTF("ret = %d\n", ret);
	return ret;
}

int releaseFile(PMSGPACK msg)
{
	int ret = -1;
    PUSER_INFO usr = findUserByAddress(&msg->address);
    if(usr != NULL)
    {
        if((msg->addon) && (msg->addon->data))
        {
            unsigned long packageNo = atoi(msg->addon->data);
            PFILEINFO file = getFileInfo(usr, packageNo, 0);
            if(file)
            {
            	PFILEINFO fileNod;
            	LOCK_SENDFILE(
                	fileNod = sendFileList;
                	while(fileNod)
                	{
                		PFILEINFO tmp = fileNod;
                        fileNod = fileNod->next;
                		if((tmp->usr == file->usr) && (tmp->packageNo == file->packageNo))
                		{
                			del_node((void**)&sendFileList, tmp);
                            delFileEx(tmp);
                            ret = 0;
                		}
                	}
                );
            }
        }
    }
	return ret;
}

static void *fileTransfer(void *arg)
{
	PCLIENTINFO cliInfo = (PCLIENTINFO)arg;
	unsigned char recvBuf[2048];
	PUSER_INFO usr;
	int size;
    PMSGPACK msg = NULL;
    PRINTF("comes a client!\n");
	do {
        REQFILEINFO fileInfo;
        PFILEINFO file;
        // check wether the user is valid
        usr = findUserByAddress(&cliInfo->address);
		if(usr == NULL)
			break;
        PRINTF("usr:[%s]\n", usr->name);
        // check if there is some file waiting to be send
		if(getFileNumberOfUsr(usr) <= 0)
			break;
        // recv GETFILEDATA cmd
		size = read(cliInfo->connfd, recvBuf, sizeof(recvBuf));
		if(size <= 0)
			break;
        PRINTF("recv content = [%s]\n", recvBuf);
		msg = (PMSGPACK)malloc(sizeof(MSGPACK));
		parseRawMsg(recvBuf, size, msg);
        if(parseRawRequestFileMsg(msg->addon->data, &fileInfo) < 0)
            break;
        // check if have the file to be send
        file = getFileInfo(usr, fileInfo.packageNo, fileInfo.fileNo);
        if(file == NULL)
            break;
		switch(GET_MODE(msg->cmd))
		{
		case IPMSG_GETFILEDATA:
            PRINTF("[IPMSG_GETFILEDATA]\n");
            if(file->flags != IPMSG_FILE_REGULAR)
                break;
            uploadFile(cliInfo->connfd, file);
			break;
        case IPMSG_GETDIRFILES:
            PRINTF("[IPMSG_GETDIRFILES]\n");
            if(file->flags != IPMSG_FILE_DIR)
                break;
            uploadFolder(cliInfo->connfd, file);
            break;
		}
	} while(0);
    if(msg)
        delMsgEx(msg);
	close(cliInfo->connfd);
	free(cliInfo);
	return NULL;
}

static void *fileServer(void *arg)
{
	int tcpSock = getTcpSock();
	while(1)
	{
		PCLIENTINFO cliInfo = malloc(sizeof(CLIENTINFO));
		socklen_t cliAddrLen = sizeof(cliInfo->address);
		pthread_t transfer;
		if((cliInfo->connfd = accept(tcpSock, (struct sockaddr*)&cliInfo->address, &cliAddrLen)) < 0)
			printf("\nUnknown error!!\n");
		else
			pthread_create(&transfer, NULL, &fileTransfer, (void*)cliInfo);
	}
	return NULL;
}

static int packFile(PMSG_ADDON addon, PFILEINFO file)
{
    char tmpBuf[1024];
    // 文件序号:文件名:大小:修改时间:文件属性
    sprintf(tmpBuf, "\a%x:%s:%x:%x:%x:", file->FileNo, file->name, file->size, file->mtime, file->flags);
    if(addon->data)
    {
        addon->data = realloc(addon->data, addon->len + strlen(tmpBuf));
        strcpy(&addon->data[addon->len - 1], tmpBuf);
        addon->len += strlen(tmpBuf);
    }
    else
    {
        addon->len = strlen(tmpBuf) + 1;
        addon->data = malloc(addon->len);
        tmpBuf[0] = '\0';
        memcpy(addon->data, tmpBuf, addon->len);
    }
    return 0;
}

int addSendingFile(int usrID, const char *files[], int num)
{
    int i;
    int ret = 0;
    unsigned long packageNo = time(NULL);
    PUSER_INFO usr;
    PMSGPACK msg;
    PMSG_ADDON addon;
    if(num <= 0)
        return -1;
    usr = findUserByID(usrID);
    if(usr == NULL)
        return -1;
    msg = (PMSGPACK)malloc(sizeof(MSGPACK));
    addon = (PMSG_ADDON)malloc(sizeof(MSG_ADDON));
    addon->data = NULL;
    addon->len = 0;
    for(i = 0; i < num; i++)
    {
        struct stat buf;
        PFILEINFO file;
        if(stat(files[i], &buf) != 0)
            continue;
        file = (PFILEINFO)malloc(sizeof(FILEINFO));
        file->usr = usr;
        file->packageNo = packageNo;
        file->FileNo = i;
        file->path = (char *)malloc(strlen(files[i]) + 1);
        strcpy(file->path, files[i]);
        file->name = getFileName(file->path);
        file->size = buf.st_size;
        file->mtime = buf.st_mtime;
        file->flags = S_ISDIR(buf.st_mode) ? IPMSG_FILE_DIR : IPMSG_FILE_REGULAR;
        LOCK_SENDFILE(
            add_node((void**)&sendFileList, file);
        );
        packFile(addon, file);
        ret++;
    }
    if(ret > 0)
    {
        createMsgWithPackNo(msg, IPMSG_SENDMSG | IPMSG_SENDCHECKOPT | IPMSG_FILEATTACHOPT, packageNo, addon);
        memcpy(&msg->address, &usr->address, sizeof(msg->address));
        addSayingMsg(msg);
    }
    else
    {
        free(msg);
        free(addon);
    }
    return ret;
}

int addSendingFolder(int usrID, const char *path)
{
    unsigned long packageNo = time(NULL);
    PUSER_INFO usr;
    PMSGPACK msg;
    PMSG_ADDON addon;
    struct stat buf;
    PFILEINFO file;
    usr = findUserByID(usrID);
    if(usr == NULL)
        return -1;
    if(stat(path, &buf) != 0)
        return -1;
    msg = (PMSGPACK)malloc(sizeof(MSGPACK));
    addon = (PMSG_ADDON)malloc(sizeof(MSG_ADDON));
    addon->data = NULL;
    addon->len = 0;
    file = (PFILEINFO)malloc(sizeof(FILEINFO));
    file->usr = usr;
    file->packageNo = packageNo;
    file->FileNo = 0;
    file->path = (char *)malloc(strlen(path) + 1);
    strcpy(file->path, path);
    if(file->path[strlen(file->path) - 1] == '/')
        file->path[strlen(file->path) - 1] = '\0';
    file->name = getFileName(file->path);
    file->size = buf.st_size;
    file->mtime = buf.st_mtime;
    file->flags = S_ISDIR(buf.st_mode) ? IPMSG_FILE_DIR : IPMSG_FILE_REGULAR;
    LOCK_SENDFILE(
        add_node((void**)&sendFileList, file);
    );
    packFile(addon, file);
    createMsgWithPackNo(msg, IPMSG_SENDMSG | IPMSG_SENDCHECKOPT | IPMSG_FILEATTACHOPT, packageNo, addon);
    memcpy(&msg->address, &usr->address, sizeof(msg->address));
    addSayingMsg(msg);
    return 0;
}

void initFileServer(void)
{
    pthread_t fileserver;
    pthread_create(&fileserver, NULL, &fileServer, NULL);
}

int fileMonitor(FILE *out)
{
#define FILENO_OFFSET		"\x1b[0G"
#define FILENAME_OFFSET		"\x1b[10G"
#define FILESIZE_OFFSET		"\x1b[40G"
#define FILEFLAG_OFFSET		"\x1b[52G"
#define FILEUSER_OFFSET		"\x1b[60G"
	int count = 0;
	PFILEINFO fileInfo;
	LOCK_SENDFILE(
    	fileInfo = sendFileList;
		fprintf(out, FILENO_OFFSET "%s" FILENAME_OFFSET "|%s" FILESIZE_OFFSET "|%s" FILEFLAG_OFFSET "|%s" FILEUSER_OFFSET "|%s\n", "File No", "File Name", "File Size", "Type", "User");
		while(fileInfo)
        {
			fprintf(out, FILENO_OFFSET "%d" FILENAME_OFFSET "|%s" FILESIZE_OFFSET "|%d" FILEFLAG_OFFSET "|%s" FILEUSER_OFFSET "|%s\n", fileInfo->FileNo, fileInfo->name, fileInfo->size, (fileInfo->flags == IPMSG_FILE_REGULAR) ? "File" : "Folder", fileInfo->usr->name);
			count++;
 			fileInfo = fileInfo->next;
		}
	);
	return count;
}

