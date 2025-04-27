#include "AddressImpl.h"
#include "Address.h"
#include <cstring>
#include <sstream>
#include <regex>

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
// Define ssize_t for Windows
#ifdef _WIN64
typedef __int64 ssize_t;
#else
typedef int ssize_t;
#endif
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif

namespace osc
{

    // Initialize the static member
    bool AddressImpl::networkingInitialized = false;

    // Platform-specific initialization of networking subsystem
    bool AddressImpl::initializeNetworking()
    {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        {
            errorCode_ = WSAGetLastError();
            return false;
        }
#endif

        networkingInitialized = true;
        return true;
    }

    // Platform-specific cleanup of networking subsystem
    void AddressImpl::cleanupNetworking()
    {
#ifdef _WIN32
        WSACleanup();
#endif

        networkingInitialized = false;
    }

    // Constructor
    AddressImpl::AddressImpl(std::string host, std::string port, Protocol protocol)
        : host_(std::move(host)), port_(std::move(port)), protocol_(protocol)
    {

        // Initialize networking subsystem if needed
        if (!networkingInitialized)
        {
            initializeNetworking();
        }

        // Initialize socket and resolve address
        if (networkingInitialized)
        {
            resolveAddress();
            initializeSocket();
        }
    }

    // Destructor
    AddressImpl::~AddressImpl()
    {
        cleanup();
    }

    // Clean up resources
    void AddressImpl::cleanup()
    {
        if (socket_ != INVALID_SOCKET_VALUE)
        {
            CLOSE_SOCKET(socket_);
        }

        if (addrInfo_)
        {
            freeaddrinfo(addrInfo_);
        }
    }

    // Initialize the socket
    bool AddressImpl::initializeSocket()
    {
        if (!addrInfo_)
        {
            return false;
        }

        // Try each address until we successfully create a socket
        struct addrinfo *addr;
        for (addr = addrInfo_; addr != nullptr; addr = addr->ai_next)
        {
            socket_ = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
            if (socket_ != INVALID_SOCKET_VALUE)
            {
                if (bind(socket_, addr->ai_addr, addr->ai_addrlen) == 0)
                {
                    break; // Successfully bound
                }
                CLOSE_SOCKET(socket_);
            }
        }

        if (socket_ == INVALID_SOCKET_VALUE)
        {
            return false;
        }

        return true;
    }

    // Resolve the address
    bool AddressImpl::resolveAddress()
    {
        struct addrinfo hints;
        std::memset(&hints, 0, sizeof(hints));

        // Set appropriate address family and socket type
        switch (protocol_)
        {
        case Protocol::UDP:
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_DGRAM;
            break;
        case Protocol::TCP:
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            break;
        case Protocol::UNIX:
#ifdef _WIN32
#else
            break;
#endif
        }

        // Resolve the address
        int result = getaddrinfo(host_.c_str(), port_.c_str(), &hints, &addrInfo_);
        if (result != 0)
        {
            return false;
        }

        return true;
    }

    // Send data over the socket
    bool AddressImpl::send(const std::vector<std::byte> &data)
    {
        if (socket_ == INVALID_SOCKET_VALUE || !addrInfo_)
        {
            return false;
        }

        ssize_t bytesSent = 0;

        switch (protocol_)
        {
        case Protocol::UDP:
            // For UDP, we need to specify the destination each time
            bytesSent = sendto(socket_,
                               reinterpret_cast<const char *>(data.data()),
                               data.size(),
                               0,
                               addrInfo_->ai_addr,
                               addrInfo_->ai_addrlen);
            break;

        case Protocol::TCP:
            if (!connected_)
            {
                // Connect to the server
                connect(socket_, addrInfo_->ai_addr, addrInfo_->ai_addrlen);
                connected_ = true;
            }

            // For TCP, first send the size
            uint32_t size = htonl(static_cast<uint32_t>(data.size()));
            if (::send(socket_, reinterpret_cast<const char *>(&size), sizeof(size), 0) != sizeof(size))
            {
                return false;
            }

            // Then send the actual data
            bytesSent = ::send(socket_,
                               reinterpret_cast<const char *>(data.data()),
                               data.size(),
                               0);
            break;

        case Protocol::UNIX:
#ifdef _WIN32
#else
            break;
#endif
        }

        if (bytesSent == SOCKET_ERROR_VALUE)
        {
            return false;
        }

        return static_cast<size_t>(bytesSent) == data.size();
    }

    // Generate a URL representing this address
    std::string AddressImpl::url() const
    {
        std::ostringstream url;

        // Add protocol
        switch (protocol_)
        {
        case Protocol::UDP:
            url << "osc.udp://";
            break;
        case Protocol::TCP:
            url << "osc.tcp://";
            break;
        case Protocol::UNIX:
            url << "osc.unix://";
            break;
        }

        // Add host and port (if applicable)
        if (protocol_ == Protocol::UNIX)
        {
            url << host_;
        }
        else
        {
            url << host_ << ":" << port_;
        }

        url << "/";

        return url.str();
    }

    // Set the TTL for multicast
    bool AddressImpl::setTTL(int ttl)
    {
        if (socket_ == INVALID_SOCKET_VALUE || protocol_ != Protocol::UDP)
        {
            return false;
        }

        // Clamp TTL to valid range
        ttl = std::max(1, std::min(ttl, 255));

        unsigned char ttlVal = static_cast<unsigned char>(ttl);
        int result = setsockopt(socket_,
                                IPPROTO_IP,
                                IP_MULTICAST_TTL,
                                reinterpret_cast<const char *>(&ttlVal),
                                sizeof(ttlVal));

        if (result != SOCKET_ERROR_VALUE)
        {
            return true;
        }

        return false;
    }

    // Disable Nagle's algorithm for TCP
    bool AddressImpl::setNoDelay(bool enable)
    {
        if (socket_ == INVALID_SOCKET_VALUE || protocol_ != Protocol::TCP)
        {
            return false;
        }

        int flag = enable ? 1 : 0;
        return setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&flag), sizeof(flag)) != SOCKET_ERROR_VALUE;
    }

    // Set socket timeout
    bool AddressImpl::setTimeout(std::chrono::milliseconds timeout)
    {
        if (socket_ == INVALID_SOCKET_VALUE)
        {
            return false;
        }

        struct timeval tv;
        tv.tv_sec = static_cast<long>(timeout.count() / 1000);
        tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);
        return setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&tv), sizeof(tv)) != SOCKET_ERROR_VALUE;
    }

    // Get human-readable error message
    std::string AddressImpl::getErrorMessage() const
    {
        return std::strerror(errorCode_);
    }

} // namespace osc
