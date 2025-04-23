#pragma once

#include "DeviceState.h"
#include "Configuration.h"

#include <functional>
#include <string>
#include <memory>

namespace AudioEngine
{
    /**
     * @brief Interface for device state management
     *
     * This abstract class defines the interface for interacting with audio device state.
     * It provides methods to query the current state of a device and apply configuration
     * changes to it. Implementations should handle the specifics of different device types
     * (ASIO, OSC-controlled hardware, etc.).
     */
    class DeviceStateInterface
    {
    public:
        virtual ~DeviceStateInterface() = default;

        /**
         * @brief Query current device state
         *
         * @param deviceName Name of the device
         * @param callback Callback to be called with device state information
         * @return bool True if query was initiated successfully
         */
        virtual bool queryState(const std::string &deviceName,
                                std::function<void(bool, const DeviceState &)> callback) = 0;

        /**
         * @brief Apply configuration to device
         *
         * @param deviceName Name of the device
         * @param config Configuration to apply
         * @param callback Callback reporting success/failure and optional message
         * @return bool True if configuration application was initiated successfully
         */
        virtual bool applyConfiguration(const std::string &deviceName,
                                        const Configuration &config,
                                        std::function<void(bool, const std::string &)> callback) = 0;

        /**
         * @brief Convert device state to configuration
         *
         * @param state Current device state
         * @return Configuration Configuration representing current state
         */
        static Configuration stateToConfiguration(const DeviceState &state);
    };

} // namespace AudioEngine
