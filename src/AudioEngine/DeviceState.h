#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>

namespace AudioEngine
{

    /**
     * @brief Represents the state of an audio device
     *
     * Stores device capabilities, current settings, and operational state
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

    private:
        std::string m_name;
        DeviceType m_type;
        Status m_status = Status::Disconnected;

        int m_inputChannelCount = 0;
        int m_outputChannelCount = 0;
        double m_sampleRate = 44100.0;
        int m_bufferSize = 512;

        std::map<std::string, std::string> m_properties;
    };

} // namespace AudioEngine
