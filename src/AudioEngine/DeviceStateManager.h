#pragma once

#include "Configuration.h"
#include "OscController.h"
#include "IExternalControl.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

/**
 * @file DeviceStateManager.h
 * @brief Manager for device state tracking and synchronization
 *
 * The DeviceStateManager is responsible for:
 * - Tracking the current state of hardware devices
 * - Querying devices for their current state
 * - Calculating differences between current state and desired configuration
 * - Applying configurations to devices
 *
 * This class acts as a bridge between Configuration objects (what we want)
 * and the actual hardware devices (what is currently active).
 *
 * It can work with either a direct OscController or any implementation of
 * the IExternalControl interface, allowing for more flexible architecture.
 */
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
     *
     * Can operate with either a direct OscController or through the IExternalControl interface
     */
    class DeviceStateManager
    {
    public:
        /**
         * @brief Constructor with OscController
         * @param controller Pointer to an OscController for device communication
         */
        DeviceStateManager(OscController *controller);

        /**
         * @brief Constructor with IExternalControl
         * @param externalControl Shared pointer to an IExternalControl implementation
         */
        DeviceStateManager(std::shared_ptr<IExternalControl> externalControl);

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
         * @brief DeviceState callback (for configuration operations)
         */
        using DeviceStateCallback = std::function<void(bool success)>;

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

        /**
         * @brief Get current device state
         * @return const DeviceState& Current state
         */
        const DeviceState &getCurrentState() const;

        /**
         * @brief Calculate changes needed to reach desired configuration
         * @param targetConfig Target configuration
         * @return std::map<std::string, std::any> Required changes
         */
        std::map<std::string, std::any> calculateStateChanges(const Configuration &targetConfig) const;

        /**
         * @brief Apply configuration to device
         * @param config Configuration to apply
         * @param callback Callback to be called when complete
         * @return bool True if operation was initiated
         */
        bool applyConfiguration(const Configuration &config, DeviceStateCallback callback);

    private:
        OscController *m_controller;                         ///< OSC controller for device communication (legacy interface)
        std::shared_ptr<IExternalControl> m_externalControl; ///< External control interface (new interface)
        std::map<std::string, QueryCallback> m_callbacks;    ///< Map of pending query callbacks
        std::map<std::string, float> m_queryResults;         ///< Results of batch queries
        int m_pendingQueries;                                ///< Counter for pending queries
        DeviceState m_currentState;                          ///< Current state of the device

        /**
         * @brief Check if the device is accessible (either controller or externalControl is valid)
         * @return True if we have a way to communicate with the device
         */
        bool hasDeviceAccess() const;

        /**
         * @brief Send a parameter query to the device using available control interface
         * @param address OSC address to query
         * @param callback Function to call with the result
         * @return True if the query was started successfully
         */
        bool sendParameterQuery(const std::string &address, QueryCallback callback);
    };

} // namespace AudioEngine
