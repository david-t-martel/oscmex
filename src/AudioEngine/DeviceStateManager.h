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
        bool queryDeviceState(std::function<void(bool, const Configuration &)> callback);

        /**
         * @brief Save the current device state to a file
         *
         * @param filename Path to save the configuration to
         * @param callback Function to call when the operation is complete (success)
         * @return true if the save operation was initiated successfully
         */
        bool saveDeviceStateToFile(const std::string &filename, std::function<void(bool)> callback);

        /**
         * @brief Load a device state from a file
         *
         * @param filename Path to load the configuration from
         * @param callback Function to call with the result (success, configuration)
         * @return true if the load operation was initiated successfully
         */
        bool loadDeviceStateFromFile(const std::string &filename, std::function<void(bool, const Configuration &)> callback);

        /**
         * @brief Apply a configuration to the device
         *
         * @param config Configuration to apply
         * @param callback Function to call when the operation is complete (success)
         * @return true if the apply operation was initiated successfully
         */
        bool applyConfiguration(const Configuration &config, std::function<void(bool)> callback);

    private:
        OscController *m_controller;
    };

} // namespace AudioEngine
