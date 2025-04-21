#include "AsioNodes.h"
#include "AudioEngine.h"
#include <iostream>
#include <sstream>

// FFmpeg includes for format conversion
extern "C"
{
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

// JSON library for parameter parsing
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace AudioEngine
{

    //-------------------------------------------------------------------------
    // AsioSourceNode Implementation
    //-------------------------------------------------------------------------

    AsioSourceNode::AsioSourceNode(const std::string &name, AudioEngine *engine, AsioManager *asioManager)
        : AudioNode(name, engine),
          m_asioManager(asioManager),
          m_swrContext(nullptr)
    {
        // Nothing else to do here
    }

    AsioSourceNode::~AsioSourceNode()
    {
        // Ensure we're stopped
        if (m_running)
        {
            stop();
        }

        // Clean up resources
        if (m_swrContext)
        {
            swr_free(&m_swrContext);
            m_swrContext = nullptr;
        }
    }

    bool AsioSourceNode::configure(const std::string &params, double sampleRate, long bufferSize,
                                   AVSampleFormat format, AVChannelLayout channelLayout)
    {
        // Lock during configuration
        std::lock_guard<std::mutex> lock(m_bufferMutex);

        if (m_configured)
        {
            m_lastError = "Node is already configured";
            return false;
        }

        // Store format parameters
        m_sampleRate = sampleRate;
        m_bufferSize = bufferSize;
        m_format = format;
        m_channelLayout = channelLayout;

        // Parse the parameters
        if (!parseChannelParams(params))
        {
            m_lastError = "Failed to parse channel parameters";
            return false;
        }

        // Create the output buffer
        m_outputBuffer = std::make_shared<AudioBuffer>();
        if (!m_outputBuffer->allocate(m_bufferSize, m_sampleRate, m_format, m_channelLayout))
        {
            m_lastError = "Failed to allocate output buffer";
            return false;
        }

        // Initialize the format converter if needed
        // This depends on ASIO sample format which we don't know yet
        // We'll initialize it in start() when ASIO buffers are created

        m_configured = true;
        return true;
    }

    bool AsioSourceNode::start()
    {
        if (!m_configured)
        {
            m_lastError = "Node is not configured";
            return false;
        }

        if (m_running)
        {
            // Already running
            return true;
        }

        // Check that ASIO is active
        if (!m_asioManager || !m_asioChannelIndices.size())
        {
            m_lastError = "ASIO manager not available or no channels configured";
            return false;
        }

        // Get ASIO sample type
        ASIOSampleType asioSampleType = m_asioManager->getSampleType();

        // Create the resampler context
        // We need to convert from ASIO format to our internal format

        // Determine source format based on ASIO sample type
        AVSampleFormat srcFormat;
        int srcBits = 0;

        switch (asioSampleType)
        {
        case ASIOSTInt16LSB:
            srcFormat = AV_SAMPLE_FMT_S16;
            srcBits = 16;
            break;
        case ASIOSTInt24LSB:
            srcFormat = AV_SAMPLE_FMT_S32; // No native 24-bit format in FFmpeg
            srcBits = 24;
            break;
        case ASIOSTInt32LSB:
            srcFormat = AV_SAMPLE_FMT_S32;
            srcBits = 32;
            break;
        case ASIOSTFloat32LSB:
            srcFormat = AV_SAMPLE_FMT_FLT;
            srcBits = 32;
            break;
        case ASIOSTFloat64LSB:
            srcFormat = AV_SAMPLE_FMT_DBL;
            srcBits = 64;
            break;
        default:
            m_lastError = "Unsupported ASIO sample type: " + m_asioManager->getSampleTypeName();
            return false;
        }

        // Get channel count from layout
        int channelCount = av_channel_layout_nb_channels(&m_channelLayout);

        // Create a simple channel layout for ASIO input
        AVChannelLayout srcLayout;
        av_channel_layout_default(&srcLayout, channelCount);

        // Create the swresample context
        m_swrContext = swr_alloc();
        if (!m_swrContext)
        {
            m_lastError = "Failed to allocate swresample context";
            return false;
        }

        // Set options
        av_opt_set_int(m_swrContext, "in_channel_count", channelCount, 0);
        av_opt_set_channel_layout(m_swrContext, "in_channel_layout", srcLayout.u.mask, 0);
        av_opt_set_int(m_swrContext, "in_sample_rate", (int)m_sampleRate, 0);
        av_opt_set_sample_fmt(m_swrContext, "in_sample_fmt", srcFormat, 0);

        av_opt_set_int(m_swrContext, "out_channel_count", channelCount, 0);
        av_opt_set_channel_layout(m_swrContext, "out_channel_layout", m_channelLayout.u.mask, 0);
        av_opt_set_int(m_swrContext, "out_sample_rate", (int)m_sampleRate, 0);
        av_opt_set_sample_fmt(m_swrContext, "out_sample_fmt", m_format, 0);

        // Initialize the converter
        int ret = swr_init(m_swrContext);
        if (ret < 0)
        {
            char errBuff[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errBuff, sizeof(errBuff));
            m_lastError = "Failed to initialize swresample context: " + std::string(errBuff);
            swr_free(&m_swrContext);
            m_swrContext = nullptr;
            return false;
        }

        // Free the temporary layout
        av_channel_layout_uninit(&srcLayout);

        m_running = true;
        return true;
    }

    bool AsioSourceNode::process()
    {
        // Nothing to do here - ASIO callback handles processing
        return true;
    }

    bool AsioSourceNode::stop()
    {
        if (!m_running)
        {
            return true;
        }

        // Clean up resampler
        if (m_swrContext)
        {
            swr_free(&m_swrContext);
            m_swrContext = nullptr;
        }

        m_running = false;
        return true;
    }

    std::shared_ptr<AudioBuffer> AsioSourceNode::getOutputBuffer(int padIndex)
    {
        if (padIndex != 0)
        {
            std::cerr << "AsioSourceNode: Invalid output pad index: " << padIndex << std::endl;
            return nullptr;
        }

        std::lock_guard<std::mutex> lock(m_bufferMutex);
        return m_outputBuffer;
    }

    bool AsioSourceNode::receiveAsioData(long doubleBufferIndex, const std::vector<void *> &asioBuffers)
    {
        if (!m_running || !m_configured)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(m_bufferMutex);

        if (!m_outputBuffer || !m_swrContext)
        {
            return false;
        }

        // Check that we have the right number of buffers
        if (asioBuffers.size() != m_asioChannelIndices.size())
        {
            std::cerr << "AsioSourceNode: Received " << asioBuffers.size()
                      << " buffers, expected " << m_asioChannelIndices.size() << std::endl;
            return false;
        }

        // Get ASIO sample type
        ASIOSampleType asioSampleType = m_asioManager->getSampleType();

        // Determine bytes per sample
        int bytesPerSample = 0;
        switch (asioSampleType)
        {
        case ASIOSTInt16LSB:
            bytesPerSample = 2;
            break;
        case ASIOSTInt24LSB:
            bytesPerSample = 3;
            break;
        case ASIOSTInt32LSB:
            bytesPerSample = 4;
            break;
        case ASIOSTFloat32LSB:
            bytesPerSample = 4;
            break;
        case ASIOSTFloat64LSB:
            bytesPerSample = 8;
            break;
        default:
            std::cerr << "AsioSourceNode: Unsupported ASIO sample type: "
                      << m_asioManager->getSampleTypeName() << std::endl;
            return false;
        }

        // Get the output buffer planes
        std::vector<uint8_t *> outputPlanes;
        for (int i = 0; i < av_channel_layout_nb_channels(&m_channelLayout); i++)
        {
            outputPlanes.push_back(m_outputBuffer->getPlaneData(i));
        }

        // Set up source and destination data pointers
        const uint8_t **srcData = (const uint8_t **)asioBuffers.data();
        uint8_t **dstData = outputPlanes.data();

        // Convert the data
        int ret = swr_convert(m_swrContext, dstData, m_bufferSize,
                              srcData, m_bufferSize);

        if (ret < 0)
        {
            char errBuff[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errBuff, sizeof(errBuff));
            std::cerr << "AsioSourceNode: Error converting samples: " << errBuff << std::endl;
            return false;
        }

        return true;
    }

    bool AsioSourceNode::parseChannelParams(const std::string &params)
    {
        try
        {
            json paramsJson = json::parse(params);

            // Check for channel specification
            if (paramsJson.contains("channels"))
            {
                std::string channelsStr = paramsJson["channels"];

                // Parse the comma-separated list of indices
                std::vector<long> indices;
                std::stringstream ss(channelsStr);
                std::string item;

                while (std::getline(ss, item, ','))
                {
                    try
                    {
                        long index = std::stol(item);
                        indices.push_back(index);
                    }
                    catch (const std::exception &e)
                    {
                        std::cerr << "AsioSourceNode: Error parsing channel index: " << item << std::endl;
                        return false;
                    }
                }

                if (indices.empty())
                {
                    std::cerr << "AsioSourceNode: No channel indices provided" << std::endl;
                    return false;
                }

                m_asioChannelIndices = indices;
            }
            else if (paramsJson.contains("channelNames"))
            {
                // Handle channel names
                std::vector<std::string> names;

                if (paramsJson["channelNames"].is_array())
                {
                    for (const auto &item : paramsJson["channelNames"])
                    {
                        names.push_back(item);
                    }
                }
                else if (paramsJson["channelNames"].is_string())
                {
                    std::string namesStr = paramsJson["channelNames"];
                    std::stringstream ss(namesStr);
                    std::string item;

                    while (std::getline(ss, item, ','))
                    {
                        names.push_back(item);
                    }
                }

                if (names.empty())
                {
                    std::cerr << "AsioSourceNode: No channel names provided" << std::endl;
                    return false;
                }

                m_asioChannelNames = names;

                // Lookup indices by name
                if (!findChannelIndices(names))
                {
                    std::cerr << "AsioSourceNode: Failed to find one or more channel indices" << std::endl;
                    return false;
                }
            }
            else
            {
                std::cerr << "AsioSourceNode: No channels or channelNames provided" << std::endl;
                return false;
            }

            return true;
        }
        catch (const json::exception &e)
        {
            std::cerr << "AsioSourceNode: Error parsing parameters: " << e.what() << std::endl;
            return false;
        }
    }

    bool AsioSourceNode::findChannelIndices(const std::vector<std::string> &channelNames)
    {
        if (!m_asioManager)
        {
            std::cerr << "AsioSourceNode: No ASIO manager available" << std::endl;
            return false;
        }

        m_asioChannelIndices.clear();

        for (const auto &name : channelNames)
        {
            long index = 0;
            if (!m_asioManager->findChannelIndex(name, true, index))
            {
                std::cerr << "AsioSourceNode: Failed to find channel: " << name << std::endl;
                return false;
            }

            m_asioChannelIndices.push_back(index);
        }

        return true;
    }

    //-------------------------------------------------------------------------
    // AsioSinkNode Implementation
    //-------------------------------------------------------------------------

    AsioSinkNode::AsioSinkNode(const std::string &name, AudioEngine *engine, AsioManager *asioManager)
        : AudioNode(name, engine),
          m_asioManager(asioManager),
          m_swrContext(nullptr)
    {
        // Nothing else to do here
    }

    AsioSinkNode::~AsioSinkNode()
    {
        // Ensure we're stopped
        if (m_running)
        {
            stop();
        }

        // Clean up resources
        if (m_swrContext)
        {
            swr_free(&m_swrContext);
            m_swrContext = nullptr;
        }
    }

    bool AsioSinkNode::configure(const std::string &params, double sampleRate, long bufferSize,
                                 AVSampleFormat format, AVChannelLayout channelLayout)
    {
        // Lock during configuration
        std::lock_guard<std::mutex> lock(m_bufferMutex);

        if (m_configured)
        {
            m_lastError = "Node is already configured";
            return false;
        }

        // Store format parameters
        m_sampleRate = sampleRate;
        m_bufferSize = bufferSize;
        m_format = format;
        m_channelLayout = channelLayout;

        // Parse the parameters
        if (!parseChannelParams(params))
        {
            m_lastError = "Failed to parse channel parameters";
            return false;
        }

        m_configured = true;
        return true;
    }

    bool AsioSinkNode::start()
    {
        if (!m_configured)
        {
            m_lastError = "Node is not configured";
            return false;
        }

        if (m_running)
        {
            // Already running
            return true;
        }

        // Check that ASIO is active
        if (!m_asioManager || !m_asioChannelIndices.size())
        {
            m_lastError = "ASIO manager not available or no channels configured";
            return false;
        }

        // Get ASIO sample type
        ASIOSampleType asioSampleType = m_asioManager->getSampleType();

        // Create the resampler context
        // We need to convert from our internal format to ASIO format

        // Determine destination format based on ASIO sample type
        AVSampleFormat dstFormat;
        int dstBits = 0;

        switch (asioSampleType)
        {
        case ASIOSTInt16LSB:
            dstFormat = AV_SAMPLE_FMT_S16;
            dstBits = 16;
            break;
        case ASIOSTInt24LSB:
            dstFormat = AV_SAMPLE_FMT_S32; // No native 24-bit format in FFmpeg
            dstBits = 24;
            break;
        case ASIOSTInt32LSB:
            dstFormat = AV_SAMPLE_FMT_S32;
            dstBits = 32;
            break;
        case ASIOSTFloat32LSB:
            dstFormat = AV_SAMPLE_FMT_FLT;
            dstBits = 32;
            break;
        case ASIOSTFloat64LSB:
            dstFormat = AV_SAMPLE_FMT_DBL;
            dstBits = 64;
            break;
        default:
            m_lastError = "Unsupported ASIO sample type: " + m_asioManager->getSampleTypeName();
            return false;
        }

        // Get channel count from layout
        int channelCount = av_channel_layout_nb_channels(&m_channelLayout);

        // Create a simple channel layout for ASIO output
        AVChannelLayout dstLayout;
        av_channel_layout_default(&dstLayout, channelCount);

        // Create the swresample context
        m_swrContext = swr_alloc();
        if (!m_swrContext)
        {
            m_lastError = "Failed to allocate swresample context";
            return false;
        }

        // Set options
        av_opt_set_int(m_swrContext, "in_channel_count", channelCount, 0);
        av_opt_set_channel_layout(m_swrContext, "in_channel_layout", m_channelLayout.u.mask, 0);
        av_opt_set_int(m_swrContext, "in_sample_rate", (int)m_sampleRate, 0);
        av_opt_set_sample_fmt(m_swrContext, "in_sample_fmt", m_format, 0);

        av_opt_set_int(m_swrContext, "out_channel_count", channelCount, 0);
        av_opt_set_channel_layout(m_swrContext, "out_channel_layout", dstLayout.u.mask, 0);
        av_opt_set_int(m_swrContext, "out_sample_rate", (int)m_sampleRate, 0);
        av_opt_set_sample_fmt(m_swrContext, "out_sample_fmt", dstFormat, 0);

        // Initialize the converter
        int ret = swr_init(m_swrContext);
        if (ret < 0)
        {
            char errBuff[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errBuff, sizeof(errBuff));
            m_lastError = "Failed to initialize swresample context: " + std::string(errBuff);
            swr_free(&m_swrContext);
            m_swrContext = nullptr;
            return false;
        }

        // Free the temporary layout
        av_channel_layout_uninit(&dstLayout);

        m_running = true;
        return true;
    }

    bool AsioSinkNode::process()
    {
        // Nothing to do here - ASIO callback handles processing
        return true;
    }

    bool AsioSinkNode::stop()
    {
        if (!m_running)
        {
            return true;
        }

        // Clean up resampler
        if (m_swrContext)
        {
            swr_free(&m_swrContext);
            m_swrContext = nullptr;
        }

        // Clear the input buffer
        {
            std::lock_guard<std::mutex> lock(m_bufferMutex);
            m_inputBuffer.reset();
        }

        m_running = false;
        return true;
    }

    bool AsioSinkNode::setInputBuffer(std::shared_ptr<AudioBuffer> buffer, int padIndex)
    {
        if (padIndex != 0)
        {
            std::cerr << "AsioSinkNode: Invalid input pad index: " << padIndex << std::endl;
            return false;
        }

        std::lock_guard<std::mutex> lock(m_bufferMutex);

        // Check buffer compatibility
        if (buffer && !isBufferFormatCompatible(buffer))
        {
            std::cerr << "AsioSinkNode: Incompatible buffer format" << std::endl;
            return false;
        }

        m_inputBuffer = buffer;
        return true;
    }

    bool AsioSinkNode::provideAsioData(long doubleBufferIndex, std::vector<void *> &asioBuffers)
    {
        if (!m_running || !m_configured)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(m_bufferMutex);

        if (!m_inputBuffer || !m_swrContext)
        {
            // No input available, send silence

            // Get ASIO sample type
            ASIOSampleType asioSampleType = m_asioManager->getSampleType();

            // Determine bytes per sample
            int bytesPerSample = 0;
            switch (asioSampleType)
            {
            case ASIOSTInt16LSB:
                bytesPerSample = 2;
                break;
            case ASIOSTInt24LSB:
                bytesPerSample = 3;
                break;
            case ASIOSTInt32LSB:
                bytesPerSample = 4;
                break;
            case ASIOSTFloat32LSB:
                bytesPerSample = 4;
                break;
            case ASIOSTFloat64LSB:
                bytesPerSample = 8;
                break;
            default:
                std::cerr << "AsioSinkNode: Unsupported ASIO sample type: "
                          << m_asioManager->getSampleTypeName() << std::endl;
                return false;
            }

            // Fill buffers with zeros
            size_t bufferSizeBytes = m_bufferSize * bytesPerSample;
            for (auto &buffer : asioBuffers)
            {
                memset(buffer, 0, bufferSizeBytes);
            }

            return true;
        }

        // Check that we have the right number of buffers
        if (asioBuffers.size() != m_asioChannelIndices.size())
        {
            std::cerr << "AsioSinkNode: Expected " << m_asioChannelIndices.size()
                      << " buffers, got " << asioBuffers.size() << std::endl;
            return false;
        }

        // Get the input buffer planes
        std::vector<const uint8_t *> inputPlanes;
        for (int i = 0; i < av_channel_layout_nb_channels(&m_channelLayout); i++)
        {
            inputPlanes.push_back(m_inputBuffer->getPlaneData(i));
        }

        // Convert the data
        int ret = swr_convert(m_swrContext,
                              (uint8_t **)asioBuffers.data(), m_bufferSize,
                              inputPlanes.data(), m_bufferSize);

        if (ret < 0)
        {
            char errBuff[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errBuff, sizeof(errBuff));
            std::cerr << "AsioSinkNode: Error converting samples: " << errBuff << std::endl;
            return false;
        }

        return true;
    }

    bool AsioSinkNode::parseChannelParams(const std::string &params)
    {
        try
        {
            json paramsJson = json::parse(params);

            // Check for channel specification
            if (paramsJson.contains("channels"))
            {
                std::string channelsStr = paramsJson["channels"];

                // Parse the comma-separated list of indices
                std::vector<long> indices;
                std::stringstream ss(channelsStr);
                std::string item;

                while (std::getline(ss, item, ','))
                {
                    try
                    {
                        long index = std::stol(item);
                        indices.push_back(index);
                    }
                    catch (const std::exception &e)
                    {
                        std::cerr << "AsioSinkNode: Error parsing channel index: " << item << std::endl;
                        return false;
                    }
                }

                if (indices.empty())
                {
                    std::cerr << "AsioSinkNode: No channel indices provided" << std::endl;
                    return false;
                }

                m_asioChannelIndices = indices;
            }
            else if (paramsJson.contains("channelNames"))
            {
                // Handle channel names
                std::vector<std::string> names;

                if (paramsJson["channelNames"].is_array())
                {
                    for (const auto &item : paramsJson["channelNames"])
                    {
                        names.push_back(item);
                    }
                }
                else if (paramsJson["channelNames"].is_string())
                {
                    std::string namesStr = paramsJson["channelNames"];
                    std::stringstream ss(namesStr);
                    std::string item;

                    while (std::getline(ss, item, ','))
                    {
                        names.push_back(item);
                    }
                }

                if (names.empty())
                {
                    std::cerr << "AsioSinkNode: No channel names provided" << std::endl;
                    return false;
                }

                m_asioChannelNames = names;

                // Lookup indices by name
                if (!findChannelIndices(names))
                {
                    std::cerr << "AsioSinkNode: Failed to find one or more channel indices" << std::endl;
                    return false;
                }
            }
            else
            {
                std::cerr << "AsioSinkNode: No channels or channelNames provided" << std::endl;
                return false;
            }

            return true;
        }
        catch (const json::exception &e)
        {
            std::cerr << "AsioSinkNode: Error parsing parameters: " << e.what() << std::endl;
            return false;
        }
    }

    bool AsioSinkNode::findChannelIndices(const std::vector<std::string> &channelNames)
    {
        if (!m_asioManager)
        {
            std::cerr << "AsioSinkNode: No ASIO manager available" << std::endl;
            return false;
        }

        m_asioChannelIndices.clear();

        for (const auto &name : channelNames)
        {
            long index = 0;
            if (!m_asioManager->findChannelIndex(name, false, index)) // false = output channel
            {
                std::cerr << "AsioSinkNode: Failed to find channel: " << name << std::endl;
                return false;
            }

            m_asioChannelIndices.push_back(index);
        }

        return true;
    }

} // namespace AudioEngine
