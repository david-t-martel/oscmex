#include "AudioEngine.h"
#include "AsioManager.h"
#include "OscController.h"
#include "DeviceStateManager.h"
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
        m_config = std::move(config);

        // Initialize the ASIO manager if we're using ASIO
        if (!m_config.getAsioDeviceName().empty())
        {
            m_asioManager = std::make_unique<AsioManager>();

            // Load the driver
            if (!m_asioManager->loadDriver(m_config.getAsioDeviceName()))
            {
                reportStatus("Error", "Failed to load ASIO driver: " + m_config.getAsioDeviceName());
                return false;
            }

            // Initialize with explicitly configured values or auto-configure
            if (m_config.useAsioAutoConfig())
            {
                if (!m_asioManager->initDevice(0, 0))
                {
                    reportStatus("Error", "Failed to initialize ASIO device with default settings");
                    return false;
                }

                // Update configuration with actual device capabilities
                long bufferSize;
                double sampleRate;
                if (m_asioManager->getDefaultDeviceConfiguration(bufferSize, sampleRate))
                {
                    m_config.setBufferSize(bufferSize);
                    m_config.setSampleRate(sampleRate);

                    // Set internal format and layout
                    m_config.setInternalFormat("f32");
                    m_config.setInternalLayout("stereo");

                    reportStatus("Info", "Auto-configured with sample rate: " +
                                             std::to_string(sampleRate) + " Hz, buffer size: " +
                                             std::to_string(bufferSize));
                }
            }
            else
            {
                // Use explicit configuration
                if (!m_asioManager->initDevice(m_config.getSampleRate(), m_config.getBufferSize()))
                {
                    reportStatus("Error", "Failed to initialize ASIO device with explicit settings");
                    return false;
                }
            }

            // Auto-generate a default graph if needed
            if (m_config.getNodes().empty())
            {
                ConfigurationParser::autoConfigureAsio(m_config, m_asioManager.get());
                reportStatus("Info", "Generated default audio processing graph");
            }
        }

        // Create and configure nodes
        if (!createAndConfigureNodes())
        {
            reportStatus("Error", "Failed to create and configure nodes");
            return false;
        }

        // Set up connections between nodes
        if (!setupConnections())
        {
            reportStatus("Error", "Failed to set up connections between nodes");
            return false;
        }

        // Calculate process order for the nodes
        if (!calculateProcessOrder())
        {
            reportStatus("Error", "Failed to determine node processing order");
            return false;
        }

        // Set up ASIO buffers if needed
        if (m_asioManager)
        {
            std::vector<long> inputChannels;
            std::vector<long> outputChannels;

            // Collect channel indices from nodes
            for (auto &node : m_nodes)
            {
                if (auto *source = dynamic_cast<AsioSourceNode *>(node.get()))
                {
                    auto indices = source->getAsioChannelIndices();
                    inputChannels.insert(inputChannels.end(), indices.begin(), indices.end());
                }
                else if (auto *sink = dynamic_cast<AsioSinkNode *>(node.get()))
                {
                    auto indices = sink->getAsioChannelIndices();
                    outputChannels.insert(outputChannels.end(), indices.begin(), indices.end());
                }
            }

            // Create the ASIO buffers
            if (!m_asioManager->createBuffers(inputChannels, outputChannels))
            {
                reportStatus("Error", "Failed to create ASIO buffers");
                return false;
            }

            // Set up the ASIO callback
            m_asioManager->setCallback([this](long doubleBufferIndex)
                                       { this->processAsioBlock(doubleBufferIndex, true); });
        }

        // Initialize external control (OSC)
        if (!m_config.getTargetIp().empty() && m_config.getTargetPort() > 0)
        {
            m_oscController = std::make_unique<OscController>();
            if (!m_oscController->configure(m_config.getTargetIp(), m_config.getTargetPort()))
            {
                reportStatus("Warning", "Failed to configure OSC controller - continuing without OSC");
            }
            else
            {
                // Send initial configuration commands
                for (const auto &cmd : m_config.getCommands())
                {
                    if (!m_oscController->sendCommand(cmd.address, cmd.args))
                    {
                        reportStatus("Warning", "Failed to send OSC command: " + cmd.address);
                    }
                }
            }
        }

        reportStatus("Info", "Audio engine initialized successfully");
        return true;
    }

    bool AudioEngine::autoConfigureAsio()
    {
        if (!m_asioManager)
        {
            reportStatus("Error", "ASIO manager not created");
            return false;
        }

        // Get the device name from config
        std::string deviceName = m_config.getAsioDeviceName();
        if (deviceName.empty())
        {
            // Get available ASIO devices
            auto devices = AsioManager::getDeviceList();
            if (devices.empty())
            {
                reportStatus("Error", "No ASIO devices found");
                return false;
            }

            // Use first available device
            deviceName = devices[0];
            reportStatus("Info", "Using first available ASIO device: " + deviceName);
        }

        // Load the driver
        if (!m_asioManager->loadDriver(deviceName))
        {
            reportStatus("Error", "Failed to load ASIO driver: " + deviceName);
            return false;
        }

        // Initialize with default settings (passing 0 for sample rate and buffer size)
        if (!m_asioManager->initDevice(0, 0))
        {
            reportStatus("Error", "Failed to initialize ASIO device");
            return false;
        }

        // Get optimal settings from the driver
        long bufferSize;
        double sampleRate;
        if (!m_asioManager->getDefaultDeviceConfiguration(bufferSize, sampleRate))
        {
            reportStatus("Error", "Failed to get default ASIO configuration");
            return false;
        }

        // Update our configuration with the optimal settings
        m_config.setBufferSize(bufferSize);
        m_config.setSampleRate(sampleRate);
        reportStatus("Info", "Auto-configured ASIO with sample rate: " + std::to_string(sampleRate) +
                                 " Hz, buffer size: " + std::to_string(bufferSize));

        // Get available channels
        long inputChannelCount = m_asioManager->getInputChannelCount();
        long outputChannelCount = m_asioManager->getOutputChannelCount();

        // Prepare default channel configuration:
        // Use first two input channels and first two output channels if available
        std::vector<long> inputChannels;
        std::vector<long> outputChannels;

        // Add up to 2 input channels
        for (long i = 0; i < std::min(2L, inputChannelCount); i++)
        {
            inputChannels.push_back(i);
        }

        // Add up to 2 output channels
        for (long i = 0; i < std::min(2L, outputChannelCount); i++)
        {
            outputChannels.push_back(i);
        }

        // Create buffers for the selected channels
        if (!m_asioManager->createBuffers(inputChannels, outputChannels))
        {
            reportStatus("Error", "Failed to create ASIO buffers");
            return false;
        }

        // Auto-create ASIO nodes if needed
        if (m_config.getNodes().empty())
        {
            // Create default ASIO nodes
            createDefaultAsioNodes(inputChannels, outputChannels);

            // Create connections between the nodes if needed
            if (m_config.getConnections().empty() &&
                !m_nodes.empty() && m_nodes.size() >= 2)
            {
                // Simple passthrough connection between first and last node
                ConnectionConfig connection;
                connection.sourceName = m_nodes.front()->getName();
                connection.sourcePad = 0;
                connection.sinkName = m_nodes.back()->getName();
                connection.sinkPad = 0;
                m_config.addConnection(connection);

                reportStatus("Info", "Created default connection between " +
                                         connection.sourceName + " and " + connection.sinkName);
            }
        }

        // Set up the ASIO callback
        m_asioManager->setCallback([this](long doubleBufferIndex)
                                   { processAsioBlock(doubleBufferIndex, true); });

        return true;
    }

    void AudioEngine::createDefaultAsioNodes(const std::vector<long> &inputChannels, const std::vector<long> &outputChannels)
    {
        if (!m_asioManager)
            return;

        // Get channel names for better node labeling
        auto inputChannelNames = m_asioManager->getInputChannelNames();
        auto outputChannelNames = m_asioManager->getOutputChannelNames();

        // Create an ASIO source node if we have input channels
        if (!inputChannels.empty())
        {
            std::string nodeName = "asio_input";
            nlohmann::json sourceParams;

            // Add channel indices to the parameters
            std::string channelIndices;
            for (size_t i = 0; i < inputChannels.size(); i++)
            {
                if (i > 0)
                    channelIndices += ",";
                channelIndices += std::to_string(inputChannels[i]);
            }

            sourceParams["channels"] = channelIndices;
            sourceParams["device"] = m_asioManager->getDeviceName();

            // Create a node config
            NodeConfig nodeConfig;
            nodeConfig.name = nodeName;
            nodeConfig.type = "asio_source";
            nodeConfig.params = sourceParams.dump();

            // Add the node to the configuration
            m_config.addNode(nodeConfig);

            reportStatus("Info", "Created ASIO source node with channels: " + channelIndices);
        }

        // Create an ASIO sink node if we have output channels
        if (!outputChannels.empty())
        {
            std::string nodeName = "asio_output";
            nlohmann::json sinkParams;

            // Add channel indices to the parameters
            std::string channelIndices;
            for (size_t i = 0; i < outputChannels.size(); i++)
            {
                if (i > 0)
                    channelIndices += ",";
                channelIndices += std::to_string(outputChannels[i]);
            }

            sinkParams["channels"] = channelIndices;
            sinkParams["device"] = m_asioManager->getDeviceName();

            // Create a node config
            NodeConfig nodeConfig;
            nodeConfig.name = nodeName;
            nodeConfig.type = "asio_sink";
            nodeConfig.params = sinkParams.dump();

            // Add the node to the configuration
            m_config.addNode(nodeConfig);

            reportStatus("Info", "Created ASIO sink node with channels: " + channelIndices);
        }
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
            // Start the non-ASIO processing loop
            m_stopProcessingThread = false;
            m_processingThread = std::thread(&AudioEngine::runFileProcessingLoop, this);
            reportStatus("Info", "Started non-ASIO processing thread");
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
            // Stop the non-ASIO processing thread
            if (m_processingThread.joinable())
            {
                m_stopProcessingThread = true;
                m_processingThread.join();
                reportStatus("Info", "Non-ASIO processing thread stopped");
            }
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

        // Make sure processing thread is stopped
        if (m_processingThread.joinable())
        {
            m_stopProcessingThread = true;
            m_processingThread.join();
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
            // Get all AsioSourceNodes and AsioSinkNodes
            std::vector<AsioSourceNode *> asioSources;
            std::vector<AsioSinkNode *> asioSinks;

            for (const auto &node : m_nodes)
            {
                if (node->getType() == NodeType::ASIO_SOURCE)
                {
                    asioSources.push_back(static_cast<AsioSourceNode *>(node.get()));
                }
                else if (node->getType() == NodeType::ASIO_SINK)
                {
                    asioSinks.push_back(static_cast<AsioSinkNode *>(node.get()));
                }
            }

            // Get ASIO buffers for all active input channels
            std::vector<long> inputChannelIndices;
            std::vector<void *> inputBuffers;

            // Collect all input channel indices from source nodes
            for (AsioSourceNode *node : asioSources)
            {
                // Implementation needed for getting channel indices from node
                // This is a placeholder - actual implementation needs to access private members
                // which should be done through a better interface

                // For now, assume we have a getter for channel indices
                // std::vector<long> indices = node->getChannelIndices();
                // inputChannelIndices.insert(inputChannelIndices.end(), indices.begin(), indices.end());
            }

            // Get the input buffer pointers from ASIO
            if (!inputChannelIndices.empty())
            {
                if (!m_asioManager->getBufferPointers(doubleBufferIndex, inputChannelIndices, inputBuffers))
                {
                    reportStatus("Error", "Failed to get ASIO input buffer pointers");
                    return false;
                }
            }

            // Deliver input buffers to source nodes
            // This is a simplified approach - in reality, we might need a more sophisticated
            // way to map buffer pointers to nodes

            int bufferIndex = 0;
            for (AsioSourceNode *node : asioSources)
            {
                // Again, this is a simplified approach
                // Would need to get the channel count and slice the buffer array accordingly
                // node->receiveAsioData(doubleBufferIndex, std::vector<void*>(inputBuffers.begin() + bufferIndex, ...));
                // bufferIndex += node->getChannelCount();
            }

            // Process all nodes in the calculated order
            for (auto node : m_processOrder)
            {
                // Skip ASIO nodes - they're handled separately
                if (node->getType() == NodeType::ASIO_SOURCE || node->getType() == NodeType::ASIO_SINK)
                {
                    continue;
                }

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

            // Get ASIO buffers for all active output channels
            std::vector<long> outputChannelIndices;
            std::vector<void *> outputBuffers;

            // Collect all output channel indices from sink nodes
            for (AsioSinkNode *node : asioSinks)
            {
                // Implementation needed for getting channel indices from node
                // Similar to the source nodes above
            }

            // Get the output buffer pointers from ASIO
            if (!outputChannelIndices.empty())
            {
                if (!m_asioManager->getBufferPointers(doubleBufferIndex, outputChannelIndices, outputBuffers))
                {
                    reportStatus("Error", "Failed to get ASIO output buffer pointers");
                    return false;
                }
            }

            // Get output data from sink nodes and copy to ASIO buffers
            bufferIndex = 0;
            for (AsioSinkNode *node : asioSinks)
            {
                // Again, this is a simplified approach
                // node->provideAsioData(doubleBufferIndex, std::vector<void*>(outputBuffers.begin() + bufferIndex, ...));
                // bufferIndex += node->getChannelCount();
            }

            // Signal ASIO that we're done with this buffer
            ASIOOutputReady();

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

    void AudioEngine::runFileProcessingLoop()
    {
        reportStatus("Info", "Non-ASIO processing loop started");

        // Calculate processing interval based on buffer size and sample rate
        double framesPerSecond = m_config.getSampleRate();
        double framesPerBuffer = m_config.getBufferSize();
        std::chrono::microseconds processingInterval(static_cast<long long>(1000000.0 * framesPerBuffer / framesPerSecond));

        reportStatus("Info", "Processing interval: " + std::to_string(processingInterval.count()) + " microseconds");

        auto nextProcessingTime = std::chrono::high_resolution_clock::now();

        // Get list of file source nodes
        std::vector<FileSourceNode *> fileSourceNodes;
        for (const auto &node : m_nodes)
        {
            if (node->getType() == NodeType::FILE_SOURCE)
            {
                fileSourceNodes.push_back(static_cast<FileSourceNode *>(node.get()));
            }
        }

        // Main processing loop
        while (!m_stopProcessingThread)
        {
            auto startTime = std::chrono::high_resolution_clock::now();

            try
            {
                // Process all nodes in the calculated order
                for (auto node : m_processOrder)
                {
                    // Skip file source nodes, as they're handled separately
                    if (node->getType() == NodeType::FILE_SOURCE)
                        continue;

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

                // Check if any file source is at end of file
                bool allDone = !fileSourceNodes.empty();
                for (auto fileSource : fileSourceNodes)
                {
                    if (!fileSource->isEndOfFile())
                    {
                        allDone = false;
                        break;
                    }
                }

                if (allDone)
                {
                    reportStatus("Info", "All file sources reached end of file");
                    break;
                }
            }
            catch (const std::exception &e)
            {
                reportStatus("Error", "Exception in processing loop: " + std::string(e.what()));
                // Continue processing
            }
            catch (...)
            {
                reportStatus("Error", "Unknown exception in processing loop");
                // Continue processing
            }

            // Calculate elapsed time and sleep if needed
            auto endTime = std::chrono::high_resolution_clock::now();
            auto elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

            // Update next processing time
            nextProcessingTime += processingInterval;

            // If we're ahead of schedule, sleep until the next processing time
            if (endTime < nextProcessingTime)
            {
                std::this_thread::sleep_until(nextProcessingTime);
            }
            else
            {
                // We're behind schedule, log a warning and adjust the next processing time
                reportStatus("Warning", "Processing took longer than interval: " +
                                            std::to_string(elapsedTime.count()) + " microseconds");
                nextProcessingTime = endTime + processingInterval;
            }
        }

        reportStatus("Info", "Non-ASIO processing loop ended");
    }

    /**
     * @brief Configure ASIO using the device's default settings
     *
     * This method initializes the ASIO device with its preferred settings,
     * and creates a default configuration with the first stereo input and
     * stereo output channels.
     *
     * @param deviceName Name of the ASIO device to use
     * @return true if configuration was successful
     */
    bool AudioEngine::configureAsioDefaults(const std::string &deviceName)
    {
        if (!m_asioManager)
        {
            m_asioManager = std::make_unique<AsioManager>();
        }

        // Load the selected driver
        if (!m_asioManager->loadDriver(deviceName))
        {
            reportStatus("Error", "Failed to load ASIO driver: " + deviceName);
            return false;
        }

        // Initialize the device with its default settings
        // Pass 0 for preferredSampleRate and preferredBufferSize to use the device defaults
        if (!m_asioManager->initDevice(0, 0))
        {
            reportStatus("Error", "Failed to initialize ASIO device");
            return false;
        }

        // Get the device's default configuration
        double sampleRate = m_asioManager->getSampleRate();
        long bufferSize = m_asioManager->getPreferredBufferSize();

        // Get available channels
        long inputChannelCount = m_asioManager->getInputChannelCount();
        long outputChannelCount = m_asioManager->getOutputChannelCount();

        // Prepare default channel configuration:
        // Use first two input channels and first two output channels if available
        std::vector<long> inputChannels;
        std::vector<long> outputChannels;

        // Add up to 2 input channels
        for (long i = 0; i < std::min(2L, inputChannelCount); i++)
        {
            inputChannels.push_back(i);
        }

        // Add up to 2 output channels
        for (long i = 0; i < std::min(2L, outputChannelCount); i++)
        {
            outputChannels.push_back(i);
        }

        // Create buffers for the selected channels
        if (!m_asioManager->createBuffers(inputChannels, outputChannels))
        {
            reportStatus("Error", "Failed to create ASIO buffers");
            return false;
        }

        // Configure the audio engine with the ASIO device's settings
        bool success = configure(sampleRate, bufferSize);
        if (!success)
        {
            reportStatus("Error", "Failed to configure audio engine with ASIO settings");
            return false;
        }

        // Set up the ASIO callback
        m_asioManager->setCallback([this](long doubleBufferIndex)
                                   { processAsioBlock(doubleBufferIndex, true); });

        // Create default ASIO nodes based on the configured channels
        createDefaultAsioNodes(inputChannels, outputChannels);

        reportStatus("Info", "ASIO configured with device defaults: " +
                                 std::to_string(sampleRate) + " Hz, " +
                                 std::to_string(bufferSize) + " samples buffer size");

        return true;
    }

    /**
     * @brief Create default ASIO source and sink nodes
     *
     * Creates ASIO nodes with default names for the specified channels
     *
     * @param inputChannels Vector of input channel indices
     * @param outputChannels Vector of output channel indices
     */
    void AudioEngine::createDefaultAsioNodes(const std::vector<long> &inputChannels, const std::vector<long> &outputChannels)
    {
        // Get channel names for better node labeling
        auto inputChannelNames = m_asioManager->getInputChannelNames();
        auto outputChannelNames = m_asioManager->getOutputChannelNames();

        // Create an ASIO source node if we have input channels
        if (!inputChannels.empty())
        {
            std::string nodeName = "AsioInput";
            std::map<std::string, std::string> sourceParams;

            // Add channel indices to the parameters
            std::string channelIndices;
            for (size_t i = 0; i < inputChannels.size(); i++)
            {
                if (i > 0)
                    channelIndices += ",";
                channelIndices += std::to_string(inputChannels[i]);

                // Add a readable name if available
                if (inputChannels[i] < inputChannelNames.size())
                {
                    nodeName = "AsioInput_" + inputChannelNames[inputChannels[i]];
                    break; // Just use the first channel name to keep it simple
                }
            }

            sourceParams["channels"] = channelIndices;

            // Create the node
            auto sourceNode = std::make_shared<AsioSourceNode>(nodeName, this, m_asioManager.get());

            // Configure the node and add it to the engine
            if (sourceNode->configure(sourceParams, m_sampleRate, m_bufferSize, m_sampleFormat, m_channelLayout))
            {
                addNode(sourceNode);
                reportStatus("Info", "Created ASIO source node: " + nodeName);
            }
            else
            {
                reportStatus("Warning", "Failed to configure ASIO source node");
            }
        }

        // Create an ASIO sink node if we have output channels
        if (!outputChannels.empty())
        {
            std::string nodeName = "AsioOutput";
            std::map<std::string, std::string> sinkParams;

            // Add channel indices to the parameters
            std::string channelIndices;
            for (size_t i = 0; i < outputChannels.size(); i++)
            {
                if (i > 0)
                    channelIndices += ",";
                channelIndices += std::to_string(outputChannels[i]);

                // Add a readable name if available
                if (outputChannels[i] < outputChannelNames.size())
                {
                    nodeName = "AsioOutput_" + outputChannelNames[outputChannels[i]];
                    break; // Just use the first channel name to keep it simple
                }
            }

            sinkParams["channels"] = channelIndices;

            // Create the node
            auto sinkNode = std::make_shared<AsioSinkNode>(nodeName, this, m_asioManager.get());

            // Configure the node and add it to the engine
            if (sinkNode->configure(sinkParams, m_sampleRate, m_bufferSize, m_sampleFormat, m_channelLayout))
            {
                addNode(sinkNode);
                reportStatus("Info", "Created ASIO sink node: " + nodeName);
            }
            else
            {
                reportStatus("Warning", "Failed to configure ASIO sink node");
            }
        }
    }

    // Implementation for the initializeFromJson method
    bool AudioEngine::initializeFromJson(const std::string &jsonFilePath, std::shared_ptr<IExternalControl> externalControl)
    {
        reportStatus("Info", "Initializing from JSON file: " + jsonFilePath);

        // Load the configuration from JSON file
        bool success = false;
        Configuration config = Configuration::loadFromFile(jsonFilePath, success);

        if (!success)
        {
            reportStatus("Error", "Failed to load configuration from file: " + jsonFilePath);
            return false;
        }

        // Initialize the engine with the loaded configuration
        return initialize(std::move(config), externalControl);
    }

} // namespace AudioEngine
