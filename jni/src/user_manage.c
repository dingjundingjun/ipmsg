#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "user_manage.h"

// user list queue
static PUSER_INFO userList = NULL;
// for user list operation
pthread_mutex_t usrMutex = PTHREAD_MUTEX_INITIALIZER;

#define LOCK_USER(a)                                \
    pthread_mutex_lock(&usrMutex);                  \
    {                                               \
        a;                                          \
    }                                               \
    pthread_mutex_unlock(&usrMutex);

PUSER_INFO findUserByAddress(const struct sockaddr_in *addr)
{
    PUSER_INFO ret = NULL;
	PUSER_INFO tmpNod;
    LOCK_USER(
        tmpNod = userList;
       	while(tmpNod)
    	{
    		if(tmpNod->address.sin_addr.s_addr == addr->sin_addr.s_addr)
            {
    			ret = tmpNod;
                break;
            }
    		tmpNod = tmpNod->next;
    	}
    );
	return ret;
}

PUSER_INFO findUserByIP(const char *IP)
{
	PUSER_INFO ret = NULL;
	PUSER_INFO tmpNod;
    LOCK_USER(
        tmpNod = userList;
    	while(tmpNod)
    	{
    		if(strcmp(tmpNod->IP, IP) == 0)
            {
    			ret = tmpNod;
                break;
            }
    		tmpNod = tmpNod->next;
    	}
    );
	return ret;
}

PUSER_INFO findUserByName(const char *name)
{
	PUSER_INFO ret = NULL;
	PUSER_INFO tmpNod;
    LOCK_USER(
        tmpNod = userList;
    	while(tmpNod)
    	{
    		if(strcmp(tmpNod->name, name) == 0)
            {
    			ret = tmpNod;
                break;
            }
    		tmpNod = tmpNod->next;
    	}
    );
	return ret;
}

PUSER_INFO findUserByMachine(const char *machine)
{
	PUSER_INFO ret = NULL;
	PUSER_INFO tmpNod;
    LOCK_USER(
        tmpNod = userList;
    	while(tmpNod)
    	{
    		if(strcmp(tmpNod->machine, machine) == 0)
            {
    			ret = tmpNod;
                break;
            }
    		tmpNod = tmpNod->next;
    	}
    );
	return ret;
}

PUSER_INFO findUserByID(int ID)
{
    PUSER_INFO ret = NULL;
    PUSER_INFO tmpNod;
    int count = 0;
    LOCK_USER(
        tmpNod = userList;
        while(tmpNod)
        {
            if(count == ID)
            {
                ret = tmpNod;
                break;
            }
            count++;
            tmpNod = tmpNod->next;
        }
    );
    return ret;
}

int initUserList(void)
{
	PUSER_INFO tmpNod;
	LOCK_USER(
    	tmpNod = userList;
		while(tmpNod)
		{
			PUSER_INFO tmp = tmpNod->next;
			free(tmpNod->name);
			free(tmpNod->machine);
			del_node(&userList, tmpNod);
			free(tmpNod);
			tmpNod = tmp;
		}
	);
	return 0;
}

int addUser(const char *name, const char *machine, struct sockaddr_in *addr)
{
	PUSER_INFO tmpNod = findUserByAddress(addr);
	if(tmpNod == NULL)
	{
		tmpNod = (PUSER_INFO)malloc(sizeof(USER_INFO));
		tmpNod->name = (char *)malloc(strlen(name) + 1);
		tmpNod->machine = (char *)malloc(strlen(machine) + 1);
		strcpy(tmpNod->name, name);
		strcpy(tmpNod->machine, machine);
		strcpy(tmpNod->IP, inet_ntoa(addr->sin_addr));
		memcpy(&(tmpNod->address), addr, sizeof(tmpNod->address));
        LOCK_USER(
    		add_node(&userList, tmpNod);
        );
	}
	return 0;
}

int delUser(const struct sockaddr_in *addr, const char *machine, const char *name)
{
	PUSER_INFO tmpNod = NULL;
	if(addr != NULL)
	{
		tmpNod = findUserByAddress(addr);
	}
	else if((machine != NULL) && (strlen(machine) > 0))
	{
		tmpNod = findUserByName(machine);
	}
	else if((name != NULL) && (strlen(name) > 0))
	{
		tmpNod = findUserByMachine(machine);
	}
	else
		return -1;
	if(tmpNod != NULL)
	{
        LOCK_USER(
    		free(tmpNod->name);
    		free(tmpNod->machine);
    		del_node(&userList, tmpNod);
    		free(tmpNod);
        );
		return 0;
	}
	return -2;
}

int listUser(FILE *file)
{
	PUSER_INFO tmpNod;
    LOCK_USER(
    	tmpNod = userList;
    	if(tmpNod == NULL)
    	{
    		fprintf(file, "User List is Empty!\n");
    	}
        else
        {
            int count = 0;
        	fprintf(file, "%4s%16s%16s%16s\n", "ID", "Username", "Machine", "IP Address");
        	do {
        		fprintf(file, "%4d%16s%16s%16s\n",
        					count++, tmpNod->name, tmpNod->machine, tmpNod->IP);
        		tmpNod = tmpNod->next;
        	} while(tmpNod);
        }
    );
	return 0;
}

int getUserNumber(void)
{
	int ret = 0;
	LOCK_USER(
    	ret = get_node_count(&userList);
	);
	return ret;
}

