#pragma once

#include "Types.h"
#include "Message.h"
#include "Bundle.h"
#include <string>
#include <memory>
#include <map>
#include <vector>
#include <regex>
#include <functional>
#include <chrono>
#include <atomic>
#include <mutex>

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
#include <poll.h>
#define SOCKET_TYPE int
#define INVALID_SOCKET_VALUE (-1)
#define SOCKET_ERROR_VALUE (-1)
#define CLOSE_SOCKET close
#endif

namespace osc
{

    class ServerImpl
    {
    public:
        /**
         * @brief Create a new OSC server implementation
         * @param port Port number or service name
         * @param protocol Transport protocol
         */
        ServerImpl(std::string port, Protocol protocol);

        /**
         * @brief Create a multicast OSC server
         * @param group Multicast group address
         * @param port Port number or service name
         * @return New ServerImpl object
         */
        static std::unique_ptr<ServerImpl> createMulticast(const std::string &group,
                                                           const std::string &port);

        /**
         * @brief Destructor - cleans up sockets and resources
         */
        ~ServerImpl();

        /**
         * @brief Handler type for OSC method callbacks
         */
        using MethodHandler = std::function<void(const Message &)>;

        /**
         * @brief Handler type for bundle start/end callbacks
         */
        using BundleStartHandler = std::function<void(const TimeTag &)>;
        using BundleEndHandler = std::function<void()>;

        /**
         * @brief Handler type for error callbacks
         */
        using ErrorHandler = std::function<void(int, const std::string &, const std::string &)>;

        /**
         * @brief Add a method handler for a specific OSC path pattern
         * @param pathPattern The OSC path pattern to match
         * @param typeSpec Type specification string
         * @param handler The callback function
         * @return Method ID
         */
        MethodId addMethod(const std::string &pathPattern, const std::string &typeSpec,
                           MethodHandler handler);

        /**
         * @brief Add a catch-all handler for any unmatched messages
         * @param handler The callback function
         * @return Method ID
         */
        MethodId addDefaultMethod(MethodHandler handler);

        /**
         * @brief Remove a method handler
         * @param id Method ID returned by addMethod
         * @return true if removed, false if not found
         */
        bool removeMethod(MethodId id);

        /**
         * @brief Set bundle handlers
         * @param startHandler Called when a bundle starts
         * @param endHandler Called when a bundle ends
         */
        void setBundleHandlers(BundleStartHandler startHandler, BundleEndHandler endHandler);

        /**
         * @brief Set error handler
         * @param handler The error handler function
         */
        void setErrorHandler(ErrorHandler handler);

        /**
         * @brief Wait for and receive messages
         * @param timeout Maximum time to wait in milliseconds
         * @return true if a message was received, false if timeout
         */
        bool wait(std::chrono::milliseconds timeout);

        /**
         * @brief Receive and dispatch a single message (non-blocking)
         * @param timeout Maximum time to wait for a message
         * @return true if a message was received, false if timeout
         */
        bool receive(std::chrono::milliseconds timeout);

        /**
         * @brief Check if there are pending messages
         * @return true if there are pending messages
         */
        bool hasPendingMessages() const;

        /**
         * @brief Get the port number
         * @return Port number, or 0 if not available
         */
        int port() const;

        /**
         * @brief Get the URL of the server
         * @return URL in format "osc.udp://host:port/"
         */
        std::string url() const;

        /**
         * @brief Get the socket file descriptor
         * @return Socket descriptor, or -1 if not available
         */
        SOCKET_TYPE socket() const;

        /**
         * @brief Set type coercion option
         * @param enable true to enable, false to disable
         */
        void setTypeCoercion(bool enable) { typeCoercion_ = enable; }

        /**
         * @brief Set maximum message size
         * @param size Maximum size in bytes
         */
        void setMaxMessageSize(size_t size) { maxMessageSize_ = size; }

    private:
        // Structure to represent a registered method
        struct Method
        {
            std::string pathPattern;
            std::string typeSpec;
            std::regex pathRegex;
            MethodHandler handler;
            bool isDefaultHandler;
        };

        std::string port_;                          // Port or service name
        Protocol protocol_;                         // UDP, TCP, or UNIX
        SOCKET_TYPE socket_ = INVALID_SOCKET_VALUE; // Main socket
        std::vector<SOCKET_TYPE> clientSockets_;    // For TCP/UNIX, client connections
        int errorCode_ = 0;                         // Last error code
        size_t maxMessageSize_ = 1024 * 1024;       // 1MB default

        // Method handling
        MethodId nextMethodId_ = 1;
        std::map<MethodId, Method> methods_;
        std::mutex methodMutex_;

        // Flags and options
        bool typeCoercion_ = true; // Whether to coerce argument types

        // Callback handlers
        BundleStartHandler bundleStartHandler_;
        BundleEndHandler bundleEndHandler_;
        ErrorHandler errorHandler_;

        // Helper methods
        bool initializeSocket();
        void cleanup();
        bool convertPathToRegex(const std::string &pattern, std::regex &regex);
        bool receiveData(std::vector<std::byte> &buffer, SOCKET_TYPE sock, std::chrono::milliseconds timeout);
        void dispatchMessage(const Message &message);
        void dispatchBundle(const Bundle &bundle);
        bool matchMethod(const std::string &path, const std::string &types,
                         std::vector<Method *> &matchedMethods);

        // Platform-specific networking initialization
        static bool initializeNetworking();
        static void cleanupNetworking();
        static bool networkingInitialized;
    };

} // namespace osc
