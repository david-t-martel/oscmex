#include "osc/Address.h"

#include <stdexcept>

#include "osc/AddressImpl.h"
#include "osc/Bundle.h"
#include "osc/Exceptions.h"
#include "osc/Message.h"
#include "osc/Types.h"

namespace osc {

    // Constructor
    Address::Address(const std::string &host, const std::string &port, Protocol protocol)
        : impl_(new AddressImpl(host, port, protocol)) {}

    // URL parser factory method
    Address Address::fromUrl(const std::string &url) {
        // Parse URL in format "osc.proto://host:port/"
        std::string urlStr(url);

        // Parse protocol
        Protocol protocol = Protocol::UDP;  // Default to UDP

        if (urlStr.find("osc.udp://") == 0) {
            protocol = Protocol::UDP;
            urlStr = urlStr.substr(8);
        } else if (urlStr.find("osc.tcp://") == 0) {
            protocol = Protocol::TCP;
            urlStr = urlStr.substr(8);
        } else if (urlStr.find("osc.unix://") == 0) {
            protocol = Protocol::UNIX;
            urlStr = urlStr.substr(9);
        } else {
            throw OSCException("Invalid OSC URL format: " + std::string(url),
                               OSCException::ErrorCode::AddressError);
        }

        // Extract host and port
        size_t colonPos = urlStr.find(':');
        size_t slashPos = urlStr.find('/', colonPos);

        if (colonPos == std::string::npos) {
            throw OSCException("Invalid OSC URL format, missing port in " + std::string(url),
                               OSCException::ErrorCode::AddressError);
        }

        std::string host = urlStr.substr(0, colonPos);
        std::string port;

        if (slashPos != std::string::npos) {
            port = urlStr.substr(colonPos + 1, slashPos - colonPos - 1);
        } else {
            port = urlStr.substr(colonPos + 1);
        }

        return Address(host, port, protocol);
    }

    // Destructor
    Address::~Address() { delete impl_; }

    // Move operations
    Address::Address(Address &&other) noexcept : impl_(other.impl_) { other.impl_ = nullptr; }

    Address &Address::operator=(Address &&other) noexcept {
        if (this != &other) {
            delete impl_;
            impl_ = other.impl_;
            other.impl_ = nullptr;
        }
        return *this;
    }

    // Copy operations
    Address::Address(const Address &other)
        : impl_(
              new AddressImpl(other.impl_->host(), other.impl_->port(), other.impl_->protocol())) {
        // Copy other settings
        if (other.impl_->getTTL() != 1) {
            impl_->setTTL(other.impl_->getTTL());
        }
    }

    Address &Address::operator=(const Address &other) {
        if (this != &other) {
            AddressImpl *newImpl =
                new AddressImpl(other.impl_->host(), other.impl_->port(), other.impl_->protocol());

            // Copy other settings
            if (other.impl_->getTTL() != 1) {
                newImpl->setTTL(other.impl_->getTTL());
            }

            delete impl_;
            impl_ = newImpl;
        }
        return *this;
    }

    // Message sending
    bool Address::send(const std::vector<std::byte> &data) {
        if (!impl_ || !impl_->isValid()) {
            return false;
        }

        try {
            return impl_->send(data);
        } catch (const OSCException &) {
            // Let exceptions propagate upward
            throw;
        } catch (const std::exception &e) {
            throw OSCException("Error sending data: " + std::string(e.what()),
                               OSCException::ErrorCode::NetworkError);
        }
    }

    bool Address::send(const Message &message) {
        if (!impl_ || !impl_->isValid()) {
            return false;
        }

        try {
            std::vector<std::byte> data = message.serialize();
            return impl_->send(data);
        } catch (const OSCException &) {
            // Let exceptions propagate upward
            throw;
        } catch (const std::exception &e) {
            throw OSCException("Error sending message: " + std::string(e.what()),
                               OSCException::ErrorCode::NetworkError);
        }
    }

    bool Address::send(const Bundle &bundle) {
        if (!impl_ || !impl_->isValid()) {
            return false;
        }

        try {
            std::vector<std::byte> data = bundle.serialize();
            return impl_->send(data);
        } catch (const OSCException &) {
            // Let exceptions propagate upward
            throw;
        } catch (const std::exception &e) {
            throw OSCException("Error sending bundle: " + std::string(e.what()),
                               OSCException::ErrorCode::NetworkError);
        }
    }

    // URL accessor
    std::string Address::url() const { return impl_ ? impl_->url() : "osc://invalid/"; }

    // Getters
    bool Address::isValid() const { return impl_ && impl_->isValid(); }

    int Address::getErrorCode() const { return impl_ ? impl_->getErrorCode() : 0; }

    std::string Address::getErrorMessage() const {
        return impl_ ? impl_->getErrorMessage() : "Invalid address";
    }

    // Configuration methods
    void Address::setTTL(int ttl) {
        if (impl_) {
            try {
                impl_->setTTL(ttl);
            } catch (const std::exception &) {
                // Silently fail on error
            }
        }
    }

    int Address::getTTL() const { return impl_ ? impl_->getTTL() : 1; }

    void Address::setNoDelay(bool enable) {
        if (impl_) {
            try {
                impl_->setNoDelay(enable);
            } catch (const std::exception &) {
                // Silently fail on error
            }
        }
    }

    void Address::setTimeout(std::chrono::milliseconds timeout) {
        if (impl_) {
            try {
                impl_->setTimeout(timeout);
            } catch (const std::exception &) {
                // Silently fail on error
            }
        }
    }

    // Host/port/protocol accessors
    std::string Address::host() const { return impl_ ? impl_->host() : ""; }

    std::string Address::port() const { return impl_ ? impl_->port() : ""; }

    Protocol Address::protocol() const { return impl_ ? impl_->protocol() : Protocol::UDP; }

}  // namespace osc
