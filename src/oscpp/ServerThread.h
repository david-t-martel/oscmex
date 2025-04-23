#pragma once

#include "Types.h"
#include "Server.h"
#include <string>
#include <string_view>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

namespace osc
{

    /**
     * @brief OSC Server with a background thread for processing
     *
     * The ServerThread class wraps a Server object and runs it in a separate
     * thread, automatically handling incoming messages.
     */
    class ServerThread
    {
    public:
        // Forward method handler types from Server
        using MethodHandler = Server::MethodHandler;

        /**
         * @brief Create a new threaded OSC server
         * @param port Port number or service name
         * @param protocol Transport protocol to use
         */
        ServerThread(std::string_view port = "", Protocol protocol = Protocol::UDP);

        /**
         * @brief Create a multicast OSC server thread
         * @param group Multicast group address
         * @param port Port number or service name
         * @return New server thread instance
         */
        static ServerThread multicast(std::string_view group, std::string_view port);

        /**
         * @brief Create a server thread from a URL
         * @param url URL in format "osc.proto://host:port/"
         * @return New server thread instance
         */
        static ServerThread fromUrl(std::string_view url);

        /**
         * @brief Destructor - stops the thread if running
         */
        ~ServerThread();

        /**
         * @brief Move constructor
         */
        ServerThread(ServerThread &&other) noexcept;

        /**
         * @brief Move assignment operator
         */
        ServerThread &operator=(ServerThread &&other) noexcept;

        // Disable copying
        ServerThread(const ServerThread &) = delete;
        ServerThread &operator=(const ServerThread &) = delete;

        /**
         * @brief Start the server thread
         * @return true if started successfully, false otherwise
         */
        bool start();

        /**
         * @brief Stop the server thread
         * @return true if stopped successfully, false otherwise
         */
        bool stop();

        /**
         * @brief Check if the server thread is running
         * @return true if running, false otherwise
         */
        bool isRunning() const;

        /**
         * @brief Get the underlying server object
         * @return Reference to the server
         */
        Server &server();

        /**
         * @brief Get the underlying server object (const version)
         * @return Const reference to the server
         */
        const Server &server() const;

        /**
         * @brief Get the port number
         * @return Port number, or 0 if not available
         */
        int port() const;

        /**
         * @brief Get the URL of the server
         * @return URL in format "osc.proto://host:port/"
         */
        std::string url() const;

        /**
         * @brief Add a method handler for a specific OSC path pattern
         * @param pathPattern The OSC path pattern to match
         * @param typeSpec Type specification string (optional)
         * @param handler The callback function
         * @return Method ID that can be used to remove the handler later
         */
        MethodId addMethod(std::string_view pathPattern, std::string_view typeSpec,
                           MethodHandler handler);

        /**
         * @brief Add a catch-all handler for any unmatched messages
         * @param handler The callback function
         * @return Method ID that can be used to remove the handler later
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
        void setBundleHandlers(Server::BundleStartHandler startHandler,
                               Server::BundleEndHandler endHandler);

        /**
         * @brief Set error handler
         * @param handler The error handler function
         */
        void setErrorHandler(Server::ErrorHandler handler);

        /**
         * @brief Set callbacks for thread initialization and cleanup
         * @param initCallback Called in the server thread before it starts processing
         * @param cleanupCallback Called in the server thread before it exits
         */
        void setThreadCallbacks(std::function<void()> initCallback,
                                std::function<void()> cleanupCallback);

    private:
        // Default constructor for use by static factory methods
        ServerThread() = default;

        // Thread function
        void threadFunction();

        std::unique_ptr<Server> server_;   // Wrapped server object
        std::thread thread_;               // Server thread
        std::atomic<bool> running_{false}; // Thread running flag
        std::mutex mutex_;                 // Mutex for thread-safe operations

        // Thread callbacks
        std::function<void()> initCallback_;    // Called on thread start
        std::function<void()> cleanupCallback_; // Called on thread end
    };

} // namespace osc
