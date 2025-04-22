#include "AsioManager.h"
#include <iostream>
#include <string>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <cstring>

// Include the ASIO SDK headers
#include "asiosys.h"
#include "asio.h"
#include "asiodrivers.h"

namespace AudioEngine
{

    // Static instance for callback forwarding
    AsioManager *AsioManager::s_instance = nullptr;

    // Add this definition for the standard sample rates to check
    const std::vector<double> AsioManager::m_standardSampleRates = {
        44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0, 352800.0, 384000.0};

    AsioManager::AsioManager()
        : m_driverLoaded(false),
          m_asioInitialized(false),
          m_buffersCreated(false),
          m_processing(false),
          m_inputChannels(0),
          m_outputChannels(0),
          m_bufferSize(0),
          m_sampleRate(0.0),
          m_sampleType(0),
          m_inputLatency(0),
          m_outputLatency(0),
          m_numActiveChannels(0),
          m_asioDrivers(nullptr)
    {
        // Set the static instance for ASIO callbacks
        s_instance = this;

        // Create the AsioDrivers object
        m_asioDrivers = new AsioDrivers();
    }

    AsioManager::~AsioManager()
    {
        // Make sure we clean up ASIO
        cleanup();

        // Clean up AsioDrivers
        if (m_asioDrivers)
        {
            delete static_cast<AsioDrivers *>(m_asioDrivers);
            m_asioDrivers = nullptr;
        }

        // Clear the static instance
        if (s_instance == this)
        {
            s_instance = nullptr;
        }
    }

    bool AsioManager::loadDriver(const std::string &deviceName)
    {
        if (m_driverLoaded)
        {
            std::cout << "ASIO driver already loaded, cleaning up first" << std::endl;
            cleanup();
        }

        // Get the AsioDrivers instance
        AsioDrivers *drivers = static_cast<AsioDrivers *>(m_asioDrivers);

        // Load the driver by name
        if (!drivers->loadDriver(const_cast<char *>(deviceName.c_str())))
        {
            std::cerr << "Failed to load ASIO driver: " << deviceName << std::endl;
            return false;
        }

        m_driverLoaded = true;
        std::cout << "ASIO driver loaded: " << deviceName << std::endl;
        return true;
    }

    bool AsioManager::initDevice(double preferredSampleRate, long preferredBufferSize)
    {
        if (!m_driverLoaded)
        {
            LogError("ASIO driver not loaded");
            return false;
        }

        if (m_asioInitialized)
        {
            // Already initialized
            return true;
        }

        // Initialize ASIO
        if (ASIOInit(nullptr) != ASE_OK)
        {
            LogError("Failed to initialize ASIO");
            return false;
        }

        m_asioInitialized = true;

        // Get channel counts
        ASIOError result = ASIOGetChannels(&m_inputChannels, &m_outputChannels);
        if (result != ASE_OK)
        {
            LogError("Failed to get channel counts");
            cleanup();
            return false;
        }

        LogInfo(fmt::format("ASIO channels: {} inputs, {} outputs", m_inputChannels, m_outputChannels));

        // Get buffer size options
        result = ASIOGetBufferSize(&m_minBufferSize, &m_maxBufferSize, &m_preferredBufferSize, &m_bufferSizeGranularity);
        if (result != ASE_OK)
        {
            LogError("Failed to get buffer size information");
            cleanup();
            return false;
        }

        LogInfo(fmt::format("ASIO buffer size: min={}, max={}, preferred={}, granularity={}",
                            m_minBufferSize, m_maxBufferSize, m_preferredBufferSize, m_bufferSizeGranularity));

        // Get the current sample rate
        result = asioGetSampleRate(&m_sampleRate);
        if (result != ASE_OK)
        {
            LogError("Failed to get sample rate");
            cleanup();
            return false;
        }

        LogInfo(fmt::format("Current ASIO sample rate: {:.1f} Hz", m_sampleRate));

        // If a preferred sample rate is provided and different from current, try to set it
        if (preferredSampleRate > 0 && std::abs(preferredSampleRate - m_sampleRate) > 0.01)
        {
            // Check if the preferred sample rate is supported
            if (asioCanSampleRate(preferredSampleRate) == ASE_OK)
            {
                LogInfo(fmt::format("Setting sample rate to {:.1f} Hz", preferredSampleRate));

                // Set the sample rate
                result = asioSetSampleRate(preferredSampleRate);
                if (result != ASE_OK)
                {
                    LogWarning(fmt::format("Failed to set sample rate to {:.1f} Hz, using device default",
                                           preferredSampleRate));
                }
                else
                {
                    // Update the sample rate
                    m_sampleRate = preferredSampleRate;
                }
            }
            else
            {
                LogWarning(fmt::format("Sample rate {:.1f} Hz not supported, using device default",
                                       preferredSampleRate));
            }
        }

        // Get the latencies
        result = asioGetLatencies(&m_inputLatency, &m_outputLatency);
        if (result != ASE_OK)
        {
            LogError("Failed to get latencies");
            cleanup();
            return false;
        }

        LogInfo(fmt::format("ASIO latencies: input={} samples, output={} samples",
                            m_inputLatency, m_outputLatency));

        // Store buffer size to use
        if (preferredBufferSize > 0)
        {
            // Check if the preferred buffer size is within the valid range
            if (preferredBufferSize >= m_minBufferSize && preferredBufferSize <= m_maxBufferSize)
            {
                m_bufferSize = preferredBufferSize;
                LogInfo(fmt::format("Using preferred buffer size: {}", m_bufferSize));
            }
            else
            {
                LogWarning(fmt::format("Preferred buffer size {} out of range [{}, {}], using device preferred size: {}",
                                       preferredBufferSize, m_minBufferSize, m_maxBufferSize, m_preferredBufferSize));
                m_bufferSize = m_preferredBufferSize;
            }
        }
        else
        {
            // Use the device's preferred buffer size
            m_bufferSize = m_preferredBufferSize;
            LogInfo(fmt::format("Using device preferred buffer size: {}", m_bufferSize));
        }

        // Query and store channel names for all available channels
        m_inputChannelNames.clear();
        m_outputChannelNames.clear();

        // Get input channel names
        for (long i = 0; i < m_inputChannels; i++)
        {
            ASIOChannelInfo channelInfo;
            channelInfo.channel = i;
            channelInfo.isInput = ASIOTrue;

            if (asioGetChannelInfo(&channelInfo) == ASE_OK)
            {
                std::string name(channelInfo.name);
                m_inputChannelNames.push_back(name);
                LogInfo(fmt::format("Input channel {}: {}", i, name));
            }
            else
            {
                m_inputChannelNames.push_back(fmt::format("Input {}", i));
            }
        }

        // Get output channel names
        for (long i = 0; i < m_outputChannels; i++)
        {
            ASIOChannelInfo channelInfo;
            channelInfo.channel = i;
            channelInfo.isInput = ASIOFalse;

            if (asioGetChannelInfo(&channelInfo) == ASE_OK)
            {
                std::string name(channelInfo.name);
                m_outputChannelNames.push_back(name);
                LogInfo(fmt::format("Output channel {}: {}", i, name));
            }
            else
            {
                m_outputChannelNames.push_back(fmt::format("Output {}", i));
            }
        }

        return true;
    }

    bool AsioManager::createBuffers(const std::vector<long> &inputChannels, const std::vector<long> &outputChannels)
    {
        if (!m_asioInitialized)
        {
            std::cerr << "ASIO not initialized" << std::endl;
            return false;
        }

        if (m_buffersCreated)
        {
            std::cout << "ASIO buffers already created, disposing first" << std::endl;
            if (ASIODisposeBuffers() != ASE_OK)
            {
                std::cerr << "Failed to dispose buffers" << std::endl;
                return false;
            }
            m_buffersCreated = false;
        }

        // Validate input channels
        for (const auto &chan : inputChannels)
        {
            if (chan < 0 || chan >= m_inputChannels)
            {
                std::cerr << "Invalid input channel index: " << chan << std::endl;
                return false;
            }
        }

        // Validate output channels
        for (const auto &chan : outputChannels)
        {
            if (chan < 0 || chan >= m_outputChannels)
            {
                std::cerr << "Invalid output channel index: " << chan << std::endl;
                return false;
            }
        }

        // Store active channel indices
        m_activeInputIndices = inputChannels;
        m_activeOutputIndices = outputChannels;
        m_numActiveChannels = static_cast<long>(inputChannels.size() + outputChannels.size());

        if (m_numActiveChannels == 0)
        {
            std::cerr << "No channels selected for activation" << std::endl;
            return false;
        }

        // Create ASIOBufferInfo array
        std::vector<ASIOBufferInfo> bufferInfos(m_numActiveChannels);

        // Set up input buffers
        for (size_t i = 0; i < inputChannels.size(); i++)
        {
            bufferInfos[i].isInput = ASIOTrue;
            bufferInfos[i].channelNum = inputChannels[i];
            bufferInfos[i].buffers[0] = nullptr;
            bufferInfos[i].buffers[1] = nullptr;
        }

        // Set up output buffers
        for (size_t i = 0; i < outputChannels.size(); i++)
        {
            bufferInfos[inputChannels.size() + i].isInput = ASIOFalse;
            bufferInfos[inputChannels.size() + i].channelNum = outputChannels[i];
            bufferInfos[inputChannels.size() + i].buffers[0] = nullptr;
            bufferInfos[inputChannels.size() + i].buffers[1] = nullptr;
        }

        // Set up callbacks
        ASIOCallbacks callbacks;
        callbacks.bufferSwitch = &AsioManager::bufferSwitchCallback;
        callbacks.sampleRateDidChange = &AsioManager::sampleRateDidChangeCallback;
        callbacks.asioMessage = &AsioManager::asioMessageCallback;
        callbacks.bufferSwitchTimeInfo = &AsioManager::bufferSwitchTimeInfoCallback;

        // Create buffers
        if (ASIOCreateBuffers(bufferInfos.data(), m_numActiveChannels, m_bufferSize, &callbacks) != ASE_OK)
        {
            std::cerr << "Failed to create ASIO buffers" << std::endl;
            return false;
        }

        m_buffersCreated = true;

        // Store buffer infos (need to keep these around)
        m_bufferInfos.resize(m_numActiveChannels);
        for (long i = 0; i < m_numActiveChannels; i++)
        {
            m_bufferInfos[i] = &bufferInfos[i];
        }

        // Get the sample type of the first channel
        ASIOChannelInfo channelInfo;
        channelInfo.channel = bufferInfos[0].channelNum;
        channelInfo.isInput = bufferInfos[0].isInput;
        if (ASIOGetChannelInfo(&channelInfo) != ASE_OK)
        {
            std::cerr << "Failed to get channel info" << std::endl;
            ASIODisposeBuffers();
            m_buffersCreated = false;
            return false;
        }

        m_sampleType = channelInfo.type;
        std::cout << "ASIO sample type: " << getSampleTypeName() << std::endl;

        std::cout << "ASIO buffers created for " << inputChannels.size() << " input and "
                  << outputChannels.size() << " output channels" << std::endl;

        return true;
    }

    bool AsioManager::start()
    {
        if (!m_buffersCreated)
        {
            std::cerr << "ASIO buffers not created" << std::endl;
            return false;
        }

        if (m_processing)
        {
            std::cout << "ASIO already processing" << std::endl;
            return true;
        }

        if (ASIOStart() != ASE_OK)
        {
            std::cerr << "Failed to start ASIO" << std::endl;
            return false;
        }

        m_processing = true;
        std::cout << "ASIO processing started" << std::endl;
        return true;
    }

    void AsioManager::stop()
    {
        if (!m_processing)
        {
            return;
        }

        if (ASIOStop() != ASE_OK)
        {
            std::cerr << "Failed to stop ASIO" << std::endl;
        }

        m_processing = false;
        std::cout << "ASIO processing stopped" << std::endl;
    }

    void AsioManager::cleanup()
    {
        // Stop processing if needed
        if (m_processing)
        {
            stop();
        }

        // Dispose buffers if needed
        if (m_buffersCreated)
        {
            if (ASIODisposeBuffers() != ASE_OK)
            {
                std::cerr << "Failed to dispose ASIO buffers" << std::endl;
            }
            m_buffersCreated = false;
            m_bufferInfos.clear();
        }

        // Exit ASIO if needed
        if (m_asioInitialized)
        {
            if (ASIOExit() != ASE_OK)
            {
                std::cerr << "Failed to exit ASIO" << std::endl;
            }
            m_asioInitialized = false;
        }

        // Unload driver if needed
        if (m_driverLoaded)
        {
            AsioDrivers *drivers = static_cast<AsioDrivers *>(m_asioDrivers);
            drivers->removeCurrentDriver();
            m_driverLoaded = false;
        }

        // Reset variables
        m_inputChannels = 0;
        m_outputChannels = 0;
        m_bufferSize = 0;
        m_sampleRate = 0.0;
        m_sampleType = 0;
        m_inputLatency = 0;
        m_outputLatency = 0;
        m_activeInputIndices.clear();
        m_activeOutputIndices.clear();
        m_numActiveChannels = 0;
    }

    std::string AsioManager::getSampleTypeName() const
    {
        switch (m_sampleType)
        {
        case ASIOSTInt16MSB:
            return "Int16MSB";
        case ASIOSTInt24MSB:
            return "Int24MSB";
        case ASIOSTInt32MSB:
            return "Int32MSB";
        case ASIOSTFloat32MSB:
            return "Float32MSB";
        case ASIOSTFloat64MSB:
            return "Float64MSB";
        case ASIOSTInt32MSB16:
            return "Int32MSB16";
        case ASIOSTInt32MSB18:
            return "Int32MSB18";
        case ASIOSTInt32MSB20:
            return "Int32MSB20";
        case ASIOSTInt32MSB24:
            return "Int32MSB24";
        case ASIOSTInt16LSB:
            return "Int16LSB";
        case ASIOSTInt24LSB:
            return "Int24LSB";
        case ASIOSTInt32LSB:
            return "Int32LSB";
        case ASIOSTFloat32LSB:
            return "Float32LSB";
        case ASIOSTFloat64LSB:
            return "Float64LSB";
        case ASIOSTInt32LSB16:
            return "Int32LSB16";
        case ASIOSTInt32LSB18:
            return "Int32LSB18";
        case ASIOSTInt32LSB20:
            return "Int32LSB20";
        case ASIOSTInt32LSB24:
            return "Int32LSB24";
        default:
            return "Unknown";
        }
    }

    bool AsioManager::findChannelIndex(const std::string &channelName, bool isInput, long &index) const
    {
        if (!m_asioInitialized)
        {
            std::cerr << "ASIO not initialized" << std::endl;
            return false;
        }

        long numChannels = isInput ? m_inputChannels : m_outputChannels;

        for (long i = 0; i < numChannels; i++)
        {
            ASIOChannelInfo info;
            info.channel = i;
            info.isInput = isInput ? ASIOTrue : ASIOFalse;

            if (ASIOGetChannelInfo(&info) != ASE_OK)
            {
                std::cerr << "Failed to get channel info" << std::endl;
                continue;
            }

            std::string name(info.name);
            if (name == channelName)
            {
                index = i;
                return true;
            }
        }

        return false;
    }

    void AsioManager::setCallback(AsioCallback callback)
    {
        m_callback = callback;
    }

    bool AsioManager::getBufferPointers(long doubleBufferIndex, const std::vector<long> &activeIndices, std::vector<void *> &bufferPtrs)
    {
        if (!m_buffersCreated || !m_processing)
        {
            std::cerr << "ASIO buffers not created or not processing" << std::endl;
            return false;
        }

        if (doubleBufferIndex < 0 || doubleBufferIndex > 1)
        {
            std::cerr << "Invalid double buffer index: " << doubleBufferIndex << std::endl;
            return false;
        }

        // Resize the output vector
        bufferPtrs.resize(activeIndices.size());

        // Find the corresponding ASIOBufferInfo for each index
        for (size_t i = 0; i < activeIndices.size(); i++)
        {
            bool found = false;

            for (long j = 0; j < m_numActiveChannels; j++)
            {
                ASIOBufferInfo *info = static_cast<ASIOBufferInfo *>(m_bufferInfos[j]);

                if (info->channelNum == activeIndices[i])
                {
                    bufferPtrs[i] = info->buffers[doubleBufferIndex];
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                std::cerr << "Channel index not found: " << activeIndices[i] << std::endl;
                return false;
            }
        }

        return true;
    }

    // Static callbacks for ASIO SDK
    void AsioManager::bufferSwitchCallback(long doubleBufferIndex, bool directProcess)
    {
        if (s_instance)
        {
            s_instance->onBufferSwitch(doubleBufferIndex, directProcess);
        }
    }

    void AsioManager::sampleRateDidChangeCallback(double sRate)
    {
        if (s_instance)
        {
            s_instance->onSampleRateDidChange(sRate);
        }
    }

    long AsioManager::asioMessageCallback(long selector, long value, void *message, double *opt)
    {
        if (s_instance)
        {
            return s_instance->onAsioMessage(selector, value, message, opt);
        }
        return 0;
    }

    void *AsioManager::bufferSwitchTimeInfoCallback(void *params, long doubleBufferIndex, bool directProcess)
    {
        // We don't use time info in this implementation
        if (s_instance)
        {
            s_instance->onBufferSwitch(doubleBufferIndex, directProcess);
        }
        return nullptr;
    }

    // Instance callback handlers
    void AsioManager::onBufferSwitch(long doubleBufferIndex, bool directProcess)
    {
        if (m_callback)
        {
            m_callback(doubleBufferIndex);
        }
    }

    void AsioManager::onSampleRateDidChange(double sRate)
    {
        std::cout << "ASIO sample rate changed to " << sRate << " Hz" << std::endl;
        m_sampleRate = sRate;
    }

    long AsioManager::onAsioMessage(long selector, long value, void *message, double *opt)
    {
        switch (selector)
        {
        case kAsioSelectorSupported:
            if (value == kAsioResetRequest || value == kAsioEngineVersion ||
                value == kAsioResyncRequest || value == kAsioLatenciesChanged)
            {
                return 1;
            }
            return 0;

        case kAsioResetRequest:
            std::cout << "ASIO reset requested" << std::endl;
            return 1;

        case kAsioEngineVersion:
            return 2; // ASIO SDK version 2

        case kAsioResyncRequest:
            std::cout << "ASIO resync requested" << std::endl;
            return 1;

        case kAsioLatenciesChanged:
            std::cout << "ASIO latencies changed" << std::endl;
            ASIOGetLatencies(&m_inputLatency, &m_outputLatency);
            return 1;

        case kAsioBufferSizeChange:
            std::cout << "ASIO buffer size change requested" << std::endl;
            return 0; // Not supported

        case kAsioSupportsTimeInfo:
            return 0; // Not supported in this implementation

        case kAsioSupportsTimeCode:
            return 0; // Not supported

        default:
            return 0;
        }
    }

    // Implementation of device information methods

    std::vector<std::string> AsioManager::getDeviceList()
    {
        std::vector<std::string> deviceList;

        // Create a temporary AsioDrivers instance
        AsioDrivers drivers;

        // Get the driver names
        char nameBuffer[128];
        int driverCount = 0;

        for (int i = 0; i < 32; i++) // Limit to 32 drivers to avoid infinite loops
        {
            if (drivers.getDriverName(i, nameBuffer, 128))
            {
                deviceList.push_back(nameBuffer);
                driverCount++;
            }
            else
            {
                break; // No more drivers
            }
        }

        std::cout << "Found " << driverCount << " ASIO drivers" << std::endl;
        return deviceList;
    }

    std::string AsioManager::getDeviceInfo() const
    {
        if (!m_driverLoaded || !m_asioInitialized)
        {
            return "{}"; // Empty JSON object
        }

        // Use nlohmann::json to construct the device info
        nlohmann::json info;

        // Get driver name
        info["name"] = getDeviceName();

        // Channel info
        info["inputChannels"] = m_inputChannels;
        info["outputChannels"] = m_outputChannels;

        // Buffer sizes
        info["minBufferSize"] = m_minBufferSize;
        info["maxBufferSize"] = m_maxBufferSize;
        info["preferredBufferSize"] = m_preferredBufferSize;
        info["bufferSizeGranularity"] = m_bufferSizeGranularity;
        info["currentBufferSize"] = m_bufferSize;

        // Sample rates
        info["currentSampleRate"] = m_sampleRate;
        info["supportedSampleRates"] = getSupportedSampleRates();

        // Sample type
        info["sampleType"] = getSampleTypeName();

        // Latency
        info["inputLatency"] = m_inputLatency;
        info["outputLatency"] = m_outputLatency;

        // Channel names
        info["inputChannelNames"] = getInputChannelNames();
        info["outputChannelNames"] = getOutputChannelNames();

        return info.dump(2); // Pretty print with 2-space indent
    }

    std::string AsioManager::getDeviceName() const
    {
        if (!m_driverLoaded)
        {
            return "";
        }

        char name[128] = {0};
        ASIODriverInfo driverInfo;
        driverInfo.asioVersion = 2;
        driverInfo.driverVersion = 0;
        driverInfo.name[0] = '\0';
        driverInfo.errorMessage[0] = '\0';
        driverInfo.sysRef = nullptr;

        if (ASIOGetDriverName(name) != ASE_OK)
        {
            std::cerr << "Failed to get ASIO driver name" << std::endl;
            return "";
        }

        return std::string(name);
    }

    std::vector<std::string> AsioManager::getInputChannelNames() const
    {
        std::vector<std::string> names;

        if (!m_asioInitialized)
        {
            return names;
        }

        names.reserve(m_inputChannels);

        for (long i = 0; i < m_inputChannels; i++)
        {
            ASIOChannelInfo info;
            info.channel = i;
            info.isInput = ASIOTrue;

            if (ASIOGetChannelInfo(&info) != ASE_OK)
            {
                std::cerr << "Failed to get input channel info for channel " << i << std::endl;
                names.push_back("Input " + std::to_string(i));
            }
            else
            {
                names.push_back(info.name);
            }
        }

        return names;
    }

    std::vector<std::string> AsioManager::getOutputChannelNames() const
    {
        std::vector<std::string> names;

        if (!m_asioInitialized)
        {
            return names;
        }

        names.reserve(m_outputChannels);

        for (long i = 0; i < m_outputChannels; i++)
        {
            ASIOChannelInfo info;
            info.channel = i;
            info.isInput = ASIOFalse;

            if (ASIOGetChannelInfo(&info) != ASE_OK)
            {
                std::cerr << "Failed to get output channel info for channel " << i << std::endl;
                names.push_back("Output " + std::to_string(i));
            }
            else
            {
                names.push_back(info.name);
            }
        }

        return names;
    }

    std::vector<double> AsioManager::getSupportedSampleRates() const
    {
        std::vector<double> supportedRates;

        if (!m_driverLoaded || !m_asioInitialized)
        {
            return supportedRates;
        }

        // Check standard sample rates
        const std::vector<double> standardRates = {
            44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0};

        for (const auto &rate : standardRates)
        {
            if (canSampleRate(rate))
            {
                supportedRates.push_back(rate);
            }
        }

        return supportedRates;
    }

    bool AsioManager::canSampleRate(double sampleRate) const
    {
        if (!m_driverLoaded || !m_asioInitialized)
        {
            return false;
        }

        return asioCanSampleRate(sampleRate) == ASE_OK;
    }

    long AsioManager::getOptimalBufferSize() const
    {
        if (!m_asioInitialized)
        {
            return 0;
        }

        // Return the preferred buffer size from the driver if available
        if (m_preferredBufferSize > 0)
        {
            return m_preferredBufferSize;
        }

        // Otherwise, choose a reasonable default based on the driver capabilities
        if (m_minBufferSize > 0 && m_maxBufferSize > 0)
        {
            // Try for a common buffer size that's within the valid range
            const long commonBufferSizes[] = {256, 512, 1024, 2048};

            for (long size : commonBufferSizes)
            {
                if (size >= m_minBufferSize && size <= m_maxBufferSize)
                {
                    return size;
                }
            }

            // If none of the common sizes work, use the minimum size
            return m_minBufferSize;
        }

        // If we can't determine a proper buffer size, use a reasonable default
        return 1024;
    }

    double AsioManager::getOptimalSampleRate() const
    {
        if (!m_asioInitialized)
        {
            return 0.0;
        }

        // Return the current sample rate if available
        if (m_sampleRate > 0.0)
        {
            return m_sampleRate;
        }

        // Get supported sample rates
        auto supportedRates = getSupportedSampleRates();

        // Preferred sample rates in order of preference
        const double preferredRates[] = {48000.0, 44100.0, 96000.0, 88200.0, 192000.0};

        // First try to find a preferred rate that is supported
        for (double rate : preferredRates)
        {
            if (std::find(supportedRates.begin(), supportedRates.end(), rate) != supportedRates.end())
            {
                return rate;
            }
        }

        // If none of the preferred rates are supported, return the first supported rate
        if (!supportedRates.empty())
        {
            return supportedRates[0];
        }

        // If we can't determine a proper sample rate, use a reasonable default
        return 48000.0;
    }

    bool AsioManager::getDefaultDeviceConfiguration(long &bufferSize, double &sampleRate) const
    {
        if (!m_asioInitialized)
        {
            return false;
        }

        // Get the preferred buffer size
        bufferSize = getOptimalBufferSize();

        // Get the optimal sample rate
        sampleRate = getOptimalSampleRate();

        return true;
    }

    // Create a hardware-specific implementation of DeviceStateInterface
    class AsioDeviceStateInterface : public DeviceStateInterface
    {
    private:
        AsioManager &m_asioManager;

    public:
        AsioDeviceStateInterface(AsioManager &manager) : m_asioManager(manager) {}

        bool queryState(const std::string &deviceName,
                        std::function<void(bool, const DeviceState &)> callback) override
        {
            // Use AsioManager to query hardware state
            if (!m_asioManager.isDriverLoaded())
            {
                if (!m_asioManager.loadDriver(deviceName))
                {
                    callback(false, DeviceState(deviceName, deviceName));
                    return false;
                }
            }

            if (!m_asioManager.isInitialized())
            {
                if (!m_asioManager.initDevice())
                {
                    callback(false, DeviceState(deviceName, deviceName));
                    return false;
                }
            }

            // Create DeviceState from AsioManager state
            DeviceState state(deviceName, m_asioManager.getDeviceName());
            state.setInputChannelCount(m_asioManager.getInputChannelCount());
            state.setOutputChannelCount(m_asioManager.getOutputChannelCount());
            state.setSampleRate(m_asioManager.getSampleRate());
            state.setBufferSize(m_asioManager.getBufferSize());
            state.setActive(m_asioManager.isProcessing());

            callback(true, state);
            return true;
        }

        bool applyConfiguration(const std::string &deviceName,
                                const Configuration &config) override
        {
            // Apply configuration to hardware via AsioManager
            if (!m_asioManager.isDriverLoaded())
            {
                if (!m_asioManager.loadDriver(deviceName))
                {
                    return false;
                }
            }

            double sampleRate = config.getSampleRate();
            long bufferSize = config.getBufferSize();

            if (!m_asioManager.initDevice(sampleRate, bufferSize))
            {
                return false;
            }

            // Set up channels from configuration
            std::vector<long> inputChannels;
            std::vector<long> outputChannels;

            // ... populate channel lists from config ...

            if (!m_asioManager.createBuffers(inputChannels, outputChannels))
            {
                return false;
            }

            return true;
        }
    };

} // namespace AudioEngine
