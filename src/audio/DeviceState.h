#pragma once

#include <string>
#include <vector>

namespace oscmex
{
    namespace audio
    {

        /**
         * @brief Represents the state of an audio device
         *
         * Contains information about an audio device including its name,
         * ID, sample rates, channel counts, and current configuration.
         */
        class DeviceState
        {
        public:
            DeviceState() = default;
            DeviceState(int deviceId, const std::string &deviceName);
            ~DeviceState() = default;

            // Getters
            int getDeviceId() const { return m_deviceId; }
            const std::string &getDeviceName() const { return m_deviceName; }
            const std::vector<double> &getSupportedSampleRates() const { return m_supportedSampleRates; }
            const std::vector<int> &getSupportedInputChannels() const { return m_supportedInputChannels; }
            const std::vector<int> &getSupportedOutputChannels() const { return m_supportedOutputChannels; }

            double getCurrentSampleRate() const { return m_currentSampleRate; }
            int getCurrentInputChannels() const { return m_currentInputChannels; }
            int getCurrentOutputChannels() const { return m_currentOutputChannels; }
            bool isActive() const { return m_isActive; }

            // Setters
            void setSupportedSampleRates(const std::vector<double> &rates) { m_supportedSampleRates = rates; }
            void setSupportedInputChannels(const std::vector<int> &channels) { m_supportedInputChannels = channels; }
            void setSupportedOutputChannels(const std::vector<int> &channels) { m_supportedOutputChannels = channels; }

            void setCurrentSampleRate(double rate) { m_currentSampleRate = rate; }
            void setCurrentInputChannels(int channels) { m_currentInputChannels = channels; }
            void setCurrentOutputChannels(int channels) { m_currentOutputChannels = channels; }
            void setActive(bool active) { m_isActive = active; }

        private:
            int m_deviceId = -1;
            std::string m_deviceName;
            std::vector<double> m_supportedSampleRates;
            std::vector<int> m_supportedInputChannels;
            std::vector<int> m_supportedOutputChannels;

            double m_currentSampleRate = 44100.0;
            int m_currentInputChannels = 0;
            int m_currentOutputChannels = 0;
            bool m_isActive = false;
        };

    } // namespace audio
} // namespace oscmex
