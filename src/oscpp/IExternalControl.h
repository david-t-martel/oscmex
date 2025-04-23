/**
 * @file IExternalControl.h
 * @brief Interface for external control systems
 *
 * This header defines the interface for external control systems that can be used
 * with the AudioEngine. This allows the engine to work with different control systems
 * (OSC, MIDI, custom protocols) through a common interface.
 */

#pragma once

#include <string>
#include <vector>
#include <any>
#include <functional>
#include <memory>

// Forward declaration
class Configuration;

namespace AudioEngine
{

    /**
     * @brief Interface for external control systems like OSC
     *
     * This interface defines the contract between the audio engine and
     * external control systems, allowing for loose coupling between components.
     */
    class IExternalControl
    {
    public:
        virtual ~IExternalControl() = default;

        /**
         * @brief Configure the controller with target IP and ports
         *
         * @param targetIp IP address of the target device
         * @param targetPort Port number for sending commands
         * @param receivePort Optional port number for receiving responses (0 for none)
         * @return true if configuration was successful
         */
        virtual bool configure(const std::string &targetIp, int targetPort, int receivePort = 0) = 0;

        /**
         * @brief Configure from a configuration object
         *
         * @param config Configuration object containing control settings
         * @return true if configuration was successful
         */
        virtual bool configure(const Configuration &config) = 0;

        /**
         * @brief Send a parameter change command
         *
         * @param address Command address/path
         * @param args Command arguments
         * @return true if command was sent successfully
         */
        virtual bool sendCommand(const std::string &address, const std::vector<std::any> &args) = 0;

        /**
         * @brief Query a parameter value
         *
         * @param address Parameter address/path
         * @param callback Function to call with results (success, values)
         * @return true if query was sent successfully
         */
        virtual bool queryParameter(const std::string &address,
                                    std::function<void(bool, const std::vector<std::any> &)> callback) = 0;

        /**
         * @brief Apply a batch of configuration commands
         *
         * @param config Configuration containing commands to apply
         * @return true if all commands were processed successfully
         */
        virtual bool applyConfiguration(const Configuration &config) = 0;

        /**
         * @brief Query the current state of the controlled device
         *
         * @param callback Function to call with results (success, configuration)
         * @return true if query was initiated successfully
         */
        virtual bool queryDeviceState(
            std::function<void(bool, const Configuration &)> callback) = 0;

        /**
         * @brief Register a callback for incoming events/messages
         *
         * @param callback Function to call when events are received (address, args)
         * @return int Callback ID for later removal
         */
        virtual int addEventCallback(
            std::function<void(const std::string &, const std::vector<std::any> &)> callback) = 0;

        /**
         * @brief Remove a previously registered event callback
         *
         * @param callbackId ID returned from addEventCallback
         */
        virtual void removeEventCallback(int callbackId) = 0;
    };

} // namespace AudioEngine
