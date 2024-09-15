#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

int createTCPIpv4Socket();

struct sockaddr_in* createIPv4Address(char *ip, int port);

#endif