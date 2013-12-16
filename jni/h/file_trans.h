#pragma once
#include "msg_manage.h"

/* 用于描述一个待传输的文件 */
typedef struct file_info_t {
	struct file_info_t *next;
	PUSER_INFO usr;						/* 所属用户 */
	unsigned long int packageNo;		/* 包 编 号 */
	unsigned long int FileNo;			/* 文件编号 */
	char *name;							/* 文 件 名 */
    char *path;                         /* 文件路径 */ /* 用于发送文件 */
	unsigned long int size;				/* 文件大小 */
	unsigned long int mtime;			/* 修改时间 */
	unsigned long int flags;			/* 文件属性 */
} FILEINFO, *PFILEINFO;

static __inline void delFileEx(PFILEINFO file)
{
    if(file->path)
        free(file->path);
    else if(file->name)
        free(file->name);
    free(file);
}

static __inline int filePkg_compare(void *p1, void *p2)
{
	unsigned long int res = ((PFILEINFO)p1)->packageNo - ((PFILEINFO)p2)->packageNo;
	if(res == 0)
		res = ((PFILEINFO)p1)->usr - ((PFILEINFO)p2)->usr;
	return res;
}

/* file client */
int gotAFileMsg(PMSGPACK msg);
int listAllRecvFile(FILE *out);
int SaveFile(int fileNo, const char *to);
int listDropFileList(FILE *out, int fileID);
int dropFile(int fileID);

/* file server */
void initFileServer(void);
int addSendingFile(int usrID, const char *files[], int num);
int addSendingFolder(int usrID, const char *path);
int releaseFile(PMSGPACK msg);
int fileMonitor(FILE *out);


