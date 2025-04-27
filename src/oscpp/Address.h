#pragma once

#include "Types.h"
#include <string>
#include <vector>
#include <memory>
#include <chrono>

/*
 *  OSCPP - Open Sound Control C++ (OSCPP) Library.
 *  TO DO: Add Exceptions implementation from REQUIREMENTS.md and from SPECIFICATION.md
 */

namespace osc
{

    // Forward declarations
    class Message;
    class Bundle;

    /**
     * @brief Manages OSC network communication with target destinations
     *
     * This class provides a modern C++ interface for sending OSC messages
     * and bundles to remote destinations using different transport protocols.
     */
    class Address
    {
    public:
        /**
         * @brief Construct a new Address object
         *
         * @param host Hostname or IP address
         * @param port Port number or service name
         * @param protocol Protocol to use (UDP, TCP, or UNIX)
         */
        Address(std::string_view host, std::string_view port,
                Protocol protocol = Protocol::UDP);

        /**
         * @brief Create an Address from an OSC URL string
         *
         * @param url OSC URL in format "osc.proto://host:port/"
         * @return Address object
         * @throws OSCException if URL is invalid
         */
        static Address fromUrl(std::string_view url);

        /**
         * @brief Destroy the Address object
         */
        ~Address();

        /**
         * @brief Move constructor
         */
        Address(Address &&other) noexcept;

        /**
         * @brief Move assignment operator
         */
        Address &operator=(Address &&other) noexcept;

        /**
         * @brief Copy constructor
         */
        Address(const Address &other);

        /**
         * @brief Copy assignment operator
         */
        Address &operator=(const Address &other);

        /**
         * @brief Send an OSC message to this address
         *
         * @param message The message to send
         * @return true if send was successful, false otherwise
         */
        bool send(const Message &message) const;

        /**
         * @brief Send an OSC bundle to this address
         *
         * @param bundle The bundle to send
         * @return true if send was successful, false otherwise
         */
        bool send(const Bundle &bundle) const;

        /**
         * @brief Get the full URL of this address
         *
         * @return URL string in format "osc.proto://host:port/"
         */
        std::string url() const;

        /**
         * @brief Get the hostname or IP
         *
         * @return Hostname string
         */
        std::string host() const;

        /**
         * @brief Get the port or service name
         *
         * @return Port string
         */
        std::string port() const;

        /**
         * @brief Get the protocol used
         *
         * @return Protocol enum value
         */
        Protocol protocol() const;

        /**
         * @brief Set the TTL (Time To Live) for multicast messages
         *
         * @param ttl TTL value (1-255)
         */
        void setTTL(int ttl);

        /**
         * @brief Get the current TTL value
         *
         * @return TTL value
         */
        int getTTL() const;

        /**
         * @brief Set TCP_NODELAY option (Nagle's algorithm)
         *
         * @param enable True to enable, false to disable
         */
        void setNoDelay(bool enable);

        /**
         * @brief Set socket send timeout
         *
         * @param timeout Timeout duration
         */
        void setTimeout(std::chrono::milliseconds timeout);

        /**
         * @brief Check if the address is valid and can be used
         *
         * @return true if valid, false otherwise
         */
        bool isValid() const;

        /**
         * @brief Get numeric error code from last error
         *
         * @return Platform-specific error code
         */
        int getErrorCode() const;

        /**
         * @brief Get error message from last error
         *
         * @return Error description string
         */
        std::string getErrorMessage() const;

    private:
        // Implementation using PIMPL idiom
        class AddressImpl;
        std::unique_ptr<AddressImpl> impl_;
    };

} // namespace osc
