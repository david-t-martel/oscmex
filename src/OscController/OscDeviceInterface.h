#pragma once

#include "DeviceStateInterface.h"
#include "DeviceState.h"
#include "Configuration.h"

namespace AudioEngine
{
    // Forward declarations
    class OscController;

    /**
     * @brief Implementation of DeviceStateInterface for OSC-controlled devices
     *
     * Provides interface to query and configure devices that support control
     * via the Open Sound Control (OSC) protocol.
     */
    class OscDeviceInterface : public DeviceStateInterface
    {
    public:
        /**
         * @brief Construct an OSC device interface
         *
         * @param oscController OSC controller to use for device communication
         */
        OscDeviceInterface(OscController *oscController);

        /**
         * @brief Destroy the OSC device interface
         */
        ~OscDeviceInterface() override = default;

        /**
         * @brief Query OSC device state
         *
         * @param deviceName Device identifier (may be IP address or alias)
         * @param callback Function to call with query results
         * @return bool True if query was initiated successfully
         */
        bool queryState(const std::string &deviceName,
                        std::function<void(bool, const DeviceState &)> callback) override;

        /**
         * @brief Apply configuration to OSC device
         *
         * @param deviceName Device identifier (may be IP address or alias)
         * @param config Configuration to apply
         * @param callback Function to call with results
         * @return bool True if configuration application was initiated
         */
        bool applyConfiguration(const std::string &deviceName,
                                const Configuration &config,
                                std::function<void(bool, const std::string &)> callback) override;

    private:
        OscController *m_oscController; // Non-owning pointer to OSC controller

        /**
         * @brief Query standard OSC parameters
         *
         * @param deviceState Device state to populate
         * @param callback Function to call when query completes
         */
        void queryOscParameters(DeviceState &deviceState,
                                std::function<void(bool)> callback);

        /**
         * @brief Send a batch of OSC commands from configuration
         *
         * @param config Configuration containing commands to send
         * @param callback Function to call when batch completes
         */
        void sendOscCommands(const Configuration &config,
                             std::function<void(bool, const std::string &)> callback);
    };

} // namespace AudioEngine
