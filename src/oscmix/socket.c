#define _WIN32_WINNT 0x0601 // Windows 7 or later
#include "platform.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "socket.h"
#include "util.h"

#pragma comment(lib, "ws2_32.lib")

socket_t sockopen(const char *addr, int recv)
{
	socket_t fd;
	const char *type, *host, *port;
	struct addrinfo hints, *res, *ai;
	int err, optval;
	char *addrstr, *p;

	addrstr = strdup(addr);
	if (!addrstr)
		fatal("strdup:");

	type = addrstr;
	p = strchr(addrstr, '!');
	if (!p)
	{
		free(addrstr);
		fatal("invalid address format '%s'", addr);
	}
	*p++ = '\0';
	host = p;
	p = strchr(p, '!');
	if (!p)
	{
		free(addrstr);
		fatal("invalid address format '%s'", addr);
	}
	*p++ = '\0';
	port = p;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	if (strcmp(type, "udp") == 0)
	{
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;
	}
	else if (strcmp(type, "tcp") == 0)
	{
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
	}
	else
	{
		free(addrstr);
		fatal("unknown socket type '%s'", type);
	}

	if (recv)
		hints.ai_flags = AI_PASSIVE;

	err = getaddrinfo(recv && *host == '*' ? NULL : host, port, &hints, &res);
	if (err)
	{
#ifdef _WIN32
		fatal("getaddrinfo: %d", WSAGetLastError());
#else
		fatal("getaddrinfo: %s", gai_strerror(err));
#endif
	}

	for (ai = res; ai; ai = ai->ai_next)
	{
		platform_socket_t fd = platform_socket_create(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (fd == PLATFORM_INVALID_SOCKET)
			continue;

		if (recv)
		{
			optval = 1;
			if (platform_socket_setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
			{
				perror("setsockopt");
				platform_socket_close(fd);
				continue;
			}

			if (*host != '*')
			{
				/* check if multicast */
				if (ai->ai_family == AF_INET)
				{
					struct sockaddr_in *sin;
					sin = (struct sockaddr_in *)ai->ai_addr;
					if (IN_MULTICAST(ntohl(sin->sin_addr.s_addr)))
					{
						struct ip_mreq mreq;
						mreq.imr_multiaddr = sin->sin_addr;
						mreq.imr_interface.s_addr = htonl(INADDR_ANY);
						if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
#ifdef _WIN32
									   (const char *)&mreq,
#else
									   &mreq,
#endif
									   sizeof mreq) < 0)
						{
							perror("setsockopt");
							socket_close(fd);
							continue;
						}
					}
				}
				else if (ai->ai_family == AF_INET6)
				{
					/* TODO: IPv6 multicast */
				}
			}

			if (platform_socket_bind(fd, host, atoi(port)) < 0)
			{
				platform_socket_close(fd);
				continue;
			}
		}
		else
		{
			if (platform_socket_connect(fd, host, atoi(port)) < 0)
			{
				platform_socket_close(fd);
				continue;
			}
		}
		break;
	}

	if (!ai)
	{
		freeaddrinfo(res);
		free(addrstr);
#ifdef _WIN32
		fatal("could not open socket: %d", WSAGetLastError());
#else
		fatal("could not open socket: %s", strerror(errno));
#endif
	}

	freeaddrinfo(res);
	free(addrstr);
	return fd;
}

void socket_close(socket_t fd)
{
	platform_socket_close(fd);
}
