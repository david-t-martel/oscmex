// filepath: c:\codedev\auricleinc\oscmex\src\AddressImpl.h
/*
 *  OSCPP - Open Sound Control C++ (OSCPP) Library.
 *  This file declares the AddressImpl class, which is responsible for managing OSC addresses,
 *  including sending data and handling socket operations.
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
        AddressImpl(std::string host, std::string port, Protocol protocol);
        ~AddressImpl();

        bool send(const std::vector<std::byte> &data);
        std::string url() const;
        std::string host() const { return host_; }
        std::string port() const { return port_; }
        Protocol protocol() const { return protocol_; }
        bool setTTL(int ttl);
        int getTTL() const { return ttl_; }
        bool setNoDelay(bool enable);
        bool setTimeout(std::chrono::milliseconds timeout);
        bool isValid() const { return socket_ != INVALID_SOCKET_VALUE; }
        int getErrorCode() const { return errorCode_; }
        std::string getErrorMessage() const;

    private:
        std::string host_;
        std::string port_;
        Protocol protocol_;
        SOCKET_TYPE socket_ = INVALID_SOCKET_VALUE;
        int ttl_ = 1;
        int errorCode_ = 0;
        bool connected_ = false;
        struct addrinfo *addrInfo_ = nullptr;

        bool initializeSocket();
        bool resolveAddress();
        void cleanup();
        static bool initializeNetworking();
        static void cleanupNetworking();
        static bool networkingInitialized;
    };

} // namespace osc