#pragma once

#include <stdio.h>
#include "lnklst.h"
#include "com_pthread.h"
#include "com_ether.h"

typedef struct user_info_t
{
	struct user_info_t *next;
	char *name;
	char *machine;
	char IP[16];
	struct sockaddr_in address;
} USER_INFO, *PUSER_INFO;

PUSER_INFO findUserByAddress(const struct sockaddr_in *addr);
PUSER_INFO findUserByIP(const char *IP);
PUSER_INFO findUserByName(const char *name);
PUSER_INFO findUserByMachine(const char *machine);
PUSER_INFO findUserByID(int ID);
int initUserList(void);
int addUser(const char *name, const char *machine, struct sockaddr_in *addr);
int delUser(const struct sockaddr_in *addr, const char *machine, const char *name);
int listUser(FILE *file);
int getUserNumber(void);

