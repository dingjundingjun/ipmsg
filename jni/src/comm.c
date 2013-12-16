#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ipmsg.h"
#include "sys_info.h"
#include "comm.h"
#include "user_manage.h"
#include "com_pthread.h"

// user list queue
static PMSGPACK sayingList = NULL;
// for user list operation
static pthread_mutex_t sayingMutex = PTHREAD_MUTEX_INITIALIZER;

#define LOCK_SAYING(a)                          \
    pthread_mutex_lock(&sayingMutex);           \
    {                                           \
        a;                                      \
    }                                           \
    pthread_mutex_unlock(&sayingMutex);

static void *sayingThread(void *arg)
{
	int udpSock = getUdpSock();
	while(1)
	{
		sleep(1);
		LOCK_SAYING(
    		if(sayingList != NULL)
    		{
    			PMSGPACK msg = sayingList;
    			while(msg)
                {
    				if((int)msg->priv < 4)
    				{
    					sendMsg(udpSock, msg);
    					msg->priv = (void*)((int)(msg->priv) + 1);
    				}
    				else
    				{
    					printf("\nmsg send timeout!\n");
    					del_node(&sayingList, msg);
    					delUser(&(msg->address), NULL, NULL);
    					if(sayingList == NULL)
    						break;
    				}
    				msg = msg->next;
    			}
    		}
		);
	}
	return NULL;
}

static PMSGPACK findTalkMsg(PMSGPACK msg)
{
	PMSGPACK ret = NULL;
	PMSGPACK tmpMsg = sayingList;
	while(tmpMsg)
    {
		if(msg->addon && msg->addon->data && (strtoul(msg->addon->data, NULL, 10) == tmpMsg->packageNo))
		{
			ret = tmpMsg;
			break;
		}
	}
	return ret;
}

int login(void)
{
	static int sayingThreadCreated = 0;
	int udpSock = getUdpSock();
	MSGPACK msg;
	if(sayingThreadCreated == 0)
	{
		pthread_t pid;
		pthread_create(&pid, NULL, sayingThread, NULL);
		sayingThreadCreated = 1;
	}
	initUserList();
	createMsg(&msg, IPMSG_BR_ENTRY, NULL);
	memset(&(msg.address), 0, sizeof(msg.address));
	msg.address.sin_family = AF_INET;
	msg.address.sin_port = htons(IPMSG_DEFAULT_PORT);
	msg.address.sin_addr.s_addr = htonl(-1);
	return sendMsg(udpSock, &msg);
}

int ansEntry(PMSGPACK msg)
{
	MSGPACK ans;
	int udpSock = getUdpSock();
	createMsg(&ans, IPMSG_ANSENTRY, NULL);
	memcpy(&(ans.address), &(msg->address), sizeof(msg->address));
	return sendMsg(udpSock, &ans);
}

int addSayingMsg(PMSGPACK msg)
{
    msg->priv = 0;
	LOCK_SAYING(
    	add_node(&sayingList, msg);
	);
}

int sayToSomebody(const char *say, int person)
{
	PMSGPACK msg;
	PUSER_INFO usr = findUserByID(person);
	if(usr == NULL)
		return -1;
	msg = (PMSGPACK)malloc(sizeof(MSGPACK));
	msg->addon = (PMSG_ADDON)malloc(sizeof(MSG_ADDON));
	msg->addon->len = strlen(say) + 1;
	msg->addon->data = (char *)malloc(msg->addon->len);
	memcpy(msg->addon->data, say, msg->addon->len);
	createMsg(msg, IPMSG_SENDMSG | IPMSG_SENDCHECKOPT, msg->addon);
	memcpy(&(msg->address), &(usr->address), sizeof(usr->address));
	msg->priv = 0;
	LOCK_SAYING(
    	add_node(&sayingList, msg);
	);
	return 0;
}

int gotSayingCheck(PMSGPACK msg)
{
	PMSGPACK talkMsg;
	LOCK_SAYING(
    	talkMsg = findTalkMsg(msg);
    	if(talkMsg)
    	{
    		del_node(&sayingList, talkMsg);
    	}
	);
	return 0;
}

int answerSaying(PMSGPACK msg)
{
	int ret;
	char tmpString[20];
	MSGPACK ans;
	createMsg(&ans, IPMSG_RECVMSG, NULL);
	// copy the remote msg Package No!!!
	memcpy(&(ans.address), &(msg->address), sizeof(msg->address));
	ans.addon = (PMSG_ADDON)malloc(sizeof(MSG_ADDON));
	sprintf(tmpString, "%d", msg->packageNo);
	ans.addon->len = strlen(tmpString);
	ans.addon->data = (char *)malloc(ans.addon->len + 1);
	strcpy(ans.addon->data, tmpString);
	ret = sendMsg(getUdpSock(), &ans);
	free(ans.addon->data);
	free(ans.addon);
	return ret;
}

void logout(void)
{
	MSGPACK msg;
	createMsg(&msg, IPMSG_BR_EXIT, NULL);
	memset(&(msg.address), 0, sizeof(msg.address));
	msg.address.sin_family = AF_INET;
	msg.address.sin_port = htons(IPMSG_DEFAULT_PORT);
	msg.address.sin_addr.s_addr = htonl(-1);
	sendMsg(getUdpSock(), &msg);
	sendMsg(getUdpSock(), &msg);
}

