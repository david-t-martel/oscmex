#include "osc/Server.h"

#include <iostream>

#include "osc/Exceptions.h"  // Updated to use our consolidated exception header
#include "osc/ServerImpl.h"

namespace osc {
    // Constructor
    Server::Server(const std::string &port, Protocol protocol) : impl_(nullptr) {
        try {
            impl_ = std::make_unique<ServerImpl>(port, protocol);
        } catch (const OSCException &e) {
            // Re-throw with better context information
            throw OSCException("Failed to initialize OSC Server on port " + port + ": " + e.what(),
                               e.code());
        } catch (const std::exception &e) {
            // Wrap any other exceptions in our OSCException
            throw OSCException("Failed to initialize OSC Server on port " + port + ": " + e.what(),
                               OSCException::ErrorCode::ServerError);
        }
    }

    // Destructor
    Server::~Server() = default;

    // Start the server
    bool Server::start() {
        try {
            // ServerImpl doesn't have start(), we use receive() to check if it's working
            return impl_ && impl_->hasPendingMessages();
        } catch (const OSCException &e) {
            if (impl_) {
                impl_->setErrorHandler(
                    [](int code, const std::string &msg, const std::string &where) {
                        std::cerr << "OSC Server error " << code << " in " << where << ": " << msg
                                  << std::endl;
                    });
            }
            // Log error but don't re-throw since this is a boolean-returning method
            std::cerr << "Failed to start OSC Server: " << e.what()
                      << " (Code: " << static_cast<int>(e.code()) << ")" << std::endl;
            return false;
        } catch (const std::exception &e) {
            // Log error but don't re-throw
            std::cerr << "Failed to start OSC Server: " << e.what() << std::endl;
            return false;
        }
    }

    // Stop the server
    void Server::stop() {
        // ServerImpl doesn't have stop(), we would just clean up if needed
        // No implementation needed as the destructor will handle cleanup
    }

    // Receive incoming messages
    bool Server::receive(std::chrono::milliseconds timeout) {
        if (!impl_) return false;
        return impl_->receive(timeout);
    }

    // Check for pending messages
    bool Server::hasPendingMessages() const {
        if (!impl_) return false;
        return impl_->hasPendingMessages();
    }

    // Add a method handler
    MethodId Server::addMethod(const std::string &pathPattern, const std::string &typeSpec,
                               std::function<void(const Message &)> handler) {
        if (!impl_) {
            throw OSCException("Server not initialized", OSCException::ErrorCode::ServerError);
        }

        try {
            return impl_->addMethod(pathPattern, typeSpec, handler);
        } catch (const OSCException &e) {
            throw OSCException(
                "Failed to add OSC method for pattern '" + pathPattern + "': " + e.what(),
                e.code());
        } catch (const std::exception &e) {
            throw OSCException(
                "Failed to add OSC method for pattern '" + pathPattern + "': " + e.what(),
                OSCException::ErrorCode::ServerError);
        }
    }

    // Set default method handler
    void Server::setDefaultMethod(std::function<void(const Message &)> handler) {
        if (!impl_) return;

        try {
            impl_->addDefaultMethod(handler);
        } catch (const OSCException &e) {
            throw OSCException("Failed to set default OSC method handler: " + std::string(e.what()),
                               e.code());
        } catch (const std::exception &e) {
            throw OSCException("Failed to set default OSC method handler: " + std::string(e.what()),
                               OSCException::ErrorCode::ServerError);
        }
    }

    // Remove a method handler - implementation omitted as it's not in the header
    // bool Server::removeMethod(MethodId id) { return impl_->removeMethod(id); }

    // Set error handler
    void Server::setErrorHandler(std::function<void(int, const std::string &)> handler) {
        if (!impl_) return;

        try {
            // Adapt the external handler to the internal format
            impl_->setErrorHandler(
                [handler](int code, const std::string &msg, const std::string &where) {
                    handler(code, where + ": " + msg);
                });
        } catch (const std::exception &e) {
            throw OSCException("Failed to set OSC error handler: " + std::string(e.what()),
                               OSCException::ErrorCode::ServerError);
        }
    }
}  // namespace osc
