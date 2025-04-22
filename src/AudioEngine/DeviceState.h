#pragma once

#include <Configuration.h>

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <functional>
#include <any>
#include <mutex>

namespace AudioEngine
{

    /**
     * @brief Represents the state of an audio device
     *
     * Stores device capabilities, current settings, and operational state.
     * DeviceState is the central representation of a device's current status,
     * serving as the "single source of truth" about device state.
     */
    class DeviceState
    {
    public:
        /**
         * @brief Device type enumeration
         */
        enum class DeviceType
        {
            Unknown,
            ASIO,
            CoreAudio,
            WASAPI,
            ALSA,
            JACK
        };

        /**
         * @brief Device status enumeration
         */
        enum class Status
        {
            Disconnected,
            Connected,
            Initialized,
            Running,
            Error
        };

        /**
         * @brief Create a new device state
         *
         * @param name The device name
         * @param type The device type
         */
        DeviceState(const std::string &name, DeviceType type = DeviceType::Unknown);

        /**
         * @brief Destroy the device state
         */
        ~DeviceState();

        /**
         * @brief Get the device name
         */
        const std::string &getName() const { return m_name; }

        /**
         * @brief Get the device type
         */
        DeviceType getType() const { return m_type; }

        /**
         * @brief Set the device type
         */
        void setType(DeviceType type) { m_type = type; }

        /**
         * @brief Get the device status
         */
        Status getStatus() const { return m_status; }

        /**
         * @brief Set the device status
         */
        void setStatus(Status status) { m_status = status; }

        /**
         * @brief Get the number of input channels
         */
        int getInputChannelCount() const { return m_inputChannelCount; }

        /**
         * @brief Set the number of input channels
         */
        void setInputChannelCount(int count) { m_inputChannelCount = count; }

        /**
         * @brief Get the number of output channels
         */
        int getOutputChannelCount() const { return m_outputChannelCount; }

        /**
         * @brief Set the number of output channels
         */
        void setOutputChannelCount(int count) { m_outputChannelCount = count; }

        /**
         * @brief Get the sample rate
         */
        double getSampleRate() const { return m_sampleRate; }

        /**
         * @brief Set the sample rate
         */
        void setSampleRate(double rate) { m_sampleRate = rate; }

        /**
         * @brief Get the buffer size
         */
        int getBufferSize() const { return m_bufferSize; }

        /**
         * @brief Set the buffer size
         */
        void setBufferSize(int size) { m_bufferSize = size; }

        /**
         * @brief Check if the device is active
         */
        bool isActive() const { return m_status == Status::Running; }

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
         * @return The property value or defaultValue
         */
        std::string getProperty(const std::string &key, const std::string &defaultValue = "") const;

        /**
         * @brief Check if a property exists
         *
         * @param key Property name to check
         * @return true if property exists
         */
        bool hasProperty(const std::string &key) const;

        /**
         * @brief Set a typed parameter value
         *
         * @param key Parameter name
         * @param value Parameter value
         * @param paramType Parameter type identifier
         */
        void setParameter(const std::string &key, const std::any &value, const std::string &paramType = "");

        /**
         * @brief Get a typed parameter value
         *
         * @param key Parameter name
         * @param defaultValue Default value if parameter doesn't exist
         * @return The parameter value or defaultValue
         */
        std::any getParameter(const std::string &key, const std::any &defaultValue = std::any()) const;

        /**
         * @brief Check if a parameter exists
         *
         * @param key Parameter name to check
         * @return true if parameter exists
         */
        bool hasParameter(const std::string &key) const;

        /**
         * @brief Get the type of a parameter
         *
         * @param key Parameter name
         * @return The parameter type or empty string if not found
         */
        std::string getParameterType(const std::string &key) const;

        /**
         * @brief Add a callback for state changes
         *
         * @param callback Function to call when state changes
         * @return Callback ID for later removal
         */
        int addStateChangedCallback(std::function<void(const std::string &param, const std::any &value)> callback);

        /**
         * @brief Remove a state change callback
         *
         * @param callbackId ID returned from addStateChangedCallback
         * @return true if callback was found and removed
         */
        bool removeStateChangedCallback(int callbackId);

        /**
         * @brief Compare current state with another state
         *
         * @param other DeviceState to compare against
         * @return Map of differences (parameter key to value in other)
         */
        std::map<std::string, std::any> diffFrom(const DeviceState &other) const;

        /**
         * @brief Check if this state is compatible with another
         *
         * @param other DeviceState to check compatibility with
         * @return true if compatible (can be merged/updated)
         */
        bool isCompatibleWith(const DeviceState &other) const;

        /**
         * @brief Update from a Configuration object
         *
         * @param config Configuration to apply
         */
        void updateFromConfiguration(const Configuration &config);

        /**
         * @brief Convert to a Configuration object
         *
         * @return Configuration representing this state
         */
        Configuration toConfiguration() const;

        /**
         * @brief Serialize to JSON
         *
         * @return JSON string representation
         */
        std::string toJson() const;

        /**
         * @brief Create from JSON string
         *
         * @param json JSON string to parse
         * @return DeviceState object
         */
        static DeviceState fromJson(const std::string &json);

        /**
         * @brief Perform a health check
         *
         * @return true if device is in a healthy state
         */
        bool isHealthy() const;

        /**
         * @brief Attempt recovery from error state
         *
         * @return true if recovery was successful
         */
        bool attemptRecovery();

    private:
        void notifyStateChanged(const std::string &param, const std::any &value);

        std::string m_name;
        DeviceType m_type;
        Status m_status = Status::Disconnected;

        int m_inputChannelCount = 0;
        int m_outputChannelCount = 0;
        double m_sampleRate = 44100.0;
        int m_bufferSize = 512;

        std::map<std::string, std::string> m_properties;
        std::map<std::string, std::any> m_parameters;
        std::map<std::string, std::string> m_parameterTypes;

        std::map<int, std::function<void(const std::string &param, const std::any &value)>> m_callbacks;
        int m_nextCallbackId = 0;
        mutable std::mutex m_callbackMutex;
    };

} // namespace AudioEngine
