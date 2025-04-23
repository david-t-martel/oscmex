#include "ServerThread.h"
#include "Server.h"
#include <iostream>

namespace osc
{

    // Constructor
    ServerThread::ServerThread(std::string_view port, Protocol protocol)
        : server_(std::make_unique<Server>(port, protocol))
    {
    }

    // Static factory method for multicast servers
    ServerThread ServerThread::multicast(std::string_view group, std::string_view port)
    {
        ServerThread st;
        st.server_ = std::make_unique<Server>(Server::multicast(group, port));
        return st;
    }

    // Create from URL
    ServerThread ServerThread::fromUrl(std::string_view url)
    {
        ServerThread st;
        st.server_ = std::make_unique<Server>(Server::fromUrl(url));
        return st;
    }

    // Destructor
    ServerThread::~ServerThread()
    {
        if (running_)
        {
            stop();
        }
    }

    // Move operations
    ServerThread::ServerThread(ServerThread &&other) noexcept
        : server_(std::move(other.server_)),
          thread_(std::move(other.thread_)),
          running_(other.running_.load()),
          initCallback_(std::move(other.initCallback_)),
          cleanupCallback_(std::move(other.cleanupCallback_))
    {

        other.running_ = false;
    }

    ServerThread &ServerThread::operator=(ServerThread &&other) noexcept
    {
        if (this != &other)
        {
            if (running_)
            {
                stop();
            }

            server_ = std::move(other.server_);
            thread_ = std::move(other.thread_);
            running_ = other.running_.load();
            initCallback_ = std::move(other.initCallback_);
            cleanupCallback_ = std::move(other.cleanupCallback_);

            other.running_ = false;
        }
        return *this;
    }

    // Start the server thread
    bool ServerThread::start()
    {
        if (running_ || !server_)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        if (running_)
        {
            return false;
        }

        running_ = true;

        // Create and start the thread
        thread_ = std::thread(&ServerThread::threadFunction, this);

        return true;
    }

    // Stop the server thread
    bool ServerThread::stop()
    {
        if (!running_ || !server_)
        {
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
        }

        // Wait for the thread to finish
        if (thread_.joinable())
        {
            thread_.join();
        }

        return true;
    }

    // Check if the thread is running
    bool ServerThread::isRunning() const
    {
        return running_;
    }

    // Get the server object
    Server &ServerThread::server()
    {
        return *server_;
    }

    // Get the server object (const version)
    const Server &ServerThread::server() const
    {
        return *server_;
    }

    // Get port number
    int ServerThread::port() const
    {
        return server_ ? server_->port() : 0;
    }

    // Get URL
    std::string ServerThread::url() const
    {
        return server_ ? server_->url() : "";
    }

    // Add method handler
    MethodId ServerThread::addMethod(std::string_view pathPattern, std::string_view typeSpec,
                                     MethodHandler handler)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!server_)
        {
            throw OSCException(OSCException::ErrorCode::ServerError,
                               "Cannot add method: Server not initialized");
        }

        return server_->addMethod(pathPattern, typeSpec, handler);
    }

    // Add default method handler
    MethodId ServerThread::addDefaultMethod(MethodHandler handler)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!server_)
        {
            throw OSCException(OSCException::ErrorCode::ServerError,
                               "Cannot add default method: Server not initialized");
        }

        return server_->addDefaultMethod(handler);
    }

    // Remove method
    bool ServerThread::removeMethod(MethodId id)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!server_)
        {
            return false;
        }

        return server_->removeMethod(id);
    }

    // Set bundle handlers
    void ServerThread::setBundleHandlers(
        Server::BundleStartHandler startHandler,
        Server::BundleEndHandler endHandler)
    {

        std::lock_guard<std::mutex> lock(mutex_);

        if (server_)
        {
            server_->setBundleHandlers(startHandler, endHandler);
        }
    }

    // Set error handler
    void ServerThread::setErrorHandler(Server::ErrorHandler handler)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (server_)
        {
            server_->setErrorHandler(handler);
        }
    }

    // Set thread lifecycle callbacks
    void ServerThread::setThreadCallbacks(
        std::function<void()> initCallback,
        std::function<void()> cleanupCallback)
    {

        std::lock_guard<std::mutex> lock(mutex_);

        initCallback_ = initCallback;
        cleanupCallback_ = cleanupCallback;
    }

    // Thread function
    void ServerThread::threadFunction()
    {
        // Call initialization callback if set
        if (initCallback_)
        {
            try
            {
                initCallback_();
            }
            catch (const std::exception &e)
            {
                std::cerr << "Exception in thread init callback: " << e.what() << std::endl;
            }
        }

        // Main loop: receive messages while the thread is running
        while (running_)
        {
            try
            {
                // Receive messages with a short timeout to periodically check running_ flag
                server_->receive(std::chrono::milliseconds(100));
            }
            catch (const std::exception &e)
            {
                std::cerr << "Exception in server thread: " << e.what() << std::endl;

                // Sleep a bit to prevent tight loop in case of persistent error
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        // Call cleanup callback if set
        if (cleanupCallback_)
        {
            try
            {
                cleanupCallback_();
            }
            catch (const std::exception &e)
            {
                std::cerr << "Exception in thread cleanup callback: " << e.what() << std::endl;
            }
        }
    }

} // namespace osc
