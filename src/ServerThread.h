#pragma once

#include <string>
#include <memory>
#include <thread>
#include "Types.h"

namespace osc
{
    class Server;

    class ServerThread
    {
    public:
        // Constructor
        ServerThread(const std::string &port, Protocol protocol);

        // Destructor
        ~ServerThread();

        // Start the server thread
        bool start();

        // Stop the server thread
        void stop();

    private:
        // Thread execution function
        void run();

        std::unique_ptr<Server> server_;
        std::thread thread_;
        bool running_;
    };

} // namespace osc
