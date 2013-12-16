#pragma once
#include "msg_manage.h"

/* ��������һ����������ļ� */
typedef struct file_info_t {
	struct file_info_t *next;
	PUSER_INFO usr;						/* �����û� */
	unsigned long int packageNo;		/* �� �� �� */
	unsigned long int FileNo;			/* �ļ���� */
	char *name;							/* �� �� �� */
    char *path;                         /* �ļ�·�� */ /* ���ڷ����ļ� */
	unsigned long int size;				/* �ļ���С */
	unsigned long int mtime;			/* �޸�ʱ�� */
	unsigned long int flags;			/* �ļ����� */
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


