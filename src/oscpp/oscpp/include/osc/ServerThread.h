// filepath: c:\codedev\auricleinc\oscmex\include\osc\ServerThread.h
#pragma once

#include "Types.h"
#include <thread>
#include <atomic>
#include <functional>
#include <memory>

namespace osc
{

    class Server;

    /**
     * @brief Class for running an OSC server in a separate thread.
     */
    class ServerThread
    {
    public:
        /**
         * @brief Construct a new ServerThread object.
         * @param server The OSC server to run in the thread.
         */
        ServerThread(std::shared_ptr<Server> server);

        /**
         * @brief Start the server thread.
         * @return true if the thread started successfully, false otherwise.
         */
        bool start();

        /**
         * @brief Stop the server thread.
         */
        void stop();

        /**
         * @brief Check if the server thread is running.
         * @return true if running, false otherwise.
         */
        bool isRunning() const;

    private:
        std::shared_ptr<Server> server_;  ///< The OSC server instance.
        std::thread thread_;               ///< The thread running the server.
        std::atomic<bool> running_;        ///< Flag indicating if the thread is running.

        /**
         * @brief The main loop for the server thread.
         */
        void run();
    };

} // namespace osc