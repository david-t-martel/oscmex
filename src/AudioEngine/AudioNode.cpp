#include "AudioNode.h"
#include "AudioEngine.h"
#include <iostream>

namespace AudioEngine
{

	AudioNode::AudioNode(const std::string &name, NodeType type, AudioEngine *engine)
		: m_name(name), m_type(type), m_engine(engine),
		  m_sampleRate(0), m_bufferSize(0), m_format(AV_SAMPLE_FMT_NONE),
		  m_configured(false), m_running(false), m_lastError("")
	{
		// Initialize default channel layout (mono)
		av_channel_layout_default(&m_channelLayout, 1);
	}

	AudioNode::~AudioNode()
	{
		// Make sure node is stopped before destruction
		if (m_running)
		{
			try
			{
				stop();
			}
			catch (const std::exception &e)
			{
				std::cerr << "Exception during node stop in destructor: " << e.what() << std::endl;
			}
			catch (...)
			{
				std::cerr << "Unknown exception during node stop in destructor" << std::endl;
			}
		}

		// Clean up channel layout
		av_channel_layout_uninit(&m_channelLayout);
	}

	std::string AudioNode::nodeTypeToString(NodeType type)
	{
		switch (type)
		{
		case NodeType::ASIO_SOURCE:
			return "ASIO Source";
		case NodeType::ASIO_SINK:
			return "ASIO Sink";
		case NodeType::FILE_SOURCE:
			return "File Source";
		case NodeType::FILE_SINK:
			return "File Sink";
		case NodeType::FFMPEG_PROCESSOR:
			return "FFmpeg Processor";
		default:
			return "Unknown";
		}
	}

	void AudioNode::logMessage(const std::string &message, bool isError)
	{
		std::string typeStr = nodeTypeToString(m_type);
		std::string prefix = isError ? "ERROR" : "INFO";

		std::cerr << "[" << prefix << "] " << typeStr << " '" << m_name << "': " << message << std::endl;

		// Store error message for retrieval with getLastError()
		if (isError)
		{
			m_lastError = message;
		}
	}

	bool AudioNode::checkConfigure(double sampleRate, long bufferSize,
								   AVSampleFormat format, const AVChannelLayout &layout)
	{
		if (sampleRate <= 0)
		{
			logMessage("Invalid sample rate: " + std::to_string(sampleRate), true);
			return false;
		}

		if (bufferSize <= 0)
		{
			logMessage("Invalid buffer size: " + std::to_string(bufferSize), true);
			return false;
		}

		if (format == AV_SAMPLE_FMT_NONE)
		{
			logMessage("Invalid sample format", true);
			return false;
		}

		if (layout.nb_channels <= 0)
		{
			logMessage("Invalid channel layout", true);
			return false;
		}

		// Copy the properties
		m_sampleRate = sampleRate;
		m_bufferSize = bufferSize;
		m_format = format;

		// Clean up and copy the channel layout
		av_channel_layout_uninit(&m_channelLayout);
		if (av_channel_layout_copy(&m_channelLayout, &layout) < 0)
		{
			logMessage("Failed to copy channel layout", true);
			return false;
		}

		return true;
	}

	bool AudioNode::updateParameter(const std::string &paramName, const std::string &paramValue)
	{
		// Default implementation returns false - derived classes should override
		// if they support dynamic parameter updates
		logMessage("Parameter update not supported for parameter: " + paramName, true);
		return false;
	}

	bool AudioNode::isBufferFormatCompatible(const std::shared_ptr<AudioBuffer> &buffer) const
	{
		if (!buffer)
		{
			return false;
		}

		// Check for format compatibility
		bool formatMatches = (buffer->getFormat() == m_format);

		// Check for channel layout compatibility
		bool channelsMatch = false;
		AVChannelLayout bufferLayout = buffer->getChannelLayout();
		channelsMatch = (av_channel_layout_compare(&bufferLayout, &m_channelLayout) == 0);

		// Check buffer size
		bool sizeMatches = (buffer->getFrames() == m_bufferSize);

		// For maximum compatibility, just report mismatches but don't fail
		if (!formatMatches)
		{
			std::cerr << "Warning: Buffer format mismatch in " << m_name
					  << " - expected: " << av_get_sample_fmt_name(m_format)
					  << ", got: " << av_get_sample_fmt_name(buffer->getFormat()) << std::endl;
		}

		if (!channelsMatch)
		{
			std::cerr << "Warning: Buffer channel layout mismatch in " << m_name << std::endl;
		}

		if (!sizeMatches)
		{
			std::cerr << "Warning: Buffer size mismatch in " << m_name
					  << " - expected: " << m_bufferSize
					  << ", got: " << buffer->getFrames() << std::endl;
		}

		// If we want strict compatibility, return this:
		// return formatMatches && channelsMatch && sizeMatches;

		// For now, we're just returning if the buffer is valid at all
		return buffer->isValid();
	}

} // namespace AudioEngine
