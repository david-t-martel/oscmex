#include "DeviceStateInterface.h"
#include "DeviceState.h"
#include "Configuration.h"

namespace AudioEngine
{
    Configuration DeviceStateInterface::stateToConfiguration(const DeviceState &state)
    {
        // Create a new configuration object
        Configuration config;

        // Set basic device properties
        std::string deviceName = state.getName();
        config.setAsioDeviceName(deviceName);

        // Map DeviceState::DeviceType to Configuration::DeviceType
        DeviceType configDeviceType;
        switch (state.getType())
        {
        case DeviceState::DeviceType::ASIO:
            configDeviceType = DeviceType::ASIO;
            break;
        case DeviceState::DeviceType::CoreAudio:
        case DeviceState::DeviceType::WASAPI:
        case DeviceState::DeviceType::ALSA:
        case DeviceState::DeviceType::JACK:
            // Map to most appropriate type
            configDeviceType = DeviceType::GENERIC_OSC;
            break;
        default:
            configDeviceType = DeviceType::GENERIC_OSC;
            break;
        }
        config.setDeviceType(configDeviceType);

        // Set audio configuration parameters
        config.setSampleRate(state.getSampleRate());
        config.setBufferSize(state.getBufferSize());

        // Add I/O channel counts
        // (These may not have direct equivalents in Configuration, so we might use properties)

        // Configure target IP/port if available in device properties
        std::string targetIp = state.getProperty("target_ip", "127.0.0.1");
        std::string targetPortStr = state.getProperty("target_port", "9000");
        std::string receivePortStr = state.getProperty("receive_port", "8000");

        try
        {
            int targetPort = std::stoi(targetPortStr);
            int receivePort = std::stoi(receivePortStr);
            config.setConnectionParams(targetIp, targetPort, receivePort);
        }
        catch (const std::exception &e)
        {
            // Use defaults if parsing fails
            config.setConnectionParams(targetIp, 9000, 8000);
        }

        // Add all parameters as commands
        // This will need to be customized based on the format of your parameters

        return config;
    }
} // namespace AudioEngine
