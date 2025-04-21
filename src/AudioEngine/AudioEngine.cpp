#include "AudioEngine.h"
#include "AsioManager.h"
#include "OscController.h"
#include "AudioBuffer.h"
#include "AsioNodes.h"
#include "FileNodes.h"
#include "FfmpegProcessorNode.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <stdexcept>
#include <queue>

namespace AudioEngine
{

    AudioEngine::AudioEngine()
        : m_running(false),
          m_nextCallbackId(0)
    {
        // Initialize process order
        m_processOrder.clear();
    }

    AudioEngine::~AudioEngine()
    {
        // Ensure everything is stopped and cleaned up
        if (m_running.load())
        {
            stop();
        }
        cleanup();
    }

    bool AudioEngine::initialize(Configuration config)
    {
        // Store configuration
        m_config = std::move(config);

        // Set up OSC controller
        m_oscController = std::make_unique<OscController>();
        if (!m_oscController->configure(m_config.getTargetIp(), m_config.getTargetPort(), m_config.getReceivePort()))
        {
            reportStatus("Error", "Failed to configure OSC controller");
            return false;
        }

        // Determine if we need ASIO based on the configuration
        bool needAsio = false;
        for (const auto &nodeConfig : m_config.getNodes())
        {
            if (nodeConfig.type == "asio_source" || nodeConfig.type == "asio_sink")
            {
                needAsio = true;
                break;
            }
        }

        // Initialize ASIO if needed
        if (needAsio)
        {
            m_asioManager = std::make_unique<AsioManager>();
            if (!m_asioManager->loadDriver(m_config.getAsioDeviceName()))
            {
                reportStatus("Error", "Failed to load ASIO driver: " + m_config.getAsioDeviceName());
                return false;
            }

            if (!m_asioManager->initDevice(m_config.getSampleRate(), m_config.getBufferSize()))
            {
                reportStatus("Error", "Failed to initialize ASIO device");
                return false;
            }

            // Set callback function - will need to be implemented properly
            m_asioManager->setCallback([this](long doubleBufferIndex, bool directProcess)
                                       { return this->processAsioBlock(doubleBufferIndex, directProcess); });
        }

        // Create and configure audio nodes
        if (!createAndConfigureNodes())
        {
            reportStatus("Error", "Failed to create and configure nodes");
            return false;
        }

        // Set up connections between nodes
        if (!setupConnections())
        {
            reportStatus("Error", "Failed to set up connections");
            return false;
        }

        // Calculate processing order
        if (!calculateProcessOrder())
        {
            reportStatus("Error", "Failed to calculate processing order");
            return false;
        }

        // Apply OSC commands from configuration
        if (!sendOscCommands())
        {
            reportStatus("Error", "Failed to send initial OSC commands");
            // Continue anyway, as this might not be critical
        }

        reportStatus("Info", "AudioEngine initialized successfully");
        return true;
    }

    bool AudioEngine::run()
    {
        std::lock_guard<std::mutex> lock(m_processMutex);

        if (m_running.load())
        {
            reportStatus("Warning", "AudioEngine is already running");
            return true;
        }

        // Start all nodes
        for (auto &node : m_nodes)
        {
            if (!node->start())
            {
                reportStatus("Error", "Failed to start node: " + node->getName());
                // Clean up nodes that were already started
                for (auto &n : m_nodes)
                {
                    if (n->isRunning())
                    {
                        n->stop();
                    }
                }
                return false;
            }
        }

        // Start ASIO if it's being used
        if (m_asioManager)
        {
            if (!m_asioManager->start())
            {
                reportStatus("Error", "Failed to start ASIO");
                // Stop all nodes
                for (auto &node : m_nodes)
                {
                    if (node->isRunning())
                    {
                        node->stop();
                    }
                }
                return false;
            }
        }
        else
        {
            // TODO: Implement non-ASIO processing loop
            // This would be a thread that manually processes nodes
            // in the correct order at regular intervals
            reportStatus("Warning", "Non-ASIO processing loop not implemented yet");
        }

        m_running.store(true);
        reportStatus("Info", "AudioEngine started");
        return true;
    }

    void AudioEngine::stop()
    {
        std::lock_guard<std::mutex> lock(m_processMutex);

        if (!m_running.load())
        {
            return;
        }

        // Stop ASIO if it's being used
        if (m_asioManager)
        {
            m_asioManager->stop();
        }
        else
        {
            // TODO: Stop the non-ASIO processing thread if it exists
        }

        // Stop all nodes
        for (auto &node : m_nodes)
        {
            if (node->isRunning())
            {
                node->stop();
            }
        }

        m_running.store(false);
        reportStatus("Info", "AudioEngine stopped");
    }

    void AudioEngine::cleanup()
    {
        std::lock_guard<std::mutex> lock(m_processMutex);

        // Make sure everything is stopped first
        if (m_running.load())
        {
            stop();
        }

        // Clear all nodes and connections
        m_nodes.clear();
        m_nodeMap.clear();
        m_connections.clear();
        m_processOrder.clear();

        // Clean up ASIO and OSC controllers
        if (m_asioManager)
        {
            m_asioManager.reset();
        }

        if (m_oscController)
        {
            m_oscController.reset();
        }

        reportStatus("Info", "AudioEngine cleaned up");
    }

    bool AudioEngine::processAsioBlock(long doubleBufferIndex, bool directProcess)
    {
        // This is called from the ASIO thread, so be careful about thread safety
        if (!m_running.load())
        {
            return false;
        }

        try
        {
            // Process all nodes in the calculated order
            for (auto node : m_processOrder)
            {
                // Process the node
                if (!node->process())
                {
                    reportStatus("Warning", "Node processing failed: " + node->getName());
                    // Continue with other nodes
                }
            }

            // Transfer data between connected nodes
            for (const auto &connection : m_connections)
            {
                // Get output buffer from source node
                auto sourceNode = connection.getSourceNode();
                auto sinkNode = connection.getSinkNode();

                if (!sourceNode || !sinkNode)
                {
                    continue;
                }

                int sourcePad = connection.getSourcePad();
                int sinkPad = connection.getSinkPad();

                // Get output buffer from source node
                auto buffer = sourceNode->getOutputBuffer(sourcePad);

                // Set input buffer on sink node
                if (buffer)
                {
                    if (!sinkNode->setInputBuffer(buffer, sinkPad))
                    {
                        reportStatus("Warning", "Failed to transfer buffer from " +
                                                    sourceNode->getName() + " to " + sinkNode->getName());
                    }
                }
            }

            return true;
        }
        catch (const std::exception &e)
        {
            reportStatus("Error", "Exception in processAsioBlock: " + std::string(e.what()));
            return false;
        }
        catch (...)
        {
            reportStatus("Error", "Unknown exception in processAsioBlock");
            return false;
        }
    }

    AudioNode *AudioEngine::getNodeByName(const std::string &name)
    {
        auto it = m_nodeMap.find(name);
        if (it != m_nodeMap.end())
        {
            return it->second;
        }
        return nullptr;
    }

    int AudioEngine::addStatusCallback(std::function<void(const std::string &, const std::string &)> callback)
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        int id = m_nextCallbackId++;
        m_statusCallbacks[id] = callback;
        return id;
    }

    void AudioEngine::removeStatusCallback(int callbackId)
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        m_statusCallbacks.erase(callbackId);
    }

    bool AudioEngine::createAndConfigureNodes()
    {
        reportStatus("Info", "Creating and configuring nodes");

        for (const auto &nodeConfig : m_config.getNodes())
        {
            std::unique_ptr<AudioNode> node = nullptr;

            // Create the appropriate type of node
            if (nodeConfig.type == "asio_source")
            {
                if (!m_asioManager)
                {
                    reportStatus("Error", "ASIO manager not available for asio_source node: " + nodeConfig.name);
                    return false;
                }
                node = std::make_unique<AsioSourceNode>(nodeConfig.name, this, m_asioManager.get());
            }
            else if (nodeConfig.type == "asio_sink")
            {
                if (!m_asioManager)
                {
                    reportStatus("Error", "ASIO manager not available for asio_sink node: " + nodeConfig.name);
                    return false;
                }
                node = std::make_unique<AsioSinkNode>(nodeConfig.name, this, m_asioManager.get());
            }
            else if (nodeConfig.type == "file_source")
            {
                node = std::make_unique<FileSourceNode>(nodeConfig.name, this);
            }
            else if (nodeConfig.type == "file_sink")
            {
                node = std::make_unique<FileSinkNode>(nodeConfig.name, this);
            }
            else if (nodeConfig.type == "ffmpeg_processor")
            {
                node = std::make_unique<FfmpegProcessorNode>(nodeConfig.name, this);
            }
            else
            {
                reportStatus("Error", "Unknown node type: " + nodeConfig.type);
                return false;
            }

            // Configure the node
            if (!node->configure(nodeConfig.params,
                                 m_config.getSampleRate(),
                                 m_config.getBufferSize(),
                                 AV_SAMPLE_FMT_FLTP,        // Default format - could be configurable
                                 AV_CHANNEL_LAYOUT_STEREO)) // Default layout - could be configurable
            {
                reportStatus("Error", "Failed to configure node: " + nodeConfig.name);
                return false;
            }

            // Store the node
            m_nodeMap[nodeConfig.name] = node.get();
            m_nodes.push_back(std::move(node));
        }

        reportStatus("Info", "Created " + std::to_string(m_nodes.size()) + " nodes");
        return true;
    }

    bool AudioEngine::setupConnections()
    {
        reportStatus("Info", "Setting up connections");

        for (const auto &connConfig : m_config.getConnections())
        {
            // Find the source and sink nodes
            auto sourceNode = getNodeByName(connConfig.sourceNode);
            auto sinkNode = getNodeByName(connConfig.sinkNode);

            if (!sourceNode)
            {
                reportStatus("Error", "Source node not found: " + connConfig.sourceNode);
                return false;
            }

            if (!sinkNode)
            {
                reportStatus("Error", "Sink node not found: " + connConfig.sinkNode);
                return false;
            }

            // Create the connection
            Connection connection(sourceNode, connConfig.sourcePad, sinkNode, connConfig.sinkPad);
            m_connections.push_back(connection);
        }

        reportStatus("Info", "Created " + std::to_string(m_connections.size()) + " connections");
        return true;
    }

    bool AudioEngine::sendOscCommands()
    {
        if (!m_oscController)
        {
            reportStatus("Warning", "OSC controller not available");
            return false;
        }

        reportStatus("Info", "Sending initial OSC commands");

        for (const auto &cmd : m_config.getCommands())
        {
            if (!m_oscController->sendCommand(cmd.address, cmd.args))
            {
                reportStatus("Warning", "Failed to send OSC command: " + cmd.address);
                // Continue with other commands
            }
        }

        return true;
    }

    void AudioEngine::reportStatus(const std::string &category, const std::string &message)
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);

        // Log to console first
        std::cout << "[" << category << "] " << message << std::endl;

        // Call all registered callbacks
        for (const auto &callback : m_statusCallbacks)
        {
            try
            {
                callback.second(category, message);
            }
            catch (...)
            {
                // Ignore exceptions from callbacks
                std::cerr << "Exception in status callback" << std::endl;
            }
        }
    }

    bool AudioEngine::calculateProcessOrder()
    {
        // This is a simple implementation that just ensures sources come before
        // processors, which come before sinks. A more sophisticated approach would
        // use a topological sort based on the connections.

        m_processOrder.clear();

        // First add all source nodes
        for (const auto &node : m_nodes)
        {
            if (node->getType() == NodeType::ASIO_SOURCE ||
                node->getType() == NodeType::FILE_SOURCE)
            {
                m_processOrder.push_back(node.get());
            }
        }

        // Then add all processor nodes
        for (const auto &node : m_nodes)
        {
            if (node->getType() == NodeType::FFMPEG_PROCESSOR)
            {
                m_processOrder.push_back(node.get());
            }
        }

        // Finally add all sink nodes
        for (const auto &node : m_nodes)
        {
            if (node->getType() == NodeType::ASIO_SINK ||
                node->getType() == NodeType::FILE_SINK)
            {
                m_processOrder.push_back(node.get());
            }
        }

        // Verify that all nodes are in the processing order
        if (m_processOrder.size() != m_nodes.size())
        {
            reportStatus("Error", "Not all nodes were added to the processing order");
            return false;
        }

        return true;
    }

} // namespace AudioEngine
