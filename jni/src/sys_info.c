#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sys_info.h"

static SYSINFO sysInfo;

//初始化用户名和机器名
void initSystem(const char *name, const char *machine)
{
	memset(&sysInfo, 0, sizeof(sysInfo));
	if(name)
	{
		sysInfo.name = (char *)malloc(strlen(name) + 1);
		strcpy(sysInfo.name, name);
	}
	if(machine)
	{
		sysInfo.machine = (char *)malloc(strlen(machine) + 1);
		strcpy(sysInfo.machine, machine);
	}
	sysInfo.tcpSock = sysInfo.udpSock = -1;
}

void setSystemName(const char *name, const char *machine)
{
	if(sysInfo.name)
		free(sysInfo.name);
	sysInfo.name = (char *)malloc(strlen(name) + 1);
	strcpy(sysInfo.name, name);
	if(sysInfo.machine)
		free(sysInfo.machine);
	sysInfo.machine = (char *)malloc(strlen(name) + 1);
	strcpy(sysInfo.machine, machine);
}

void setSysSock(int tcpSock, int udpSock)
{
	sysInfo.tcpSock = tcpSock;
	sysInfo.udpSock = udpSock;
}

int getUdpSock(void)
{
    return sysInfo.udpSock;
}

int getTcpSock(void)
{
    return sysInfo.tcpSock;
}

char *getSelfName(void)
{
	return sysInfo.name;
}

char *getSelfMachine(void)
{
	return sysInfo.machine;
}
