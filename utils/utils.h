#ifndef UTILS_H
#define UTILS_H
#include "../log/log.h"
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>

int setnonblocking( int fd );

void addfd( int epollfd, int fd, bool one_shot,bool unblock);

void addfd_LT( int epollfd, int fd, bool one_shot,bool unblock);

void removefd( int epollfd, int fd );

void modfd( int epollfd, int fd, int ev );

void show_error( int connfd, const char* info );

#endif