/*
 * liblo C++ wrapper
 *
 * This is a C++ wrapper for the liblo OSC library, providing convenient
 * C++ style interfaces and RAII-compliant object lifecycle management.
 */

#pragma once

#include "lo.h"
#include <string>
#include <stdexcept>
#include <memory>
#include <vector>
#include <functional>

namespace lo
{

    // Forward declarations
    class Address;
    class Message;
    class ServerThread;
    class Method;

    /**
     * @brief Exception class for liblo errors
     */
    class Exception : public std::runtime_error
    {
    public:
        explicit Exception(const std::string &message)
            : std::runtime_error(message) {}
    };

    /**
     * @brief RAII wrapper for lo_address
     */
    class Address
    {
    public:
        /**
         * @brief Create an OSC address from hostname and port
         *
         * @param host Hostname or IP address
         * @param port Port number as string
         */
        Address(const std::string &host, const std::string &port)
        {
            addr = lo_address_new(host.c_str(), port.c_str());
            if (!addr)
            {
                throw Exception("Failed to create lo_address");
            }
        }

        /**
         * @brief Create from URL
         *
         * @param url URL in format "protocol://host:port"
         */
        explicit Address(const std::string &url)
        {
            addr = lo_address_new_from_url(url.c_str());
            if (!addr)
            {
                throw Exception("Failed to create lo_address from URL: " + url);
            }
        }

        /**
         * @brief Destructor
         */
        ~Address()
        {
            if (addr)
            {
                lo_address_free(addr);
                addr = nullptr;
            }
        }

        /**
         * @brief Move constructor
         */
        Address(Address &&other) noexcept : addr(other.addr)
        {
            other.addr = nullptr;
        }

        /**
         * @brief Move assignment
         */
        Address &operator=(Address &&other) noexcept
        {
            if (this != &other)
            {
                if (addr)
                {
                    lo_address_free(addr);
                }
                addr = other.addr;
                other.addr = nullptr;
            }
            return *this;
        }

        /**
         * @brief Copy constructor is deleted
         */
        Address(const Address &) = delete;

        /**
         * @brief Copy assignment is deleted
         */
        Address &operator=(const Address &) = delete;

        /**
         * @brief Check if the address is valid
         *
         * @return true if address is valid
         */
        bool is_valid() const
        {
            return addr != nullptr;
        }

        /**
         * @brief Get the protocol of the address
         *
         * @return Protocol name
         */
        std::string get_protocol() const
        {
            if (!is_valid())
                return "";
            char *proto = lo_address_get_protocol(addr);
            std::string result = proto ? proto : "";
            free(proto);
            return result;
        }

        /**
         * @brief Get the hostname of the address
         *
         * @return Hostname
         */
        std::string get_hostname() const
        {
            if (!is_valid())
                return "";
            char *host = lo_address_get_hostname(addr);
            std::string result = host ? host : "";
            free(host);
            return result;
        }

        /**
         * @brief Get the port of the address
         *
         * @return Port
         */
        std::string get_port() const
        {
            if (!is_valid())
                return "";
            char *port = lo_address_get_port(addr);
            std::string result = port ? port : "";
            free(port);
            return result;
        }

        /**
         * @brief Get the URL of the address
         *
         * @return URL in format "protocol://host:port"
         */
        std::string get_url() const
        {
            if (!is_valid())
                return "";
            char *url = lo_address_get_url(addr);
            std::string result = url ? url : "";
            free(url);
            return result;
        }

        /**
         * @brief Get the error code of the last operation
         *
         * @return Error code
         */
        int get_errno() const
        {
            return lo_address_errno(addr);
        }

        /**
         * @brief Get the error string of the last operation
         *
         * @return Error string
         */
        std::string errstr() const
        {
            if (!is_valid())
                return "Invalid address";
            return lo_address_errstr(addr);
        }

        /**
         * @brief Set the TTL for the address
         *
         * @param ttl Time to live
         */
        void set_ttl(int ttl)
        {
            if (is_valid())
            {
                lo_address_set_ttl(addr, ttl);
            }
        }

        /**
         * @brief Set the timeout for the address
         *
         * @param timeout_ms Timeout in milliseconds
         */
        void set_timeout_ms(int timeout_ms)
        {
            if (is_valid())
            {
                lo_address_set_tcp_nodelay(addr, 1); // Disable Nagle's algorithm
                lo_address_set_timeout_ms(addr, timeout_ms);
            }
        }

        /**
         * @brief Send a message
         *
         * @param path OSC path
         * @param message Message to send
         * @return int < 0 on error, >= 0 on success
         */
        int send(const std::string &path, const Message &message) const;

        /**
         * @brief Get the underlying lo_address
         *
         * @return lo_address
         */
        lo_address get_raw() const
        {
            return addr;
        }

    private:
        lo_address addr;

        friend class ServerThread;
    };

    /**
     * @brief RAII wrapper for lo_message
     */
    class Message
    {
    public:
        /**
         * @brief Create an empty message
         */
        Message()
        {
            msg = lo_message_new();
            if (!msg)
            {
                throw Exception("Failed to create lo_message");
            }
        }

        /**
         * @brief Destructor
         */
        ~Message()
        {
            if (msg)
            {
                lo_message_free(msg);
                msg = nullptr;
            }
        }

        /**
         * @brief Move constructor
         */
        Message(Message &&other) noexcept : msg(other.msg)
        {
            other.msg = nullptr;
        }

        /**
         * @brief Move assignment
         */
        Message &operator=(Message &&other) noexcept
        {
            if (this != &other)
            {
                if (msg)
                {
                    lo_message_free(msg);
                }
                msg = other.msg;
                other.msg = nullptr;
            }
            return *this;
        }

        /**
         * @brief Copy constructor is deleted
         */
        Message(const Message &) = delete;

        /**
         * @brief Copy assignment is deleted
         */
        Message &operator=(const Message &) = delete;

        /**
         * @brief Add a float to the message
         *
         * @param f Float value
         * @return Message& Reference to this message
         */
        Message &add_float(float f)
        {
            lo_message_add_float(msg, f);
            return *this;
        }

        /**
         * @brief Add an integer to the message
         *
         * @param i Integer value
         * @return Message& Reference to this message
         */
        Message &add_int32(int i)
        {
            lo_message_add_int32(msg, i);
            return *this;
        }

        /**
         * @brief Add a string to the message
         *
         * @param s String value
         * @return Message& Reference to this message
         */
        Message &add_string(const char *s)
        {
            lo_message_add_string(msg, s);
            return *this;
        }

        /**
         * @brief Add a boolean to the message
         *
         * @param b Boolean value
         * @return Message& Reference to this message
         */
        Message &add_bool(bool b)
        {
            if (b)
            {
                lo_message_add_true(msg);
            }
            else
            {
                lo_message_add_false(msg);
            }
            return *this;
        }

        /**
         * @brief Add a double to the message
         *
         * @param d Double value
         * @return Message& Reference to this message
         */
        Message &add_double(double d)
        {
            lo_message_add_double(msg, d);
            return *this;
        }

        /**
         * @brief Get the underlying lo_message
         *
         * @return lo_message
         */
        lo_message get_raw() const
        {
            return msg;
        }

    private:
        lo_message msg;

        friend class Address;
        friend class ServerThread;
    };

    /**
     * @brief RAII wrapper for lo_server_thread
     *
     * Note: This is a partial implementation. The full wrapper would include
     * more methods and support for callbacks.
     */
    class ServerThread
    {
    public:
        /**
         * @brief Create a server thread on the specified port
         *
         * @param port Port number as string
         */
        explicit ServerThread(const std::string &port)
        {
            server = lo_server_thread_new(port.c_str(), nullptr);
            if (!server)
            {
                throw Exception("Failed to create lo_server_thread on port " + port);
            }
        }

        /**
         * @brief Destructor
         */
        ~ServerThread()
        {
            if (server)
            {
                stop();
                lo_server_thread_free(server);
                server = nullptr;
            }
        }

        /**
         * @brief Move constructor
         */
        ServerThread(ServerThread &&other) noexcept : server(other.server)
        {
            other.server = nullptr;
        }

        /**
         * @brief Move assignment
         */
        ServerThread &operator=(ServerThread &&other) noexcept
        {
            if (this != &other)
            {
                if (server)
                {
                    stop();
                    lo_server_thread_free(server);
                }
                server = other.server;
                other.server = nullptr;
            }
            return *this;
        }

        /**
         * @brief Copy constructor is deleted
         */
        ServerThread(const ServerThread &) = delete;

        /**
         * @brief Copy assignment is deleted
         */
        ServerThread &operator=(const ServerThread &) = delete;

        /**
         * @brief Start the server thread
         *
         * @return int < 0 on error, 0 on success
         */
        int start()
        {
            return lo_server_thread_start(server);
        }

        /**
         * @brief Stop the server thread
         */
        void stop()
        {
            if (server)
            {
                lo_server_thread_stop(server);
            }
        }

        /**
         * @brief Get the port the server is listening on
         *
         * @return Port number as integer
         */
        int get_port() const
        {
            return lo_server_thread_get_port(server);
        }

        /**
         * @brief Get the underlying lo_server_thread
         *
         * @return lo_server_thread
         */
        lo_server_thread get_raw() const
        {
            return server;
        }

    private:
        lo_server_thread server;
    };

    // Implementation of Address::send that depends on Message
    inline int Address::send(const std::string &path, const Message &message) const
    {
        if (!is_valid())
        {
            throw Exception("Cannot send message: Invalid address");
        }
        return lo_send_message(addr, path.c_str(), message.get_raw());
    }

} // namespace lo
