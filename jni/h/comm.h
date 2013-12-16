#pragma once
#include "msg_manage.h"

int login(void);
int ansEntry(PMSGPACK msg);
int addSayingMsg(PMSGPACK msg);
int sayToSomebody(const char *say, int person);
int gotSayingCheck(PMSGPACK msg);
int answerSaying(PMSGPACK msg);
void logout(void);

