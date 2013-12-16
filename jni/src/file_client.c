#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include "ipmsg.h"
#include "sys_info.h"
#include "comm.h"
#include "user_manage.h"
#include "com_pthread.h"
#include "file_trans.h"

static PFILEINFO recvFileList = NULL;
static pthread_mutex_t recvFileMutex = PTHREAD_MUTEX_INITIALIZER;

#define LOCK_RECVFILE(a)									\
	pthread_mutex_lock(&recvFileMutex);						\
	{                                                       \
        a;                                                  \
	}														\
	pthread_mutex_unlock(&recvFileMutex);


static PFILEINFO getFileInfoByID(int fileID)
{
	PFILEINFO ret;
	LOCK_RECVFILE(
    	ret = get_node_byID(recvFileList, fileID);
    );
	return ret;
}

static int __listRecvFileOfUser(FILE *out, PUSER_INFO usr)
{
#define FILENO_OFFSET		"\x1b[0G"
#define FILENAME_OFFSET		"\x1b[10G"
#define FILESIZE_OFFSET		"\x1b[40G"
#define FILEFLAG_OFFSET		"\x1b[52G"
#define FILEUSER_OFFSET		"\x1b[60G"
	int count = 0;
	PFILEINFO fileInfo;
	LOCK_RECVFILE(
    	fileInfo = recvFileList;
		fprintf(out, FILENO_OFFSET "%s" FILENAME_OFFSET "|%s" FILESIZE_OFFSET "|%s" FILEFLAG_OFFSET "|%s" FILEUSER_OFFSET "|%s\n", "File No", "File Name", "File Size", "Type", "User");
		while(fileInfo)
        {
			if((usr == NULL) || (fileInfo->usr == usr))
			{
				fprintf(out, FILENO_OFFSET "%d" FILENAME_OFFSET "|%s" FILESIZE_OFFSET "|%d" FILEFLAG_OFFSET "|%s" FILEUSER_OFFSET "|%s\n", count/*fileInfo->FileNo*/, fileInfo->name, fileInfo->size, (fileInfo->flags == IPMSG_FILE_REGULAR) ? "File" : "Folder", fileInfo->usr->name);
				count++;
			}
			fileInfo = fileInfo->next;
		}
	);
	return count;
}

int listRecvFileOfUser(FILE *out, int UserID)
{
	PUSER_INFO usr = findUserByID(UserID);
	if(usr == NULL)
	{
		fprintf(out, "No such user!!\n");
		return -1;
	}
	else
	{
		return __listRecvFileOfUser(out, usr);
	}
}

int listRecvFileOfPkg(FILE *out, unsigned long int PkgID)
{
#define FILENO_OFFSET		"\x1b[0G"
#define FILENAME_OFFSET		"\x1b[10G"
#define FILESIZE_OFFSET		"\x1b[40G"
#define FILEFLAG_OFFSET		"\x1b[52G"
#define FILEUSER_OFFSET		"\x1b[60G"
	int count = 0;
	PFILEINFO fileInfo;
	LOCK_RECVFILE(
    	fileInfo = recvFileList;
    	fprintf(out, FILENO_OFFSET "%s" FILENAME_OFFSET "|%s" FILESIZE_OFFSET "|%s" FILEFLAG_OFFSET "|%s" FILEUSER_OFFSET "|%s\n", "File No", "File Name", "File Size", "Type", "User");
    	while(fileInfo)
    	{
    		if(fileInfo->packageNo == PkgID)
    		{
    			fprintf(out, FILENO_OFFSET "%d" FILENAME_OFFSET "|%s" FILESIZE_OFFSET "|%d" FILEFLAG_OFFSET "|%s" FILEUSER_OFFSET "|%s\n", count/*fileInfo->FileNo*/, fileInfo->name, fileInfo->size, (fileInfo->flags == IPMSG_FILE_REGULAR) ? "File" : "Folder", fileInfo->usr->name);
    			count++;
    		}
    		fileInfo = fileInfo->next;
    	}
    );
	return count;
}

int listDropFileList(FILE *out, int fileID)
{
    int ret = 0;
	PFILEINFO file = getFileInfoByID(fileID);
	if(file == NULL)
		fprintf(out, "No such file!\n");
	else
	{
		ret = listRecvFileOfPkg(out, file->packageNo);
	}
	return ret;
}

static int __delRecvFileOfPkg(PFILEINFO file)
{
	PFILEINFO fileInfo;
	LOCK_RECVFILE(
    	fileInfo = recvFileList;
    	while(fileInfo)
    	{
    		PFILEINFO tmp = fileInfo;
            fileInfo = fileInfo->next;
    		if(tmp->packageNo == file->packageNo)
    		{
    			del_node((void**)&recvFileList, tmp);
                delFileEx(tmp);
    		}
    	}
    );
	return 0;
}

int listAllRecvFile(FILE *out)
{
	return __listRecvFileOfUser(out, NULL);
}

int gotAFileMsg(PMSGPACK msg)
{
	if(msg->cmd & IPMSG_FILEATTACHOPT)
	{
		if(msg->addon)
		{
			/* 开始分析对方传送的文件列表 */
			/* 文件序号:文件名:大小(单位:字节):最后修改时间:文件属性[: 附加属性=val1[,val2…][:附加信息=…]]:\a:文件序号… */
			int ok = 0;
			char *p = msg->addon->data + strlen(msg->addon->data) + 1;	/* 跳过消息部分 */
			PUSER_INFO usr = findUserByAddress(&(msg->address));		/* 寻找消息拥有者 */
			PFILEINFO file = NULL;										/* 准备向接收文件列表中增加文件 */
			while(p)
			{
				char *sec = p;
				file = (PFILEINFO)malloc(sizeof(FILEINFO));
				file->usr = usr;
				file->packageNo = msg->packageNo;
				file->name = NULL;
                file->path = NULL;
				if((sec = strchr(p, ':')) != NULL)
					sec++;
				else
					break;
				file->FileNo = atoi(p);									/* FileNo comes first */
				p = sec;
				if((sec = strchr(p, ':')) != NULL)
					*sec++ = '\0';
				else
					break;
                file->path = NULL;
				file->name = (char *)malloc(strlen(p) + 1);
				strcpy(file->name, p);									/* then the file name */
				p = sec;
				if((sec = strchr(p, ':')) != NULL)
					sec++;
				else
					break;
				file->size = strtoul(p, NULL, 16);						/* then the file size */
				p = sec;
				if((sec = strchr(p, ':')) != NULL)
					sec++;
				else
					break;
				file->mtime = strtoul(p, NULL, 16);						/* then the file modify time */
				p = sec;
				if((sec = strchr(p, ':')) != NULL)
					sec++;
				else
					break;
				file->flags = strtoul(p, NULL, 16);						/* then the file flags */
				/* 忽略文件的附加属性 */
				ok = 1;
				add_node_sorted((void *)&recvFileList, file, filePkg_compare);
				if((sec = strchr(p, '\a')) == NULL)
					break;
				p = sec + 1;
                file = NULL;
			}
			if(ok)
			{
				__listRecvFileOfUser(stdout, usr);
				return 0;
			}
            else
            {
                if(file)
                {
                    if(file->name)
                        free(file->name);
                    free(file);
                }
            }
		}
	}
	return -1;
}

static int requestFile(PFILEINFO file, FILE *saveTo)
{
	char *requestString = (char *)malloc(strlen(getSelfName()) + strlen(getSelfMachine()) + strlen(file->name) + 100);
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	unsigned long int fileSize = file->size;
	if(sockfd < 0)
	{
		perror("requestFile Error");
		return sockfd;
	}
	if(connect(sockfd, (struct sockaddr*)&(file->usr->address), sizeof(file->usr->address)) < 0)
	{
		perror("requestFile Error");
		close(sockfd);
		return -1;
	}
	sprintf(requestString, "%d:%d:%s:%s:%d:%x:%x:%x", IPMSG_VERSION, time(NULL), getSelfName(), getSelfMachine(), IPMSG_GETFILEDATA, file->packageNo, file->FileNo, 0);
	send(sockfd, requestString, strlen(requestString), 0);
	while(1)
	{
		char recvBuf[10240];
		int recvLen = fileSize > 10240 ? 10240 : fileSize;
		recvLen = recv(sockfd, recvBuf, recvLen, 0);
		if(recvLen <= 0)
		{
			break;
		}
		fwrite(recvBuf, 1, recvLen, saveTo);
		fileSize -= recvLen;
		if(fileSize == 0)
			break;
	}
	close(sockfd);
	return 0;
}

typedef struct folder_file_info_t {
    unsigned long int headSize;
    char *fileName;
    unsigned long int fileSize;
    unsigned int flag;
}FOLDER_FILEINFO, *PFOLDER_FILEINFO;

static int parseRawFolderHead(char *buf, PFOLDER_FILEINFO headInfo)
{
    char *sep = strchr(buf, ':');
    if(sep == NULL)
        return -1;
    headInfo->fileName = malloc(sep - buf + 1);
    memcpy(headInfo->fileName, buf, sep - buf);
    headInfo->fileName[sep - buf] = '\0';
    buf = sep + 1;
    headInfo->fileSize = strtoul(buf, &sep, 16);
    if((sep <= buf) || (*sep != ':'))
    {
        free(headInfo->fileName);
        headInfo->fileName = NULL;
        return -1;
    }
    sep++;
    headInfo->flag = strtoul(sep, NULL, 16);
    return 0;
}

static int saveFileInFolder(int sockfd, PFOLDER_FILEINFO headInfo)
{
    switch(headInfo->flag)
    {
    case IPMSG_FILE_REGULAR:
        {
            FILE *fp = fopen(headInfo->fileName, "wb+");
            char buf[4096];
            unsigned long int size = headInfo->fileSize;
            while(size > 0)
            {
                int readl = size > sizeof(buf) ? sizeof(buf) : size;
                int l = read(sockfd, buf, readl);
                if(l <= 0)
                    return -1;
                size -= l;
                fwrite(buf, 1, l, fp);
            }
            fclose(fp);
        }
        break;
    case IPMSG_FILE_DIR:
        mkdir(headInfo->fileName, 0777);
        chdir(headInfo->fileName);
        break;
    case IPMSG_FILE_RETPARENT:
        chdir("..");
        break;
    }
}

static int requestFolder(PFILEINFO file)
{
    int depth = 0;
    int ret = -1;
	char *requestString = (char *)malloc(strlen(getSelfName()) + strlen(getSelfMachine()) + strlen(file->name) + 100);
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	unsigned long int fileSize = file->size;
	if(sockfd < 0)
	{
		perror("requestFile Error");
		return sockfd;
	}
	if(connect(sockfd, (struct sockaddr*)&(file->usr->address), sizeof(file->usr->address)) < 0)
	{
		perror("requestFile Error");
		close(sockfd);
		return -1;
	}
	sprintf(requestString, "%d:%d:%s:%s:%d:%x:%x", IPMSG_VERSION, time(NULL), getSelfName(), getSelfMachine(), IPMSG_GETDIRFILES, file->packageNo, file->FileNo);
	send(sockfd, requestString, strlen(requestString), 0);
    do {
        FOLDER_FILEINFO headInfo;
        char *StopAt;
		char headBuf[100];
        char *head = NULL;
		int recvLen = readUtilCharacter(sockfd, headBuf, 100, ':');
        if(errno != 0)
            break;
		if(recvLen <= 0)
		{
			break;
		}
        headInfo.headSize = strtoul(headBuf, &StopAt, 16);
        head = (char *)malloc(headInfo.headSize - recvLen + 1);
        if(*StopAt != ':')
            break;
        read(sockfd, head, headInfo.headSize - recvLen);
        if(parseRawFolderHead(head, &headInfo) != 0)
            break;
        if(saveFileInFolder(sockfd, &headInfo) != 0)
            break;
        if(headInfo.flag == IPMSG_FILE_DIR)
            depth++;
        else if(headInfo.flag == IPMSG_FILE_RETPARENT)
            depth--;
        ret++;
    }while(depth > 0);
	close(sockfd);
	return ret;
}

 static int SaveFolder(PFILEINFO file, const char *to)
 {
     char curpath[1024];
     struct stat buf;
     if(stat(to, &buf) != 0)
     {
         printf("invalid path!\n");
         return -1;
     }
     if(!S_ISDIR(buf.st_mode))
     {
         printf("the path [%s] isn't a directory!\n", to);
         return -1;
     }
     getcwd(curpath, sizeof(curpath));
     if(chdir(to) != 0)
     {
         printf("unable change to the path [%s]!\n", to);
         return -1;
     }
     requestFolder(file);
     chdir(curpath);
     return 0;
 }

 int SaveFile(int fileNo, const char *to)
{
	PFILEINFO file = getFileInfoByID(fileNo);
    if(file == NULL)
    {
        printf("No such file!\n");
        return -1;
    }
    if(file->flags == IPMSG_FILE_REGULAR)
    {
    	FILE *saveTo;
        saveTo = fopen(to, "wb+");
    	if(file == NULL)
    		return -1;
    	if(saveTo == NULL)
    		return -2;
    	requestFile(file, saveTo);
    	fclose(saveTo);
        return 0;
    }
    else if(file->flags == IPMSG_FILE_DIR)
    {
        return SaveFolder(file, to);
    }
    printf("unsupported file type: %x\n", file->flags);
	return -1;
}

int dropFile(int fileID)
{
    char packNo[20];
    MSGPACK msg;
    MSG_ADDON addon;
    PFILEINFO file = getFileInfoByID(fileID);
    unsigned long int packageNo;
    if(file == NULL)
        return -1;
    packageNo = file->packageNo;
    memcpy(&msg.address, &file->usr->address, sizeof(msg.address));
    __delRecvFileOfPkg(file);
    sprintf(packNo, "%d", packageNo);
    addon.data = packNo;
    addon.len = strlen(packNo);
    createMsg(&msg, IPMSG_RELEASEFILES, &addon);
    return sendMsg(getUdpSock(), &msg);
}

