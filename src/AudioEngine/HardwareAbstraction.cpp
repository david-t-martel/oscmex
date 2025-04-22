#include "HardwareAbstraction.h"
#include "AsioDeviceInterface.h"
#include "OscDeviceInterface.h"
#include "AsioManager.h"
#include "OscController.h"

#include <iostream>
#include <regex>

namespace AudioEngine
{
    std::unique_ptr<DeviceStateInterface> HardwareAbstractionFactory::createInterface(
        const std::string &deviceType,
        AsioManager *asioManager,
        OscController *oscController)
    {
        try
        {
            // Convert string device type to enum
            DeviceType type = stringToDeviceType(deviceType);

            // Create appropriate interface based on device type
            switch (type)
            {
            case DeviceType::ASIO:
                if (asioManager)
                {
                    return std::make_unique<AsioDeviceInterface>(asioManager);
                }
                else
                {
                    std::cerr << "Cannot create ASIO device interface: AsioManager is null" << std::endl;
                }
                break;

            case DeviceType::GENERIC_OSC:
                if (oscController)
                {
                    return std::make_unique<OscDeviceInterface>(oscController);
                }
                else
                {
                    std::cerr << "Cannot create OSC device interface: OscController is null" << std::endl;
                }
                break;

            case DeviceType::RME_TOTALMIX:
                if (oscController)
                {
                    // For RME_TOTALMIX devices, we prefer the OSC interface
                    // In a more complete implementation, this could create a specialized RME interface
                    return std::make_unique<OscDeviceInterface>(oscController);
                }
                else
                {
                    std::cerr << "Cannot create RME device interface: OscController is null" << std::endl;
                }
                break;

            default:
                std::cerr << "Unknown device type: " << deviceType << std::endl;
                break;
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error creating device interface: " << e.what() << std::endl;
        }

        return nullptr;
    }

    std::vector<std::string> HardwareCapabilities::getAsioDevices()
    {
        return AsioManager::getDeviceList();
    }

    std::map<std::string, std::any> HardwareCapabilities::getDeviceCapabilities(const std::string &deviceName)
    {
        std::map<std::string, std::any> capabilities;

        // Create temporary managers to query capabilities
        AsioManager asioManager;

        // Check if this is an ASIO device
        bool isAsioDevice = false;
        auto asioDevices = getAsioDevices();
        for (const auto &device : asioDevices)
        {
            if (device == deviceName)
            {
                isAsioDevice = true;
                break;
            }
        }

        if (isAsioDevice)
        {
            // Add ASIO capability
            capabilities["asio"] = true;

            // Try to load the driver to get more detailed capabilities
            if (asioManager.loadDriver(deviceName))
            {
                if (asioManager.initDevice())
                {
                    // Get device capabilities
                    capabilities["inputChannels"] = asioManager.getInputChannelCount();
                    capabilities["outputChannels"] = asioManager.getOutputChannelCount();
                    capabilities["currentSampleRate"] = asioManager.getCurrentSampleRate();
                    capabilities["bufferSize"] = asioManager.getBufferSize();

                    // Get supported sample rates
                    std::vector<double> supportedRates = asioManager.getSupportedSampleRates();
                    capabilities["supportedSampleRates"] = supportedRates;
                }
            }
        }

        // Check if this might be an RME device (based on name)
        // This is a simplistic approach - a more robust implementation would
        // use actual device detection
        std::regex rmePattern("(?i)RME|Fireface|Babyface|UFX|Digiface|MADIface");
        bool isRmeDevice = std::regex_search(deviceName, rmePattern);

        if (isRmeDevice)
        {
            capabilities["rme"] = true;
            capabilities["oscSupport"] = true;
        }

        return capabilities;
    }

    bool HardwareCapabilities::hasCapability(const std::string &deviceName, const std::string &capability)
    {
        auto capabilities = getDeviceCapabilities(deviceName);
        return capabilities.find(capability) != capabilities.end();
    }

    Configuration HardwareCapabilities::getOptimalConfiguration(const std::string &deviceName)
    {
        Configuration config;

        // Set device name
        config.setAsioDeviceName(deviceName);

        // Get device capabilities
        auto capabilities = getDeviceCapabilities(deviceName);

        // Determine if this is an RME device
        bool isRmeDevice = false;
        auto it = capabilities.find("rme");
        if (it != capabilities.end())
        {
            isRmeDevice = std::any_cast<bool>(it->second);
        }

        // Set appropriate device type
        if (isRmeDevice)
        {
            config.setDeviceType(DeviceType::RME_TOTALMIX);

            // Set default OSC connection parameters for RME
            config.setConnectionParams("127.0.0.1", 9001, 9002);
        }
        else
        {
            // Check if this is an ASIO device
            bool isAsioDevice = false;
            it = capabilities.find("asio");
            if (it != capabilities.end())
            {
                isAsioDevice = std::any_cast<bool>(it->second);
            }

            if (isAsioDevice)
            {
                config.setDeviceType(DeviceType::ASIO);
            }
            else
            {
                config.setDeviceType(DeviceType::GENERIC_OSC);
            }
        }

        // Set optimal sample rate and buffer size from capabilities
        it = capabilities.find("currentSampleRate");
        if (it != capabilities.end())
        {
            config.setSampleRate(std::any_cast<double>(it->second));
        }
        else
        {
            config.setSampleRate(48000.0); // Default to 48kHz if unknown
        }

        it = capabilities.find("bufferSize");
        if (it != capabilities.end())
        {
            config.setBufferSize(std::any_cast<long>(it->second));
        }
        else
        {
            config.setBufferSize(512); // Default to 512 samples if unknown
        }

        // Create default processing graph based on channel counts
        int inputChannels = 2; // Default to stereo
        int outputChannels = 2;

        it = capabilities.find("inputChannels");
        if (it != capabilities.end())
        {
            inputChannels = std::any_cast<int>(it->second);
            inputChannels = std::min(inputChannels, 8); // Limit to 8 channels for default graph
        }

        it = capabilities.find("outputChannels");
        if (it != capabilities.end())
        {
            outputChannels = std::any_cast<int>(it->second);
            outputChannels = std::min(outputChannels, 8); // Limit to 8 channels for default graph
        }

        config.createDefaultGraph(inputChannels, outputChannels, true);

        return config;
    }

} // namespace AudioEngine
