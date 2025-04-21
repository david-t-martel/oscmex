#pragma once

#include "Configuration.h"
#include "OscController.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace AudioEngine
{
    /**
     * @brief Manages querying, saving, loading, and applying device state configurations
     *
     * This class provides a unified interface for working with device states, including:
     * - Querying current device state parameters
     * - Saving device states to configuration files
     * - Loading device states from configuration files
     * - Applying configurations to devices
     */
    class DeviceStateManager
    {
    public:
        /**
         * @brief Constructor
         * @param controller Pointer to an OscController for device communication
         */
        DeviceStateManager(OscController *controller);

        /**
         * @brief Destructor
         */
        ~DeviceStateManager();

        /**
         * @brief Device parameter query callback type
         */
        using QueryCallback = std::function<void(bool success, float value)>;

        /**
         * @brief Query completion callback
         */
        using BatchCompletionCallback = std::function<void(bool allSucceeded)>;

        /**
         * @brief Query the complete current state of the device
         * @param callback Function to call with the result (success flag and configuration)
         * @return True if the query was started successfully
         */
        bool queryDeviceState(std::function<void(bool, const Configuration &)> callback);

        /**
         * @brief Query device state with a specific number of channels
         * @param callback Function to call with the configuration
         * @param channelCount Number of channels to query
         * @return True if the query was started successfully
         */
        bool queryDeviceStateWithChannels(std::function<void(bool, const Configuration &)> callback, int channelCount = 32);

        /**
         * @brief Query a single parameter from the device
         * @param address OSC address of the parameter
         * @param callback Function to call with the result (success flag and value)
         * @return True if the query was started successfully
         */
        bool queryParameter(const std::string &address, QueryCallback callback);

        /**
         * @brief Query multiple parameters from the device
         * @param addresses Vector of OSC addresses to query
         * @param callback Function to call with the results (success flag and map of address->value)
         * @return True if the query was started successfully
         */
        bool queryParameters(const std::vector<std::string> &addresses,
                             std::function<void(bool, const std::map<std::string, float> &)> callback);

        /**
         * @brief Save the current device state to a file
         * @param filename Path to save the file
         * @param callback Function to call with the result (success flag)
         * @return True if the save operation was started successfully
         */
        bool saveDeviceStateToFile(const std::string &filename, std::function<void(bool)> callback);

        /**
         * @brief Load a device state from a file
         * @param filename Path to the file to load
         * @param callback Function to call with the result (success flag and loaded configuration)
         * @return True if the load operation was successful
         */
        bool loadDeviceStateFromFile(const std::string &filename, std::function<void(bool, const Configuration &)> callback);

        /**
         * @brief Apply a configuration to the device
         * @param config The configuration to apply
         * @param callback Function to call when complete (success flag)
         * @return True if the application was started successfully
         */
        bool applyConfiguration(const Configuration &config, std::function<void(bool)> callback);

    private:
        OscController *m_controller;                      ///< OSC controller for device communication
        std::map<std::string, QueryCallback> m_callbacks; ///< Map of pending query callbacks
        std::map<std::string, float> m_queryResults;      ///< Results of batch queries
        int m_pendingQueries;                             ///< Counter for pending queries
    };

} // namespace AudioEngine
