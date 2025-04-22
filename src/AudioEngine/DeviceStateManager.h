#pragma once

#include "DeviceState.h"
#include "IExternalControl.h"
#include <memory>
#include <map>
#include <functional>
#include <mutex>

namespace AudioEngine
{
    /**
     * @brief Manages the state of all devices in the system
     *
     * This class provides a central place to track and update device states,
     * and to synchronize those states with external control interfaces.
     */
    class DeviceStateManager
    {
    public:
        /**
         * @brief Default constructor
         */
        DeviceStateManager();

        /**
         * @brief Destructor
         */
        ~DeviceStateManager();

        /**
         * @brief Set the external control interface for state synchronization
         *
         * @param externalControl The external control implementation or nullptr to remove
         */
        void setExternalControl(std::shared_ptr<IExternalControl> externalControl);

        /**
         * @brief Get the external control interface
         *
         * @return The external control interface or nullptr if not set
         */
        std::shared_ptr<IExternalControl> getExternalControl() const;

        /**
         * @brief Get or create a device state
         *
         * @param deviceId Unique identifier for the device
         * @return Reference to the device state
         */
        DeviceState &getDeviceState(const std::string &deviceId);

        /**
         * @brief Check if a device exists in the manager
         *
         * @param deviceId Device identifier
         * @return true if device exists
         */
        bool hasDevice(const std::string &deviceId) const;

        /**
         * @brief Remove a device from the manager
         *
         * @param deviceId Device identifier
         * @return true if device was removed
         */
        bool removeDevice(const std::string &deviceId);

        /**
         * @brief Get all device identifiers
         *
         * @return Vector of device identifiers
         */
        std::vector<std::string> getDeviceIds() const;

        /**
         * @brief Update a parameter value and synchronize with external control
         *
         * @param deviceId Device identifier
         * @param path Parameter path/address
         * @param value Parameter value
         * @param syncWithExternal Whether to sync with external control
         */
        template <typename T>
        void updateParameter(const std::string &deviceId, const std::string &path,
                             const T &value, bool syncWithExternal = true)
        {
            // Update the local device state
            std::lock_guard<std::mutex> lock(m_mutex);
            auto &deviceState = getDeviceState(deviceId);
            deviceState.setParameter(path, value);

            // Synchronize with external control if needed
            if (syncWithExternal && m_externalControl)
            {
                std::string fullPath = "/" + deviceId + path;
                std::vector<std::any> args = {value};
                m_externalControl->sendCommand(fullPath, args);
            }
        }

        /**
         * @brief Register a callback for parameter changes
         *
         * @param deviceId Device identifier (* for all devices)
         * @param path Parameter path/address (* for all parameters)
         * @param callback Function to call when parameter changes
         * @return Unique callback ID for removing the callback
         */
        int addParameterChangeCallback(
            const std::string &deviceId,
            const std::string &path,
            std::function<void(const std::string &, const std::string &, const std::any &)> callback);

        /**
         * @brief Remove a parameter change callback
         *
         * @param callbackId ID of the callback to remove
         * @return true if callback was removed
         */
        bool removeParameterChangeCallback(int callbackId);

        /**
         * @brief Handle a command received from external control
         *
         * @param address Command address
         * @param args Command arguments
         * @return true if command was handled
         */
        bool handleExternalCommand(const std::string &address, const std::vector<std::any> &args);

    private:
        // Map of device IDs to device states
        std::map<std::string, DeviceState> m_deviceStates;

        // External control interface
        std::shared_ptr<IExternalControl> m_externalControl;

        // Parameter change callbacks
        struct CallbackInfo
        {
            std::string deviceId;
            std::string path;
            std::function<void(const std::string &, const std::string &, const std::any &)> callback;
        };

        std::map<int, CallbackInfo> m_callbacks;
        int m_nextCallbackId;

        // Thread safety
        mutable std::mutex m_mutex;

        // Helper to parse an address into device ID and parameter path
        bool parseAddress(const std::string &address, std::string &deviceId, std::string &path);
    };

} // namespace AudioEngine
