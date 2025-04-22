#include "Configuration.h"
#include "OscController.h"
#include "AsioManager.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <nlohmann/json.hpp> // Use nlohmann/json for JSON parsing
#include <thread>
#include <future>

using json = nlohmann::json;

namespace AudioEngine
{
    // Implementation of Configuration class methods
    Configuration::Configuration()
        : m_deviceType(DeviceType::GENERIC_OSC),
          m_targetIp("127.0.0.1"),
          m_targetPort(9000),
          m_receivePort(8000),
          m_asioDeviceName(""),
          m_sampleRate(48000.0),
          m_bufferSize(512),
          m_useAsioAutoConfig(true),
          m_internalFormat("f32"),
          m_internalLayout("stereo")
    {
    }

    const NodeConfig *Configuration::getNodeConfig(const std::string &name) const
    {
        for (const auto &node : m_nodes)
        {
            if (node.name == name)
            {
                return &node;
            }
        }
        return nullptr;
    }

    void Configuration::addNodeConfig(const NodeConfig &node)
    {
        m_nodes.push_back(node);
    }

    void Configuration::addConnectionConfig(const ConnectionConfig &connection)
    {
        m_connections.push_back(connection);
    }

    void Configuration::setInternalFormat(const std::string &format)
    {
        // Validate format
        if (format == "f32" || format == "s16" || format == "s32" || format == "u8" || format == "flt" || format == "dbl")
        {
            m_internalFormat = format;
        }
        else
        {
            std::cerr << "Warning: Invalid format '" << format << "', defaulting to 'f32'" << std::endl;
            m_internalFormat = "f32";
        }
    }

    void Configuration::setInternalLayout(const std::string &layout)
    {
        // Validate layout
        if (layout == "mono" || layout == "stereo" || layout == "5.1" || layout == "7.1")
        {
            m_internalLayout = layout;
        }
        else
        {
            std::cerr << "Warning: Invalid layout '" << layout << "', defaulting to 'stereo'" << std::endl;
            m_internalLayout = "stereo";
        }
    }

    bool Configuration::autoDiscover(AsioManager *asioManager, bool createDefaultGraph)
    {
        if (!asioManager)
            return false;

        // Get device info
        if (!asioManager->isInitialized())
        {
            // If no device is specified, try to use the first available device
            if (m_asioDeviceName.empty())
            {
                auto devices = AsioManager::getDeviceList();
                if (devices.empty())
                    return false;

                m_asioDeviceName = devices[0];
            }

            // Load and initialize the driver
            if (!asioManager->loadDriver(m_asioDeviceName))
                return false;

            if (!asioManager->initDevice())
                return false;
        }

        // Get optimal device settings
        long bufferSize;
        double sampleRate;

        if (asioManager->getDefaultDeviceConfiguration(bufferSize, sampleRate))
        {
            m_bufferSize = bufferSize;
            m_sampleRate = sampleRate;
        }

        if (createDefaultGraph)
        {
            return this->createDefaultGraph(2, 2, true);
        }

        return true;
    }

    bool Configuration::createDefaultGraph(int inputChannels, int outputChannels, bool addProcessor)
    {
        // Clear existing nodes and connections
        m_nodes.clear();
        m_connections.clear();

        // Create source node if input channels > 0
        if (inputChannels > 0)
        {
            NodeConfig sourceNode;
            sourceNode.name = "asio_input";
            sourceNode.type = "asio_source";
            sourceNode.inputPads = 0; // Source has no inputs
            sourceNode.outputPads = 1;
            sourceNode.description = "ASIO Hardware Input";

            // Add channel indices
            for (int i = 0; i < inputChannels; i++)
            {
                sourceNode.channelIndices.push_back(i);
            }

            // Convert channel indices to parameter string
            std::stringstream ss;
            for (size_t i = 0; i < sourceNode.channelIndices.size(); i++)
            {
                if (i > 0)
                    ss << ",";
                ss << sourceNode.channelIndices[i];
            }
            sourceNode.params["channels"] = ss.str();

            m_nodes.push_back(sourceNode);
        }

        // Create processor node if requested
        if (addProcessor && inputChannels > 0 && outputChannels > 0)
        {
            NodeConfig processorNode;
            processorNode.name = "main_processor";
            processorNode.type = "ffmpeg_processor";
            processorNode.inputPads = 1;
            processorNode.outputPads = 1;
            processorNode.description = "Main Processor";
            processorNode.filterGraph = "volume=0dB"; // Identity filter by default
            processorNode.params["filterGraph"] = "volume=0dB";

            m_nodes.push_back(processorNode);
        }

        // Create sink node if output channels > 0
        if (outputChannels > 0)
        {
            NodeConfig sinkNode;
            sinkNode.name = "asio_output";
            sinkNode.type = "asio_sink";
            sinkNode.inputPads = 1;
            sinkNode.outputPads = 0; // Sink has no outputs
            sinkNode.description = "ASIO Hardware Output";

            // Add channel indices
            for (int i = 0; i < outputChannels; i++)
            {
                sinkNode.channelIndices.push_back(i);
            }

            // Convert channel indices to parameter string
            std::stringstream ss;
            for (size_t i = 0; i < sinkNode.channelIndices.size(); i++)
            {
                if (i > 0)
                    ss << ",";
                ss << sinkNode.channelIndices[i];
            }
            sinkNode.params["channels"] = ss.str();

            m_nodes.push_back(sinkNode);
        }

        // Create connections
        if (m_nodes.size() >= 2)
        {
            if (addProcessor && m_nodes.size() >= 3)
            {
                // Source -> Processor
                ConnectionConfig conn1;
                conn1.sourceName = "asio_input";
                conn1.sourcePad = 0;
                conn1.sinkName = "main_processor";
                conn1.sinkPad = 0;
                conn1.formatConversion = true;
                conn1.bufferPolicy = "auto";
                m_connections.push_back(conn1);

                // Processor -> Sink
                ConnectionConfig conn2;
                conn2.sourceName = "main_processor";
                conn2.sourcePad = 0;
                conn2.sinkName = "asio_output";
                conn2.sinkPad = 0;
                conn2.formatConversion = true;
                conn2.bufferPolicy = "auto";
                m_connections.push_back(conn2);
            }
            else
            {
                // Direct Source -> Sink
                ConnectionConfig conn;
                conn.sourceName = m_nodes[0].name;
                conn.sourcePad = 0;
                conn.sinkName = m_nodes[m_nodes.size() - 1].name;
                conn.sinkPad = 0;
                conn.formatConversion = true;
                conn.bufferPolicy = "auto";
                m_connections.push_back(conn);
            }
        }

        return true;
    }

    std::string Configuration::toJsonString() const
    {
        nlohmann::json j;

        // Basic configuration
        j["deviceType"] = deviceTypeToString(m_deviceType);
        j["targetIp"] = m_targetIp;
        j["targetPort"] = m_targetPort;
        j["receivePort"] = m_receivePort;
        j["asioDeviceName"] = m_asioDeviceName;
        j["sampleRate"] = m_sampleRate;
        j["bufferSize"] = m_bufferSize;
        j["useAsioAutoConfig"] = m_useAsioAutoConfig;
        j["internalFormat"] = m_internalFormat;
        j["internalLayout"] = m_internalLayout;

        // OSC commands
        nlohmann::json cmds = nlohmann::json::array();
        for (const auto &cmd : m_commands)
        {
            nlohmann::json command;
            command["address"] = cmd.address;

            // Convert args to JSON-compatible types
            nlohmann::json args = nlohmann::json::array();
            for (const auto &arg : cmd.args)
            {
                if (arg.type() == typeid(float))
                {
                    args.push_back(std::any_cast<float>(arg));
                }
                else if (arg.type() == typeid(int))
                {
                    args.push_back(std::any_cast<int>(arg));
                }
                else if (arg.type() == typeid(bool))
                {
                    args.push_back(std::any_cast<bool>(arg));
                }
                else if (arg.type() == typeid(std::string))
                {
                    args.push_back(std::any_cast<std::string>(arg));
                }
            }
            command["args"] = args;
            cmds.push_back(command);
        }
        j["commands"] = cmds;

        // Nodes
        nlohmann::json nodes = nlohmann::json::array();
        for (const auto &node : m_nodes)
        {
            nlohmann::json n;
            n["name"] = node.name;
            n["type"] = node.type;
            n["inputPads"] = node.inputPads;
            n["outputPads"] = node.outputPads;
            n["description"] = node.description;

            // Node-specific fields
            if (!node.filterGraph.empty())
            {
                n["filterGraph"] = node.filterGraph;
            }
            if (!node.filePath.empty())
            {
                n["filePath"] = node.filePath;
            }
            if (!node.fileFormat.empty())
            {
                n["fileFormat"] = node.fileFormat;
            }

            // Channel indices for ASIO nodes
            if (!node.channelIndices.empty())
            {
                nlohmann::json indices = nlohmann::json::array();
                for (auto idx : node.channelIndices)
                {
                    indices.push_back(idx);
                }
                n["channelIndices"] = indices;
            }

            // Parameters map
            nlohmann::json params;
            for (const auto &[key, value] : node.params)
            {
                params[key] = value;
            }
            n["params"] = params;

            nodes.push_back(n);
        }
        j["nodes"] = nodes;

        // Connections
        nlohmann::json connections = nlohmann::json::array();
        for (const auto &conn : m_connections)
        {
            nlohmann::json c;
            c["sourceName"] = conn.sourceName;
            c["sourcePad"] = conn.sourcePad;
            c["sinkName"] = conn.sinkName;
            c["sinkPad"] = conn.sinkPad;
            c["formatConversion"] = conn.formatConversion;

            if (!conn.bufferPolicy.empty())
            {
                c["bufferPolicy"] = conn.bufferPolicy;
            }

            connections.push_back(c);
        }
        j["connections"] = connections;

        return j.dump(2); // Pretty print with 2-space indent
    }

    Configuration Configuration::fromJsonString(const std::string &jsonStr)
    {
        Configuration config;
        try
        {
            json j = json::parse(jsonStr);

            // Parse basic configuration
            if (j.contains("deviceType"))
                config.setDeviceType(stringToDeviceType(j["deviceType"].get<std::string>()));

            if (j.contains("targetIp") && j.contains("targetPort"))
            {
                int receivePort = j.contains("receivePort") ? j["receivePort"].get<int>() : 0;
                config.setConnectionParams(
                    j["targetIp"].get<std::string>(),
                    j["targetPort"].get<int>(),
                    receivePort);
            }

            if (j.contains("asioDeviceName"))
                config.setAsioDeviceName(j["asioDeviceName"].get<std::string>());

            if (j.contains("sampleRate"))
                config.setSampleRate(j["sampleRate"].get<double>());

            if (j.contains("bufferSize"))
                config.setBufferSize(j["bufferSize"].get<long>());

            if (j.contains("useAsioAutoConfig"))
                config.setUseAsioAutoConfig(j["useAsioAutoConfig"].get<bool>());

            if (j.contains("internalFormat"))
                config.setInternalFormat(j["internalFormat"].get<std::string>());

            if (j.contains("internalLayout"))
                config.setInternalLayout(j["internalLayout"].get<std::string>());

            // Parse OSC commands
            if (j.contains("commands") && j["commands"].is_array())
            {
                for (const auto &cmdJson : j["commands"])
                {
                    if (cmdJson.contains("address") && cmdJson.contains("args"))
                    {
                        std::string address = cmdJson["address"].get<std::string>();
                        std::vector<std::any> args;

                        for (const auto &arg : cmdJson["args"])
                        {
                            if (arg.is_number_float())
                                args.push_back(std::any(arg.get<float>()));
                            else if (arg.is_number_integer())
                                args.push_back(std::any(arg.get<int>()));
                            else if (arg.is_boolean())
                                args.push_back(std::any(arg.get<bool>()));
                            else if (arg.is_string())
                                args.push_back(std::any(arg.get<std::string>()));
                        }

                        config.addCommand(address, args);
                    }
                }
            }

            // Parse nodes
            if (j.contains("nodes") && j["nodes"].is_array())
            {
                for (const auto &nodeJson : j["nodes"])
                {
                    NodeConfig node;

                    if (nodeJson.contains("name"))
                        node.name = nodeJson["name"].get<std::string>();

                    if (nodeJson.contains("type"))
                        node.type = nodeJson["type"].get<std::string>();

                    if (nodeJson.contains("inputPads"))
                        node.inputPads = nodeJson["inputPads"].get<int>();

                    if (nodeJson.contains("outputPads"))
                        node.outputPads = nodeJson["outputPads"].get<int>();

                    if (nodeJson.contains("description"))
                        node.description = nodeJson["description"].get<std::string>();

                    if (nodeJson.contains("filterGraph"))
                        node.filterGraph = nodeJson["filterGraph"].get<std::string>();

                    if (nodeJson.contains("filePath"))
                        node.filePath = nodeJson["filePath"].get<std::string>();

                    if (nodeJson.contains("fileFormat"))
                        node.fileFormat = nodeJson["fileFormat"].get<std::string>();

                    // Parse channel indices
                    if (nodeJson.contains("channelIndices") && nodeJson["channelIndices"].is_array())
                    {
                        for (const auto &idx : nodeJson["channelIndices"])
                        {
                            node.channelIndices.push_back(idx.get<long>());
                        }
                    }

                    // Parse parameters
                    if (nodeJson.contains("params") && nodeJson["params"].is_object())
                    {
                        for (auto it = nodeJson["params"].begin(); it != nodeJson["params"].end(); ++it)
                        {
                            node.params[it.key()] = it.value().is_string() ? it.value().get<std::string>() : it.value().dump();
                        }
                    }

                    config.addNodeConfig(node);
                }
            }

            // Parse connections
            if (j.contains("connections") && j["connections"].is_array())
            {
                for (const auto &connJson : j["connections"])
                {
                    ConnectionConfig conn;

                    if (connJson.contains("sourceName"))
                        conn.sourceName = connJson["sourceName"].get<std::string>();

                    if (connJson.contains("sourcePad"))
                        conn.sourcePad = connJson["sourcePad"].get<int>();

                    if (connJson.contains("sinkName"))
                        conn.sinkName = connJson["sinkName"].get<std::string>();

                    if (connJson.contains("sinkPad"))
                        conn.sinkPad = connJson["sinkPad"].get<int>();

                    if (connJson.contains("formatConversion"))
                        conn.formatConversion = connJson["formatConversion"].get<bool>();

                    if (connJson.contains("bufferPolicy"))
                        conn.bufferPolicy = connJson["bufferPolicy"].get<std::string>();

                    config.addConnectionConfig(conn);
                }
            }
        }
        catch (const json::exception &e)
        {
            std::cerr << "JSON parsing error: " << e.what() << std::endl;
        }

        return config;
    }

    bool Configuration::saveToFile(const std::string &filePath) const
    {
        try
        {
            std::ofstream file(filePath);
            if (!file.is_open())
            {
                std::cerr << "Failed to open file for writing: " << filePath << std::endl;
                return false;
            }

            file << toJsonString();
            file.close();

            std::cout << "Configuration saved to: " << filePath << std::endl;
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to save configuration: " << e.what() << std::endl;
            return false;
        }
    }

    Configuration Configuration::loadFromFile(const std::string &filePath, bool &success)
    {
        success = false;
        Configuration config;

        try
        {
            std::ifstream file(filePath);
            if (!file.is_open())
            {
                std::cerr << "Failed to open configuration file: " << filePath << std::endl;
                return config;
            }

            std::stringstream buffer;
            buffer << file.rdbuf();
            file.close();

            config = fromJsonString(buffer.str());
            success = true;

            std::cout << "Configuration loaded from: " << filePath << std::endl;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to load configuration: " << e.what() << std::endl;
        }

        return config;
    }

    // Implementation of ConfigurationParser class methods
    bool ConfigurationParser::parse(int argc, char *argv[], Configuration &config)
    {
        // Parse command line arguments
        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];

            if (arg == "--config" && i + 1 < argc)
            {
                // Load configuration from file
                std::string configFile = argv[++i];
                if (!parseFromFile(configFile, config))
                {
                    return false;
                }
            }
            else if (arg == "--asio-device" && i + 1 < argc)
            {
                config.asioDeviceName = argv[++i];
            }
            else if (arg == "--rme-ip" && i + 1 < argc)
            {
                config.rmeOscIp = argv[++i];
            }
            else if (arg == "--rme-port" && i + 1 < argc)
            {
                try
                {
                    config.rmeOscPort = std::stoi(argv[++i]);
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Invalid RME OSC port: " << argv[i] << std::endl;
                    return false;
                }
            }
            else if (arg == "--sample-rate" && i + 1 < argc)
            {
                try
                {
                    config.sampleRate = std::stod(argv[++i]);
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Invalid sample rate: " << argv[i] << std::endl;
                    return false;
                }
            }
            else if (arg == "--buffer-size" && i + 1 < argc)
            {
                try
                {
                    config.bufferSize = std::stol(argv[++i]);
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Invalid buffer size: " << argv[i] << std::endl;
                    return false;
                }
            }
            // Add more command line arguments as needed
        }

        return true;
    }

    bool ConfigurationParser::parseFromFile(const std::string &filePath, Configuration &config)
    {
        try
        {
            std::ifstream file(filePath);
            if (!file.is_open())
            {
                std::cerr << "Failed to open configuration file: " << filePath << std::endl;
                return false;
            }

            std::stringstream buffer;
            buffer << file.rdbuf();
            file.close();

            return parseJson(buffer.str(), config);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to parse configuration file: " << e.what() << std::endl;
            return false;
        }
    }

    bool ConfigurationParser::parseJson(const std::string &content, Configuration &config)
    {
        try
        {
            config = Configuration::fromJsonString(content);
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to parse JSON configuration: " << e.what() << std::endl;
            return false;
        }
    }

    bool ConfigurationParser::autoConfigureAsio(Configuration &config, AsioManager *asioManager)
    {
        if (!asioManager || !asioManager->isInitialized())
        {
            return false;
        }

        // Get optimal device settings
        long bufferSize;
        double sampleRate;
        if (!asioManager->getDefaultDeviceConfiguration(bufferSize, sampleRate))
        {
            return false;
        }

        // Update configuration with optimal settings
        config.setBufferSize(bufferSize);
        config.setSampleRate(sampleRate);

        // Get available channels for auto-configuration
        long inputChannels = asioManager->getInputChannelCount();
        long outputChannels = asioManager->getOutputChannelCount();

        // Create default node configurations if none exist
        if (config.getNodes().empty())
        {
            // Create source node if we have inputs
            if (inputChannels > 0)
            {
                NodeConfig sourceNode;
                sourceNode.name = "asio_input";
                sourceNode.type = "asio_source";
                sourceNode.inputPads = 0; // Source has no inputs
                sourceNode.outputPads = 1;
                sourceNode.description = "ASIO Hardware Input";

                // Use first 2 channels by default (or whatever is available)
                std::string channelIndices;
                for (long i = 0; i < std::min(2L, inputChannels); i++)
                {
                    if (i > 0)
                        channelIndices += ",";
                    channelIndices += std::to_string(i);
                    sourceNode.channelIndices.push_back(i);
                }
                sourceNode.params["channels"] = channelIndices;

                config.addNodeConfig(sourceNode);
            }

            // Create processor node if we have both inputs and outputs
            if (inputChannels > 0 && outputChannels > 0)
            {
                NodeConfig procNode;
                procNode.name = "main_processor";
                procNode.type = "ffmpeg_processor";
                procNode.inputPads = 1;
                procNode.outputPads = 1;
                procNode.description = "Main Processor";
                procNode.filterGraph = "volume=0dB"; // Identity filter by default
                procNode.params["filterGraph"] = "volume=0dB";

                config.addNodeConfig(procNode);
            }

            // Create sink node if we have outputs
            if (outputChannels > 0)
            {
                NodeConfig sinkNode;
                sinkNode.name = "asio_output";
                sinkNode.type = "asio_sink";
                sinkNode.inputPads = 1;
                sinkNode.outputPads = 0; // Sink has no outputs
                sinkNode.description = "ASIO Hardware Output";

                // Use first 2 channels by default (or whatever is available)
                std::string channelIndices;
                for (long i = 0; i < std::min(2L, outputChannels); i++)
                {
                    if (i > 0)
                        channelIndices += ",";
                    channelIndices += std::to_string(i);
                    sinkNode.channelIndices.push_back(i);
                }
                sinkNode.params["channels"] = channelIndices;

                config.addNodeConfig(sinkNode);
            }

            // Create connections if we have created nodes
            const auto &nodes = config.getNodes();
            if (nodes.size() >= 2)
            {
                // Simple case: connect source -> sink
                if (nodes.size() == 2)
                {
                    ConnectionConfig conn;
                    conn.sourceName = nodes[0].name;
                    conn.sourcePad = 0;
                    conn.sinkName = nodes[1].name;
                    conn.sinkPad = 0;
                    conn.formatConversion = true;
                    conn.bufferPolicy = "auto";
                    config.addConnectionConfig(conn);
                }
                // More complex case: connect source -> processor -> sink
                else if (nodes.size() >= 3)
                {
                    // Source to processor
                    ConnectionConfig conn1;
                    conn1.sourceName = "asio_input";
                    conn1.sourcePad = 0;
                    conn1.sinkName = "main_processor";
                    conn1.sinkPad = 0;
                    conn1.formatConversion = true;
                    conn1.bufferPolicy = "auto";
                    config.addConnectionConfig(conn1);

                    // Processor to sink
                    ConnectionConfig conn2;
                    conn2.sourceName = "main_processor";
                    conn2.sourcePad = 0;
                    conn2.sinkName = "asio_output";
                    conn2.sinkPad = 0;
                    conn2.formatConversion = true;
                    conn2.bufferPolicy = "auto";
                    config.addConnectionConfig(conn2);
                }
            }
        }

        return true;
    }

    // Implementation of DeviceStateManager class methods
    void DeviceStateManager::queryDeviceState(OscController *controller, DeviceStateCallback callback)
    {
        if (!controller)
        {
            callback(false, "OscController is null");
            return;
        }

        // Launch asynchronous task to query device state
        std::thread([controller, callback]()
                    {
            try
            {
                // Request a refresh to ensure we get current state
                controller->requestRefresh();

                // Allow time for the device to process the refresh
                std::this_thread::sleep_for(std::chrono::milliseconds(300));

                // Prepare the list of parameters to query
                std::vector<std::string> paramAddresses;
                const int maxChannels = 16; // Adjust based on device capabilities

                // Add inputs, playbacks, and outputs volume/mute parameters
                for (int chType = 0; chType < 3; chType++) {
                    std::string typePrefix;
                    switch (chType) {
                        case 0: typePrefix = "/input"; break;
                        case 1: typePrefix = "/playback"; break;
                        case 2: typePrefix = "/output"; break;
                    }

                    for (int ch = 1; ch <= maxChannels; ch++) {
                        paramAddresses.push_back("/" + std::to_string(ch) + typePrefix + "/volume");
                        paramAddresses.push_back("/" + std::to_string(ch) + typePrefix + "/mute");
                    }
                }

                // Add matrix routing parameters
                for (int output = 1; output <= maxChannels; output++) {
                    for (int input = 1; input <= maxChannels; input++) {
                        paramAddresses.push_back("/matrix/volA/" + std::to_string(input) + "/" + std::to_string(output));
                    }
                }

                // Add global parameters
                paramAddresses.push_back("/main/volume");
                paramAddresses.push_back("/main/mute");

                // Create a Configuration object to hold the results
                Configuration config;
                config.asioDeviceName = "RME Device"; // Placeholder
                config.rmeOscIp = controller->getTargetIp();
                config.rmeOscPort = controller->getTargetPort();

                // Query all parameters
                int totalQueries = paramAddresses.size();
                int completedQueries = 0;
                int successfulQueries = 0;

                for (const auto& address : paramAddresses) {
                    float value = 0.0f;
                    bool success = controller->querySingleValue(address, value);

                    if (success) {
                        successfulQueries++;

                        // Create command config
                        RmeOscCommandConfig cmd;
                        cmd.address = address;
                        cmd.args.push_back(std::any(value));
                        config.rmeCommands.push_back(cmd);
                    }

                    completedQueries++;
                    // Add small delay between queries to avoid overwhelming the device
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }

                bool overallSuccess = (successfulQueries > 0);
                std::string resultMessage = "Device state query completed. " +
                                            std::to_string(successfulQueries) + "/" +
                                            std::to_string(totalQueries) + " parameters retrieved successfully.";

                callback(overallSuccess, resultMessage);
            }
            catch (const std::exception& e)
            {
                callback(false, std::string("Failed to query device state: ") + e.what());
            } })
            .detach();
    }

    Configuration DeviceStateManager::deviceStateToConfig(
        const std::string &deviceName,
        const std::string &oscIp,
        int oscPort)
    {
        Configuration config;

        // Set the basic connection parameters
        config.asioDeviceName = deviceName;
        config.rmeOscIp = oscIp;
        config.rmeOscPort = oscPort;

        // TODO: Populate with actual queried state from the device
        // This would require enhancements to OscController to query all parameters

        return config;
    }

    bool DeviceStateManager::saveDeviceStateToFile(
        OscController *controller,
        const std::string &filePath,
        DeviceStateCallback callback)
    {
        if (!controller)
        {
            callback(false, "OscController is null");
            return false;
        }

        // Launch asynchronous task to save device state
        std::thread([controller, filePath, callback]()
                    {
            try
            {
                // Request a refresh to ensure we get current state
                controller->requestRefresh();

                // Allow time for the device to process the refresh
                std::this_thread::sleep_for(std::chrono::milliseconds(300));

                // Prepare the list of parameters to query
                std::vector<std::string> paramAddresses;
                const int maxChannels = 16; // Adjust based on device capabilities

                // Add inputs, playbacks, and outputs volume/mute parameters
                for (int chType = 0; chType < 3; chType++) {
                    std::string typePrefix;
                    switch (chType) {
                        case 0: typePrefix = "/input"; break;
                        case 1: typePrefix = "/playback"; break;
                        case 2: typePrefix = "/output"; break;
                    }

                    for (int ch = 1; ch <= maxChannels; ch++) {
                        paramAddresses.push_back("/" + std::to_string(ch) + typePrefix + "/volume");
                        paramAddresses.push_back("/" + std::to_string(ch) + typePrefix + "/mute");
                    }
                }

                // Add matrix routing parameters
                for (int output = 1; output <= maxChannels; output++) {
                    for (int input = 1; input <= maxChannels; input++) {
                        paramAddresses.push_back("/matrix/volA/" + std::to_string(input) + "/" + std::to_string(output));
                    }
                }

                // Add global parameters
                paramAddresses.push_back("/main/volume");
                paramAddresses.push_back("/main/mute");

                // Create a Configuration object to hold the results
                Configuration config;
                config.asioDeviceName = "RME Device"; // Placeholder - would ideally query this
                config.rmeOscIp = controller->getTargetIp();
                config.rmeOscPort = controller->getTargetPort();

                // Query all parameters
                int totalQueries = paramAddresses.size();
                int completedQueries = 0;
                int successfulQueries = 0;
                std::vector<RmeOscCommandConfig> commands;

                for (const auto& address : paramAddresses) {
                    float value = 0.0f;
                    bool success = controller->querySingleValue(address, value);

                    if (success) {
                        successfulQueries++;

                        // Create command config
                        RmeOscCommandConfig cmd;
                        cmd.address = address;
                        cmd.args.push_back(std::any(value));
                        commands.push_back(cmd);
                    }

                    completedQueries++;
                    // Add small delay between queries to avoid overwhelming the device
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }

                // Store the commands in the configuration
                config.rmeCommands = commands;

                // Save to file
                bool saveSuccess = config.saveToFile(filePath);

                if (saveSuccess)
                {
                    std::string resultMessage = "Device state saved to " + filePath + ". " +
                                                std::to_string(successfulQueries) + "/" +
                                                std::to_string(totalQueries) + " parameters retrieved.";
                    callback(true, resultMessage);
                }
                else
                {
                    callback(false, "Failed to save device state to file");
                }
            }
            catch (const std::exception& e)
            {
                callback(false, std::string("Failed to save device state: ") + e.what());
            } })
            .detach();

        return true; // Return true to indicate the async task was started
    }

    Configuration::Configuration()
        : m_targetIp("127.0.0.1"),
          m_targetPort(9000),
          m_receivePort(8000)
    {
    }

    Configuration::~Configuration()
    {
        // Nothing to clean up
    }

    bool Configuration::loadFromJson(const std::string &filepath)
    {
        try
        {
            std::ifstream file(filepath);
            if (!file.is_open())
            {
                std::cerr << "Failed to open configuration file: " << filepath << std::endl;
                return false;
            }

            json config;
            file >> config;
            file.close();

            // Clear existing commands
            m_commands.clear();

            // Load connection parameters
            if (config.contains("osc"))
            {
                const auto &osc = config["osc"];
                if (osc.contains("ip"))
                    m_targetIp = osc["ip"].get<std::string>();
                if (osc.contains("port"))
                    m_targetPort = osc["port"].get<int>();
                if (osc.contains("receive_port"))
                    m_receivePort = osc["receive_port"].get<int>();
            }

            // Process structured configuration sections

            // Process global settings
            if (config.contains("global"))
            {
                const auto &global = config["global"];
                for (auto it = global.begin(); it != global.end(); ++it)
                {
                    std::string paramName = it.key();
                    std::string address = "/main/" + paramName;

                    if (it.value().is_boolean())
                    {
                        bool value = it.value().get<bool>();
                        addOscCommand(address, value);
                    }
                    else if (it.value().is_number())
                    {
                        float value = it.value().get<float>();
                        addOscCommand(address, value);
                    }
                }
            }

            // Process inputs section
            if (config.contains("inputs"))
            {
                const auto &inputs = config["inputs"];
                for (auto channelIt = inputs.begin(); channelIt != inputs.end(); ++channelIt)
                {
                    int channelNum = std::stoi(channelIt.key());
                    const auto &channelParams = channelIt.value();

                    for (auto paramIt = channelParams.begin(); paramIt != channelParams.end(); ++paramIt)
                    {
                        std::string paramName = paramIt.key();
                        std::string address = "/" + std::to_string(channelNum) + "/input/" + paramName;

                        if (paramName == "volume")
                        {
                            // Convert from dB to normalized value
                            float valueDb = paramIt.value().get<float>();
                            float normalized = (valueDb + 65.0f) / 65.0f;
                            addOscCommand(address, normalized);
                        }
                        else if (paramName == "mute" || paramName == "phase" || paramName == "solo")
                        {
                            bool value = paramIt.value().get<bool>();
                            addOscCommand(address, value);
                        }
                        else
                        {
                            float value = paramIt.value().get<float>();
                            addOscCommand(address, value);
                        }
                    }
                }
            }

            // Process playbacks section
            if (config.contains("playbacks"))
            {
                const auto &playbacks = config["playbacks"];
                for (auto channelIt = playbacks.begin(); channelIt != playbacks.end(); ++channelIt)
                {
                    int channelNum = std::stoi(channelIt.key());
                    const auto &channelParams = channelIt.value();

                    for (auto paramIt = channelParams.begin(); paramIt != channelParams.end(); ++paramIt)
                    {
                        std::string paramName = paramIt.key();
                        std::string address = "/" + std::to_string(channelNum) + "/playback/" + paramName;

                        if (paramName == "volume")
                        {
                            // Convert from dB to normalized value
                            float valueDb = paramIt.value().get<float>();
                            float normalized = (valueDb + 65.0f) / 65.0f;
                            addOscCommand(address, normalized);
                        }
                        else if (paramName == "mute" || paramName == "phase" || paramName == "solo")
                        {
                            bool value = paramIt.value().get<bool>();
                            addOscCommand(address, value);
                        }
                        else
                        {
                            float value = paramIt.value().get<float>();
                            addOscCommand(address, value);
                        }
                    }
                }
            }

            // Process outputs section
            if (config.contains("outputs"))
            {
                const auto &outputs = config["outputs"];
                for (auto channelIt = outputs.begin(); channelIt != outputs.end(); ++channelIt)
                {
                    int channelNum = std::stoi(channelIt.key());
                    const auto &channelParams = channelIt.value();

                    for (auto paramIt = channelParams.begin(); paramIt != channelParams.end(); ++paramIt)
                    {
                        std::string paramName = paramIt.key();
                        std::string address = "/" + std::to_string(channelNum) + "/output/" + paramName;

                        if (paramName == "volume")
                        {
                            // Convert from dB to normalized value
                            float valueDb = paramIt.value().get<float>();
                            float normalized = (valueDb + 65.0f) / 65.0f;
                            addOscCommand(address, normalized);
                        }
                        else if (paramName == "mute" || paramName == "phase")
                        {
                            bool value = paramIt.value().get<bool>();
                            addOscCommand(address, value);
                        }
                        else
                        {
                            float value = paramIt.value().get<float>();
                            addOscCommand(address, value);
                        }
                    }
                }
            }

            // Process matrix section
            if (config.contains("matrix"))
            {
                const auto &matrix = config["matrix"];
                for (auto it = matrix.begin(); it != matrix.end(); ++it)
                {
                    std::string key = it.key();
                    float valueDb = it.value().get<float>();

                    // Parse the input_output key
                    size_t pos = key.find('_');
                    if (pos != std::string::npos)
                    {
                        int input = std::stoi(key.substr(0, pos));
                        int output = std::stoi(key.substr(pos + 1));

                        // Convert from dB to normalized value
                        float normalized = (valueDb + 65.0f) / 65.0f;

                        std::string address = "/matrix/volA/" + std::to_string(input) + "/" + std::to_string(output);
                        addOscCommand(address, normalized);
                    }
                }
            }

            // Process raw commands array (for backward compatibility or complex commands)
            if (config.contains("commands") && config["commands"].is_array())
            {
                const auto &cmds = config["commands"];
                for (const auto &cmd : cmds)
                {
                    if (cmd.contains("address") && cmd.contains("args"))
                    {
                        std::string address = cmd["address"].get<std::string>();
                        const auto &jsonArgs = cmd["args"];

                        std::vector<std::any> args;
                        for (const auto &arg : jsonArgs)
                        {
                            if (arg.is_number_float())
                                args.push_back(arg.get<float>());
                            else if (arg.is_number_integer())
                                args.push_back(arg.get<int>());
                            else if (arg.is_boolean())
                                args.push_back(arg.get<bool>());
                            else if (arg.is_string())
                                args.push_back(arg.get<std::string>());
                            // Add more types as needed
                        }

                        m_commands.push_back({address, args});
                    }
                }
            }
            // For backward compatibility with the old format
            else if (config.contains("commands") && config["commands"].is_object())
            {
                const auto &cmds = config["commands"];
                for (auto it = cmds.begin(); it != cmds.end(); ++it)
                {
                    std::string address = it.key();
                    const auto &jsonArgs = it.value();

                    if (jsonArgs.is_array())
                    {
                        std::vector<std::any> args;
                        for (const auto &arg : jsonArgs)
                        {
                            if (arg.is_number_float())
                                args.push_back(arg.get<float>());
                            else if (arg.is_number_integer())
                                args.push_back(arg.get<int>());
                            else if (arg.is_boolean())
                                args.push_back(arg.get<bool>());
                            else if (arg.is_string())
                                args.push_back(arg.get<std::string>());
                            // Add more types as needed
                        }

                        m_commands.push_back({address, args});
                    }
                    else if (jsonArgs.is_number_float())
                    {
                        float value = jsonArgs.get<float>();
                        m_commands.push_back({address, {value}});
                    }
                    else if (jsonArgs.is_number_integer())
                    {
                        int value = jsonArgs.get<int>();
                        m_commands.push_back({address, {value}});
                    }
                    else if (jsonArgs.is_boolean())
                    {
                        bool value = jsonArgs.get<bool>();
                        m_commands.push_back({address, {value}});
                    }
                    else if (jsonArgs.is_string())
                    {
                        std::string value = jsonArgs.get<std::string>();
                        m_commands.push_back({address, {value}});
                    }
                }
            }

            std::cout << "Configuration loaded from: " << filepath << std::endl;
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error loading configuration: " << e.what() << std::endl;
            return false;
        }
    }

    bool Configuration::saveToJson(const std::string &filepath) const
    {
        try
        {
            // Create the root JSON object
            json config;

            // Add OSC connection parameters
            config["osc"] = {
                {"ip", m_targetIp},
                {"port", m_targetPort},
                {"receive_port", m_receivePort}};

            // Create containers for different sections
            json inputs = json::object();
            json playbacks = json::object();
            json outputs = json::object();
            json matrix = json::object();
            json global = json::object();
            json rawCommands = json::array();

            // Sort commands into appropriate sections
            for (const auto &cmd : m_commands)
            {
                const std::string &address = cmd.first;
                const std::vector<std::any> &args = cmd.second;

                if (args.empty())
                    continue;

                // Parse address to determine the section
                if (address.find("/matrix/volA/") == 0)
                {
                    // Matrix routing format: /matrix/volA/[input]/[output]
                    std::stringstream ss(address.substr(13)); // Skip "/matrix/volA/"
                    int input, output;
                    char delimiter;
                    if (ss >> input >> delimiter >> output)
                    {
                        float value = 0.0f;
                        if (args[0].type() == typeid(float))
                            value = std::any_cast<float>(args[0]);
                        else if (args[0].type() == typeid(int))
                            value = static_cast<float>(std::any_cast<int>(args[0]));

                        // Convert normalized value to dB
                        float dB = (value * 65.0f) - 65.0f;

                        // Use format "input_output" as key
                        std::string key = std::to_string(input) + "_" + std::to_string(output);
                        matrix[key] = dB;
                    }
                }
                else if (address[0] == '/' && std::isdigit(address[1]))
                {
                    // Channel specific parameter format: /[channel]/[type]/[param]
                    std::string addrCopy = address.substr(1); // Remove leading slash
                    size_t firstSlash = addrCopy.find('/');
                    if (firstSlash != std::string::npos)
                    {
                        std::string channelStr = addrCopy.substr(0, firstSlash);
                        std::string rest = addrCopy.substr(firstSlash + 1);

                        int channel = std::stoi(channelStr);

                        // Find parameter type (input, playback, output)
                        size_t secondSlash = rest.find('/');
                        if (secondSlash != std::string::npos)
                        {
                            std::string type = rest.substr(0, secondSlash);
                            std::string param = rest.substr(secondSlash + 1);
                            json *targetSection = nullptr;

                            if (type == "input")
                                targetSection = &inputs;
                            else if (type == "playback")
                                targetSection = &playbacks;
                            else if (type == "output")
                                targetSection = &outputs;

                            if (targetSection)
                            {
                                // Create channel if it doesn't exist
                                std::string channelKey = channelStr;
                                if (!targetSection->contains(channelKey))
                                    (*targetSection)[channelKey] = json::object();

                                // Add parameter value
                                if (args[0].type() == typeid(float))
                                {
                                    float value = std::any_cast<float>(args[0]);

                                    // For volume, convert normalized value to dB
                                    if (param == "volume")
                                    {
                                        float dB = (value * 65.0f) - 65.0f;
                                        (*targetSection)[channelKey][param] = dB;
                                    }
                                    else
                                    {
                                        (*targetSection)[channelKey][param] = value;
                                    }
                                }
                                else if (args[0].type() == typeid(bool))
                                {
                                    bool value = std::any_cast<bool>(args[0]);
                                    (*targetSection)[channelKey][param] = value;
                                }
                                else if (args[0].type() == typeid(int))
                                {
                                    int value = std::any_cast<int>(args[0]);
                                    (*targetSection)[channelKey][param] = value;
                                }
                                else if (args[0].type() == typeid(std::string))
                                {
                                    std::string value = std::any_cast<std::string>(args[0]);
                                    (*targetSection)[channelKey][param] = value;
                                }
                            }
                        }
                    }
                }
                else if (address.find("/main/") == 0)
                {
                    // Global parameters format: /main/[param]
                    std::string param = address.substr(6); // Skip "/main/"

                    // Add parameter value
                    if (args[0].type() == typeid(float))
                    {
                        float value = std::any_cast<float>(args[0]);
                        global[param] = value;
                    }
                    else if (args[0].type() == typeid(bool))
                    {
                        bool value = std::any_cast<bool>(args[0]);
                        global[param] = value;
                    }
                    else if (args[0].type() == typeid(int))
                    {
                        int value = std::any_cast<int>(args[0]);
                        global[param] = value;
                    }
                    else if (args[0].type() == typeid(std::string))
                    {
                        std::string value = std::any_cast<std::string>(args[0]);
                        global[param] = value;
                    }
                }
                else
                {
                    // This is an unsorted command, add to raw commands array
                    json jsonCmd;
                    jsonCmd["address"] = address;

                    json jsonArgs = json::array();
                    for (const auto &arg : args)
                    {
                        if (arg.type() == typeid(float))
                            jsonArgs.push_back(std::any_cast<float>(arg));
                        else if (arg.type() == typeid(int))
                            jsonArgs.push_back(std::any_cast<int>(arg));
                        else if (arg.type() == typeid(bool))
                            jsonArgs.push_back(std::any_cast<bool>(arg));
                        else if (arg.type() == typeid(std::string))
                            jsonArgs.push_back(std::any_cast<std::string>(arg));
                        // Add more types as needed
                    }

                    jsonCmd["args"] = jsonArgs;
                    rawCommands.push_back(jsonCmd);
                }
            }

            // Add non-empty sections to the config
            if (!inputs.empty())
                config["inputs"] = inputs;
            if (!playbacks.empty())
                config["playbacks"] = playbacks;
            if (!outputs.empty())
                config["outputs"] = outputs;
            if (!matrix.empty())
                config["matrix"] = matrix;
            if (!global.empty())
                config["global"] = global;
            if (!rawCommands.empty())
                config["commands"] = rawCommands;

            // Write the configuration to file
            std::ofstream file(filepath);
            if (!file.is_open())
            {
                std::cerr << "Failed to open file for writing: " << filepath << std::endl;
                return false;
            }

            file << config.dump(4); // Pretty print with 4-space indentation
            file.close();

            std::cout << "Configuration saved to: " << filepath << std::endl;
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error saving configuration: " << e.what() << std::endl;
            return false;
        }
    }

    // Helper method to process command arguments for JSON serialization
    json Configuration::processCommandArgs(const std::vector<std::any> &args) const
    {
        json jsonArgs = json::array();

        for (const auto &arg : args)
        {
            if (arg.type() == typeid(float))
            {
                jsonArgs.push_back(std::any_cast<float>(arg));
            }
            else if (arg.type() == typeid(int))
            {
                jsonArgs.push_back(std::any_cast<int>(arg));
            }
            else if (arg.type() == typeid(bool))
            {
                jsonArgs.push_back(std::any_cast<bool>(arg));
            }
            else if (arg.type() == typeid(std::string))
            {
                jsonArgs.push_back(std::any_cast<std::string>(arg));
            }
            // Add more types as needed
        }

        return jsonArgs;
    }

    void Configuration::setConnectionParams(const std::string &ip, int sendPort, int receivePort)
    {
        m_targetIp = ip;
        m_targetPort = sendPort;
        if (receivePort > 0)
            m_receivePort = receivePort;
    }

    void Configuration::setMatrixCrosspointGain(int input, int output, float gainDb)
    {
        // Create OSC address for the matrix crosspoint
        std::string address = "/matrix/volA/" + std::to_string(input) + "/" + std::to_string(output);

        // Convert gain in dB to normalized value (assuming RME's scaling)
        // This is a simplification - actual conversion should match OscController::dbToNormalized
        float normalized = (gainDb + 65.0f) / 65.0f; // Simple linear conversion example
        if (normalized < 0.0f)
            normalized = 0.0f;
        if (normalized > 1.0f)
            normalized = 1.0f;

        // Add the command
        std::vector<std::any> args = {std::any(normalized)};
        addCommand(address, args);
    }

    void Configuration::setChannelMute(int channelType, int channel, bool mute)
    {
        // Determine channel type string
        std::string typeStr;
        switch (channelType)
        {
        case 0:
            typeStr = "input";
            break;
        case 1:
            typeStr = "playback";
            break;
        case 2:
            typeStr = "output";
            break;
        default:
            return; // Invalid channel type
        }

        // Create OSC address for the channel mute
        std::string address = "/" + std::to_string(channel) + "/" + typeStr + "/mute";

        // Add the command
        std::vector<std::any> args = {std::any(mute ? 1.0f : 0.0f)};
        addCommand(address, args);
    }

    void Configuration::setChannelVolume(int channelType, int channel, float volumeDb)
    {
        // Determine channel type string
        std::string typeStr;
        switch (channelType)
        {
        case 0:
            typeStr = "input";
            break;
        case 1:
            typeStr = "playback";
            break;
        case 2:
            typeStr = "output";
            break;
        default:
            return; // Invalid channel type
        }

        // Create OSC address for the channel volume
        std::string address = "/" + std::to_string(channel) + "/" + typeStr + "/volume";

        // Convert volume in dB to normalized value (assuming RME's scaling)
        // This is a simplification - actual conversion should match OscController::dbToNormalized
        float normalized = (volumeDb + 65.0f) / 65.0f; // Simple linear conversion example
        if (normalized < 0.0f)
            normalized = 0.0f;
        if (normalized > 1.0f)
            normalized = 1.0f;

        // Add the command
        std::vector<std::any> args = {std::any(normalized)};
        addCommand(address, args);
    }

    void Configuration::addCommand(const std::string &address, const std::vector<std::any> &args)
    {
        OscCommandConfig cmd;
        cmd.address = address;
        cmd.args = args;
        m_commands.push_back(cmd);
    }

    void Configuration::clearCommands()
    {
        m_commands.clear();
    }

    // Helper methods for the Configuration class
    void Configuration::setChannelMute(int channel, const std::string &type, bool mute)
    {
        std::string address = "/" + std::to_string(channel) + "/" + type + "/mute";
        std::vector<std::any> args = {std::any(mute ? 1.0f : 0.0f)};
        addCommand(address, args);
    }

    void Configuration::setChannelVolume(int channel, const std::string &type, float normalizedValue)
    {
        std::string address = "/" + std::to_string(channel) + "/" + type + "/volume";
        std::vector<std::any> args = {std::any(normalizedValue)};
        addCommand(address, args);
    }

    void Configuration::setChannelPhase(int channel, const std::string &type, bool phase)
    {
        std::string address = "/" + std::to_string(channel) + "/" + type + "/phase";
        std::vector<std::any> args = {std::any(phase ? 1.0f : 0.0f)};
        addCommand(address, args);
    }

    void Configuration::setChannelSolo(int channel, const std::string &type, bool solo)
    {
        std::string address = "/" + std::to_string(channel) + "/" + type + "/solo";
        std::vector<std::any> args = {std::any(solo ? 1.0f : 0.0f)};
        addCommand(address, args);
    }

    void Configuration::setPhantomPower(int channel, bool enabled)
    {
        std::string address = "/" + std::to_string(channel) + "/input/phantom";
        std::vector<std::any> args = {std::any(enabled ? 1.0f : 0.0f)};
        addCommand(address, args);
    }

    // DeviceStateManager implementation
    DeviceStateManager::DeviceStateManager(OscController *controller)
        : m_controller(controller), m_pendingQueries(0)
    {
    }

    DeviceStateManager::~DeviceStateManager()
    {
        // Nothing to clean up
    }

    void DeviceStateManager::queryDeviceState(std::function<void(const Configuration &)> callback, int channelCount)
    {
        // Create a new configuration to populate
        auto config = std::make_shared<Configuration>();

        // Set connection parameters from controller
        config->setConnectionParams(
            m_controller->getTargetIp(),
            m_controller->getTargetPort(),
            0 // No receive port set in the config when reading
        );

        // Build a list of parameters to query
        std::vector<std::string> queries;

        // Add channel volumes and mutes
        for (int typeIdx = 0; typeIdx < 3; typeIdx++)
        {
            std::string typeStr;
            switch (typeIdx)
            {
            case 0:
                typeStr = "input";
                break;
            case 1:
                typeStr = "playback";
                break;
            case 2:
                typeStr = "output";
                break;
            }

            for (int ch = 1; ch <= channelCount; ch++)
            {
                // Volume query
                queries.push_back("/" + std::to_string(ch) + "/" + typeStr + "/volume");

                // Mute query
                queries.push_back("/" + std::to_string(ch) + "/" + typeStr + "/mute");

                // Phase query (if supported)
                queries.push_back("/" + std::to_string(ch) + "/" + typeStr + "/phase");

                // Solo query (if supported)
                queries.push_back("/" + std::to_string(ch) + "/" + typeStr + "/solo");

                // If it's input, also query phantom power
                if (typeStr == "input")
                {
                    queries.push_back("/" + std::to_string(ch) + "/input/phantom");
                }

                // EQ parameters (if supported)
                if (typeStr != "playback")
                {
                    queries.push_back("/" + std::to_string(ch) + "/" + typeStr + "/eq/on");
                    for (int band = 1; band <= 4; band++)
                    {
                        queries.push_back("/" + std::to_string(ch) + "/" + typeStr + "/eq/" + std::to_string(band) + "/gain");
                        queries.push_back("/" + std::to_string(ch) + "/" + typeStr + "/eq/" + std::to_string(band) + "/freq");
                        queries.push_back("/" + std::to_string(ch) + "/" + typeStr + "/eq/" + std::to_string(band) + "/q");
                    }
                }
            }
        }

        // Query main/master settings
        queries.push_back("/main/dim");
        queries.push_back("/main/volume");
        queries.push_back("/main/mute");
        queries.push_back("/main/mono");

        // Add matrix routing entries for all channels
        // Matrix routing follows the pattern /matrix/volA/output/input
        for (int output = 1; output <= channelCount; output++)
        {
            for (int input = 1; input <= channelCount; input++)
            {
                queries.push_back("/matrix/volA/" + std::to_string(output) + "/" + std::to_string(input));
                queries.push_back("/matrix/muteA/" + std::to_string(output) + "/" + std::to_string(input));
            }
        }

        // Display query information
        std::cout << "Querying " << queries.size() << " parameters from RME device..." << std::endl;

        // Query all parameters
        queryParameters(queries, [callback, config, this, channelCount](const std::map<std::string, float> &results)
                        {
            std::cout << "Received " << results.size() << " parameter values from device" << std::endl;

            // Process results and populate the configuration
            for (const auto& [address, value] : results) {
                // Parse different types of addresses
                if (address.find("/matrix/volA/") == 0 || address.find("/matrix/muteA/") == 0) {
                    // Handle matrix routing - extract output and input channels
                    std::string addrCopy = address;
                    // Skip the initial "/matrix/volA/" or "/matrix/muteA/"
                    size_t typeLength = (address.find("/volA/") != std::string::npos) ? 6 : 7;
                    addrCopy = addrCopy.substr(typeLength);

                    size_t slashPos = addrCopy.find('/');
                    if (slashPos == std::string::npos) continue;

                    int output = std::stoi(addrCopy.substr(0, slashPos));
                    int input = std::stoi(addrCopy.substr(slashPos + 1));

                    // Add matrix command to config
                    std::vector<std::any> args = {std::any(output), std::any(input), std::any(value)};
                    config->addCommand(address, args);
                }
                else if (address.find("/main/") == 0) {
                    // Handle main/global settings - just store as raw commands for now
                    std::vector<std::any> args = {std::any(value)};
                    config->addCommand(address, args);
                }
                else {
                    // Handle channel parameters (format: /channel/type/param)
                    std::string addrCopy = address;
                    if (addrCopy[0] == '/') addrCopy = addrCopy.substr(1);

                    size_t pos1 = addrCopy.find('/');
                    if (pos1 == std::string::npos) continue;

                    std::string channelStr = addrCopy.substr(0, pos1);
                    addrCopy = addrCopy.substr(pos1 + 1);

                    size_t pos2 = addrCopy.find('/');
                    if (pos2 == std::string::npos) continue;

                    std::string typeStr = addrCopy.substr(0, pos2);
                    std::string paramStr = addrCopy.substr(pos2 + 1);

                    // Parse channel number
                    int channel;
                    try {
                        channel = std::stoi(channelStr);
                    }
                    catch (const std::exception& e) {
                        continue; // Skip if channel is not a number
                    }

                    // Create appropriate command based on parameter type
                    if (paramStr == "volume") {
                        config->setChannelVolume(channel, typeStr, value);
                    }
                    else if (paramStr == "mute") {
                        config->setChannelMute(channel, typeStr, value > 0.5f);
                    }
                    else if (paramStr == "phantom") {
                        config->setPhantomPower(channel, value > 0.5f);
                    }
                    else if (paramStr == "phase") {
                        config->setChannelPhase(channel, typeStr, value > 0.5f);
                    }
                    else if (paramStr == "solo") {
                        config->setChannelSolo(channel, typeStr, value > 0.5f);
                    }
                    else if (paramStr.find("eq/") == 0) {
                        // Handle EQ parameters
                        std::vector<std::any> args = {std::any(channel), std::any(value)};
                        config->addCommand(address, args);
                    }
                    else {
                        // Generic parameter handling for other types
                        std::vector<std::any> args = {std::any(value)};
                        config->addCommand(address, args);
                    }
                }
            }

            // Invoke callback with the populated configuration
            callback(*config); });
    }

    void DeviceStateManager::queryParameter(const std::string &address, QueryCallback callback)
    {
        // Store the callback
        m_callbacks[address] = callback;

        // Increment pending queries count
        m_pendingQueries++;

        // Query the parameter
        float value;
        bool success = m_controller->querySingleValue(address, value);

        if (success)
        {
            // Store the result
            m_queryResults[address] = value;

            // Call the callback
            callback(true, value);
        }
        else
        {
            // Call the callback with failure
            callback(false, 0.0f);
        }

        // Decrement pending queries count
        m_pendingQueries--;
    }

    void DeviceStateManager::queryParameters(
        const std::vector<std::string> &addresses,
        std::function<void(const std::map<std::string, float> &)> callback)
    {
        // Clear previous results
        m_queryResults.clear();

        // Set up total queries
        m_pendingQueries = addresses.size();

        // If no queries to perform, call callback immediately
        if (addresses.empty())
        {
            callback(m_queryResults);
            return;
        }

        // Create counter for completed queries
        auto completedQueries = std::make_shared<int>(0);
        auto totalQueries = addresses.size();
        auto results = std::make_shared<std::map<std::string, float>>();

        // Create a callback for individual queries
        auto individualCallback = [callback, completedQueries, totalQueries, results](
                                      const std::string &address, bool success, float value)
        {
            if (success)
            {
                (*results)[address] = value;
            }

            (*completedQueries)++;

            // If all queries have completed, call the batch callback
            if (*completedQueries >= totalQueries)
            {
                callback(*results);
            }
        };

        // Query each parameter
        for (const auto &address : addresses)
        {
            // Capture address in lambda for callback
            queryParameter(address, [individualCallback, address](bool success, float value)
                           { individualCallback(address, success, value); });
        }
    }

} // namespace AudioEngine
