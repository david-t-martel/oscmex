#include "osc/AddressImpl.h"

#include <cstring>
#include <regex>
#include <sstream>

#include "osc/Address.h"
#include "osc/Bundle.h"
#include "osc/Exceptions.h"

#ifdef _WIN32
#include <WS2tcpip.h>
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")
// Define ssize_t for Windows
#ifdef _WIN64
typedef __int64 ssize_t;
#else
typedef int ssize_t;
#endif
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace osc {

    // Initialize the static member
    bool AddressImpl::networkingInitialized = false;

    // Platform-specific initialization of networking subsystem
    bool AddressImpl::initializeNetworking() {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            int errorCode = WSAGetLastError();
            throw OSCException("Failed to initialize Windows networking (WSAStartup): Error " +
                                   std::to_string(errorCode),
                               OSCException::ErrorCode::NetworkingError);
        }
#endif

        networkingInitialized = true;
        return true;
    }

    // Platform-specific cleanup of networking subsystem
    void AddressImpl::cleanupNetworking() {
#ifdef _WIN32
        WSACleanup();
#endif

        networkingInitialized = false;
    }

    // Constructor
    AddressImpl::AddressImpl(std::string host, std::string port, Protocol protocol)
        : host_(std::move(host)), port_(std::move(port)), protocol_(protocol) {
        // Initialize networking subsystem if needed
        if (!networkingInitialized) {
            try {
                initializeNetworking();
            } catch (const OSCException &e) {
                // Re-throw with improved context
                throw OSCException("Failed to initialize networking for address " + host_ + ":" +
                                       port_ + ": " + e.what(),
                                   e.code());
            }
        }

        // Initialize socket and resolve address
        if (networkingInitialized) {
            try {
                if (!resolveAddress()) {
                    throw AddressException("Failed to resolve host '" + host_ + "' with port '" +
                                               port_ + "': " + getSystemErrorMessage(),
                                           OSCException::ErrorCode::AddressError);
                }

                if (!initializeSocket()) {
                    throw SocketException("Failed to initialize socket for " + host_ + ":" + port_ +
                                              ": " + getSystemErrorMessage(),
                                          OSCException::ErrorCode::SocketError);
                }
            } catch (const OSCException &e) {
                // Clean up any resources that might have been allocated
                cleanup();
                // Re-throw the exception
                throw;
            } catch (const std::exception &e) {
                // Clean up any resources that might have been allocated
                cleanup();
                // Wrap in our standard exception
                throw AddressException("Address initialization error: " + std::string(e.what()),
                                       OSCException::ErrorCode::AddressError);
            }
        }
    }

    // Destructor
    AddressImpl::~AddressImpl() { cleanup(); }

    // Clean up resources
    void AddressImpl::cleanup() {
        if (socket_ != INVALID_SOCKET_VALUE) {
            CLOSE_SOCKET(socket_);
            socket_ = INVALID_SOCKET_VALUE;
        }

        if (addrInfo_) {
            freeaddrinfo(addrInfo_);
            addrInfo_ = nullptr;
        }
    }

    // Initialize the socket
    bool AddressImpl::initializeSocket() {
        if (!addrInfo_) {
            return false;
        }

        // Try each address until we successfully create a socket
        struct addrinfo *addr;
        for (addr = addrInfo_; addr != nullptr; addr = addr->ai_next) {
            socket_ = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
            if (socket_ != INVALID_SOCKET_VALUE) {
                // For client sockets (which is what AddressImpl is designed for),
                // we don't need to bind to a specific local address.
                // This allows the OS to choose an appropriate port.
                break;
            }
        }

        if (socket_ == INVALID_SOCKET_VALUE) {
            return false;
        }

        return true;
    }

    // Resolve the address
    bool AddressImpl::resolveAddress() {
        struct addrinfo hints;
        std::memset(&hints, 0, sizeof(hints));

        // Set appropriate address family and socket type
        switch (protocol_) {
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
                throw PlatformException("UNIX domain sockets not supported on Windows",
                                        OSCException::ErrorCode::NotImplemented);
#else
                hints.ai_family = AF_UNIX;
                hints.ai_socktype = SOCK_STREAM;
                break;
#endif
        }

        // Resolve the address
        int result = getaddrinfo(host_.c_str(), port_.c_str(), &hints, &addrInfo_);
        if (result != 0) {
            std::string errorMsg;
#ifdef _WIN32
            errorMsg = gai_strerrorA(result);
#else
            errorMsg = gai_strerror(result);
#endif
            throw AddressException(
                "Failed to resolve host '" + host_ + "' with port '" + port_ + "': " + errorMsg,
                OSCException::ErrorCode::AddressError);
        }

        return true;
    }

    // Send data over the socket
    bool AddressImpl::send(const std::vector<std::byte> &data) {
        if (socket_ == INVALID_SOCKET_VALUE || !addrInfo_) {
            throw SocketException("Cannot send OSC data: Socket not initialized",
                                  OSCException::ErrorCode::SocketError);
        }

        if (data.size() > MAX_MESSAGE_SIZE) {
            throw MessageSizeException("Message exceeds maximum allowed size (" +
                                           std::to_string(data.size()) + " > " +
                                           std::to_string(MAX_MESSAGE_SIZE) + " bytes)",
                                       OSCException::ErrorCode::MessageTooLarge);
        }

        ssize_t bytesSent = 0;

        try {
            switch (protocol_) {
                case Protocol::UDP:
                    // For UDP, we need to specify the destination each time
                    bytesSent = sendto(socket_, reinterpret_cast<const char *>(data.data()),
                                       data.size(), 0, addrInfo_->ai_addr, addrInfo_->ai_addrlen);
                    break;

                case Protocol::TCP:
                    if (!connected_) {
                        // Connect to the server
                        if (connect(socket_, addrInfo_->ai_addr, addrInfo_->ai_addrlen) ==
                            SOCKET_ERROR_VALUE) {
                            throw NetworkException("Failed to connect to " + host_ + ":" + port_ +
                                                       ": " + getSystemErrorMessage(),
                                                   OSCException::ErrorCode::NetworkError);
                        }
                        connected_ = true;
                    }

                    // For TCP, first send the size
                    uint32_t size = htonl(static_cast<uint32_t>(data.size()));
                    if (::send(socket_, reinterpret_cast<const char *>(&size), sizeof(size), 0) !=
                        sizeof(size)) {
                        throw SerializationException(
                            "Failed to send message size: " + getSystemErrorMessage(),
                            OSCException::ErrorCode::SerializationError);
                    }

                    // Then send the actual data
                    bytesSent = ::send(socket_, reinterpret_cast<const char *>(data.data()),
                                       data.size(), 0);
                    break;

                case Protocol::UNIX:
#ifdef _WIN32
                    throw PlatformException("UNIX domain sockets not supported on this platform",
                                            OSCException::ErrorCode::NotImplemented);
#else
                    if (!connected_) {
                        // Connect to the server
                        if (connect(socket_, addrInfo_->ai_addr, addrInfo_->ai_addrlen) ==
                            SOCKET_ERROR_VALUE) {
                            throw NetworkException("Failed to connect to UNIX socket " + host_ +
                                                       ": " + getSystemErrorMessage(),
                                                   ErrorCode::NetworkError);
                        }
                        connected_ = true;
                    }
                    // Send the data
                    bytesSent = ::send(socket_, reinterpret_cast<const char *>(data.data()),
                                       data.size(), 0);
                    break;
#endif
            }

            if (bytesSent == SOCKET_ERROR_VALUE) {
                throw NetworkException("Error sending OSC data: " + getSystemErrorMessage(),
                                       OSCException::ErrorCode::NetworkError);
            }

            // Check if all data was sent
            if (static_cast<size_t>(bytesSent) != data.size()) {
                throw NetworkException("Incomplete OSC data transmission: Sent " +
                                           std::to_string(bytesSent) + " of " +
                                           std::to_string(data.size()) + " bytes",
                                       OSCException::ErrorCode::NetworkError);
            }

            return true;
        } catch (const OSCException &) {
            // Re-throw OSC exceptions
            throw;
        } catch (const std::exception &e) {
            // Wrap other exceptions
            throw NetworkException("Error sending OSC data: " + std::string(e.what()),
                                   OSCException::ErrorCode::NetworkError);
        }
    }

    // Generate a URL representing this address
    std::string AddressImpl::url() const {
        std::ostringstream url;

        // Add protocol
        switch (protocol_) {
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
        if (protocol_ == Protocol::UNIX) {
            url << host_;
        } else {
            url << host_ << ":" << port_;
        }

        url << "/";

        return url.str();
    }

    // Set the TTL for multicast
    bool AddressImpl::setTTL(int ttl) {
        if (socket_ == INVALID_SOCKET_VALUE || protocol_ != Protocol::UDP) {
            throw InvalidArgumentException("Cannot set TTL: Invalid socket or protocol not UDP",
                                           OSCException::ErrorCode::InvalidArgument);
        }

        // Clamp TTL to valid range
        ttl = std::max(1, std::min(ttl, 255));

        unsigned char ttlVal = static_cast<unsigned char>(ttl);
        int result = setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_TTL,
                                reinterpret_cast<const char *>(&ttlVal), sizeof(ttlVal));

        if (result == SOCKET_ERROR_VALUE) {
            throw NetworkException("Failed to set multicast TTL: " + getSystemErrorMessage(),
                                   OSCException::ErrorCode::NetworkError);
        }

        ttl_ = ttl;
        return true;
    }

    // Disable Nagle's algorithm for TCP
    bool AddressImpl::setNoDelay(bool enable) {
        if (socket_ == INVALID_SOCKET_VALUE || protocol_ != Protocol::TCP) {
            throw InvalidArgumentException(
                "Cannot set TCP_NODELAY: Invalid socket or protocol not TCP",
                OSCException::ErrorCode::InvalidArgument);
        }

        int flag = enable ? 1 : 0;
        int result = setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY,
                                reinterpret_cast<const char *>(&flag), sizeof(flag));

        if (result == SOCKET_ERROR_VALUE) {
            throw NetworkException("Failed to set TCP_NODELAY option: " + getSystemErrorMessage(),
                                   OSCException::ErrorCode::NetworkError);
        }

        return true;
    }

    // Set socket timeout
    bool AddressImpl::setTimeout(std::chrono::milliseconds timeout) {
        if (socket_ == INVALID_SOCKET_VALUE) {
            throw SocketException("Cannot set socket timeout: Invalid socket",
                                  OSCException::ErrorCode::SocketError);
        }

        struct timeval tv;
        tv.tv_sec = static_cast<long>(timeout.count() / 1000);
        tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);

        int result = setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO,
                                reinterpret_cast<const char *>(&tv), sizeof(tv));

        if (result == SOCKET_ERROR_VALUE) {
            throw NetworkException("Failed to set socket timeout: " + getSystemErrorMessage(),
                                   OSCException::ErrorCode::NetworkError);
        }

        return true;
    }

    // Get human-readable error message from system error code
    std::string AddressImpl::getSystemErrorMessage() const {
#ifdef _WIN32
        int errorCode = WSAGetLastError();
        char buffer[256];
        if (FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                           errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer,
                           sizeof(buffer), NULL)) {
            // Trim trailing whitespace and newlines
            std::string message(buffer);
            message.erase(message.find_last_not_of(" \n\r\t") + 1);
            return message;
        }
        return "Unknown error " + std::to_string(errorCode);
#else
        return std::strerror(errno);
#endif
    }

    // Get system error code
    int AddressImpl::getSystemErrorCode() const {
#ifdef _WIN32
        return WSAGetLastError();
#else
        return errno;
#endif
    }

}  // namespace osc
