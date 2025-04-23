#include "Address.h"
#include "Message.h"
#include "Bundle.h"
#include <regex>
#include <stdexcept>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#ifndef SSIZE_T_DEFINED
#ifdef _WIN64
typedef __int64 ssize_t;
#else
typedef int ssize_t;
#endif
#define SSIZE_T_DEFINED
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

    // Implementation class for Address
    class Address::AddressImpl
    {
    public:
        AddressImpl(std::string_view host, std::string_view port, Protocol protocol)
            : host_(host), port_(port), protocol_(protocol)
        {
            initialize();
        }

        ~AddressImpl()
        {
            cleanup();
        }

        bool send(const std::vector<std::byte> &data) const
        {
            if (!isValid())
            {
                return false;
            }

            switch (protocol_)
            {
            case Protocol::UDP:
                return sendUDP(data);
            case Protocol::TCP:
                return sendTCP(data);
            case Protocol::UNIX:
                return sendUNIX(data);
            default:
                return false;
            }
        }

        std::string url() const
        {
            std::string proto;
            switch (protocol_)
            {
            case Protocol::UDP:
                proto = "osc.udp://";
                break;
            case Protocol::TCP:
                proto = "osc.tcp://";
                break;
            case Protocol::UNIX:
                proto = "osc.unix://";
                break;
            }

            return proto + host_ + ":" + port_ + "/";
        }

        bool isValid() const
        {
            return socket_ != INVALID_SOCKET;
        }

        void setTTL(int ttl)
        {
            if (protocol_ != Protocol::UDP || !isValid())
            {
                return;
            }

            // Set TTL for multicast
            unsigned char ttlVal = static_cast<unsigned char>(ttl);
#ifdef _WIN32
            setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_TTL,
                       reinterpret_cast<const char *>(&ttlVal), sizeof(ttlVal));
#else
            setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_TTL,
                       &ttlVal, sizeof(ttlVal));
#endif

            ttl_ = ttl;
        }

        int getTTL() const
        {
            return ttl_;
        }

        void setNoDelay(bool enable)
        {
            if (protocol_ != Protocol::TCP || !isValid())
            {
                return;
            }

            // Set TCP_NODELAY
            int flag = enable ? 1 : 0;
#ifdef _WIN32
            setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY,
                       reinterpret_cast<const char *>(&flag), sizeof(flag));
#else
            setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY,
                       &flag, sizeof(flag));
#endif

            noDelay_ = enable;
        }

        void setTimeout(std::chrono::milliseconds timeout)
        {
            if (!isValid())
            {
                return;
            }

#ifdef _WIN32
            DWORD timeoutVal = static_cast<DWORD>(timeout.count());
            setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO,
                       reinterpret_cast<const char *>(&timeoutVal), sizeof(timeoutVal));
#else
            struct timeval tv;
            tv.tv_sec = static_cast<time_t>(timeout.count() / 1000);
            tv.tv_usec = static_cast<suseconds_t>((timeout.count() % 1000) * 1000);
            setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO,
                       &tv, sizeof(tv));
#endif

            timeout_ = timeout;
        }

        int getErrorCode() const
        {
#ifdef _WIN32
            return WSAGetLastError();
#else
            return errno;
#endif
        }

        std::string getErrorMessage() const
        {
            int errCode = getErrorCode();
#ifdef _WIN32
            char *s = NULL;
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL, errCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           (LPSTR)&s, 0, NULL);
            std::string message(s ? s : "Unknown error");
            LocalFree(s);
            return message;
#else
            return strerror(errCode);
#endif
        }

        // Getters for address info
        const std::string &host() const { return host_; }
        const std::string &port() const { return port_; }
        Protocol protocol() const { return protocol_; }

    private:
// Platform-specific socket type
#ifdef _WIN32
        using SocketType = SOCKET;
        static const SocketType INVALID_SOCKET = ::INVALID_SOCKET;
#else
        using SocketType = int;
        static const SocketType INVALID_SOCKET = -1;
#endif

        // Member variables
        std::string host_;
        std::string port_;
        Protocol protocol_;
        SocketType socket_ = INVALID_SOCKET;
        struct sockaddr_storage addr_;
        socklen_t addrLen_ = 0;
        int ttl_ = 1;
        bool noDelay_ = false;
        std::chrono::milliseconds timeout_ = std::chrono::milliseconds(0);

// Static initialization for Windows sockets
#ifdef _WIN32
        static bool initializeWSA()
        {
            static bool initialized = false;
            static WSADATA wsaData;
            if (!initialized)
            {
                initialized = (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
            }
            return initialized;
        }
#endif

        // Initialize the socket and address
        void initialize()
        {
#ifdef _WIN32
            if (!initializeWSA())
            {
                return;
            }
#endif

            // Create a hints structure for getaddrinfo
            struct addrinfo hints;
            memset(&hints, 0, sizeof(hints));

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
                return;
#else
                hints.ai_family = AF_UNIX;
                hints.ai_socktype = SOCK_STREAM;
#endif
                break;
            }

            // Resolve the address
            struct addrinfo *result = nullptr;
            int error = getaddrinfo(host_.c_str(), port_.c_str(), &hints, &result);
            if (error != 0)
            {
                return;
            }

            // Use the first valid address
            for (struct addrinfo *rp = result; rp != nullptr; rp = rp->ai_next)
            {
                socket_ = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
                if (socket_ == INVALID_SOCKET)
                {
                    continue;
                }

                // Store the address
                memcpy(&addr_, rp->ai_addr, rp->ai_addrlen);
                addrLen_ = static_cast<socklen_t>(rp->ai_addrlen);

                // For TCP, we need to connect the socket
                if (protocol_ == Protocol::TCP)
                {
                    if (connect(socket_, rp->ai_addr, rp->ai_addrlen) != 0)
                    {
#ifdef _WIN32
                        closesocket(socket_);
#else
                        close(socket_);
#endif
                        socket_ = INVALID_SOCKET;
                        continue;
                    }
                }

                // Found a working address
                break;
            }

            freeaddrinfo(result);
        }

        // Clean up resources
        void cleanup()
        {
            if (socket_ != INVALID_SOCKET)
            {
#ifdef _WIN32
                closesocket(socket_);
#else
                close(socket_);
#endif
                socket_ = INVALID_SOCKET;
            }
        }

        // Send data via UDP
        bool sendUDP(const std::vector<std::byte> &data) const
        {
            if (socket_ == INVALID_SOCKET)
            {
                return false;
            }

            ssize_t result = sendto(socket_,
                                    reinterpret_cast<const char *>(data.data()),
                                    data.size(), 0,
                                    reinterpret_cast<const struct sockaddr *>(&addr_),
                                    addrLen_);

            return (result == static_cast<ssize_t>(data.size()));
        }

        // Send data via TCP
        bool sendTCP(const std::vector<std::byte> &data) const
        {
            if (socket_ == INVALID_SOCKET)
            {
                return false;
            }

            // For TCP, we need to send the size first
            uint32_t size = htonl(static_cast<uint32_t>(data.size()));

            // Send size
            ssize_t sizeResult = send(socket_,
                                      reinterpret_cast<const char *>(&size),
                                      sizeof(size), 0);

            if (sizeResult != sizeof(size))
            {
                return false;
            }

            // Send data
            ssize_t dataResult = send(socket_,
                                      reinterpret_cast<const char *>(data.data()),
                                      data.size(), 0);

            return (dataResult == static_cast<ssize_t>(data.size()));
        }

        // Send data via UNIX domain socket
        bool sendUNIX(const std::vector<std::byte> &data) const
        {
#ifdef _WIN32
            // Windows doesn't support UNIX domain sockets in older versions
            return false;
#else
            if (socket_ == INVALID_SOCKET)
            {
                return false;
            }

            // UNIX domain sockets use stream semantics like TCP
            uint32_t size = htonl(static_cast<uint32_t>(data.size()));

            // Send size
            ssize_t sizeResult = send(socket_,
                                      reinterpret_cast<const char *>(&size),
                                      sizeof(size), 0);

            if (sizeResult != sizeof(size))
            {
                return false;
            }

            // Send data
            ssize_t dataResult = send(socket_,
                                      reinterpret_cast<const char *>(data.data()),
                                      data.size(), 0);

            return (dataResult == static_cast<ssize_t>(data.size()));
#endif
        }
    };

    // Address implementation using PIMPL pattern

    Address::Address(std::string_view host, std::string_view port, Protocol protocol)
        : impl_(std::make_unique<AddressImpl>(host, port, protocol))
    {
    }

    Address Address::fromUrl(std::string_view url)
    {
        // Parse URL in format "osc.proto://host:port/"
        std::string urlStr(url);

        // Parse protocol
        Protocol protocol = Protocol::UDP; // Default to UDP

        if (urlStr.find("osc.udp://") == 0)
        {
            protocol = Protocol::UDP;
            urlStr = urlStr.substr(8);
        }
        else if (urlStr.find("osc.tcp://") == 0)
        {
            protocol = Protocol::TCP;
            urlStr = urlStr.substr(8);
        }
        else if (urlStr.find("osc.unix://") == 0)
        {
            protocol = Protocol::UNIX;
            urlStr = urlStr.substr(9);
        }
        else
        {
            throw OSCException(OSCException::ErrorCode::InvalidAddress,
                               "Invalid OSC URL format: " + std::string(url));
        }

        // Extract host and port
        size_t colonPos = urlStr.find(':');
        size_t slashPos = urlStr.find('/', colonPos);

        if (colonPos == std::string::npos)
        {
            throw OSCException(OSCException::ErrorCode::InvalidAddress,
                               "Invalid OSC URL format, missing port in " + std::string(url));
        }

        std::string host = urlStr.substr(0, colonPos);
        std::string port;

        if (slashPos != std::string::npos)
        {
            port = urlStr.substr(colonPos + 1, slashPos - colonPos - 1);
        }
        else
        {
            port = urlStr.substr(colonPos + 1);
        }

        return Address(host, port, protocol);
    }

    Address::~Address() = default;

    Address::Address(Address &&other) noexcept = default;
    Address &Address::operator=(Address &&other) noexcept = default;

    Address::Address(const Address &other)
        : impl_(std::make_unique<AddressImpl>(other.impl_->host(), other.impl_->port(),
                                              other.impl_->protocol()))
    {

        // Copy other settings
        if (other.impl_->getTTL() != 1)
        {
            impl_->setTTL(other.impl_->getTTL());
        }

        // Note: timeout and nodelay settings are not copied as they
        // are implementation details
    }

    Address &Address::operator=(const Address &other)
    {
        if (this != &other)
        {
            impl_ = std::make_unique<AddressImpl>(other.impl_->host(), other.impl_->port(),
                                                  other.impl_->protocol());

            // Copy other settings
            if (other.impl_->getTTL() != 1)
            {
                impl_->setTTL(other.impl_->getTTL());
            }
        }
        return *this;
    }

    bool Address::send(const Message &message) const
    {
        if (!impl_ || !impl_->isValid())
        {
            return false;
        }

        std::vector<std::byte> data = message.serialize();
        return impl_->send(data);
    }

    bool Address::send(const Bundle &bundle) const
    {
        if (!impl_ || !impl_->isValid())
        {
            return false;
        }

        std::vector<std::byte> data = bundle.serialize();
        return impl_->send(data);
    }

    std::string Address::url() const
    {
        return impl_ ? impl_->url() : "osc://invalid/";
    }

    std::string Address::host() const
    {
        return impl_ ? impl_->host() : "";
    }

    std::string Address::port() const
    {
        return impl_ ? impl_->port() : "";
    }

    Protocol Address::protocol() const
    {
        return impl_ ? impl_->protocol() : Protocol::UDP;
    }

    void Address::setTTL(int ttl)
    {
        if (impl_)
        {
            impl_->setTTL(ttl);
        }
    }

    int Address::getTTL() const
    {
        return impl_ ? impl_->getTTL() : 1;
    }

    void Address::setNoDelay(bool enable)
    {
        if (impl_)
        {
            impl_->setNoDelay(enable);
        }
    }

    void Address::setTimeout(std::chrono::milliseconds timeout)
    {
        if (impl_)
        {
            impl_->setTimeout(timeout);
        }
    }

    bool Address::isValid() const
    {
        return impl_ && impl_->isValid();
    }

    int Address::getErrorCode() const
    {
        return impl_ ? impl_->getErrorCode() : 0;
    }

    std::string Address::getErrorMessage() const
    {
        return impl_ ? impl_->getErrorMessage() : "Invalid address";
    }

} // namespace osc
