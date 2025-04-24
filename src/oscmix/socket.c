#include "platform.h"
#include "logging.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "socket.h"
#include "util.h"

socket_t sockopen(const char *addr, int recv)
{
	// Let's use the platform_socket_open function directly
	return platform_socket_open(addr, recv);
}

void socket_close(socket_t fd)
{
	platform_socket_close(fd);
}

int socket_send(socket_t fd, const void *buf, size_t len)
{
	return platform_socket_send(fd, buf, len, 0);
}

int socket_recv(socket_t fd, void *buf, size_t len)
{
	return platform_socket_recv(fd, buf, len, 0);
}

// Initialize socket subsystem
int socket_init(void)
{
	int result = platform_socket_init();
	if (result != 0)
	{
		log_error("Failed to initialize socket subsystem: %s", platform_strerror(platform_errno()));
	}
	else
	{
		log_debug("Socket subsystem initialized");
	}
	return result;
}

// Cleanup socket subsystem
void socket_cleanup(void)
{
	platform_socket_cleanup();
	log_debug("Socket subsystem cleaned up");
}
