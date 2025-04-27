// filepath: c:\codedev\auricleinc\oscmex\src\ServerImpl.cpp
#include "ServerImpl.h"
#include "Message.h"
#include "Bundle.h"
#include <algorithm>
#include <iostream>
#include <cstring>
#include <regex>
#include <functional>

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
    // Initialize static networking variable
    bool ServerImpl::networkingInitialized = false;

    // Constructor
    ServerImpl::ServerImpl(std::string port, Protocol protocol)
        : port_(std::move(port)), protocol_(protocol), socket_(INVALID_SOCKET_VALUE),
          nextMethodId_(1), bundleStartHandler_(nullptr), bundleEndHandler_(nullptr),
          errorHandler_(nullptr), maxMessageSize_(65536) // 64KB default max message size
    {
        // Initialize platform-specific networking
        if (!networkingInitialized)
        {
            networkingInitialized = initializeNetworking();
        }

        // Initialize socket and start listening
        if (!initializeSocket())
        {
            if (errorHandler_)
            {
                errorHandler_(getLastSocketError(), "Failed to initialize socket", "ServerImpl::initializeSocket");
            }
        }
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

        // Create a method object with regex for pattern matching
        Method method;
        method.id = id;
        method.pathPattern = pathPattern;
        method.typeSpec = typeSpec;
        method.handler = handler;
        method.isDefault = false;

        // Attempt to convert the OSC pattern to regex
        try
        {
            method.pathRegex = convertPathToRegex(pathPattern);
        }
        catch (const std::exception &e)
        {
            throw OSCException(OSCException::ErrorCode::PatternError,
                               "Invalid path pattern: " + std::string(e.what()));
        }

        // Add the method to the map
        methods_[id] = method;

        return id;
    }

    // Add a default method handler
    MethodId ServerImpl::addDefaultMethod(MethodHandler handler)
    {
        std::lock_guard<std::mutex> lock(methodMutex_);

        // Generate a method ID
        MethodId id = nextMethodId_++;

        // Create a default method object
        Method method;
        method.id = id;
        method.pathPattern = ""; // Empty pattern for default method
        method.typeSpec = "";    // Empty type spec for default method
        method.handler = handler;
        method.isDefault = true;

        // Add the method to the map
        methods_[id] = method;

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

    // Set bundle handlers
    void ServerImpl::setBundleHandlers(BundleStartHandler startHandler, BundleEndHandler endHandler)
    {
        bundleStartHandler_ = startHandler;
        bundleEndHandler_ = endHandler;
    }

    // Set error handler
    void ServerImpl::setErrorHandler(ErrorHandler handler)
    {
        errorHandler_ = handler;
    }

    // Wait for messages
    bool ServerImpl::wait(std::chrono::milliseconds timeout)
    {
        if (socket_ == INVALID_SOCKET_VALUE)
        {
            return false;
        }

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
                            (timeout.count() >= 0) ? &tv : nullptr);

        if (result == SOCKET_ERROR_VALUE)
        {
            if (errorHandler_)
            {
                errorHandler_(getLastSocketError(), "Error in select/poll", "ServerImpl::wait");
            }
            return false;
        }

        return (result > 0);
    }

    // Receive and dispatch a message
    bool ServerImpl::receive(std::chrono::milliseconds timeout)
    {
        if (socket_ == INVALID_SOCKET_VALUE)
        {
            return false;
        }

        // Wait for data if timeout is specified and > 0
        if (timeout.count() > 0 && !wait(timeout))
        {
            return false;
        }

        // Buffer to hold the incoming message
        std::vector<std::byte> buffer(maxMessageSize_);

        // Receive data
        ssize_t bytesReceived = 0;
        sockaddr_storage senderAddr;
        socklen_t senderAddrLen = sizeof(senderAddr);

        switch (protocol_)
        {
        case Protocol::UDP:
            // For UDP, use recvfrom to get sender address
            bytesReceived = recvfrom(socket_,
                                     reinterpret_cast<char *>(buffer.data()),
                                     buffer.size(),
                                     0,
                                     reinterpret_cast<sockaddr *>(&senderAddr),
                                     &senderAddrLen);
            break;

        case Protocol::TCP:
            // For TCP, we need to handle size-prefixed messages
            {
                // First read the 4-byte size
                uint32_t messageSize = 0;
                bytesReceived = recv(socket_,
                                     reinterpret_cast<char *>(&messageSize),
                                     sizeof(messageSize),
                                     0);

                if (bytesReceived != sizeof(messageSize))
                {
                    if (bytesReceived == 0)
                    {
                        // Connection closed
                        if (errorHandler_)
                        {
                            errorHandler_(0, "TCP connection closed", "ServerImpl::receive");
                        }
                    }
                    else if (bytesReceived < 0)
                    {
                        // Error
                        if (errorHandler_)
                        {
                            errorHandler_(getLastSocketError(), "Error receiving TCP message size", "ServerImpl::receive");
                        }
                    }
                    return false;
                }

                // Convert from network byte order
                messageSize = ntohl(messageSize);

                // Check against maximum size
                if (messageSize > maxMessageSize_)
                {
                    if (errorHandler_)
                    {
                        errorHandler_(EINVAL, "TCP message too large", "ServerImpl::receive");
                    }
                    return false;
                }

                // Now read the message data
                bytesReceived = recv(socket_,
                                     reinterpret_cast<char *>(buffer.data()),
                                     messageSize,
                                     0);

                if (static_cast<uint32_t>(bytesReceived) != messageSize)
                {
                    if (bytesReceived <= 0)
                    {
                        if (errorHandler_)
                        {
                            errorHandler_(getLastSocketError(), "Error receiving TCP message data", "ServerImpl::receive");
                        }
                    }
                    return false;
                }
            }
            break;

        case Protocol::UNIX:
            // For UNIX sockets, also use size-prefixed messages like TCP
            {
                // Similar to TCP handling
                uint32_t messageSize = 0;
                bytesReceived = recv(socket_,
                                     reinterpret_cast<char *>(&messageSize),
                                     sizeof(messageSize),
                                     0);

                if (bytesReceived != sizeof(messageSize))
                {
                    return false;
                }

                messageSize = ntohl(messageSize);

                if (messageSize > maxMessageSize_)
                {
                    return false;
                }

                bytesReceived = recv(socket_,
                                     reinterpret_cast<char *>(buffer.data()),
                                     messageSize,
                                     0);

                if (static_cast<uint32_t>(bytesReceived) != messageSize)
                {
                    return false;
                }
            }
            break;
        }

        if (bytesReceived <= 0)
        {
            return false;
        }

        // Resize buffer to actual received size
        buffer.resize(bytesReceived);

        // Check if this is a bundle or a message
        if (bytesReceived >= 8 && std::memcmp(buffer.data(), "#bundle", 7) == 0)
        {
            // Handle as bundle
            try
            {
                Bundle bundle = Bundle::deserialize(buffer.data(), buffer.size());
                dispatchBundle(bundle);
                return true;
            }
            catch (const OSCException &e)
            {
                if (errorHandler_)
                {
                    errorHandler_(static_cast<int>(e.code()), e.what(), "ServerImpl::receive");
                }
                return false;
            }
        }
        else
        {
            // Handle as message
            try
            {
                Message message = Message::deserialize(buffer.data(), buffer.size());
                dispatchMessage(message);
                return true;
            }
            catch (const OSCException &e)
            {
                if (errorHandler_)
                {
                    errorHandler_(static_cast<int>(e.code()), e.what(), "ServerImpl::receive");
                }
                return false;
            }
        }
    }

    // Get the port number
    int ServerImpl::port() const
    {
        return std::stoi(port_);
    }

    // Get the URL
    std::string ServerImpl::url() const
    {
        std::string protocol;
        switch (protocol_)
        {
        case Protocol::UDP:
            protocol = "osc.udp://";
            break;
        case Protocol::TCP:
            protocol = "osc.tcp://";
            break;
        case Protocol::UNIX:
            protocol = "osc.unix://";
            break;
        }

        return protocol + "localhost:" + port_ + "/";
    }

    // Check if there are pending messages
    bool ServerImpl::hasPendingMessages() const
    {
        // Use select with zero timeout to check if there is data available
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(socket_, &readfds);

        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        int result = select(static_cast<int>(socket_ + 1), &readfds, nullptr, nullptr, &tv);
        return (result > 0);
    }

    // Initialize the socket
    bool ServerImpl::initializeSocket()
    {
        if (socket_ != INVALID_SOCKET_VALUE)
        {
            // Socket already initialized
            return true;
        }

        // Create the socket based on the protocol
        switch (protocol_)
        {
        case Protocol::UDP:
            socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            break;
        case Protocol::TCP:
            socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            break;
        case Protocol::UNIX:
#ifdef _WIN32
            // Windows doesn't fully support UNIX domain sockets in older versions
            if (errorHandler_)
            {
                errorHandler_(WSAEOPNOTSUPP, "UNIX domain sockets not supported on this platform", "ServerImpl::initializeSocket");
            }
            return false;
#else
            socket_ = socket(AF_UNIX, SOCK_STREAM, 0);
            break;
#endif
        }

        if (socket_ == INVALID_SOCKET_VALUE)
        {
            if (errorHandler_)
            {
                errorHandler_(getLastSocketError(), "Failed to create socket", "ServerImpl::initializeSocket");
            }
            return false;
        }

        // Set socket options
        int reuseAddr = 1;
        if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR,
                       reinterpret_cast<char *>(&reuseAddr), sizeof(reuseAddr)) != 0)
        {
            if (errorHandler_)
            {
                errorHandler_(getLastSocketError(), "Failed to set SO_REUSEADDR", "ServerImpl::initializeSocket");
            }
            // Continue anyway, this is not fatal
        }

        // Bind to address
        switch (protocol_)
        {
        case Protocol::UDP:
        case Protocol::TCP:
        {
            struct sockaddr_in addr;
            std::memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(static_cast<uint16_t>(std::stoi(port_)));
            addr.sin_addr.s_addr = htonl(INADDR_ANY);

            if (bind(socket_, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) != 0)
            {
                if (errorHandler_)
                {
                    errorHandler_(getLastSocketError(), "Failed to bind socket", "ServerImpl::initializeSocket");
                }
                cleanup();
                return false;
            }
            break;
        }
#ifndef _WIN32
        case Protocol::UNIX:
        {
            struct sockaddr_un addr;
            std::memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, port_.c_str(), sizeof(addr.sun_path) - 1);

            // Remove existing socket file if it exists
            unlink(port_.c_str());

            if (bind(socket_, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) != 0)
            {
                if (errorHandler_)
                {
                    errorHandler_(getLastSocketError(), "Failed to bind UNIX socket", "ServerImpl::initializeSocket");
                }
                cleanup();
                return false;
            }
            break;
        }
#endif
        }

        // For TCP and UNIX, we need to listen
        if (protocol_ == Protocol::TCP || protocol_ == Protocol::UNIX)
        {
            if (listen(socket_, SOMAXCONN) != 0)
            {
                if (errorHandler_)
                {
                    errorHandler_(getLastSocketError(), "Failed to listen on socket", "ServerImpl::initializeSocket");
                }
                cleanup();
                return false;
            }
        }

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

#ifndef _WIN32
        // For UNIX sockets, remove the socket file
        if (protocol_ == Protocol::UNIX)
        {
            unlink(port_.c_str());
        }
#endif
    }

    // Platform-specific networking initialization
    bool ServerImpl::initializeNetworking()
    {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        {
            return false;
        }
#endif
        networkingInitialized = true;
        return true;
    }

    // Platform-specific networking cleanup
    void ServerImpl::cleanupNetworking()
    {
#ifdef _WIN32
        WSACleanup();
#endif
        networkingInitialized = false;
    }

    // Get the last socket error code
    int ServerImpl::getLastSocketError()
    {
#ifdef _WIN32
        return WSAGetLastError();
#else
        return errno;
#endif
    }

    // Dispatch a message to registered handlers
    void ServerImpl::dispatchMessage(const Message &message)
    {
        std::lock_guard<std::mutex> lock(methodMutex_);

        // Get the path and build type tag string from arguments
        std::string path = message.getPath();
        std::string types;
        for (const auto &arg : message.getArguments())
        {
            types += arg.typeTag();
        }

        // Find matching methods
        std::vector<Method *> matchedMethods;
        bool found = matchMethod(path, types, matchedMethods);

        // Call matched methods
        for (auto *method : matchedMethods)
        {
            if (method && method->handler)
            {
                try
                {
                    method->handler(message);
                }
                catch (const std::exception &e)
                {
                    if (errorHandler_)
                    {
                        errorHandler_(0, std::string("Exception in method handler: ") + e.what(),
                                      "ServerImpl::dispatchMessage");
                    }
                }
            }
        }

        // If no methods matched and we have a default handler, call it
        if (!found)
        {
            for (auto &pair : methods_)
            {
                if (pair.second.isDefault && pair.second.handler)
                {
                    try
                    {
                        pair.second.handler(message);
                    }
                    catch (const std::exception &e)
                    {
                        if (errorHandler_)
                        {
                            errorHandler_(0, std::string("Exception in default handler: ") + e.what(),
                                          "ServerImpl::dispatchMessage");
                        }
                    }
                    break; // Only call the first default handler
                }
            }
        }
    }

    // Dispatch a bundle to registered handlers
    void ServerImpl::dispatchBundle(const Bundle &bundle)
    {
        // Call the bundle start handler if registered
        if (bundleStartHandler_)
        {
            try
            {
                bundleStartHandler_(bundle);
            }
            catch (const std::exception &e)
            {
                if (errorHandler_)
                {
                    errorHandler_(0, std::string("Exception in bundle start handler: ") + e.what(),
                                  "ServerImpl::dispatchBundle");
                }
            }
        }

        // Dispatch each message in the bundle
        for (const auto &message : bundle.messages())
        {
            dispatchMessage(message);
        }

        // Call the bundle end handler if registered
        if (bundleEndHandler_)
        {
            try
            {
                bundleEndHandler_(bundle);
            }
            catch (const std::exception &e)
            {
                if (errorHandler_)
                {
                    errorHandler_(0, std::string("Exception in bundle end handler: ") + e.what(),
                                  "ServerImpl::dispatchBundle");
                }
            }
        }
    }

    // Convert OSC pattern to regex
    std::regex ServerImpl::convertPathToRegex(const std::string &pattern)
    {
        std::string regexStr;
        regexStr.reserve(pattern.size() * 2); // Reserve space to avoid reallocations
        regexStr += "^";                      // Match start of string

        for (size_t i = 0; i < pattern.size(); ++i)
        {
            char c = pattern[i];
            switch (c)
            {
            case '.': // Escape special regex chars
            case '^':
            case '$':
            case '+':
            case '\\':
                regexStr += '\\';
                regexStr += c;
                break;
            case '?': // OSC '?' matches any single character except '/'
                regexStr += "[^/]";
                break;
            case '*': // OSC '*' matches any sequence of zero or more characters except '/'
                regexStr += "[^/]*";
                break;
            case '[': // OSC '[abc]' matches any character in the set
            {
                regexStr += '[';
                size_t closePos = pattern.find(']', i);
                if (closePos == std::string::npos)
                {
                    throw OSCException(OSCException::ErrorCode::PatternError, "Unclosed '[' in pattern");
                }
                // Copy the bracket contents verbatim
                regexStr += pattern.substr(i + 1, closePos - i - 1);
                regexStr += ']';
                i = closePos; // Skip past the matched bracket
                break;
            }
            case '{': // OSC '{foo,bar}' matches any of the comma-separated strings
            {
                regexStr += '(';
                size_t closePos = pattern.find('}', i);
                if (closePos == std::string::npos)
                {
                    throw OSCException(OSCException::ErrorCode::PatternError, "Unclosed '{' in pattern");
                }
                // Replace commas with '|'
                std::string options = pattern.substr(i + 1, closePos - i - 1);
                for (size_t j = 0; j < options.size(); ++j)
                {
                    if (options[j] == ',')
                    {
                        regexStr += '|';
                    }
                    else
                    {
                        regexStr += options[j];
                    }
                }
                regexStr += ')';
                i = closePos; // Skip past the matched brace
                break;
            }
            case '/': // Forward slash is a literal in OSC patterns
                regexStr += '/';
                break;
            default:
                regexStr += c;
                break;
            }
        }

        regexStr += "$"; // Match end of string
        return std::regex(regexStr);
    }

    // Match a message against registered methods
    bool ServerImpl::matchMethod(const std::string &path, const std::string &types,
                                 std::vector<Method *> &matchedMethods)
    {
        bool found = false;

        for (auto &pair : methods_)
        {
            Method &method = pair.second;

            // Skip default methods, they're handled separately
            if (method.isDefault)
            {
                continue;
            }

            // Try to match the path using regex
            if (std::regex_match(path, method.pathRegex))
            {
                // Path matches, now check the type spec
                if (method.typeSpec.empty() ||
                    (types.length() >= method.typeSpec.length() &&
                     types.substr(0, method.typeSpec.length()) == method.typeSpec))
                {

                    // Both path and types match
                    matchedMethods.push_back(&method);
                    found = true;
                }
            }
        }

        return found;
    }

} // namespace osc
