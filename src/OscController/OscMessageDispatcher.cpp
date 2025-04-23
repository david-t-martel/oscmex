#include "OscMessageDispatcher.h"
#include <iostream>

namespace AudioEngine
{
    OscMessageDispatcher::OscMessageDispatcher(OscController *controller)
        : m_controller(controller), m_running(false), m_callbackId(-1)
    {
    }

    OscMessageDispatcher::~OscMessageDispatcher()
    {
        stop();

        // Delete all handlers
        for (auto handler : m_handlers)
        {
            delete handler;
        }
        m_handlers.clear();
    }

    void OscMessageDispatcher::addHandler(OscMessageHandler *handler)
    {
        std::lock_guard<std::mutex> lock(m_handlerMutex);
        m_handlers.push_back(handler);
    }

    PatternOscMessageHandler *OscMessageDispatcher::addPatternHandler(
        const std::string &pattern,
        PatternOscMessageHandler::HandlerFunction handler)
    {
        PatternOscMessageHandler *patternHandler = new PatternOscMessageHandler(pattern, handler);
        addHandler(patternHandler);
        return patternHandler;
    }

    bool OscMessageDispatcher::start()
    {
        if (m_running || !m_controller)
        {
            return false;
        }

        // Register callback with the OSC controller
        m_callbackId = m_controller->addEventCallback([this](const std::string &address, const std::vector<std::any> &args)
                                                      { oscMessageCallback(address, args); });

        // Start the processing thread
        m_running = true;
        m_thread = std::thread(&OscMessageDispatcher::processThread, this);

        return true;
    }

    void OscMessageDispatcher::stop()
    {
        // Stop the thread
        if (m_running)
        {
            m_running = false;

            // Wait for the thread to finish
            if (m_thread.joinable())
            {
                m_thread.join();
            }
        }

        // Remove callback from controller
        if (m_controller && m_callbackId >= 0)
        {
            m_controller->removeEventCallback(m_callbackId);
            m_callbackId = -1;
        }
    }

    bool OscMessageDispatcher::dispatch(const std::string &address, const std::vector<std::any> &args)
    {
        bool handled = false;

        std::lock_guard<std::mutex> lock(m_handlerMutex);
        for (auto handler : m_handlers)
        {
            if (handler->canHandle(address))
            {
                if (handler->handleMessage(address, args))
                {
                    handled = true;
                    // Don't break - allow multiple handlers to process the message if needed
                }
            }
        }

        return handled;
    }

    void OscMessageDispatcher::processThread()
    {
        std::cout << "OSC message processing thread started" << std::endl;

        while (m_running)
        {
            OscMessageQueue::Message message;

            // Wait for a message with timeout
            if (m_queue.dequeue(message, 100))
            {
                // Dispatch the message
                bool handled = dispatch(message.address, message.args);

                if (!handled)
                {
                    // No handler processed the message
                    std::cout << "Unhandled OSC message: " << message.address << std::endl;
                }
            }
        }

        std::cout << "OSC message processing thread stopped" << std::endl;
    }

    void OscMessageDispatcher::oscMessageCallback(const std::string &address, const std::vector<std::any> &args)
    {
        // Enqueue the message for processing
        m_queue.enqueue(address, args);
    }

} // namespace AudioEngine
