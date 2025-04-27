/*
 *  OSCPP - Open Sound Control C++ (OSCPP) Library.
 */

#pragma once

#include "Types.h"
#include <string>
#include <memory>
#include <chrono>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define SOCKET_TYPE SOCKET
#define INVALID_SOCKET_VALUE INVALID_SOCKET
#define SOCKET_ERROR_VALUE SOCKET_ERROR
#define CLOSE_SOCKET closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#define SOCKET_TYPE int
#define INVALID_SOCKET_VALUE (-1)
#define SOCKET_ERROR_VALUE (-1)
#define CLOSE_SOCKET close
#endif

namespace osc
{

    class AddressImpl
    {
    public:
        /**
         * @brief Create a new OSC address implementation
         * @param host Hostname or IP address
         * @param port Port number or service name
         * @param protocol Transport protocol
         */
        AddressImpl(std::string host, std::string port, Protocol protocol);

        /**
         * @brief Destructor - cleans up sockets and resources
         */
        ~AddressImpl();

        /**
         * @brief Send binary data to this address
         * @param data The data buffer to send
         * @return true if successful, false on error
         */
        bool send(const std::vector<std::byte> &data);

        /**
         * @brief Get the URL of this address
         * @return URL in format "osc.udp://host:port/"
         */
        std::string url() const;

        /**
         * @brief Get the hostname
         * @return Hostname as string
         */
        std::string host() const { return host_; }

        /**
         * @brief Get the port
         * @return Port as string
         */
        std::string port() const { return port_; }

        /**
         * @brief Get the protocol
         * @return Protocol enum value
         */
        Protocol protocol() const { return protocol_; }

        /**
         * @brief Set the TTL for multicast packets
         * @param ttl TTL value (1-255)
         * @return true if successful, false on error
         */
        bool setTTL(int ttl);

        /**
         * @brief Get the current TTL value
         * @return TTL value
         */
        int getTTL() const { return ttl_; }

        /**
         * @brief Enable/disable TCP_NODELAY (Nagle's algorithm)
         * @param enable True to enable option, false to disable
         * @return true if successful, false on error
         */
        bool setNoDelay(bool enable);

        /**
         * @brief Set socket timeout
         * @param timeout Timeout in milliseconds
         * @return true if successful, false on error
         */
        bool setTimeout(std::chrono::milliseconds timeout);

        /**
         * @brief Check if the address is valid and can be used
         * @return true if valid, false otherwise
         */
        bool isValid() const { return socket_ != INVALID_SOCKET_VALUE; }

        /**
         * @brief Get the last error code from socket operations
         * @return Platform-specific error code
         */
        int getErrorCode() const { return errorCode_; }

        /**
         * @brief Get the error message for the last error
         * @return Error message string
         */
        std::string getErrorMessage() const;

    private:
        std::string host_;  // Hostname or IP
        std::string port_;  // Port or service name
        Protocol protocol_; // UDP, TCP, or UNIX
        SOCKET_TYPE socket_ = INVALID_SOCKET_VALUE;
        int ttl_ = 1;                         // Time-to-live for multicast
        int errorCode_ = 0;                   // Last error code
        bool connected_ = false;              // Connection state (for TCP)
        struct addrinfo *addrInfo_ = nullptr; // Resolved address info

        // Helper methods
        bool initializeSocket();
        bool resolveAddress();
        void cleanup();

        // Platform-specific socket initialization
        static bool initializeNetworking();
        static void cleanupNetworking();
        static bool networkingInitialized;
    };

} // namespace osc
