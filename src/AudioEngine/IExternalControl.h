#pragma once

#include <string>
#include <vector>
#include <functional>
#include <any>
#include <memory>

namespace AudioEngine
{
    // Forward declarations
    class Configuration;

    /**
     * @brief Interface for external control mechanisms
     *
     * This interface defines the contract between the AudioEngine and
     * any external control implementation (like OscController).
     * It allows the AudioEngine to operate with or without external control,
     * and enables the control components to operate independently.
     */
    class IExternalControl
    {
    public:
        /**
         * @brief Virtual destructor
         */
        virtual ~IExternalControl() = default;

        /**
         * @brief Set a parameter value
         *
         * @param address Parameter address (e.g., "/1/channel/1/volume")
         * @param args Parameter value(s)
         * @return true if successful
         */
        virtual bool setParameter(const std::string &address, const std::vector<std::any> &args) = 0;

        /**
         * @brief Get a parameter value
         *
         * @param address Parameter address (e.g., "/1/channel/1/volume")
         * @param callback Function to receive the result (success, value)
         * @return true if request was sent successfully
         */
        virtual bool getParameter(const std::string &address,
                                  std::function<void(bool, const std::vector<std::any> &)> callback) = 0;

        /**
         * @brief Apply a complete configuration
         *
         * @param config Configuration to apply
         * @return true if successful
         */
        virtual bool applyConfiguration(const Configuration &config) = 0;

        /**
         * @brief Query the current device state
         *
         * @param callback Function to receive the result (success, configuration)
         * @return true if query was started successfully
         */
        virtual bool queryDeviceState(std::function<void(bool, const Configuration &)> callback) = 0;

        /**
         * @brief Add an event callback for receiving notifications
         *
         * @param callback Function to call when events occur (address, args)
         * @return int Callback ID for later removal
         */
        virtual int addEventCallback(
            std::function<void(const std::string &, const std::vector<std::any> &)> callback) = 0;

        /**
         * @brief Remove an event callback
         *
         * @param callbackId ID of the callback to remove
         */
        virtual void removeEventCallback(int callbackId) = 0;
    };
}
