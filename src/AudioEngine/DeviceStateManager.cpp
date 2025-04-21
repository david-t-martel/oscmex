#include "DeviceStateManager.h"
#include <iostream>
#include <fstream>
#include <thread>

namespace AudioEngine
{
    DeviceStateManager::DeviceStateManager(OscController *controller)
        : m_controller(controller), m_pendingQueries(0)
    {
        // Constructor implementation
    }

    DeviceStateManager::~DeviceStateManager()
    {
        // Destructor implementation
    }

    bool DeviceStateManager::queryDeviceState(std::function<void(bool, const Configuration &)> callback)
    {
        if (!m_controller)
        {
            std::cerr << "DeviceStateManager: No OscController available" << std::endl;
            Configuration emptyConfig;
            callback(false, emptyConfig);
            return false;
        }

        return m_controller->queryDeviceState(callback);
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

    bool DeviceStateManager::applyConfiguration(const Configuration &config, std::function<void(bool)> callback)
    {
        if (!m_controller)
        {
            std::cerr << "DeviceStateManager: No OscController available" << std::endl;
            callback(false);
            return false;
        }

        // Apply the configuration asynchronously
        std::thread([this, config, callback]()
                    {
            bool result = m_controller->applyConfiguration(config);
            callback(result); })
            .detach();

        return true;
    }

} // namespace AudioEngine
