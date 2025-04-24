#ifndef SOCKET_H
#define SOCKET_H

#include "platform.h"

// Use the platform-defined socket type
typedef platform_socket_t socket_t;

// Define invalid socket using platform constant
#define INVALID_SOCKET PLATFORM_INVALID_SOCKET

/**
 * @brief Opens a socket using the given address
 *
 * @param addr Address string in format "protocol!host!port" (e.g., "udp!127.0.0.1!8000")
 * @param recv 1 to open for receiving (bind), 0 for sending (connect)
 * @return Socket handle or INVALID_SOCKET on error
 */
socket_t sockopen(const char *addr, int recv);

/**
 * @brief Closes a socket
 *
 * @param fd Socket handle
 */
void socket_close(socket_t fd);

/**
 * @brief Sends data on a socket
 *
 * @param fd Socket handle
 * @param buf Data buffer
 * @param len Data length
 * @return Number of bytes sent or -1 on error
 */
int socket_send(socket_t fd, const void *buf, size_t len);

/**
 * @brief Receives data from a socket
 *
 * @param fd Socket handle
 * @param buf Buffer to receive data
 * @param len Buffer size
 * @return Number of bytes received or -1 on error
 */
int socket_recv(socket_t fd, void *buf, size_t len);

/**
 * @brief Initializes the socket subsystem
 *
 * This should be called before any socket operations.
 *
 * @return 0 on success, non-zero on failure
 */
int socket_init(void);

/**
 * @brief Cleans up the socket subsystem
 *
 * This should be called when the application is done with sockets.
 */
void socket_cleanup(void);

#endif /* SOCKET_H */
