// filepath: c:\codedev\auricleinc\oscmex\src\ServerThread.cpp
#include "ServerThread.h"
#include "Server.h"
#include <thread>
#include <iostream>

namespace osc
{

    // Constructor
    ServerThread::ServerThread(const std::string &port, Protocol protocol)
        : server_(std::make_unique<Server>(port, protocol)), running_(false)
    {
    }

    // Start the server thread
    bool ServerThread::start()
    {
        if (running_)
            return false;

        running_ = true;
        thread_ = std::thread(&ServerThread::run, this);
        return true;
    }

    // Stop the server thread
    void ServerThread::stop()
    {
        if (!running_)
            return;

        running_ = false;
        if (thread_.joinable())
            thread_.join();
    }

    // Thread execution function
    void ServerThread::run()
    {
        while (running_)
        {
            server_->receive();
        }
    }

    // Destructor
    ServerThread::~ServerThread()
    {
        stop();
    }

} // namespace osc
