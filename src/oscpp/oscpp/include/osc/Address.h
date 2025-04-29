// filepath: c:\codedev\auricleinc\oscmex\include\osc\Address.h
/*
 *  OSCPP - Open Sound Control C++ (OSCPP) Library.
 *  This header file declares the Address class, which is responsible for managing OSC addresses,
 *  including sending data and handling socket operations.
 */

#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "osc/Types.h"  // For Protocol enum and other types

namespace osc {

    // Forward declarations
    class AddressImpl;
    class Message;
    class Bundle;

    class Address {
       public:
        /**
         * @brief Construct an Address object with the specified host, port, and protocol.
         * @param host Hostname or IP address.
         * @param port Port number or service name.
         * @param protocol Transport protocol (UDP, TCP, UNIX).
         */
        Address(const std::string &host, const std::string &port,
                Protocol protocol = Protocol::UDP);

        /**
         * @brief Create an Address from a URL string
         * @param url URL in format "osc.proto://host:port/"
         * @return A new Address instance
         * @throws OSCException if the URL format is invalid
         */
        static Address fromUrl(const std::string &url);

        /**
         * @brief Destructor - cleans up resources.
         */
        ~Address();

        /**
         * @brief Copy constructor
         */
        Address(const Address &other);

        /**
         * @brief Copy assignment operator
         */
        Address &operator=(const Address &other);

        /**
         * @brief Move constructor
         */
        Address(Address &&other) noexcept;

        /**
         * @brief Move assignment operator
         */
        Address &operator=(Address &&other) noexcept;

        /**
         * @brief Send raw binary data to this address.
         * @param data The data buffer to send.
         * @return true if successful, false on error.
         * @throws OSCException on network errors
         */
        bool send(const std::vector<std::byte> &data);

        /**
         * @brief Send an OSC message to this address.
         * @param message The message to send.
         * @return true if successful, false on error.
         * @throws OSCException on network errors
         */
        bool send(const Message &message);

        /**
         * @brief Send an OSC bundle to this address.
         * @param bundle The bundle to send.
         * @return true if successful, false on error.
         * @throws OSCException on network errors
         */
        bool send(const Bundle &bundle);

        /**
         * @brief Get the URL of this address.
         * @return URL in format "osc.proto://host:port/".
         */
        std::string url() const;

        /**
         * @brief Get the hostname
         * @return The hostname or IP address
         */
        std::string host() const;

        /**
         * @brief Get the port
         * @return The port or service name
         */
        std::string port() const;

        /**
         * @brief Get the protocol
         * @return The transport protocol
         */
        Protocol protocol() const;

        /**
         * @brief Set the TTL (Time To Live) for multicast packets.
         * @param ttl TTL value (1-255)
         * @note Only applies to UDP protocol
         */
        void setTTL(int ttl);

        /**
         * @brief Get the current TTL value
         * @return Current TTL value
         */
        int getTTL() const;

        /**
         * @brief Enable or disable TCP_NODELAY (Nagle's algorithm)
         * @param enable true to disable Nagle's algorithm
         * @note Only applies to TCP protocol
         */
        void setNoDelay(bool enable);

        /**
         * @brief Set socket timeout
         * @param timeout Timeout duration in milliseconds
         */
        void setTimeout(std::chrono::milliseconds timeout);

        /**
         * @brief Check if the address is valid and can be used.
         * @return true if valid, false otherwise.
         */
        bool isValid() const;

        /**
         * @brief Get the last error code from socket operations.
         * @return Platform-specific error code.
         */
        int getErrorCode() const;

        /**
         * @brief Get the error message for the last error.
         * @return Error message string.
         */
        std::string getErrorMessage() const;

       private:
        AddressImpl *impl_;  // Pointer to implementation
    };

}  // namespace osc
