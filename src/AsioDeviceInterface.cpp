#include "AsioDeviceInterface.h"
#include "AsioManager.h"

#include <iostream>
#include <future>
#include <thread>

namespace AudioEngine
{
    AsioDeviceInterface::AsioDeviceInterface(AsioManager *asioManager)
        : m_asioManager(asioManager)
    {
        if (!m_asioManager)
        {
            throw std::invalid_argument("AsioManager cannot be null");
        }
    }

    bool AsioDeviceInterface::queryState(const std::string &deviceName,
                                         std::function<void(bool, const DeviceState &)> callback)
    {
        if (!m_asioManager)
        {
            DeviceState emptyState(deviceName, DeviceState::DeviceType::ASIO);
            callback(false, emptyState);
            return false;
        }

        // Create a device state to populate
        DeviceState state(deviceName, DeviceState::DeviceType::ASIO);

        // Check if we need to load the driver
        bool needToLoadDriver = !m_asioManager->isInitialized() ||
                                m_asioManager->getDriverName() != deviceName;

        // Use a lambda to handle the asynchronous query
        std::thread([this, deviceName, state, callback, needToLoadDriver]() mutable
                    {
            bool success = true;

            try {
                // Load driver if needed
                if (needToLoadDriver) {
                    if (!m_asioManager->loadDriver(deviceName)) {
                        std::cerr << "Failed to load ASIO driver: " << deviceName << std::endl;
                        callback(false, state);
                        return;
                    }
                }

                // Initialize device if not already initialized
                if (!m_asioManager->isInitialized()) {
                    if (!m_asioManager->initDevice()) {
                        std::cerr << "Failed to initialize ASIO device: " << deviceName << std::endl;
                        callback(false, state);
                        return;
                    }
                }

                // Set device state properties from ASIO manager
                state.setStatus(DeviceState::Status::Connected);
                state.setInputChannelCount(m_asioManager->getInputChannelCount());
                state.setOutputChannelCount(m_asioManager->getOutputChannelCount());
                state.setSampleRate(m_asioManager->getCurrentSampleRate());
                state.setBufferSize(m_asioManager->getBufferSize());

                // Get channel names
                std::vector<std::string> inputNames = m_asioManager->getInputChannelNames();
                std::vector<std::string> outputNames = m_asioManager->getOutputChannelNames();

                // Store channel names as properties
                for (size_t i = 0; i < inputNames.size(); ++i) {
                    state.setProperty("inputChannel_" + std::to_string(i) + "_name", inputNames[i]);
                }

                for (size_t i = 0; i < outputNames.size(); ++i) {
                    state.setProperty("outputChannel_" + std::to_string(i) + "_name", outputNames[i]);
                }

                // Get supported sample rates
                std::vector<double> supportedRates = m_asioManager->getSupportedSampleRates();
                std::string ratesStr;
                for (size_t i = 0; i < supportedRates.size(); ++i) {
                    if (i > 0) ratesStr += ",";
                    ratesStr += std::to_string(supportedRates[i]);
                }
                state.setProperty("supportedSampleRates", ratesStr);

                // Get driver info
                state.setProperty("driverVersion", m_asioManager->getDriverVersion());
                state.setProperty("asioVersion", m_asioManager->getAsioVersion());

                // Additional device-specific properties could be queried here
            }
            catch (const std::exception& e) {
                std::cerr << "Error querying ASIO device state: " << e.what() << std::endl;
                success = false;
            }

            // Call the callback with the results
            callback(success, state); })
            .detach();

        return true;
    }

    bool AsioDeviceInterface::applyConfiguration(const std::string &deviceName,
                                                 const Configuration &config,
                                                 std::function<void(bool, const std::string &)> callback)
    {
        if (!m_asioManager)
        {
            callback(false, "AsioManager is null");
            return false;
        }

        // Use a lambda to handle the asynchronous configuration
        std::thread([this, deviceName, config, callback]()
                    {
            std::string errorMessage;
            bool success = true;

            try {
                // Check if we need to load the driver
                bool needToLoadDriver = !m_asioManager->isInitialized() ||
                                     m_asioManager->getDriverName() != deviceName;

                if (needToLoadDriver) {
                    if (!m_asioManager->loadDriver(deviceName)) {
                        errorMessage = "Failed to load ASIO driver: " + deviceName;
                        callback(false, errorMessage);
                        return;
                    }
                }

                // Apply sample rate if specified
                double sampleRate = config.getSampleRate();
                if (sampleRate > 0) {
                    if (!m_asioManager->setSampleRate(sampleRate)) {
                        errorMessage += "Failed to set sample rate; ";
                        success = false;
                    }
                }

                // Apply buffer size if specified
                long bufferSize = config.getBufferSize();
                if (bufferSize > 0) {
                    if (!m_asioManager->setBufferSize(bufferSize)) {
                        errorMessage += "Failed to set buffer size; ";
                        success = false;
                    }
                }

                // Find all nodes with channel specifications
                std::vector<long> inputChannels;
                std::vector<long> outputChannels;

                // Extract channel indices from node configurations
                for (const auto& node : config.getNodes()) {
                    if (node.type == "asio_source" && !node.channelIndices.empty()) {
                        // Add input channels
                        inputChannels.insert(inputChannels.end(),
                                          node.channelIndices.begin(),
                                          node.channelIndices.end());
                    }
                    else if (node.type == "asio_sink" && !node.channelIndices.empty()) {
                        // Add output channels
                        outputChannels.insert(outputChannels.end(),
                                           node.channelIndices.begin(),
                                           node.channelIndices.end());
                    }
                }

                // Create and prepare ASIO buffers
                if (!inputChannels.empty() || !outputChannels.empty()) {
                    if (!m_asioManager->createBuffers(inputChannels, outputChannels)) {
                        errorMessage += "Failed to create ASIO buffers; ";
                        success = false;
                    }
                }

                // Additional device-specific configuration could be applied here
            }
            catch (const std::exception& e) {
                errorMessage = "Error applying ASIO configuration: ";
                errorMessage += e.what();
                success = false;
            }

            // Call the callback with the results
            callback(success, errorMessage); })
            .detach();

        return true;
    }

} // namespace AudioEngine
