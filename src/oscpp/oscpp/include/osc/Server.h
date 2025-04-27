// filepath: c:\codedev\auricleinc\oscmex\include\osc\Server.h
/*
 *  OSCPP - Open Sound Control C++ (OSCPP) Library.
 *  This header file declares the Server class, which handles incoming OSC messages.
 */

#pragma once

#include "Types.h"
#include <string>
#include <functional>
#include <memory>

namespace osc
{

    class ServerImpl; // Forward declaration of the implementation class

    /**
     * @brief The Server class handles incoming OSC messages.
     */
    class Server
    {
    public:
        /**
         * @brief Construct a new Server object
         * @param port The port number to listen on
         * @param protocol The transport protocol (UDP, TCP, etc.)
         */
        Server(const std::string &port, Protocol protocol);

        /**
         * @brief Destructor
         */
        ~Server();

        /**
         * @brief Start the server
         * @return true if the server started successfully, false otherwise
         */
        bool start();

        /**
         * @brief Stop the server
         */
        void stop();

        /**
         * @brief Add a method handler for a specific OSC address
         * @param pathPattern The OSC address pattern
         * @param typeSpec The expected argument types
         * @param handler The function to handle incoming messages
         * @return MethodId The ID of the registered method
         */
        MethodId addMethod(const std::string &pathPattern, const std::string &typeSpec,
                           std::function<void(const Message &)> handler);

        /**
         * @brief Set a default handler for unhandled messages
         * @param handler The function to handle unhandled messages
         */
        void setDefaultMethod(std::function<void(const Message &)> handler);

        /**
         * @brief Set an error handler for the server
         * @param handler The function to handle errors
         */
        void setErrorHandler(std::function<void(int, const std::string &)> handler);

    private:
        std::unique_ptr<ServerImpl> impl_; // Pointer to the implementation
    };

} // namespace osc
