#include "AudioNode.h"
#include "AudioEngine.h"
#include <iostream>
#include <nlohmann/json.hpp>

namespace AudioEngine
{

	AudioNode::AudioNode(const std::string &name, AudioEngine *engine)
		: m_name(name),
		  m_engine(engine),
		  m_sampleRate(0),
		  m_bufferSize(0),
		  m_format(AV_SAMPLE_FMT_NONE),
		  m_configured(false),
		  m_running(false)
	{
		// Initialize channel layout
		av_channel_layout_default(&m_channelLayout, 2); // Default to stereo
	}

	AudioNode::~AudioNode()
	{
		// Stop the node if it's still running
		if (m_running)
		{
			stop();
		}

		// Free the channel layout
		av_channel_layout_uninit(&m_channelLayout);
	}

	bool AudioNode::configure(const std::map<std::string, std::string> &params, double sampleRate, long bufferSize,
							  AVSampleFormat format, AVChannelLayout channelLayout)
	{
		// Convert the parameter map to a JSON string
		nlohmann::json jsonParams;
		for (const auto &[key, value] : params)
		{
			jsonParams[key] = value;
		}

		// Call the string-based configure method
		return configure(jsonParams.dump(), sampleRate, bufferSize, format, channelLayout);
	}

	bool AudioNode::sendControlMessage(const std::string &messageType, const std::map<std::string, std::string> &params)
	{
		// Base implementation just reports that the message was not handled
		reportStatus("Warning", "Control message '" + messageType + "' not handled by node type " + nodeTypeToString(getType()));
		return false;
	}

	void AudioNode::reportStatus(const std::string &category, const std::string &message)
	{
		if (m_engine)
		{
			m_engine->reportStatus(category, m_name + ": " + message);
		}
		else
		{
			std::cerr << "[" << category << "] " << m_name << ": " + message << std::endl;
		}
	}

	// Implementation of node type conversion functions
	std::string nodeTypeToString(AudioNode::NodeType type)
	{
		switch (type)
		{
		case AudioNode::NodeType::ASIO_SOURCE:
			return "asio_source";
		case AudioNode::NodeType::ASIO_SINK:
			return "asio_sink";
		case AudioNode::NodeType::FILE_SOURCE:
			return "file_source";
		case AudioNode::NodeType::FILE_SINK:
			return "file_sink";
		case AudioNode::NodeType::FFMPEG_PROCESSOR:
			return "ffmpeg_processor";
		case AudioNode::NodeType::CUSTOM:
			return "custom";
		case AudioNode::NodeType::UNKNOWN:
		default:
			return "unknown";
		}
	}

	AudioNode::NodeType nodeTypeFromString(const std::string &typeStr)
	{
		if (typeStr == "asio_source")
			return AudioNode::NodeType::ASIO_SOURCE;
		else if (typeStr == "asio_sink")
			return AudioNode::NodeType::ASIO_SINK;
		else if (typeStr == "file_source")
			return AudioNode::NodeType::FILE_SOURCE;
		else if (typeStr == "file_sink")
			return AudioNode::NodeType::FILE_SINK;
		else if (typeStr == "ffmpeg_processor")
			return AudioNode::NodeType::FFMPEG_PROCESSOR;
		else if (typeStr == "custom")
			return AudioNode::NodeType::CUSTOM;
		else
			return AudioNode::NodeType::UNKNOWN;
	}

	// Implementation of Connection class
	Connection::Connection(AudioNode *sourceNode, int sourcePad, AudioNode *sinkNode, int sinkPad)
		: m_sourceNode(sourceNode),
		  m_sourcePad(sourcePad),
		  m_sinkNode(sinkNode),
		  m_sinkPad(sinkPad),
		  m_formatConversion(true),
		  m_bufferPolicy("auto")
	{
	}

	bool Connection::transfer()
	{
		if (!m_sourceNode || !m_sinkNode)
			return false;

		// Get the output buffer from the source node
		auto buffer = m_sourceNode->getOutputBuffer(m_sourcePad);

		if (!buffer)
			return false;

		// Set the buffer to the sink node
		return m_sinkNode->setInputBuffer(buffer, m_sinkPad);
	}

} // namespace AudioEngine
