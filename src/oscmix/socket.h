#ifndef SOCKET_H
#define SOCKET_H

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_t;
#define SOCKET_ERROR_VALUE INVALID_SOCKET
#define socket_close closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
typedef int socket_t;
#define SOCKET_ERROR_VALUE -1
#define socket_close close
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

socket_t sockopen(const char *addr, int recv);

#endif
