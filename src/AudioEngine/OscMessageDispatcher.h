#pragma once

#include <string>
#include <vector>
#include <map>
#include <any>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>
#include <queue>
#include <condition_variable>
#include <regex>
#include "OscController.h"

namespace AudioEngine
{
    class OscMessageDispatcher;

    /**
     * @brief Handler interface for OSC messages
     */
    class OscMessageHandler
    {
    public:
        virtual ~OscMessageHandler() = default;

        /**
         * @brief Handle an OSC message
         *
         * @param address OSC address of the message
         * @param args Arguments of the message
         * @return true if the message was handled
         */
        virtual bool handleMessage(const std::string &address, const std::vector<std::any> &args) = 0;

        /**
         * @brief Check if this handler can process the given OSC address
         *
         * @param address OSC address to check
         * @return true if this handler can process the address
         */
        virtual bool canHandle(const std::string &address) const = 0;
    };

    /**
     * @brief Pattern-based handler for OSC messages
     *
     * This handler uses regex pattern matching to determine which messages it can handle.
     */
    class PatternOscMessageHandler : public OscMessageHandler
    {
    public:
        using HandlerFunction = std::function<bool(const std::string &, const std::vector<std::any> &)>;

        /**
         * @brief Construct a pattern-based handler
         *
         * @param pattern Regex pattern to match against OSC addresses
         * @param handlerFunc Function to call when a matching message is received
         */
        PatternOscMessageHandler(const std::string &pattern, HandlerFunction handlerFunc)
            : m_pattern(pattern), m_handlerFunc(handlerFunc) {}

        bool handleMessage(const std::string &address, const std::vector<std::any> &args) override
        {
            return m_handlerFunc(address, args);
        }

        bool canHandle(const std::string &address) const override
        {
            return std::regex_match(address, m_pattern);
        }

    private:
        std::regex m_pattern;
        HandlerFunction m_handlerFunc;
    };

    /**
     * @brief Message queue for thread-safe OSC message processing
     */
    class OscMessageQueue
    {
    public:
        /**
         * @brief Message structure
         */
        struct Message
        {
            std::string address;
            std::vector<std::any> args;
        };

        /**
         * @brief Enqueue a message
         *
         * @param address OSC address
         * @param args Message arguments
         */
        void enqueue(const std::string &address, const std::vector<std::any> &args)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push({address, args});
            m_cv.notify_one();
        }

        /**
         * @brief Dequeue a message with timeout
         *
         * @param message Output parameter for the message
         * @param timeoutMs Timeout in milliseconds
         * @return true if a message was dequeued, false if timeout
         */
        bool dequeue(Message &message, int timeoutMs = 0)
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            if (timeoutMs <= 0)
            {
                // Non-blocking check
                if (m_queue.empty())
                {
                    return false;
                }
            }
            else
            {
                // Wait with timeout
                if (!m_cv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                                   [this]
                                   { return !m_queue.empty(); }))
                {
                    return false; // Timeout
                }
            }

            // Get the message
            message = m_queue.front();
            m_queue.pop();
            return true;
        }

        /**
         * @brief Check if the queue is empty
         *
         * @return true if empty
         */
        bool empty() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_queue.empty();
        }

        /**
         * @brief Get the size of the queue
         *
         * @return Queue size
         */
        size_t size() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_queue.size();
        }

        /**
         * @brief Clear the queue
         */
        void clear()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            std::queue<Message> empty;
            std::swap(m_queue, empty);
        }

    private:
        mutable std::mutex m_mutex;
        std::condition_variable m_cv;
        std::queue<Message> m_queue;
    };

    /**
     * @brief Dispatcher for OSC messages
     *
     * This class routes OSC messages to the appropriate handlers.
     */
    class OscMessageDispatcher
    {
    public:
        /**
         * @brief Construct a new dispatcher
         *
         * @param controller OSC controller to use for communication
         */
        OscMessageDispatcher(OscController *controller);

        /**
         * @brief Destructor
         */
        ~OscMessageDispatcher();

        /**
         * @brief Add a message handler
         *
         * @param handler Handler to add (ownership transferred)
         */
        void addHandler(OscMessageHandler *handler);

        /**
         * @brief Add a pattern-based handler
         *
         * @param pattern Regex pattern to match
         * @param handler Function to call for matching messages
         * @return Pointer to the created handler (owned by the dispatcher)
         */
        PatternOscMessageHandler *addPatternHandler(
            const std::string &pattern,
            PatternOscMessageHandler::HandlerFunction handler);

        /**
         * @brief Start the message processing thread
         *
         * @return true if started successfully
         */
        bool start();

        /**
         * @brief Stop the message processing thread
         */
        void stop();

        /**
         * @brief Dispatch a message to appropriate handlers
         *
         * @param address OSC address
         * @param args Message arguments
         * @return true if at least one handler processed the message
         */
        bool dispatch(const std::string &address, const std::vector<std::any> &args);

        /**
         * @brief Get the message queue
         *
         * @return Reference to the message queue
         */
        OscMessageQueue &getQueue() { return m_queue; }

    private:
        /**
         * @brief Processing thread function
         */
        void processThread();

        /**
         * @brief OSC message callback
         *
         * @param address OSC address
         * @param args Message arguments
         */
        void oscMessageCallback(const std::string &address, const std::vector<std::any> &args);

        OscController *m_controller;
        OscMessageQueue m_queue;
        std::vector<OscMessageHandler *> m_handlers;
        std::thread m_thread;
        std::atomic<bool> m_running;
        int m_callbackId;
        std::mutex m_handlerMutex;
    };

} // namespace AudioEngine
