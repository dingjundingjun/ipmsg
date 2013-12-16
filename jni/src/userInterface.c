#include <stdio.h>
#include <stdlib.h>
#include "debug.h"
#include "userInterface.h"
#include "utils.h"
#include "ipmsg.h"

struct cmd_info_t {
	char *cmd;
	int (*func)(int argc, char *argv[]);
};

static int do_cls(int argc, char *argv[])
{
	const char clscode[] = "\x1b[H\x1b[J";
	printf(clscode);
	return 0;
}

static int do_list(int argc, char *argv[])
{
    listUser(stdout);
    return 0;
}

static int do_say(int argc, char *argv[])
{
    int ID;
    if(argc > 1)
        ID = atoi(argv[1]);
    else
    {
        listUser(stdout);
        printf("Please select a user>");
        fflush(stdout);
        ID = getul(stdin, 0);
    }
    if(ID >= getUserNumber())
    {
        printf("User does not exist!\n");
    }
    else
    {
        char buf[2048] = "";
        if(argc > 2)
        {
            int i;
            for(i = 2; i < argc; i++)
            {
                strcat(buf, argv[i]);
                strcat(buf, " ");
            }
        }
        else
        {
            printf("Please input the text you want to send:\n");
            _gets_(stdin, buf);
        }
        sayToSomebody(buf, ID);
    }
    return 0;
}

static int do_refresh(int argc, char *argv[])
{
    login();
    return 0;
}

static int do_lsfile(int argc, char *argv[])
{
    listAllRecvFile(stdout);
    fflush(stdout);
    return 0;
}

static int do_getfile(int argc, char *argv[])
{
    int ret;
    char savename[1024];
    int fileID;
    if(argc > 1)
        fileID = atoi(argv[1]);
    else
    {
        if(listAllRecvFile(stdout) == 0)
        {
            printf("there is no file in list\n");
            return 0;
        }
        printf("Please select a file>");
        fflush(stdout);
        fileID = getul(stdin, 0);
    }
    if(argc > 2)
        strcpy(savename, argv[2]);
    else
    {
        printf("Please input the save file name(path)>");
        fflush(stdout);
        _gets_(stdin, savename);
    }
    ret = SaveFile(fileID, savename);
    if(ret == 0)
        printf("receive file ok!\n");
    else
        printf("receive file err! code = %d\n", ret);
    return 0;
}

static int do_dropfile(int argc, char *argv[])
{
    char buf[10];
    int fileID;
    if(argc > 1)
        fileID = atoi(argv[1]);
    else
    {
        if(listAllRecvFile(stdout) <= 0)
        {
            printf("No file found!\n");
            return 0;
        }
        printf("Please select a file>");
        fflush(stdout);
        fileID = getul(stdin, 0);
    }
    printf("The fallowing files will be droped:\n");
    if(listDropFileList(stdout, fileID) <= 0)
    {
        printf("No file found!\n");
        return 0;
    }
    printf("Are your SURE?(y/n)");
    fflush(stdout);
    buf[0] = _getch_(stdin);
    switch(buf[0])
    {
    case 'Y':
    case 'y':
        dropFile(fileID);
        break;
    default:
        printf("no files droped!\n");
        break;
    }
    return 0;
}

static int do_sendfile(int argc, char *argv[])
{
    #define Max     (sizeof(filepaths) / sizeof(filepaths[0]))
    char filepaths[20][255];
    char *files[20];
    int filenum = 0;
    int usrID;
    int i = 0;
    if(argc > 1)
        usrID = atoi(argv[1]);
    else
    {
        listUser(stdout);
        printf("Please select a user>");
        fflush(stdout);
        usrID = getul(stdin, 0);
    }
    if(argc > 2)
    {
        filenum = argc - 2 > Max ? Max : argc - 2;
        for(i = 0; i < filenum; i++)
            files[i] = argv[i + 2];
    }
    else
    {
        printf("Please input the file path(s):\n");
        while(i < Max)
        {
            _gets_(stdin, filepaths[i]);
            files[i] = filepaths[i];
            if(strlen(filepaths[i]) <= 0)
                break;
            i++;
        }
        if(i >= Max)
        {
            printf("TIP: max number (%d) reached.\n", Max);
        }
        if((filenum = i) > 0)
        {
            for(i = 0; i < filenum; i++)
                files[i] = filepaths[i];
        }
    }
    if(filenum > 0)
    {
        printf("%d files sended!\n", addSendingFile(usrID, files, filenum));
    }
    return 0;
}

static int do_sendfolder(int argc, char *argv[])
{
    char folderpath[255];
    char *folder = NULL;
    int usrID;
    if(argc > 1)
        usrID = atoi(argv[1]);
    else
    {
        listUser(stdout);
        printf("Please select a user>");
        fflush(stdout);
        usrID = getul(stdin, 0);
    }
    if(argc > 2)
    {
        int i;
        folder = argv[2];
    }
    else
    {
        printf("Please input the folder path(empty for cancle):\n");
        _gets_(stdin, folderpath);
        if(strlen(folderpath) > 0)
            folder = folderpath;
    }
    if(folder)
    {
        if(addSendingFolder(usrID, folder) == 0)
            printf("folder sended!\n");
        else
            printf("folder sending faild!\n");
    }
    return 0;
}

static int do_fileMonitor(int argc, char *argv[])
{
    return fileMonitor(stdout);
}

static int do_quit(int argc, char *argv[])
{
    logout();
#ifdef _WIN32
    WSACleanup();
#endif
    exit(0);
}

struct cmd_info_t cmdList[] = {
	{"cls", do_cls},
    {"list", do_list},
    {"say", do_say},
	{"refresh", do_refresh},
	{"lsfile", do_lsfile},
	{"getfile", do_getfile},
	{"dropfile", do_dropfile},
	{"sendfile", do_sendfile},
	{"sendfolder", do_sendfolder},
	{"filemonitor", do_fileMonitor},
	{"quit", do_quit},
};

int exec_cmd(char *cmdline)
{
	int status;
	int i;
	int argCount;
	char **argList;
	argCount = split_cmdline(cmdline, NULL, 0);
	argList = (char **)malloc((argCount + 1) * sizeof(char *));
	argCount = split_cmdline(cmdline, argList, argCount);
	argList[argCount] = NULL;
	for(i = 0; i < (sizeof(cmdList) / sizeof(cmdList[0])); i++)
	{
		if(strcmp(cmdList[i].cmd, argList[0]) == 0)
		{
			status = (*cmdList[i].func)(argCount, argList);
			break;
		}
	}
	if(i >= (sizeof(cmdList) / sizeof(cmdList[0])))
	{
        printf("wrong command!\n");
		//status = do_default(connfd, argCount, argList);
	}
	free(argList);
	return status;
}

void *uiProcessThread(void *arg)
{
	while(1)
	{
		char inputBuf[1024] = "";
		char cmd[10];
		printf("\n" TIP_PREFIX);
		fflush(stdout);
		_gets_(stdin, inputBuf);
		strrtrim(inputBuf);
        if(strlen(inputBuf) <= 0)
            continue;
        exec_cmd(inputBuf);
        continue;
        if(strcmp(cmd, "senddir") == 0)
        {
            addSendingFolder(1, "/cygdrive/c/Downloads");
        }
	}
	return NULL;
}

