// filepath: c:\codedev\auricleinc\oscmex\include\osc\Address.h
/*
 *  OSCPP - Open Sound Control C++ (OSCPP) Library.
 *  This header file declares the Address class, which is responsible for managing OSC addresses,
 *  including sending data and handling socket operations.
 */

#pragma once

#include "Types.h"
#include <string>
#include <vector>

namespace osc
{

    class AddressImpl; // Forward declaration

    class Address
    {
    public:
        /**
         * @brief Construct an Address object with the specified host, port, and protocol.
         * @param host Hostname or IP address.
         * @param port Port number or service name.
         * @param protocol Transport protocol (UDP, TCP, UNIX).
         */
        Address(const std::string &host, const std::string &port, Protocol protocol);

        /**
         * @brief Destructor - cleans up resources.
         */
        ~Address();

        /**
         * @brief Send binary data to this address.
         * @param data The data buffer to send.
         * @return true if successful, false on error.
         */
        bool send(const std::vector<std::byte> &data);

        /**
         * @brief Get the URL of this address.
         * @return URL in format "osc.udp://host:port/".
         */
        std::string url() const;

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
        AddressImpl *impl_; // Pointer to implementation
    };

} // namespace osc