#include "DeviceStateManager.h"
#include <iostream>
#include <fstream>
#include <thread>

namespace AudioEngine
{
    DeviceStateManager::DeviceStateManager(std::unique_ptr<DeviceStateInterface> deviceInterface)
        : m_deviceInterface(std::move(deviceInterface)), m_pendingQueries(0)
    {
        // Constructor implementation
    }

    DeviceStateManager::~DeviceStateManager()
    {
        // Destructor implementation
    }

    bool DeviceStateManager::queryDeviceState(std::function<void(bool, const Configuration &)> callback)
    {
        if (m_deviceInterface)
        {
            return m_deviceInterface->queryState(m_currentDevice,
                                                 [callback](bool success, const DeviceState &state)
                                                 {
                                                     if (success)
                                                     {
                                                         Configuration config = DeviceStateInterface::stateToConfiguration(state);
                                                         callback(true, config);
                                                     }
                                                     else
                                                     {
                                                         callback(false, Configuration());
                                                     }
                                                 });
        }
        else if (m_controller)
        {
            // Existing implementation using m_controller...
        }
        else if (m_externalControl)
        {
            // Implementation using m_externalControl...
        }
        else
        {
            callback(false, Configuration());
            return false;
        }
    }

    bool DeviceStateManager::queryDeviceStateWithChannels(std::function<void(bool, const Configuration &)> callback, int channelCount)
    {
        if (!m_controller)
        {
            std::cerr << "DeviceStateManager: No OscController available" << std::endl;
            Configuration emptyConfig;
            callback(false, emptyConfig);
            return false;
        }

        // Create a list of OSC addresses to query based on channel count
        std::vector<std::string> addresses;

        // Add input channel parameters
        for (int i = 1; i <= channelCount; i++)
        {
            addresses.push_back("/1/channel/" + std::to_string(i) + "/volume");
            addresses.push_back("/1/channel/" + std::to_string(i) + "/mute");
            addresses.push_back("/1/channel/" + std::to_string(i) + "/solo");
        }

        // Add playback channel parameters
        for (int i = 1; i <= channelCount; i++)
        {
            addresses.push_back("/2/channel/" + std::to_string(i) + "/volume");
            addresses.push_back("/2/channel/" + std::to_string(i) + "/mute");
            addresses.push_back("/2/channel/" + std::to_string(i) + "/solo");
        }

        // Add output channel parameters
        for (int i = 1; i <= channelCount; i++)
        {
            addresses.push_back("/3/channel/" + std::to_string(i) + "/volume");
            addresses.push_back("/3/channel/" + std::to_string(i) + "/mute");
        }

        // Query all parameters and construct the configuration
        return queryParameters(addresses, [callback](bool success, const std::map<std::string, float> &results)
                               {
            Configuration config;

            if (success) {
                // Use results to populate configuration
                for (const auto& pair : results) {
                    std::vector<std::any> args = { std::any(pair.second) };
                    config.addCommand(pair.first, args);
                }
            }

            callback(success, config); });
    }

    bool DeviceStateManager::queryParameter(const std::string &address, QueryCallback callback)
    {
        if (!m_controller)
        {
            std::cerr << "DeviceStateManager: No OscController available" << std::endl;
            callback(false, 0.0f);
            return false;
        }

        // Store the callback for this specific address
        m_callbacks[address] = callback;
        m_pendingQueries++;

        // Ask OscController to query the parameter
        return m_controller->queryParameter(address, [this, address](bool success, float value)
                                            {
            // Find and call the stored callback
            auto iter = m_callbacks.find(address);
            if (iter != m_callbacks.end())
            {
                iter->second(success, value);
                m_callbacks.erase(iter);
            }

            // Store the result for batch queries
            if (success) {
                m_queryResults[address] = value;
            }

            // Decrement pending queries counter
            m_pendingQueries--; });
    }

    bool DeviceStateManager::queryParameters(const std::vector<std::string> &addresses,
                                             std::function<void(bool, const std::map<std::string, float> &)> callback)
    {
        if (!m_controller)
        {
            std::cerr << "DeviceStateManager: No OscController available" << std::endl;
            callback(false, std::map<std::string, float>());
            return false;
        }

        // Clear results from previous queries
        m_queryResults.clear();
        m_pendingQueries = 0;

        // Track if any query fails
        bool anyFailed = false;

        // Define a shared callback for all parameters
        auto sharedCallback = [this, addresses, callback, &anyFailed](bool success, float value)
        {
            if (!success)
            {
                anyFailed = true;
            }

            // If all queries have completed, call the original callback
            if (m_pendingQueries == 0)
            {
                callback(!anyFailed, m_queryResults);
            }
        };

        // Start all queries
        for (const auto &address : addresses)
        {
            if (!queryParameter(address, sharedCallback))
            {
                anyFailed = true;
            }
        }

        // Handle the case where no queries were started
        if (m_pendingQueries == 0)
        {
            callback(!anyFailed, m_queryResults);
            return !anyFailed;
        }

        return true;
    }

    bool DeviceStateManager::saveDeviceStateToFile(const std::string &filename, std::function<void(bool)> callback)
    {
        // Query the current device state
        return queryDeviceState([filename, callback](bool success, const Configuration &config)
                                {
            if (!success) {
                std::cerr << "DeviceStateManager: Failed to query device state" << std::endl;
                callback(false);
                return;
            }

            // Save the configuration to file
            try {
                // Create JSON object
                nlohmann::json j;

                // Add basic device info
                j["deviceName"] = config.getAsioDeviceName();
                j["deviceType"] = deviceTypeToString(config.getDeviceType());
                j["targetIp"] = config.getTargetIp();
                j["targetPort"] = config.getTargetPort();

                // Add commands for each parameter
                j["commands"] = nlohmann::json::array();

                for (const auto& cmd : config.getCommands()) {
                    nlohmann::json command;
                    command["address"] = cmd.address;

                    // Convert args to appropriate JSON types
                    nlohmann::json args = nlohmann::json::array();

                    for (const auto& arg : cmd.args) {
                        if (arg.type() == typeid(float)) {
                            args.push_back(std::any_cast<float>(arg));
                        }
                        else if (arg.type() == typeid(int)) {
                            args.push_back(std::any_cast<int>(arg));
                        }
                        else if (arg.type() == typeid(bool)) {
                            args.push_back(std::any_cast<bool>(arg));
                        }
                        else if (arg.type() == typeid(std::string)) {
                            args.push_back(std::any_cast<std::string>(arg));
                        }
                        else {
                            // Skip unsupported types
                            std::cerr << "DeviceStateManager: Skipping unsupported argument type: "
                                      << arg.type().name() << std::endl;
                        }
                    }

                    command["args"] = args;
                    j["commands"].push_back(command);
                }

                // Write to file
                std::ofstream file(filename);
                if (!file.is_open()) {
                    std::cerr << "DeviceStateManager: Failed to open file for writing: " << filename << std::endl;
                    callback(false);
                    return;
                }

                file << j.dump(4); // Pretty print with 4-space indent
                file.close();

                std::cout << "DeviceStateManager: Saved device state to " << filename << std::endl;
                callback(true);
            }
            catch (const std::exception& e) {
                std::cerr << "DeviceStateManager: Failed to save device state: " << e.what() << std::endl;
                callback(false);
            } });
    }

    bool DeviceStateManager::loadDeviceStateFromFile(const std::string &filename, std::function<void(bool, const Configuration &)> callback)
    {
        try
        {
            // Open and read the file
            std::ifstream file(filename);
            if (!file.is_open())
            {
                std::cerr << "DeviceStateManager: Failed to open file for reading: " << filename << std::endl;
                callback(false, Configuration());
                return false;
            }

            // Parse JSON
            nlohmann::json j = nlohmann::json::parse(file);
            file.close();

            // Create configuration object
            Configuration config;

            // Extract basic device info
            if (j.contains("deviceName"))
            {
                config.setAsioDeviceName(j["deviceName"].get<std::string>());
            }

            if (j.contains("deviceType"))
            {
                config.setDeviceType(stringToDeviceType(j["deviceType"].get<std::string>()));
            }

            if (j.contains("targetIp") && j.contains("targetPort"))
            {
                std::string ip = j["targetIp"].get<std::string>();
                int port = j["targetPort"].get<int>();
                int receivePort = j.contains("receivePort") ? j["receivePort"].get<int>() : 0;
                config.setConnectionParams(ip, port, receivePort);
            }

            // Extract commands
            if (j.contains("commands") && j["commands"].is_array())
            {
                for (const auto &cmdJson : j["commands"])
                {
                    if (!cmdJson.contains("address") || !cmdJson.contains("args"))
                    {
                        continue; // Skip invalid commands
                    }

                    std::string address = cmdJson["address"].get<std::string>();
                    std::vector<std::any> args;

                    // Parse arguments
                    for (const auto &argJson : cmdJson["args"])
                    {
                        if (argJson.is_number_float())
                        {
                            args.push_back(std::any(argJson.get<float>()));
                        }
                        else if (argJson.is_number_integer())
                        {
                            args.push_back(std::any(argJson.get<int>()));
                        }
                        else if (argJson.is_boolean())
                        {
                            args.push_back(std::any(argJson.get<bool>()));
                        }
                        else if (argJson.is_string())
                        {
                            args.push_back(std::any(argJson.get<std::string>()));
                        }
                        // Skip other types
                    }

                    config.addCommand(address, args);
                }
            }

            std::cout << "DeviceStateManager: Loaded device state from " << filename << std::endl;
            callback(true, config);
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "DeviceStateManager: Failed to load device state: " << e.what() << std::endl;
            callback(false, Configuration());
            return false;
        }
    }

    std::map<std::string, std::any> DeviceStateManager::calculateStateChanges(
        const Configuration &targetConfig) const
    {
        // Convert target configuration to a state
        DeviceState targetState = DeviceState::fromConfiguration(targetConfig);

        // Calculate differences
        return m_currentState.diffFrom(targetState);
    }

    bool DeviceStateManager::applyConfiguration(
        const Configuration &config,
        DeviceStateCallback callback)
    {
        // Calculate changes needed
        auto changes = calculateStateChanges(config);

        // If no changes needed
        if (changes.empty())
        {
            callback(true, "No changes needed");
            return true;
        }

        // Apply changes via OSC controller
        std::vector<OscCommand> commands = convertChangesToCommands(changes);

        // Track how many pending operations we have
        m_pendingQueries.fetch_add(commands.size());

        // Send all commands
        for (const auto &cmd : commands)
        {
            m_controller->sendOscMessage(cmd.address, cmd.args,
                                         [this, callback](bool success)
                                         {
                                             if (!success)
                                             {
                                                 // Handle failed command
                                                 errorOccurred = true;
                                             }

                                             // Check if all commands are complete
                                             if (m_pendingQueries.fetch_sub(1) == 1)
                                             {
                                                 // All commands have completed
                                                 callback(!errorOccurred, errorOccurred ? "Failed to apply some settings" : "Configuration applied successfully");
                                             }
                                         });
        }

        return true;
    }

    bool DeviceStateManager::saveToFile(const std::string &filePath) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        nlohmann::json json;

        // Save device states
        nlohmann::json deviceStatesJson = nlohmann::json::object();
        for (const auto &pair : m_deviceStates)
        {
            deviceStatesJson[pair.first] = pair.second->toJson();
        }
        json["deviceStates"] = deviceStatesJson;

        // Save current state
        json["currentState"] = m_currentState.toJson();

        // Write to file
        try
        {
            std::ofstream file(filePath);
            if (!file.is_open())
            {
                return false;
            }

            file << json.dump(4);
            file.close();
            return true;
        }
        catch (const std::exception &e)
        {
            return false;
        }
    }

    bool DeviceStateManager::queryState(const std::string &deviceName, std::function<void(bool, const DeviceState &)> callback)
    {
        // Store the device name for later use
        m_currentDevice = deviceName;

        if (m_controller)
        {
            // Using the OscController implementation
            std::thread([this, deviceName, callback]()
                        {
                DeviceState state(deviceName);

                // Request a refresh to ensure we get current state
                if (m_controller->requestRefresh())
                {
                    // Allow time for the device to process the refresh
                    std::this_thread::sleep_for(std::chrono::milliseconds(300));

                    // Query basic parameters
                    float sampleRate = 0.0f;
                    bool sampleRateSuccess = m_controller->querySingleValue("/main/sample_rate", sampleRate);
                    if (sampleRateSuccess) {
                        state.setSampleRate(static_cast<double>(sampleRate));
                    }

                    float bufferSize = 0.0f;
                    bool bufferSizeSuccess = m_controller->querySingleValue("/main/buffer_size", bufferSize);
                    if (bufferSizeSuccess) {
                        state.setBufferSize(static_cast<int>(bufferSize));
                    }

                    // Add other hardware parameters...

                    // If we got here, we've successfully queried the device state
                    callback(true, state);
                }
                else
                {
                    callback(false, state);
                } })
                .detach();

            return true;
        }
        else if (m_externalControl)
        {
            // Using the IExternalControl implementation
            std::thread([this, deviceName, callback]()
                        {
                DeviceState state(deviceName);

                // Query state through external control interface
                m_externalControl->sendCommand("/query_state", {deviceName},
                    [this, &state, callback](bool success, const std::vector<std::any>& response) {
                        if (success && !response.empty()) {
                            // Parse response and update state
                            // This depends on the format returned by the external control

                            // Example: If response contains JSON state data
                            try {
                                if (response[0].type() == typeid(std::string)) {
                                    std::string jsonData = std::any_cast<std::string>(response[0]);
                                    state = DeviceState::fromJson(jsonData);
                                    callback(true, state);
                                    return;
                                }
                            }
                            catch (const std::exception& e) {
                                // Handle parsing error
                            }
                        }

                        callback(false, state);
                    }); })
                .detach();

            return true;
        }
        else if (m_deviceInterface)
        {
            // This case is already handled in the queryDeviceState method
            return true;
        }

        // No implementation available
        DeviceState emptyState(deviceName);
        callback(false, emptyState);
        return false;
    }

    bool DeviceStateManager::applyConfiguration(const std::string &deviceName, const Configuration &config)
    {
        // Convert configuration to device state
        DeviceState targetState = config.toDeviceState();

        // Set the device name
        m_currentDevice = deviceName;

        if (m_controller)
        {
            // Using the OscController implementation
            // Calculate the difference between current state and target state
            auto changes = calculateStateChanges(config);

            // Apply each change
            for (const auto &[address, value] : changes)
            {
                std::vector<std::any> args;
                args.push_back(value);
                m_controller->sendOscMessage(address, args);
            }

            return true;
        }
        else if (m_externalControl)
        {
            // Using the IExternalControl implementation
            std::string jsonState = targetState.toJson();

            // Send the entire state to the external control interface
            m_externalControl->sendCommand("/apply_state", {deviceName, jsonState});
            return true;
        }

        return false;
    }

} // namespace AudioEngine
