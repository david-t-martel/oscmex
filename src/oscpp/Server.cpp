#include "Server.h"
#include "ServerImpl.h"
#include <regex>

namespace osc
{

    // Constructor
    Server::Server(std::string_view port, Protocol protocol)
        : impl_(std::make_unique<ServerImpl>(std::string(port), protocol))
    {
    }

    // Static factory method for multicast servers
    Server Server::multicast(std::string_view group, std::string_view port)
    {
        Server server;
        server.impl_ = ServerImpl::createMulticast(std::string(group), std::string(port));
        return server;
    }

    // Create from URL
    Server Server::fromUrl(std::string_view url)
    {
        // Parse URL in format "osc.proto://host:port"
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

        // Extract port from URL
        std::regex portRegex(".*:(\\d+).*");
        std::smatch matches;

        if (std::regex_search(urlStr, matches, portRegex) && matches.size() > 1)
        {
            std::string port = matches[1].str();
            return Server(port, protocol);
        }
        else
        {
            throw OSCException(OSCException::ErrorCode::InvalidAddress,
                               "Invalid OSC URL format, missing port in " + std::string(url));
        }
    }

    // Destructor
    Server::~Server() = default;

    // Move operations
    Server::Server(Server &&other) noexcept = default;
    Server &Server::operator=(Server &&other) noexcept = default;

    // Add method handler
    MethodId Server::addMethod(std::string_view pathPattern, std::string_view typeSpec,
                               MethodHandler handler)
    {
        return impl_->addMethod(std::string(pathPattern), std::string(typeSpec), handler);
    }

    // Add default method handler
    MethodId Server::addDefaultMethod(MethodHandler handler)
    {
        return impl_->addDefaultMethod(handler);
    }

    // Remove method
    bool Server::removeMethod(MethodId id)
    {
        return impl_->removeMethod(id);
    }

    // Set bundle handlers
    void Server::setBundleHandlers(BundleStartHandler startHandler, BundleEndHandler endHandler)
    {
        impl_->setBundleHandlers(startHandler, endHandler);
    }

    // Set error handler
    void Server::setErrorHandler(ErrorHandler handler)
    {
        impl_->setErrorHandler(handler);
    }

    // Wait for messages
    bool Server::wait(std::chrono::milliseconds timeout)
    {
        return impl_->wait(timeout);
    }

    // Receive a message
    bool Server::receive(std::chrono::milliseconds timeout)
    {
        return impl_->receive(timeout);
    }

    // Check for pending messages
    bool Server::hasPendingMessages() const
    {
        return impl_->hasPendingMessages();
    }

    // Get port number
    int Server::port() const
    {
        return impl_->port();
    }

    // Get URL
    std::string Server::url() const
    {
        return impl_->url();
    }

    // Get socket file descriptor
    int Server::socket() const
    {
        return static_cast<int>(impl_->socket());
    }

    // Set type coercion
    void Server::setTypeCoercion(bool enable)
    {
        impl_->setTypeCoercion(enable);
    }

    // Set maximum message size
    void Server::setMaxMessageSize(size_t size)
    {
        impl_->setMaxMessageSize(size);
    }

} // namespace osc
