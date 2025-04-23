#include "AddressImpl.h"
#include <cstring>
#include <sstream>
#include <algorithm>

#ifdef _WIN32
// Define ssize_t for Windows
#ifdef _WIN64
typedef __int64 ssize_t;
#else
typedef int ssize_t;
#endif
#endif

namespace osc
{

    // Initialize the static member
    bool AddressImpl::networkingInitialized = false;

    // Platform-specific initialization of networking subsystem
    bool AddressImpl::initializeNetworking()
    {
#ifdef _WIN32
        // Initialize Winsock
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        {
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
            networkingInitialized = initializeNetworking();
        }

        // Initialize socket and resolve address
        if (networkingInitialized)
        {
            if (resolveAddress())
            {
                initializeSocket();
            }
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
            socket_ = INVALID_SOCKET_VALUE;
        }

        if (addrInfo_)
        {
            freeaddrinfo(addrInfo_);
            addrInfo_ = nullptr;
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
            if (socket_ == INVALID_SOCKET_VALUE)
            {
                continue;
            }

            // For TCP, we need to establish a connection
            if (protocol_ == Protocol::TCP)
            {
                if (connect(socket_, addr->ai_addr, addr->ai_addrlen) != SOCKET_ERROR_VALUE)
                {
                    connected_ = true;
                    break;
                }

                CLOSE_SOCKET(socket_);
                socket_ = INVALID_SOCKET_VALUE;
            }
            else
            {
                // For UDP we're done
                break;
            }
        }

        if (socket_ == INVALID_SOCKET_VALUE)
        {
#ifdef _WIN32
            errorCode_ = WSAGetLastError();
#else
            errorCode_ = errno;
#endif
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
            // Windows doesn't support UNIX domain sockets in older versions
            errorCode_ = WSAEOPNOTSUPP;
            return false;
#else
            hints.ai_family = AF_UNIX;
            hints.ai_socktype = SOCK_STREAM;
#endif
            break;
        }

        // Resolve the address
        int result = getaddrinfo(host_.c_str(), port_.c_str(), &hints, &addrInfo_);
        if (result != 0)
        {
#ifdef _WIN32
            errorCode_ = WSAGetLastError();
#else
            errorCode_ = result;
#endif
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
                return false;
            }

            // For TCP, first send the size
            uint32_t size = htonl(static_cast<uint32_t>(data.size()));
            if (::send(socket_, reinterpret_cast<const char *>(&size), sizeof(size), 0) != sizeof(size))
            {
#ifdef _WIN32
                errorCode_ = WSAGetLastError();
#else
                errorCode_ = errno;
#endif
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
            // Not supported on Windows
            return false;
#else
            if (!connected_)
            {
                return false;
            }

            // UNIX domain sockets use stream semantics like TCP
            uint32_t size = htonl(static_cast<uint32_t>(data.size()));
            if (::send(socket_, reinterpret_cast<const char *>(&size), sizeof(size), 0) != sizeof(size))
            {
                errorCode_ = errno;
                return false;
            }

            bytesSent = ::send(socket_,
                               reinterpret_cast<const char *>(data.data()),
                               data.size(),
                               0);
#endif
            break;
        }

        if (bytesSent == SOCKET_ERROR_VALUE)
        {
#ifdef _WIN32
            errorCode_ = WSAGetLastError();
#else
            errorCode_ = errno;
#endif
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
            // Check if IPv6 address (needs brackets)
            if (host_.find(':') != std::string::npos)
            {
                url << "[" << host_ << "]";
            }
            else
            {
                url << host_;
            }

            url << ":" << port_;
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
            ttl_ = ttl;
            return true;
        }

#ifdef _WIN32
        errorCode_ = WSAGetLastError();
#else
        errorCode_ = errno;
#endif
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
        int result = setsockopt(socket_,
                                IPPROTO_TCP,
                                TCP_NODELAY,
                                reinterpret_cast<const char *>(&flag),
                                sizeof(flag));

        if (result != SOCKET_ERROR_VALUE)
        {
            return true;
        }

#ifdef _WIN32
        errorCode_ = WSAGetLastError();
#else
        errorCode_ = errno;
#endif
        return false;
    }

    // Set socket timeout
    bool AddressImpl::setTimeout(std::chrono::milliseconds timeout)
    {
        if (socket_ == INVALID_SOCKET_VALUE)
        {
            return false;
        }

#ifdef _WIN32
        DWORD timeoutVal = static_cast<DWORD>(timeout.count());
        int result = setsockopt(socket_,
                                SOL_SOCKET,
                                SO_SNDTIMEO,
                                reinterpret_cast<const char *>(&timeoutVal),
                                sizeof(timeoutVal));
#else
        struct timeval tv;
        tv.tv_sec = static_cast<time_t>(timeout.count() / 1000);
        tv.tv_usec = static_cast<suseconds_t>((timeout.count() % 1000) * 1000);
        int result = setsockopt(socket_,
                                SOL_SOCKET,
                                SO_SNDTIMEO,
                                &tv,
                                sizeof(tv));
#endif

        if (result != SOCKET_ERROR_VALUE)
        {
            return true;
        }

#ifdef _WIN32
        errorCode_ = WSAGetLastError();
#else
        errorCode_ = errno;
#endif
        return false;
    }

    // Get human-readable error message
    std::string AddressImpl::getErrorMessage() const
    {
        if (errorCode_ == 0)
        {
            return "No error";
        }

#ifdef _WIN32
        char *msgBuf = nullptr;
        size_t size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, errorCode_, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPSTR>(&msgBuf), 0, nullptr);

        std::string msg = "Unknown error";
        if (msgBuf)
        {
            msg = std::string(msgBuf, size);
            LocalFree(msgBuf);
        }

        return msg;
#else
        return std::string(strerror(errorCode_));
#endif
    }

} // namespace osc
