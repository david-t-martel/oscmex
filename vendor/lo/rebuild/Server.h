#pragma once

#include "Types.h"
#include "Message.h"
#include "Bundle.h"
#include <string>
#include <string_view>
#include <memory>
#include <functional>
#include <chrono>
#include <optional>
#include <regex>

namespace osc
{

    // Forward declaration of implementation class
    class ServerImpl;

    /**
     * @class Server
     * @brief OSC server for receiving messages
     *
     * The Server class listens for incoming OSC messages and dispatches them
     * to registered method handlers.
     */
    class Server
    {
    public:
        /**
         * @brief Create a new OSC server listening on a specified port
         * @param port Port number or service name, or empty for any port
         * @param protocol Transport protocol (UDP by default)
         * @throw OSCException if server creation fails
         */
        Server(std::string_view port = "", Protocol protocol = Protocol::UDP);

        /**
         * @brief Create a multicast OSC server
         * @param group Multicast group address
         * @param port Port number or service name
         * @return New Server object configured for multicast
         * @throw OSCException if server creation fails
         */
        static Server multicast(std::string_view group, std::string_view port);

        /**
         * @brief Create a server from a URL
         * @param url URL in format "osc.udp://host:port/"
         * @return New Server object
         * @throw OSCException if URL is invalid
         */
        static Server fromUrl(std::string_view url);

        /**
         * @brief Destructor
         */
        ~Server();

        // Move operations
        Server(Server &&other) noexcept;
        Server &operator=(Server &&other) noexcept;

        // Copy operations deleted - cannot have two servers on the same port
        Server(const Server &) = delete;
        Server &operator=(const Server &) = delete;

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
        using ErrorHandler = std::function<void(int code, const std::string &message,
                                                const std::string &where)>;

        /**
         * @brief Add a method handler for a specific OSC path pattern
         * @param pathPattern The OSC path pattern to match (e.g., "/foo/bar/*")
         * @param typeSpec Type specification string (e.g., "ifs") or empty for any types
         * @param handler The callback function to invoke when a matching message is received
         * @return Unique method identifier that can be used to remove the method
         */
        MethodId addMethod(std::string_view pathPattern, std::string_view typeSpec,
                           MethodHandler handler);

        /**
         * @brief Add a catch-all method handler for any unmatched messages
         * @param handler The callback function to invoke for unmatched messages
         * @return Unique method identifier
         */
        MethodId addDefaultMethod(MethodHandler handler);

        /**
         * @brief Remove a previously registered method
         * @param id The method identifier returned by addMethod()
         * @return True if the method was found and removed, false otherwise
         */
        bool removeMethod(MethodId id);

        /**
         * @brief Set handlers for bundle processing
         * @param startHandler Called when a bundle starts
         * @param endHandler Called when a bundle ends
         */
        void setBundleHandlers(BundleStartHandler startHandler, BundleEndHandler endHandler);

        /**
         * @brief Set an error handler
         * @param handler The error handler function
         */
        void setErrorHandler(ErrorHandler handler);

        /**
         * @brief Wait for incoming messages up to the specified timeout
         * @param timeout Maximum time to wait, or 0 to wait indefinitely
         * @return True if a message was received, false if timeout
         */
        bool wait(std::chrono::milliseconds timeout = std::chrono::milliseconds(0));

        /**
         * @brief Receive and dispatch a single message, non-blocking
         * @param timeout Maximum time to wait for a message
         * @return True if a message was received and processed, false if timeout
         */
        bool receive(std::chrono::milliseconds timeout = std::chrono::milliseconds(0));

        /**
         * @brief Check if there are pending messages
         * @return True if there are messages waiting to be processed
         */
        bool hasPendingMessages() const;

        /**
         * @brief Get the port number the server is listening on
         * @return Port number, or 0 if not listening
         */
        int port() const;

        /**
         * @brief Get the URL of the server
         * @return URL string in format "osc.udp://host:port/"
         */
        std::string url() const;

        /**
         * @brief Get the socket file descriptor (for integrating with other event loops)
         * @return Socket file descriptor, or -1 if not available
         */
        int socket() const;

        /**
         * @brief Set the option to coerce argument types
         * @param enable True to enable type coercion, false to require exact type matching
         */
        void setTypeCoercion(bool enable);

        /**
         * @brief Set the maximum message size the server will accept
         * @param size Maximum size in bytes
         */
        void setMaxMessageSize(size_t size);

    private:
        std::unique_ptr<ServerImpl> impl_;
    };

} // namespace osc
