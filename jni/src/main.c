#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
#endif
#include "debug.h"
#include "sys_info.h"
#include "ipmsg.h"
#include "utils.h"
#include "comm.h"
#include "user_manage.h"
#include "file_trans.h"
#include "userInterface.h"

#ifdef _WIN32
#pragma comment(lib, "lib/pthreadVC2.lib")
#pragma comment(lib, "ws2_32.lib")
#endif

void *recvThread(void *arg)
{
	int udpSock = (int)arg;
	while(1)
	{
		char recvBuf[2048];
		struct sockaddr remoteAddr;
		socklen_t remoteAddrLen = sizeof(remoteAddr);
		socklen_t size;
		size = recvfrom(udpSock, recvBuf, sizeof(recvBuf), 0, &remoteAddr, &remoteAddrLen);
		if(size > 0)
		{
			PMSGPACK msg = (PMSGPACK)malloc(sizeof(MSGPACK));
			parseRawMsg(recvBuf, size, msg);
			memcpy(&msg->address, &remoteAddr, sizeof(remoteAddr));
			addMsg(msg);
		}
	}
	return NULL;
}

void *msgProcessThread(void *arg)
{
	while(1)
	{
		PMSGPACK msg = getMsg();
        if(msg == NULL)
            continue;
		switch(GET_MODE(msg->cmd))
		{
		case IPMSG_BR_ENTRY:
			ansEntry(msg);
		case IPMSG_ANSENTRY:
			addUser(msg->name, msg->machine, &msg->address);
			break;
		case IPMSG_BR_EXIT:
			delUser(&(msg->address), NULL, NULL);
			break;
		case IPMSG_SENDMSG:
			if(msg->addon)
				printf("\n%s say:\n%s\n", msg->name, msg->addon->data);
			else
				printf("\nPackage Error!\n");
			if(GET_OPT(msg->cmd) & IPMSG_SENDCHECKOPT)
				answerSaying(msg);
			if(GET_OPT(msg->cmd) & IPMSG_FILEATTACHOPT)
				gotAFileMsg(msg);
			printf(TIP_PREFIX);
			fflush(stdout);
			break;
		case IPMSG_RECVMSG:
			printf("\nremote user recved the msg!\n");
			printf(TIP_PREFIX);
			fflush(stdout);
			gotSayingCheck(msg);
			break;
        case IPMSG_RELEASEFILES:
            PRINTF("RELEASEFILE recved\n");
            releaseFile(msg);
            break;
		}
		delMsgEx(msg);
	}
	return NULL;
}

int createSrv(unsigned short port)
{
	struct sockaddr_in server;
	int on=1;
	size_t len = sizeof(on);
	int tcpSock, udpSock;
#ifdef _WIN32
	int err;
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD( 1, 1 );

	err = WSAStartup( wVersionRequested, &wsaData );
	if ( err != 0 ) {
		return 0;
	}

	if ( LOBYTE( wsaData.wVersion ) != 1 ||
		HIBYTE( wsaData.wVersion ) != 1 ) {
		WSACleanup( );
		return 0;
	}
#endif
	tcpSock = socket(AF_INET, SOCK_STREAM, 0);
	udpSock = socket(AF_INET, SOCK_DGRAM, 0);

	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	if (setsockopt(udpSock, SOL_SOCKET, SO_BROADCAST, (const char *)&on, sizeof(on))<0)
	{
		F_PERROR("Socket Options set error!");
		exit(1);
	}
	if (bind(udpSock, (struct sockaddr*)&server, sizeof(server))<0)
	{
		F_PERROR("Udp socket bind error.");
		exit(1);
	}
	if (bind(tcpSock, (struct sockaddr*)&server, sizeof(server))<0)
	{
		F_PERROR("Tcp socket bind error.");
		exit(1);
	}
	if (listen(tcpSock, 10)<0)
	{
		F_PERROR("Tcp listen error.");
		exit(1);
	}
	setSysSock(tcpSock, udpSock);
    return 0;
}

int main(int argc, char *argv[])
{
	pthread_t recver, processer, uier;
	unsigned short port = IPMSG_DEFAULT_PORT;
	if(argc < 3)
		initSystem(getenv("USER"), getenv("HOSTNAME"));
	else
		initSystem(argv[1], argv[2]);
	initMsg();
	createSrv(port);
    initFileServer();
	login();
	pthread_create(&recver, NULL, &recvThread, (void*)getUdpSock());
	pthread_create(&processer, NULL, &msgProcessThread, (void*)getUdpSock());
	pthread_create(&uier, NULL, &uiProcessThread, (void*)getUdpSock());

	pthread_join(recver, NULL);
	pthread_join(processer, NULL);
	pthread_join(uier, NULL);
	return 0;
}
