#pragma once
#ifdef _WIN32
#include <winsock.h>
typedef int socklen_t;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif
