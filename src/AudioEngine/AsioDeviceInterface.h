#pragma once

#include "DeviceStateInterface.h"
#include "DeviceState.h"
#include "Configuration.h"

namespace AudioEngine
{
    // Forward declarations
    class AsioManager;

    /**
     * @brief Implementation of DeviceStateInterface for ASIO devices
     *
     * Provides interface to query and configure ASIO audio devices
     * using the AsioManager.
     */
    class AsioDeviceInterface : public DeviceStateInterface
    {
    public:
        /**
         * @brief Construct an ASIO device interface
         *
         * @param asioManager ASIO manager to use for device communication
         */
        AsioDeviceInterface(AsioManager *asioManager);

        /**
         * @brief Destroy the ASIO device interface
         */
        ~AsioDeviceInterface() override = default;

        /**
         * @brief Query ASIO device state
         *
         * @param deviceName ASIO device name
         * @param callback Function to call with query results
         * @return bool True if query was initiated successfully
         */
        bool queryState(const std::string &deviceName,
                        std::function<void(bool, const DeviceState &)> callback) override;

        /**
         * @brief Apply configuration to ASIO device
         *
         * @param deviceName ASIO device name
         * @param config Configuration to apply
         * @param callback Function to call with results
         * @return bool True if configuration application was initiated
         */
        bool applyConfiguration(const std::string &deviceName,
                                const Configuration &config,
                                std::function<void(bool, const std::string &)> callback) override;

    private:
        AsioManager *m_asioManager; // Non-owning pointer to ASIO manager
    };

} // namespace AudioEngine
