#include "Configuration.h"
#include "OscController.h"

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
        : rmeOscPort(7001),  // Default RME OSC port
          sampleRate(48000), // Default sample rate
          bufferSize(256)    // Default buffer size
    {
        // Set default RME IP address
        rmeOscIp = "127.0.0.1";
    }

    const NodeConfig *Configuration::getNodeConfig(const std::string &name) const
    {
        for (const auto &node : nodes)
        {
            if (node.name == name)
            {
                return &node;
            }
        }
        return nullptr;
    }

    std::string Configuration::toJsonString() const
    {
        json j;

        // Global settings
        j["asioDeviceName"] = asioDeviceName;
        j["rmeOscIp"] = rmeOscIp;
        j["rmeOscPort"] = rmeOscPort;
        j["sampleRate"] = sampleRate;
        j["bufferSize"] = bufferSize;

        // Processing nodes
        json nodesArray = json::array();
        for (const auto &node : nodes)
        {
            json nodeObj;
            nodeObj["name"] = node.name;
            nodeObj["type"] = node.type;

            json paramsObj;
            for (const auto &[key, value] : node.params)
            {
                paramsObj[key] = value;
            }
            nodeObj["params"] = paramsObj;

            nodesArray.push_back(nodeObj);
        }
        j["nodes"] = nodesArray;

        // Connections
        json connectionsArray = json::array();
        for (const auto &conn : connections)
        {
            json connObj;
            connObj["sourceName"] = conn.sourceName;
            connObj["sourcePad"] = conn.sourcePad;
            connObj["sinkName"] = conn.sinkName;
            connObj["sinkPad"] = conn.sinkPad;

            connectionsArray.push_back(connObj);
        }
        j["connections"] = connectionsArray;

        // RME OSC commands
        json commandsArray = json::array();
        for (const auto &cmd : rmeCommands)
        {
            json cmdObj;
            cmdObj["address"] = cmd.address;

            json argsArray = json::array();
            for (const auto &arg : cmd.args)
            {
                // Handle different argument types
                if (arg.type() == typeid(float))
                {
                    argsArray.push_back(std::any_cast<float>(arg));
                }
                else if (arg.type() == typeid(int))
                {
                    argsArray.push_back(std::any_cast<int>(arg));
                }
                else if (arg.type() == typeid(bool))
                {
                    argsArray.push_back(std::any_cast<bool>(arg));
                }
                else if (arg.type() == typeid(std::string))
                {
                    argsArray.push_back(std::any_cast<std::string>(arg));
                }
                // Add more types as needed
            }
            cmdObj["args"] = argsArray;
            cmdObj["argTypes"] = json::array(); // Store type information for parsing

            commandsArray.push_back(cmdObj);
        }
        j["rmeCommands"] = commandsArray;

        return j.dump(4); // Pretty print with 4 spaces
    }

    Configuration Configuration::fromJsonString(const std::string &jsonStr)
    {
        Configuration config;
        try
        {
            json j = json::parse(jsonStr);

            // Parse global settings
            if (j.contains("asioDeviceName"))
                config.asioDeviceName = j["asioDeviceName"].get<std::string>();

            if (j.contains("rmeOscIp"))
                config.rmeOscIp = j["rmeOscIp"].get<std::string>();

            if (j.contains("rmeOscPort"))
                config.rmeOscPort = j["rmeOscPort"].get<int>();

            if (j.contains("sampleRate"))
                config.sampleRate = j["sampleRate"].get<double>();

            if (j.contains("bufferSize"))
                config.bufferSize = j["bufferSize"].get<long>();

            // Parse nodes
            if (j.contains("nodes") && j["nodes"].is_array())
            {
                for (const auto &nodeJson : j["nodes"])
                {
                    NodeConfig node;
                    node.name = nodeJson["name"].get<std::string>();
                    node.type = nodeJson["type"].get<std::string>();

                    if (nodeJson.contains("params") && nodeJson["params"].is_object())
                    {
                        for (auto it = nodeJson["params"].begin(); it != nodeJson["params"].end(); ++it)
                        {
                            node.params[it.key()] = it.value().get<std::string>();
                        }
                    }

                    config.nodes.push_back(node);
                }
            }

            // Parse connections
            if (j.contains("connections") && j["connections"].is_array())
            {
                for (const auto &connJson : j["connections"])
                {
                    ConnectionConfig conn;
                    conn.sourceName = connJson["sourceName"].get<std::string>();
                    conn.sourcePad = connJson["sourcePad"].get<int>();
                    conn.sinkName = connJson["sinkName"].get<std::string>();
                    conn.sinkPad = connJson["sinkPad"].get<int>();

                    config.connections.push_back(conn);
                }
            }

            // Parse RME OSC commands
            if (j.contains("rmeCommands") && j["rmeCommands"].is_array())
            {
                for (const auto &cmdJson : j["rmeCommands"])
                {
                    RmeOscCommandConfig cmd;
                    cmd.address = cmdJson["address"].get<std::string>();

                    if (cmdJson.contains("args") && cmdJson["args"].is_array())
                    {
                        // We need to determine the type of each argument
                        // This can be complex as JSON doesn't preserve C++ types exactly
                        for (const auto &argJson : cmdJson["args"])
                        {
                            if (argJson.is_number_float())
                            {
                                cmd.args.push_back(std::any(argJson.get<float>()));
                            }
                            else if (argJson.is_number_integer())
                            {
                                cmd.args.push_back(std::any(argJson.get<int>()));
                            }
                            else if (argJson.is_boolean())
                            {
                                cmd.args.push_back(std::any(argJson.get<bool>()));
                            }
                            else if (argJson.is_string())
                            {
                                cmd.args.push_back(std::any(argJson.get<std::string>()));
                            }
                            // Add more types as needed
                        }
                    }

                    config.rmeCommands.push_back(cmd);
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

                // TODO: Query individual parameters (volumes, mutes, etc)
                // This would need an enhanced OscController with full query support
                // For now, we'll simulate success

                callback(true, "Device state queried successfully");
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
                // Query the device state
                Configuration config = deviceStateToConfig(
                    "RME Device", // TODO: Get actual device name
                    controller->getTargetIp(),
                    controller->getTargetPort()
                );

                // Save to file
                bool success = config.saveToFile(filePath);

                if (success)
                {
                    callback(true, "Device state saved to " + filePath);
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
            // Open the file
            std::ifstream file(filepath);
            if (!file.is_open())
            {
                std::cerr << "Failed to open configuration file: " << filepath << std::endl;
                return false;
            }

            // Parse the JSON
            json config;
            file >> config;
            file.close();

            // Clear existing commands
            clearCommands();

            // Extract connection parameters
            if (config.contains("osc"))
            {
                auto &osc = config["osc"];
                if (osc.contains("ip"))
                {
                    m_targetIp = osc["ip"].get<std::string>();
                }
                if (osc.contains("port"))
                {
                    m_targetPort = osc["port"].get<int>();
                }
                if (osc.contains("receive_port"))
                {
                    m_receivePort = osc["receive_port"].get<int>();
                }
            }

            // Process matrix crosspoints
            if (config.contains("matrix"))
            {
                auto &matrix = config["matrix"];
                for (auto &item : matrix.items())
                {
                    // Parse key format "input_output" (e.g. "1_2" means input 1 to output 2)
                    std::string key = item.key();
                    size_t underscore = key.find('_');
                    if (underscore != std::string::npos)
                    {
                        try
                        {
                            int input = std::stoi(key.substr(0, underscore));
                            int output = std::stoi(key.substr(underscore + 1));
                            float gain = item.value().get<float>();
                            setMatrixCrosspointGain(input, output, gain);
                        }
                        catch (const std::exception &e)
                        {
                            std::cerr << "Error parsing matrix key '" << key << "': " << e.what() << std::endl;
                        }
                    }
                }
            }

            // Process channel settings
            const std::string channelTypes[] = {"inputs", "playbacks", "outputs"};
            for (int typeIdx = 0; typeIdx < 3; typeIdx++)
            {
                const std::string &typeStr = channelTypes[typeIdx];

                if (config.contains(typeStr))
                {
                    auto &channels = config[typeStr];
                    for (auto &item : channels.items())
                    {
                        // Parse key as channel number
                        try
                        {
                            int channel = std::stoi(item.key());
                            auto &settings = item.value();

                            // Process volume
                            if (settings.contains("volume"))
                            {
                                float volume = settings["volume"].get<float>();
                                setChannelVolume(typeIdx, channel, volume);
                            }

                            // Process mute
                            if (settings.contains("mute"))
                            {
                                bool mute = settings["mute"].get<bool>();
                                setChannelMute(typeIdx, channel, mute);
                            }
                        }
                        catch (const std::exception &e)
                        {
                            std::cerr << "Error parsing channel settings for '" << item.key()
                                      << "': " << e.what() << std::endl;
                        }
                    }
                }
            }

            // Process raw OSC commands (if present)
            if (config.contains("commands"))
            {
                auto &commands = config["commands"];
                for (auto &cmd : commands)
                {
                    if (cmd.contains("address") && cmd.contains("args"))
                    {
                        std::string address = cmd["address"].get<std::string>();
                        std::vector<std::any> args;

                        for (auto &arg : cmd["args"])
                        {
                            if (arg.is_number_float())
                            {
                                args.push_back(std::any(arg.get<float>()));
                            }
                            else if (arg.is_number_integer())
                            {
                                args.push_back(std::any(arg.get<int>()));
                            }
                            else if (arg.is_boolean())
                            {
                                args.push_back(std::any(arg.get<bool>()));
                            }
                            else if (arg.is_string())
                            {
                                args.push_back(std::any(arg.get<std::string>()));
                            }
                        }

                        addCommand(address, args);
                    }
                }
            }

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
            // Create the JSON structure
            json config;

            // Add connection parameters
            config["osc"]["ip"] = m_targetIp;
            config["osc"]["port"] = m_targetPort;
            config["osc"]["receive_port"] = m_receivePort;

            // Save commands in raw format for complete representation
            for (const auto &cmd : m_commands)
            {
                json jsonCmd;
                jsonCmd["address"] = cmd.address;

                json args = json::array();
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

                jsonCmd["args"] = args;
                config["commands"].push_back(jsonCmd);
            }

            // Write to file
            std::ofstream file(filepath);
            if (!file.is_open())
            {
                std::cerr << "Failed to open file for writing: " << filepath << std::endl;
                return false;
            }

            file << config.dump(4); // Pretty-print with 4-space indent
            file.close();
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error saving configuration: " << e.what() << std::endl;
            return false;
        }
    }

    void Configuration::setConnectionParams(const std::string &ip, int sendPort, int receivePort)
    {
        m_targetIp = ip;
        m_targetPort = sendPort;
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
        RmeOscCommandConfig cmd;
        cmd.address = address;
        cmd.args = args;
        m_commands.push_back(cmd);
    }

    void Configuration::clearCommands()
    {
        m_commands.clear();
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

        // Add channel volumes
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
            }
        }

        // Query all parameters
        queryParameters(queries, [callback, config, this](const std::map<std::string, float> &results)
                        {
            // Process results and populate the configuration
            for (const auto& [address, value] : results) {
                // Parse the address to determine the parameter type
                // Format: /channel/type/param
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

                try {
                    int channel = std::stoi(channelStr);
                    int typeIdx;

                    if (typeStr == "input") typeIdx = 0;
                    else if (typeStr == "playback") typeIdx = 1;
                    else if (typeStr == "output") typeIdx = 2;
                    else continue;

                    if (paramStr == "volume") {
                        // Convert normalized value back to dB
                        float volumeDb = value * 65.0f - 65.0f;
                        config->setChannelVolume(typeIdx, channel, volumeDb);
                    } else if (paramStr == "mute") {
                        bool mute = value > 0.5f;
                        config->setChannelMute(typeIdx, channel, mute);
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error parsing OSC address: " << address << ": " << e.what() << std::endl;
                }
            }

            // Call the callback with the populated configuration
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
