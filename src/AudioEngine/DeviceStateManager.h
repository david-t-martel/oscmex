#pragma once

#include "Configuration.h"
#include "OscController.h"
#include "IExternalControl.h"
#include "DeviceState.h"
#include "DeviceStateInterface.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <mutex>
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
        using StateChangedCallback = std::function<void(const std::string &, const std::string &, const std::any &)>;

        // New constructor taking DeviceStateInterface
        DeviceStateManager(std::unique_ptr<DeviceStateInterface> deviceInterface);

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
         * @brief Default constructor
         */
        DeviceStateManager();

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

        /**
         * @brief Set the external control interface for sending state updates
         *
         * @param externalControl External control interface implementation
         */
        void setExternalControl(std::shared_ptr<IExternalControl> externalControl);

        /**
         * @brief Get the external control interface
         *
         * @return std::shared_ptr<IExternalControl> External control interface or nullptr
         */
        std::shared_ptr<IExternalControl> getExternalControl() const;

        /**
         * @brief Create or get a device state for a specific device
         *
         * @param deviceId Unique identifier for the device
         * @return DeviceState& Reference to the device state
         */
        DeviceState &getDeviceState(const std::string &deviceId);

        /**
         * @brief Check if a device state exists
         *
         * @param deviceId Unique identifier for the device
         * @return true if the device state exists
         */
        bool hasDeviceState(const std::string &deviceId) const;

        /**
         * @brief Remove a device state
         *
         * @param deviceId Unique identifier for the device
         * @return true if the device state was removed
         */
        bool removeDeviceState(const std::string &deviceId);

        /**
         * @brief Set a parameter value and notify external controllers
         *
         * @param deviceId Unique identifier for the device
         * @param address Parameter address/path
         * @param value Parameter value
         */
        template <typename T>
        void setParameter(const std::string &deviceId, const std::string &address, const T &value)
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            // Update local state
            auto &deviceState = getDeviceState(deviceId);
            deviceState.setParameter(address, value);

            // Notify external control if available
            if (m_externalControl)
            {
                std::vector<std::any> args = {value};
                m_externalControl->sendCommand(deviceId + "/" + address, args);
            }

            // Notify callbacks
            notifyStateChanged(deviceId, address, value);
        }

        /**
         * @brief Get a parameter value
         *
         * @param deviceId Unique identifier for the device
         * @param address Parameter address/path
         * @param defaultValue Value to return if parameter doesn't exist
         * @return T Parameter value or defaultValue if not found
         */
        template <typename T>
        T getParameter(const std::string &deviceId, const std::string &address, const T &defaultValue) const
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            auto it = m_deviceStates.find(deviceId);
            if (it != m_deviceStates.end())
            {
                return it->second->getParameter<T>(address, defaultValue);
            }

            return defaultValue;
        }

        /**
         * @brief Add a callback for state changes
         *
         * @param callback Function to call when state changes
         * @return int Callback ID used to remove the callback later
         */
        int addStateChangedCallback(StateChangedCallback callback);

        /**
         * @brief Remove a state changed callback
         *
         * @param callbackId ID of the callback to remove
         * @return true if callback was removed
         */
        bool removeStateChangedCallback(int callbackId);

        /**
         * @brief Save all device states to a JSON file
         *
         * @param filePath Path to save the JSON file
         * @return true if file was saved successfully
         */
        bool saveToFile(const std::string &filePath) const;

        /**
         * @brief Load device states from a JSON file
         *
         * @param filePath Path to the JSON file
         * @return true if file was loaded successfully
         */
        bool loadFromFile(const std::string &filePath);

    private:
        std::unique_ptr<DeviceStateInterface> m_deviceInterface;
        OscController *m_controller;                                        ///< OSC controller for device communication (legacy interface)
        std::shared_ptr<IExternalControl> m_externalControl;                ///< External control interface (new interface)
        std::map<std::string, QueryCallback> m_callbacks;                   ///< Map of pending query callbacks
        std::map<std::string, float> m_queryResults;                        ///< Results of batch queries
        int m_pendingQueries;                                               ///< Counter for pending queries
        DeviceState m_currentState;                                         ///< Current state of the device
        std::map<std::string, std::unique_ptr<DeviceState>> m_deviceStates; ///< Map of device states

        // Thread safety
        mutable std::mutex m_mutex;

        // Callbacks for state changes
        std::map<int, StateChangedCallback> m_callbacks;
        int m_nextCallbackId;

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

        /**
         * @brief Notify callbacks of state changes
         *
         * @param deviceId Device identifier
         * @param address Parameter address
         * @param value New parameter value
         */
        void notifyStateChanged(const std::string &deviceId, const std::string &address, const std::any &value);
    };

} // namespace AudioEngine
