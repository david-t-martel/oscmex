// filepath: c:\codedev\auricleinc\oscmex\src\ServerThread.cpp
#include "osc/ServerThread.h"

#include <iostream>
#include <mutex>
#include <thread>

#include "osc/Exceptions.h"
#include "osc/Server.h"

namespace osc {

    // Constructor
    ServerThread::ServerThread(std::shared_ptr<Server> server)
        : server_(server), running_(false), errorHandler_(nullptr) {}

    // Start the server thread
    bool ServerThread::start() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) return false;

        running_ = true;
        thread_ = std::thread(&ServerThread::run, this);
        return true;
    }

    // Stop the server thread
    void ServerThread::stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_) return;
            running_ = false;
        }

        if (thread_.joinable()) thread_.join();
    }

    // Check if the server thread is running
    bool ServerThread::isRunning() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return running_;
    }

    // Set error handler
    void ServerThread::setErrorHandler(ErrorCallback handler) {
        std::lock_guard<std::mutex> lock(mutex_);
        errorHandler_ = handler;
    }

    // Thread execution function
    void ServerThread::run() {
        while (true) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (!running_) break;
            }

            try {
                if (!server_->receive()) {
                    // Short sleep to avoid busy-waiting when there are no messages
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            } catch (const OSCException& e) {
                std::lock_guard<std::mutex> lock(mutex_);
                if (errorHandler_) {
                    errorHandler_(e.what(), static_cast<int>(e.code()));
                } else {
                    std::cerr << "OSC Server error: " << e.what()
                              << " (code: " << static_cast<int>(e.code()) << ")" << std::endl;
                }
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lock(mutex_);
                if (errorHandler_) {
                    errorHandler_(e.what(), -1);
                } else {
                    std::cerr << "OSC Server error: " << e.what() << std::endl;
                }
            } catch (...) {
                std::lock_guard<std::mutex> lock(mutex_);
                if (errorHandler_) {
                    errorHandler_("Unknown error", -1);
                } else {
                    std::cerr << "OSC Server unknown error" << std::endl;
                }
            }
        }
    }

    // Destructor
    ServerThread::~ServerThread() { stop(); }

}  // namespace osc
