#define _WIN32_WINNT 0x0601 // Windows 7 or later
#include <winsock2.h>
#include <ws2tcpip.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "socket.h"
#include "util.h"

#pragma comment(lib, "ws2_32.lib")

int sockopen(char *addr, int passive)
{
	WSADATA wsaData;
	struct addrinfo hint, *ais, *ai;
	char *type, *port, *sep, *end;
	int err, sock;
	long val;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		fatal("WSAStartup failed");
	}

	val = strtol(addr, &end, 0);
	if (*addr && !*end && val >= 0 && val < INT_MAX)
	{
		return val;
	}

	type = addr;
	addr = NULL;
	port = NULL;
	sep = strchr(type, '!');
	if (sep)
	{
		*sep = '\0';
		addr = sep + 1;
		sep = strchr(addr, '!');
		if (sep)
		{
			*sep = '\0';
			port = sep + 1;
			if (*port == '\0')
				port = NULL;
		}
		if (*addr == '\0')
			addr = NULL;
	}
	sock = INVALID_SOCKET;
	if (strcmp(type, "udp") == 0)
	{
		memset(&hint, 0, sizeof hint);
		hint.ai_flags = passive ? AI_PASSIVE : 0;
		hint.ai_family = AF_UNSPEC;
		hint.ai_socktype = SOCK_DGRAM;
		hint.ai_protocol = IPPROTO_UDP;
		err = getaddrinfo(addr, port, &hint, &ais);
		if (err != 0)
		{
			fatal("getaddrinfo: %s", gai_strerror(err));
		}
		for (ai = ais; ai; ai = ai->ai_next)
		{
			sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
			if (sock != INVALID_SOCKET)
			{
				if (passive)
				{
					union
					{
						struct ip_mreq v4;
						struct ipv6_mreq v6;
					} mreq;
					bool multicast = false;

					switch (ai->ai_family)
					{
					case AF_INET:
						mreq.v4.imr_multiaddr = ((struct sockaddr_in *)ai->ai_addr)->sin_addr;
						if ((((unsigned char *)&mreq.v4.imr_multiaddr)[0] & 0xf0) == 0xe0)
						{
							mreq.v4.imr_interface.s_addr = INADDR_ANY;
							if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq.v4, sizeof mreq.v4) != 0)
							{
								fatal("setsockopt IP_ADD_MEMBERSHIP:");
							}
							multicast = true;
						}
						break;
					case AF_INET6:
						mreq.v6.ipv6mr_multiaddr = ((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr;
						if (mreq.v6.ipv6mr_multiaddr.s6_addr[0] == 0xff)
						{
							mreq.v6.ipv6mr_interface = 0;
							if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char *)&mreq.v6, sizeof mreq.v6) != 0)
							{
								fatal("setsockopt IPV6_ADD_MEMBERSHIP:");
							}
							multicast = true;
						}
						break;
					}

					if (multicast && setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&(int){1}, sizeof(int)) != 0)
					{
						fatal("setsockopt SO_REUSEADDR:");
					}
					if (bind(sock, ai->ai_addr, (int)ai->ai_addrlen) == 0)
					{
						break;
					}
				}
				else if (connect(sock, ai->ai_addr, (int)ai->ai_addrlen) == 0)
				{
					break;
				}
				closesocket(sock);
				sock = INVALID_SOCKET;
			}
		}
		freeaddrinfo(ais);
		if (sock == INVALID_SOCKET)
		{
			fatal("connect:");
		}
	}
	else
	{
		fatal("unsupported address type '%s'", type);
	}

	WSACleanup();
	return sock;
}
