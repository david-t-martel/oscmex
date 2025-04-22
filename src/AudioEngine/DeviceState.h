#pragma once

#include <string>
#include <map>
#include <mutex>
#include <atomic>
#include <vector>
#include <functional>
#include <any>

namespace AudioEngine
{

    /**
     * @brief Represents the current state of an audio device
     *
     * Maintains information about device parameters, buffer states,
     * and operational status.
     */
    class DeviceState
    {
    public:
        /**
         * @brief Construct a new Device State object with ID and name
         *
         * @param deviceId Unique identifier for the device
         * @param deviceName Human-readable name of the device
         */
        DeviceState(const std::string &deviceId, const std::string &deviceName);

        /**
         * @brief Construct a new Device State object with just ID
         *
         * @param deviceId Unique identifier for the device
         */
        DeviceState(const std::string &deviceId);

        /**
         * @brief Destroy the Device State object
         */
        ~DeviceState();

        /**
         * @brief Get the device identifier
         *
         * @return const std::string& Device ID
         */
        std::string getDeviceId() const;

        /**
         * @brief Set the device identifier
         *
         * @param deviceId New device ID
         */
        void setDeviceId(const std::string &deviceId);

        /**
         * @brief Get the device name
         *
         * @return const std::string& Device name
         */
        const std::string &getDeviceName() const { return m_deviceName; }

        /**
         * @brief Check if device is currently active
         *
         * @return true if device is active
         */
        bool isActive() const { return m_active; }

        /**
         * @brief Set device active state
         *
         * @param active New active state
         */
        void setActive(bool active) { m_active = active; }

        /**
         * @brief Get the current sample rate
         *
         * @return double Current sample rate in Hz
         */
        double getSampleRate() const { return m_sampleRate; }

        /**
         * @brief Set the current sample rate
         *
         * @param sampleRate Sample rate in Hz
         */
        void setSampleRate(double sampleRate) { m_sampleRate = sampleRate; }

        /**
         * @brief Get the current buffer size
         *
         * @return int Buffer size in samples
         */
        int getBufferSize() const { return m_bufferSize; }

        /**
         * @brief Set the current buffer size
         *
         * @param bufferSize Buffer size in samples
         */
        void setBufferSize(int bufferSize) { m_bufferSize = bufferSize; }

        /**
         * @brief Get the number of input channels
         *
         * @return int Number of input channels
         */
        int getInputChannelCount() const { return m_inputChannelCount; }

        /**
         * @brief Set the number of input channels
         *
         * @param count Number of input channels
         */
        void setInputChannelCount(int count) { m_inputChannelCount = count; }

        /**
         * @brief Get the number of output channels
         *
         * @return int Number of output channels
         */
        int getOutputChannelCount() const { return m_outputChannelCount; }

        /**
         * @brief Set the number of output channels
         *
         * @param count Number of output channels
         */
        void setOutputChannelCount(int count) { m_outputChannelCount = count; }

        /**
         * @brief Set a property value
         *
         * @param key Property name
         * @param value Property value
         */
        void setProperty(const std::string &key, const std::string &value);

        /**
         * @brief Get a property value
         *
         * @param key Property name
         * @param defaultValue Default value if property doesn't exist
         * @return std::string Property value
         */
        std::string getProperty(const std::string &key, const std::string &defaultValue = "") const;

        /**
         * @brief Check if device has encountered an error
         *
         * @return true if in error state
         */
        bool hasError() const { return m_hasError; }

        /**
         * @brief Set error state and message
         *
         * @param hasError Error state
         * @param errorMessage Error message
         */
        void setError(bool hasError, const std::string &errorMessage = "");

        /**
         * @brief Get error message
         *
         * @return std::string Current error message
         */
        std::string getErrorMessage() const { return m_errorMessage; }

        /**
         * @brief Check if a parameter exists
         *
         * @param path Parameter path
         * @return true if parameter exists
         */
        bool hasParameter(const std::string &path) const;

        /**
         * @brief Remove a parameter
         *
         * @param path Parameter path
         * @return true if parameter was removed
         */
        bool removeParameter(const std::string &path);

        /**
         * @brief Get all parameter paths
         *
         * @return std::vector<std::string> List of parameter paths
         */
        std::vector<std::string> getParameterPaths() const;

        /**
         * @brief Add a parameter observer
         *
         * @param path Parameter path to observe
         * @param callback Callback function receiving path and value
         * @return int Observer ID used to remove the observer
         */
        int addObserver(const std::string &path,
                        std::function<void(const std::string &, const std::any &)> callback);

        /**
         * @brief Remove a parameter observer
         *
         * @param observerId Observer ID to remove
         * @return true if observer was removed
         */
        bool removeObserver(int observerId);

    private:
        struct ObserverInfo
        {
            std::string path;
            std::function<void(const std::string &, const std::any &)> callback;
        };

        std::string m_deviceId;
        std::string m_deviceName;
        std::atomic<bool> m_active;
        double m_sampleRate;
        int m_bufferSize;
        int m_inputChannelCount;
        int m_outputChannelCount;

        mutable std::mutex m_propertiesMutex;
        std::map<std::string, std::string> m_properties;

        std::atomic<bool> m_hasError;
        std::string m_errorMessage;

        mutable std::mutex m_mutex;
        std::map<std::string, std::any> m_parameters;
        std::map<int, ObserverInfo> m_observers;
        int m_nextObserverId;
    };

} // namespace AudioEngine
