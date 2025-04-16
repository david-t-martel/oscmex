#include "FfmpegProcessorNode.h"
#include "FfmpegFilter.h"
#include "AudioEngine.h"
#include <iostream>
#include <memory>

extern "C"
{
#include <libavutil/frame.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
}

namespace AudioEngine
{

	FfmpegProcessorNode::FfmpegProcessorNode(const std::string &name, AudioEngine *engine)
		: AudioNode(name, NodeType::FFMPEG_PROCESSOR, engine),
		  m_ffmpegFilter(std::make_unique<FfmpegFilter>()),
		  m_inputFrame(nullptr),
		  m_outputFrame(nullptr),
		  m_inputBuffer(nullptr),
		  m_outputBuffer(nullptr)
	{
	}

	FfmpegProcessorNode::~FfmpegProcessorNode()
	{
		stop();
		cleanupFrames();
	}

	bool FfmpegProcessorNode::configure(const std::map<std::string, std::string> &params,
										double sampleRate, long bufferSize,
										AVSampleFormat format, const AVChannelLayout &layout)
	{
		if (m_running)
		{
			logMessage("Cannot configure while running", true);
			return false;
		}

		// Check base configuration
		if (!checkConfigure(sampleRate, bufferSize, format, layout))
		{
			return false;
		}

		// Get filter description
		auto it = params.find("filter_description");
		if (it == params.end())
		{
			logMessage("Missing required 'filter_description' parameter", true);
			return false;
		}
		m_filterDescription = it->second;

		// Initialize the FFmpeg filter
		if (!m_ffmpegFilter->initGraph(m_filterDescription, m_sampleRate, m_format,
									   m_channelLayout, m_bufferSize))
		{
			logMessage("Failed to initialize FFmpeg filter graph", true);
			return false;
		}

		// Initialize AVFrames
		if (!initializeFrames())
		{
			logMessage("Failed to initialize AVFrames", true);
			return false;
		}

		// Create output buffer (input buffer is set by upstream node)
		m_outputBuffer = std::make_shared<AudioBuffer>(m_bufferSize, m_sampleRate,
													   m_format, m_channelLayout);
		if (!m_outputBuffer->isValid())
		{
			logMessage("Failed to create valid output buffer", true);
			return false;
		}

		m_configured = true;
		logMessage("Configured with filter: " + m_filterDescription, false);

		return true;
	}

	bool FfmpegProcessorNode::initializeFrames()
	{
		// Clean up existing frames first
		cleanupFrames();

		// Create input frame
		m_inputFrame = av_frame_alloc();
		if (!m_inputFrame)
		{
			logMessage("Failed to allocate input AVFrame", true);
			return false;
		}

		// Create output frame
		m_outputFrame = av_frame_alloc();
		if (!m_outputFrame)
		{
			logMessage("Failed to allocate output AVFrame", true);
			av_frame_free(&m_inputFrame);
			m_inputFrame = nullptr;
			return false;
		}

		// Set up input frame properties
		m_inputFrame->format = m_format;
		if (av_channel_layout_copy(&m_inputFrame->ch_layout, &m_channelLayout) < 0)
		{
			logMessage("Failed to copy channel layout to input frame", true);
			cleanupFrames();
			return false;
		}
		m_inputFrame->nb_samples = m_bufferSize;
		m_inputFrame->sample_rate = static_cast<int>(m_sampleRate);

		return true;
	}

	void FfmpegProcessorNode::cleanupFrames()
	{
		if (m_inputFrame)
		{
			av_frame_free(&m_inputFrame);
			m_inputFrame = nullptr;
		}

		if (m_outputFrame)
		{
			av_frame_free(&m_outputFrame);
			m_outputFrame = nullptr;
		}
	}

	bool FfmpegProcessorNode::start()
	{
		std::lock_guard<std::mutex> lock(m_processMutex);

		if (!m_configured)
		{
			logMessage("Cannot start - not configured", true);
			return false;
		}

		if (m_running)
		{
			logMessage("Already running", false);
			return true;
		}

		// Reset the filter graph to clear any state
		if (!m_ffmpegFilter->reset())
		{
			logMessage("Failed to reset filter graph", true);
			return false;
		}

		m_running = true;
		logMessage("Started", false);
		return true;
	}

	void FfmpegProcessorNode::stop()
	{
		std::lock_guard<std::mutex> lock(m_processMutex);

		if (!m_running)
		{
			return;
		}

		m_running = false;
		m_inputBuffer = nullptr;

		logMessage("Stopped", false);
	}

	bool FfmpegProcessorNode::process()
	{
		std::lock_guard<std::mutex> lock(m_processMutex);

		if (!m_running || !m_inputBuffer)
		{
			return false;
		}

		if (!m_inputBuffer->isValid() || !m_outputBuffer->isValid())
		{
			logMessage("Invalid buffers for processing", true);
			return false;
		}

		// Convert AudioBuffer to AVFrame
		// Set up data pointers in the input frame
		for (int i = 0; i < m_inputBuffer->getPlaneCount() && i < AV_NUM_DATA_POINTERS; i++)
		{
			m_inputFrame->data[i] = m_inputBuffer->getPlaneData(i);
			m_inputFrame->linesize[i] = m_inputBuffer->isPlanar() ? m_inputBuffer->getBytesPerSample() : m_inputBuffer->getBytesPerSample() * m_inputBuffer->getChannelCount();
		}

		// Process through the filter graph
		// First reset output frame
		av_frame_unref(m_outputFrame);

		if (!m_ffmpegFilter->process(m_inputFrame, m_outputFrame))
		{
			logMessage("FFmpeg filter processing failed", true);
			return false;
		}

		// Convert output AVFrame back to AudioBuffer
		if (m_outputFrame->nb_samples != m_bufferSize ||
			m_outputFrame->format != m_format ||
			av_channel_layout_compare(&m_outputFrame->ch_layout, &m_channelLayout) != 0)
		{

			logMessage("Unexpected output frame format from filter", true);
			return false;
		}

		// Copy data from output frame to output buffer
		for (int i = 0; i < m_outputBuffer->getPlaneCount() && i < AV_NUM_DATA_POINTERS; i++)
		{
			if (m_outputFrame->data[i] && m_outputBuffer->getPlaneData(i))
			{
				int bytesPerSample = m_outputBuffer->getBytesPerSample();
				int numChannels = m_outputBuffer->getChannelCount();
				int copySize = m_outputBuffer->isPlanar() ? m_bufferSize * bytesPerSample : m_bufferSize * bytesPerSample * numChannels;

				memcpy(m_outputBuffer->getPlaneData(i), m_outputFrame->data[i], copySize);
			}
		}

		return true;
	}

	std::shared_ptr<AudioBuffer> FfmpegProcessorNode::getOutputBuffer(int padIndex)
	{
		if (padIndex != 0)
		{
			logMessage("Invalid output pad index: " + std::to_string(padIndex), true);
			return nullptr;
		}

		std::lock_guard<std::mutex> lock(m_processMutex);

		if (!m_running || !m_outputBuffer)
		{
			return nullptr;
		}

		return m_outputBuffer;
	}

	bool FfmpegProcessorNode::setInputBuffer(std::shared_ptr<AudioBuffer> buffer, int padIndex)
	{
		if (padIndex != 0)
		{
			logMessage("Invalid input pad index: " + std::to_string(padIndex), true);
			return false;
		}

		std::lock_guard<std::mutex> lock(m_processMutex);

		if (!m_running)
		{
			return false;
		}

		if (!buffer || !buffer->isValid())
		{
			logMessage("Invalid input buffer", true);
			return false;
		}

		// Validate buffer format matches configuration
		if (buffer->getFrames() != m_bufferSize ||
			buffer->getFormat() != m_format ||
			buffer->getChannelCount() != m_channelLayout.nb_channels)
		{

			logMessage("Input buffer format mismatch", true);
			return false;
		}

		// Store input buffer (we don't need to copy it since we just read from it)
		m_inputBuffer = buffer;

		return true;
	}

	bool FfmpegProcessorNode::updateParameter(const std::string &filterName,
											  const std::string &paramName,
											  const std::string &value)
	{
		std::lock_guard<std::mutex> lock(m_processMutex);

		if (!m_configured)
		{
			logMessage("Cannot update parameter - not configured", true);
			return false;
		}

		if (!m_ffmpegFilter->updateParameter(filterName, paramName, value))
		{
			logMessage("Failed to update parameter '" + paramName + "' on filter '" + filterName + "'", true);
			return false;
		}

		logMessage("Updated parameter '" + paramName + "' on filter '" + filterName + "' to '" + value + "'", false);
		return true;
	}

} // namespace AudioEngine
