// filepath: c:\codedev\auricleinc\oscmex\src\Server.cpp
#include "Server.h"
#include "ServerImpl.h"
#include "OSCException.h"
#include <iostream>

namespace osc
{

    // Constructor
    Server::Server(const std::string &port, Protocol protocol)
        : impl_(std::make_unique<ServerImpl>(port, protocol))
    {
    }

    // Destructor
    Server::~Server() = default;

    // Start the server
    bool Server::start()
    {
        return impl_->start();
    }

    // Stop the server
    void Server::stop()
    {
        impl_->stop();
    }

    // Add a method handler
    MethodId Server::addMethod(const std::string &pathPattern, const std::string &typeSpec, MethodHandler handler)
    {
        return impl_->addMethod(pathPattern, typeSpec, handler);
    }

    // Add a default method handler
    MethodId Server::addDefaultMethod(MethodHandler handler)
    {
        return impl_->addDefaultMethod(handler);
    }

    // Remove a method handler
    bool Server::removeMethod(MethodId id)
    {
        return impl_->removeMethod(id);
    }

    // Set error handler
    void Server::setErrorHandler(ErrorHandler handler)
    {
        impl_->setErrorHandler(handler);
    }

    // Wait for messages
    void Server::waitForMessages(std::chrono::milliseconds timeout)
    {
        while (true)
        {
            if (impl_->wait(timeout))
            {
                impl_->receive(timeout);
            }
        }
    }

    // Get the server's port
    int Server::port() const
    {
        return impl_->port();
    }

    // Get the server's URL
    std::string Server::url() const
    {
        return impl_->url();
    }

} // namespace osc