#pragma once

#include <string>
#include <functional>
#include <iostream>
#include <fstream>
#include <thread>
#include "OscController.h"
#include "Configuration.h"

// Include nlohmann/json for JSON serialization/deserialization
#include "nlohmann/json.hpp"

namespace AudioEngine
{

    /**
     * @brief Device State Manager for querying and storing device configurations
     *
     * This class provides functionality to query the current state of a device
     * via OscController, and to save/load device configurations to/from files.
     */
    class DeviceStateManager
    {
    public:
        DeviceStateManager(OscController *controller) : m_controller(controller) {}

        /**
         * @brief Query the current state of the device
         *
         * @param callback Function to call with the result (success, configuration)
         * @return true if query was initiated successfully
         */
        bool queryDeviceState(std::function<void(bool, const Configuration &)> callback)
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

        /**
         * @brief Save the current device state to a file
         *
         * @param filename Path to save the configuration to
         * @param callback Function to call when the operation is complete (success)
         * @return true if the save operation was initiated successfully
         */
        bool saveDeviceStateToFile(const std::string &filename, std::function<void(bool)> callback)
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
                j["deviceName"] = config.deviceName;
                j["deviceType"] = config.deviceType;
                j["targetIp"] = config.targetIp;
                j["targetPort"] = config.targetPort;
                j["dspLoadPercent"] = config.dspLoadPercent;

                // Add commands for each parameter
                j["commands"] = nlohmann::json::array();

                for (const auto& cmd : config.rmeCommands) {
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

        /**
         * @brief Load a device state from a file
         *
         * @param filename Path to load the configuration from
         * @param callback Function to call with the result (success, configuration)
         * @return true if the load operation was initiated successfully
         */
        bool loadDeviceStateFromFile(const std::string &filename, std::function<void(bool, const Configuration &)> callback)
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
                    config.deviceName = j["deviceName"].get<std::string>();
                }

                if (j.contains("deviceType"))
                {
                    config.deviceType = j["deviceType"].get<std::string>();
                }

                if (j.contains("targetIp"))
                {
                    config.targetIp = j["targetIp"].get<std::string>();
                }

                if (j.contains("targetPort"))
                {
                    config.targetPort = j["targetPort"].get<int>();
                }

                if (j.contains("dspLoadPercent"))
                {
                    config.dspLoadPercent = j["dspLoadPercent"].get<int>();
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

                        RmeOscCommandConfig cmd;
                        cmd.address = cmdJson["address"].get<std::string>();

                        // Parse arguments
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
                            // Skip other types
                        }

                        config.rmeCommands.push_back(cmd);
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

        /**
         * @brief Apply a configuration to the device
         *
         * @param config Configuration to apply
         * @param callback Function to call when the operation is complete (success)
         * @return true if the apply operation was initiated successfully
         */
        bool applyConfiguration(const Configuration &config, std::function<void(bool)> callback)
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

    private:
        OscController *m_controller;
    };

} // namespace AudioEngine
