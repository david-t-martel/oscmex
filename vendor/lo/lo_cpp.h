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
#include <any>
#include <map>
#include <algorithm>
#include <chrono>

// Forward declarations of C functions that aren't in the main headers
#ifdef __cplusplus
extern "C"
{
#endif

    // Server/ServerThread functions needed by this wrapper
    lo_server lo_server_thread_get_server(lo_server_thread st);
    void lo_server_thread_set_user_data(lo_server_thread st, void *user_data);
    void *lo_server_thread_get_user_data(lo_server_thread st);
    void *lo_server_get_user_data(lo_server s);
    int lo_server_thread_start(lo_server_thread st);
    int lo_server_thread_stop(lo_server_thread st);
    void lo_server_thread_free(lo_server_thread st);
    lo_server_thread lo_server_thread_new(const char *port, lo_err_handler err_h);
    int lo_server_thread_get_port(lo_server_thread st);
    lo_server lo_message_get_source(lo_message m);

#ifdef __cplusplus
}
#endif

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

    // Message callback type for use with Method
    using MessageCallback = std::function<void(const std::string &, const std::vector<std::any> &)>;

    /**
     * @brief RAII wrapper for lo_method
     */
    class Method
    {
    public:
        /**
         * @brief Destructor
         */
        ~Method()
        {
            if (method && server)
            {
                // Use C API to remove method
                lo_server s = lo_server_thread_get_server(server);
                if (s)
                {
                    lo_server_del_method(s, path.c_str(),
                                         typespec.empty() ? nullptr : typespec.c_str());
                }
                method = nullptr;
            }
        }

        // Delete copy operations
        Method(const Method &) = delete;
        Method &operator=(const Method &) = delete;

        // Support move operations
        Method(Method &&other) noexcept
            : method(other.method), server(other.server),
              path(std::move(other.path)), typespec(std::move(other.typespec))
        {
            other.method = nullptr;
            other.server = nullptr;
        }

        Method &operator=(Method &&other) noexcept
        {
            if (this != &other)
            {
                if (method && server)
                {
                    lo_server s = lo_server_thread_get_server(server);
                    if (s)
                    {
                        lo_server_del_method(s, path.c_str(),
                                             typespec.empty() ? nullptr : typespec.c_str());
                    }
                }
                method = other.method;
                server = other.server;
                path = std::move(other.path);
                typespec = std::move(other.typespec);
                other.method = nullptr;
                other.server = nullptr;
            }
            return *this;
        }

    private:
        // Private constructor - only ServerThread can create Method objects
        Method(lo_method m, lo_server_thread s, const std::string &p, const std::string &t)
            : method(m), server(s), path(p), typespec(t) {}

        lo_method method = nullptr;
        lo_server_thread server = nullptr;
        std::string path;
        std::string typespec;

        friend class ServerThread;
    };

    // Helper function to convert from lo_arg to std::any
    inline std::any lo_arg_to_any(const lo_arg *arg, char type)
    {
        switch (type)
        {
        case 'i':
            return std::any(arg->i);
        case 'f':
            return std::any(arg->f);
        case 's':
            return std::any(std::string(&arg->s));
        case 'b':
            return std::any(true);
        case 'F':
            return std::any(false);
        case 'd':
            return std::any(arg->d);
        // Add other types as needed
        default:
            return std::any();
        }
    }

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

            const char *proto = "";
            // Protocol is an integer in lo_address
            int proto_id = lo_address_get_protocol(addr);
            if (proto_id == 0)
                proto = "udp";
            else if (proto_id == 1)
                proto = "tcp";
            else if (proto_id == 2)
                proto = "unix";

            return proto;
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
            const char *host = lo_address_get_hostname(addr);
            return host ? host : "";
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
            const char *port = lo_address_get_port(addr);
            return port ? port : "";
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
         * @brief Send a message with std::any arguments
         *
         * @param path OSC path
         * @param args Vector of arguments
         * @return int < 0 on error, >= 0 on success
         */
        int send(const std::string &path, const std::vector<std::any> &args)
        {
            if (!is_valid())
            {
                throw Exception("Cannot send message: Invalid address");
            }

            Message msg;
            msg.add_from_vector(args);
            return send(path, msg);
        }

        /**
         * @brief Send a message and wait for a reply
         *
         * @param path OSC path
         * @param args Vector of arguments
         * @param reply_handler Function to handle the reply
         * @param timeout_ms Timeout in milliseconds
         * @return bool True if message was sent successfully
         */
        bool send_with_reply(const std::string &path,
                             const std::vector<std::any> &args,
                             MessageCallback reply_handler,
                             int timeout_ms = 1000)
        {
            if (!is_valid())
            {
                return false;
            }

            // Send the message
            Message msg;
            msg.add_from_vector(args);
            if (send(path, msg) < 0)
            {
                return false;
            }

            // TODO: Implement proper reply handling
            return true;
        }

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
         * @brief Add a std::any value to the message
         *
         * @param value Any value that can be converted to an OSC type
         * @return Message& Reference to this message
         */
        Message &add_any(const std::any &value)
        {
            if (value.type() == typeid(int))
                return add_int32(std::any_cast<int>(value));
            else if (value.type() == typeid(float))
                return add_float(std::any_cast<float>(value));
            else if (value.type() == typeid(double))
                return add_double(std::any_cast<double>(value));
            else if (value.type() == typeid(const char *))
                return add_string(std::any_cast<const char *>(value));
            else if (value.type() == typeid(std::string))
                return add_string(std::any_cast<std::string>(value).c_str());
            else if (value.type() == typeid(bool))
                return add_bool(std::any_cast<bool>(value));

            // Add more type conversions as needed
            throw Exception("Unsupported type in add_any");
        }

        /**
         * @brief Add multiple std::any values from a vector
         *
         * @param args Vector of values to add
         * @return Message& Reference to this message
         */
        Message &add_from_vector(const std::vector<std::any> &args)
        {
            for (const auto &arg : args)
            {
                add_any(arg);
            }
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
     */
    class ServerThread
    {
    public:
        /**
         * @brief Create a server thread on the specified port
         *
         * @param port Port number as string
         * @param user_data Optional user data pointer
         */
        explicit ServerThread(const std::string &port, void *user_data = nullptr)
        {
            server = lo_server_thread_new(port.c_str(), nullptr);
            if (!server)
            {
                throw Exception("Failed to create lo_server_thread on port " + port);
            }
            lo_server_thread_set_user_data(server, user_data ? user_data : this);
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
        ServerThread(ServerThread &&other) noexcept : server(other.server), m_callbacks(std::move(other.m_callbacks))
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
                m_callbacks = std::move(other.m_callbacks);
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
            if (!server)
                return -1;
            return lo_server_thread_get_port(server);
        }

        /**
         * @brief Add a method handler to the server
         *
         * @param path OSC path to register the callback for
         * @param typespec String of type characters, or empty string for any types
         * @param callback C++ callback function
         * @return std::unique_ptr<Method> RAII wrapper for the method
         */
        std::unique_ptr<Method> add_method(const std::string &path,
                                           const std::string &typespec,
                                           MessageCallback callback)
        {
            if (!server)
            {
                throw Exception("Cannot add method to null server");
            }

            // Store the callback
            int callbackId = generate_callback_id();
            m_callbacks[callbackId] = callback;

            // Get the server from the server thread
            lo_server s = lo_server_thread_get_server(server);
            if (!s)
            {
                throw Exception("Failed to get server from server thread");
            }

            // Create method with liblo's C API
            lo_method m = lo_server_add_method(
                s,
                path.c_str(),
                typespec.empty() ? nullptr : typespec.c_str(),
                &ServerThread::message_handler_static,
                reinterpret_cast<void *>(static_cast<intptr_t>(callbackId)));

            if (!m)
            {
                m_callbacks.erase(callbackId);
                throw Exception("Failed to add method for " + path);
            }

            return std::unique_ptr<Method>(new Method(m, server, path, typespec));
        }

        /**
         * @brief Remove a method handler from the server
         *
         * @param path OSC path to unregister
         * @param typespec String of type characters, or empty string for any types
         */
        void remove_method(const std::string &path, const std::string &typespec)
        {
            if (server)
            {
                lo_server s = lo_server_thread_get_server(server);
                if (s)
                {
                    lo_server_del_method(s, path.c_str(),
                                         typespec.empty() ? nullptr : typespec.c_str());
                }
            }
        }

        /**
         * @brief Wait for a specified time, processing any OSC messages received
         *
         * @param timeout Timeout in milliseconds
         * @return int Number of bytes received, or 0 if timeout
         */
        int wait(int timeout)
        {
            if (!server)
                return -1;
            lo_server s = lo_server_thread_get_server(server);
            if (!s)
                return -1;
            return lo_server_wait(s, timeout);
        }

        /**
         * @brief Receive and dispatch one pending OSC message
         *
         * @param timeout Timeout in milliseconds
         * @return int Number of bytes received, or 0 if timeout
         */
        int recv(int timeout)
        {
            if (!server)
                return -1;
            lo_server s = lo_server_thread_get_server(server);
            if (!s)
                return -1;
            return lo_server_recv_noblock(s, timeout);
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

        /**
         * @brief Get the underlying lo_server
         *
         * @return lo_server
         */
        lo_server get_server() const
        {
            if (!server)
                return nullptr;
            return lo_server_thread_get_server(server);
        }

    private:
        lo_server_thread server;
        std::map<int, MessageCallback> m_callbacks;
        int m_nextCallbackId = 1;

        int generate_callback_id()
        {
            return m_nextCallbackId++;
        }

        static int message_handler_static(const char *path, const char *types,
                                          lo_arg **argv, int argc,
                                          lo_message msg, void *user_data)
        {
            // Get server from message source
            lo_server s = lo_message_get_source(msg);
            if (!s)
                return 0;

            // Get user data from server
            void *server_user_data = lo_server_get_user_data(s);
            if (!server_user_data)
                return 0;

            // Cast to our ServerThread instance
            ServerThread *self = static_cast<ServerThread *>(server_user_data);

            // Get callback ID from user_data
            int callbackId = static_cast<int>(reinterpret_cast<intptr_t>(user_data));
            auto it = self->m_callbacks.find(callbackId);
            if (it == self->m_callbacks.end())
                return 0;

            // Convert arguments to std::any vector
            std::vector<std::any> args;
            args.reserve(argc);

            for (int i = 0; i < argc; i++)
            {
                args.push_back(lo_arg_to_any(argv[i], types[i]));
            }

            // Call the callback
            it->second(path, args);
            return 0;
        }
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
