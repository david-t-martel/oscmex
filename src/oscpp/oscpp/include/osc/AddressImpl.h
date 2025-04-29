// filepath: c:\codedev\auricleinc\oscmex\src\AddressImpl.h
/*
 *  OSCPP - Open Sound Control C++ (OSCPP) Library.
 *  This file declares the AddressImpl class, which is responsible for managing OSC addresses,
 *  including sending data and handling socket operations.
 */

#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "osc/Exceptions.h"  // Add explicit inclusion of Exceptions header
#include "osc/Types.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define SOCKET_TYPE SOCKET
#define INVALID_SOCKET_VALUE INVALID_SOCKET
#define SOCKET_ERROR_VALUE SOCKET_ERROR
#define CLOSE_SOCKET closesocket
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#define SOCKET_TYPE int
#define INVALID_SOCKET_VALUE (-1)
#define SOCKET_ERROR_VALUE (-1)
#define CLOSE_SOCKET close
#endif

namespace osc {

    // Maximum allowed size for OSC messages to prevent excessive memory allocation
    // 16MB is a reasonable upper limit for most applications
    constexpr size_t MAX_MESSAGE_SIZE = 16777216;  // 16 MiB

    class AddressImpl {
       public:
        /**
         * @brief Construct a new Address Implementation object
         *
         * @param host The hostname or IP address to connect to
         * @param port The port to connect to
         * @param protocol The transport protocol to use
         * @throws OSCException if the address cannot be resolved or the socket cannot be created
         */
        AddressImpl(std::string host, std::string port, Protocol protocol);

        /**
         * @brief Destroy the Address Implementation object
         */
        ~AddressImpl();

        /**
         * @brief Send data to the address
         *
         * @param data The data to send
         * @return true if the data was sent successfully
         * @throws OSCException if the data cannot be sent
         */
        bool send(const std::vector<std::byte> &data);

        /**
         * @brief Get the URL representation of this address
         *
         * @return std::string The URL
         */
        std::string url() const;

        /**
         * @brief Get the hostname
         *
         * @return std::string The hostname
         */
        std::string host() const { return host_; }

        /**
         * @brief Get the port
         *
         * @return std::string The port
         */
        std::string port() const { return port_; }

        /**
         * @brief Get the protocol
         *
         * @return Protocol The protocol
         */
        Protocol protocol() const { return protocol_; }

        /**
         * @brief Set the Time To Live for multicast
         *
         * @param ttl The TTL value (1-255)
         * @return true if successful
         * @throws OSCException if the TTL cannot be set
         */
        bool setTTL(int ttl);

        /**
         * @brief Get the Time To Live for multicast
         *
         * @return int The TTL value
         */
        int getTTL() const { return ttl_; }

        /**
         * @brief Set the No Delay option for TCP
         *
         * @param enable Whether to enable or disable Nagle's algorithm
         * @return true if successful
         * @throws OSCException if the option cannot be set
         */
        bool setNoDelay(bool enable);

        /**
         * @brief Set the socket timeout
         *
         * @param timeout The timeout value
         * @return true if successful
         * @throws OSCException if the timeout cannot be set
         */
        bool setTimeout(std::chrono::milliseconds timeout);

        /**
         * @brief Check if the socket is valid
         *
         * @return true if the socket is valid
         */
        bool isValid() const { return socket_ != INVALID_SOCKET_VALUE; }

       private:
        std::string host_;
        std::string port_;
        Protocol protocol_;
        SOCKET_TYPE socket_ = INVALID_SOCKET_VALUE;
        int ttl_ = 1;
        bool connected_ = false;
        struct addrinfo *addrInfo_ = nullptr;

        /**
         * @brief Initialize the socket
         *
         * @return true if successful
         * @throws OSCException if the socket cannot be created
         */
        bool initializeSocket();

        /**
         * @brief Resolve the address
         *
         * @return true if successful
         * @throws OSCException if the address cannot be resolved
         */
        bool resolveAddress();

        /**
         * @brief Clean up resources
         */
        void cleanup();

        /**
         * @brief Initialize the networking subsystem
         *
         * @return true if successful
         * @throws OSCException if the networking subsystem cannot be initialized
         */
        static bool initializeNetworking();

        /**
         * @brief Clean up the networking subsystem
         */
        static void cleanupNetworking();

        /**
         * @brief Flag indicating if networking has been initialized
         */
        static bool networkingInitialized;

        /**
         * @brief Get the system error code and convert to a string message
         *
         * @return std::string The error message
         */
        std::string getSystemErrorMessage() const;

        /**
         * @brief Get the system error code
         *
         * @return int The error code
         */
        int getSystemErrorCode() const;
    };

}  // namespace osc
