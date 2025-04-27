// filepath: c:\codedev\auricleinc\oscmex\src\ServerImpl.cpp
#include "ServerImpl.h"
#include <algorithm>
#include <iostream>
#include <cstring>

namespace osc
{

    // Initialize static networking variable
    bool ServerImpl::networkingInitialized = false;

    // Constructor
    ServerImpl::ServerImpl(std::string port, Protocol protocol)
        : port_(std::move(port)), protocol_(protocol)
    {
        // Initialize platform-specific networking
        if (!networkingInitialized)
        {
            initializeNetworking();
        }

        // Initialize socket and start listening
        initializeSocket();
    }

    // Destructor
    ServerImpl::~ServerImpl()
    {
        cleanup();
    }

    // Add a method handler
    MethodId ServerImpl::addMethod(const std::string &pathPattern, const std::string &typeSpec,
                                   MethodHandler handler)
    {
        std::lock_guard<std::mutex> lock(methodMutex_);

        // Generate a method ID
        MethodId id = nextMethodId_++;

        // Create a regex for pattern matching
        std::regex pathRegex;
        if (!convertPathToRegex(pathPattern, pathRegex))
        {
            throw OSCException(OSCException::ErrorCode::PatternError, "Invalid path pattern");
        }

        // Add the method to the map
        methods_[id] = Method{
            pathPattern, typeSpec, pathRegex, handler, false};

        return id;
    }

    // Remove a method handler
    bool ServerImpl::removeMethod(MethodId id)
    {
        std::lock_guard<std::mutex> lock(methodMutex_);

        // Find and remove the method
        auto it = methods_.find(id);
        if (it != methods_.end())
        {
            methods_.erase(it);
            return true;
        }

        return false;
    }

    // Wait for messages
    bool ServerImpl::wait(std::chrono::milliseconds timeout)
    {
        // Set up file descriptor sets for select/poll
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(socket_, &readfds);

        // Set up timeout
        timeval tv;
        tv.tv_sec = static_cast<long>(timeout.count() / 1000);
        tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);

        // Wait for data
        int result = select(static_cast<int>(socket_ + 1), &readfds, nullptr, nullptr,
                            (timeout.count() == 0) ? nullptr : &tv);

        return (result > 0);
    }

    // Receive and dispatch a message
    bool ServerImpl::receive(std::chrono::milliseconds timeout)
    {
        // Wait for data if timeout is specified
        if (timeout.count() > 0 && !wait(timeout))
        {
            return false;
        }

        // Buffer to hold the incoming message
        std::vector<std::byte> buffer(maxMessageSize_);

        // Receive data
        if (!receiveData(buffer, socket_, timeout))
        {
            return false;
        }

        // Process the received data
        if (buffer.empty())
        {
            return false;
        }

        // Dispatch the message
        Message message(buffer);
        dispatchMessage(message);

        return true;
    }

    // Get the port number
    int ServerImpl::port() const
    {
        return std::stoi(port_);
    }

    // Get the URL
    std::string ServerImpl::url() const
    {
        switch (protocol_)
        {
        case Protocol::UDP:
            return "osc.udp://" + host_ + ":" + port_;
        case Protocol::TCP:
            return "osc.tcp://" + host_ + ":" + port_;
        case Protocol::UNIX:
            return "osc.unix://" + path_;
        default:
            return "";
        }
    }

    // Initialize the socket
    bool ServerImpl::initializeSocket()
    {
        // Implementation of socket initialization
        return true;
    }

    // Clean up resources
    void ServerImpl::cleanup()
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

    // Platform-specific networking initialization
    bool ServerImpl::initializeNetworking()
    {
        // Implementation of networking initialization
        networkingInitialized = true;
        return true;
    }

    // Dispatch a message to registered handlers
    void ServerImpl::dispatchMessage(const Message &message)
    {
        // Implementation of message dispatching
    }

    // Match a message against registered methods
    bool ServerImpl::matchMethod(const std::string &path, const std::string &types,
                                 std::vector<Method *> &matchedMethods)
    {
        // Implementation of method matching
        return true;
    }

} // namespace osc