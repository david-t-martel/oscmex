#include "AsioSinkNode.h"
#include "AsioManager.h"
#include "AudioEngine.h"
#include <iostream>
#include <sstream>
#include <cmath>
#include <algorithm>

namespace AudioEngine
{

	AsioSinkNode::AsioSinkNode(const std::string &name, AudioEngine *engine, AsioManager *asioManager)
		: AudioNode(name, NodeType::ASIO_SINK, engine),
		  m_asioManager(asioManager),
		  m_doubleBufferSwitch(false),
		  m_inputBuffer(nullptr),
		  m_inputBufferA(nullptr),
		  m_inputBufferB(nullptr),
		  m_silenceBuffer(nullptr)
	{
		if (!m_asioManager)
		{
			logMessage("Invalid AsioManager pointer", true);
		}
	}

	AsioSinkNode::~AsioSinkNode()
	{
		stop();
	}

	bool AsioSinkNode::configure(const std::map<std::string, std::string> &params,
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

		// Parse ASIO channel indices from parameters
		m_asioChannelIndices.clear();

		auto it = params.find("channels");
		if (it == params.end())
		{
			logMessage("Missing required 'channels' parameter", true);
			return false;
		}

		std::stringstream ss(it->second);
		std::string channel;
		while (std::getline(ss, channel, ','))
		{
			try
			{
				long channelIndex = std::stol(channel);
				m_asioChannelIndices.push_back(channelIndex);
			}
			catch (const std::exception &e)
			{
				logMessage("Invalid channel index: " + channel + " - " + e.what(), true);
				return false;
			}
		}

		if (m_asioChannelIndices.empty())
		{
			logMessage("No ASIO channels specified", true);
			return false;
		}

		// Validate channel count matches layout
		if (static_cast<int>(m_asioChannelIndices.size()) != m_channelLayout.nb_channels)
		{
			logMessage("ASIO channel count (" + std::to_string(m_asioChannelIndices.size()) +
						   ") doesn't match specified channel layout (" +
						   std::to_string(m_channelLayout.nb_channels) + ")",
					   true);
			return false;
		}

		// Create buffers
		if (!createBuffers())
		{
			return false;
		}

		m_configured = true;
		logMessage("Configured for " + std::to_string(m_asioChannelIndices.size()) +
					   " channels, sample rate: " + std::to_string(m_sampleRate),
				   false);

		return true;
	}

	bool AsioSinkNode::createBuffers()
	{
		try
		{
			// Create two buffers for double-buffering
			m_inputBufferA = std::make_shared<AudioBuffer>(m_bufferSize, m_sampleRate,
														   m_format, m_channelLayout);

			m_inputBufferB = std::make_shared<AudioBuffer>(m_bufferSize, m_sampleRate,
														   m_format, m_channelLayout);

			// Create a silence buffer
			m_silenceBuffer = std::make_shared<AudioBuffer>(m_bufferSize, m_sampleRate,
															m_format, m_channelLayout);

			if (!m_inputBufferA->isValid() || !m_inputBufferB->isValid() || !m_silenceBuffer->isValid())
			{
				logMessage("Failed to create valid buffers", true);
				return false;
			}

			// Initialize input buffer
			m_inputBuffer = m_inputBufferA;
			m_doubleBufferSwitch = false;

			return true;
		}
		catch (const std::exception &e)
		{
			logMessage("Exception creating buffers: " + std::string(e.what()), true);
			return false;
		}
	}

	bool AsioSinkNode::start()
	{
		std::lock_guard<std::mutex> lock(m_bufferMutex);

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

		// Reset state
		m_doubleBufferSwitch = false;
		m_inputBuffer = m_inputBufferA;

		m_running = true;
		logMessage("Started", false);
		return true;
	}

	void AsioSinkNode::stop()
	{
		std::lock_guard<std::mutex> lock(m_bufferMutex);

		if (!m_running)
		{
			return;
		}

		m_running = false;
		logMessage("Stopped", false);
	}

	bool AsioSinkNode::process()
	{
		// No active processing needed - data is provided via provideAsioData
		return true;
	}

	std::shared_ptr<AudioBuffer> AsioSinkNode::getOutputBuffer(int padIndex)
	{
		logMessage("Cannot get output buffer - this is a sink node", true);
		return nullptr;
	}

	bool AsioSinkNode::setInputBuffer(std::shared_ptr<AudioBuffer> buffer, int padIndex)
	{
		if (padIndex != 0)
		{
			logMessage("Invalid input pad index: " + std::to_string(padIndex), true);
			return false;
		}

		std::lock_guard<std::mutex> lock(m_bufferMutex);

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

		// Set the current input buffer (double-buffering is handled in provideAsioData)
		std::shared_ptr<AudioBuffer> currentBuffer =
			m_doubleBufferSwitch ? m_inputBufferB : m_inputBufferA;

		// Copy the incoming buffer to our internal buffer
		if (!currentBuffer->copyFrom(*buffer))
		{
			logMessage("Failed to copy input buffer", true);
			return false;
		}

		return true;
	}

	bool AsioSinkNode::provideAsioData(long doubleBufferIndex, void **asioBuffers)
	{
		std::lock_guard<std::mutex> lock(m_bufferMutex);

		if (!m_running)
		{
			// Fill with silence when not running
			for (size_t i = 0; i < m_asioChannelIndices.size(); i++)
			{
				long channelIndex = m_asioChannelIndices[i];

				if (asioBuffers[channelIndex])
				{
					// Clear buffer to silence
					uint8_t *silenceData = m_silenceBuffer->getChannelData(i);
					if (silenceData)
					{
						convertAudioBufferToAsio(silenceData, asioBuffers[channelIndex], m_bufferSize,
												 m_silenceBuffer->getFormat(), m_asioManager->getSampleType());
					}
				}
			}
			return true;
		}

		// Toggle double buffer for next time and get current buffer
		m_doubleBufferSwitch = !m_doubleBufferSwitch;
		std::shared_ptr<AudioBuffer> currentBuffer =
			m_doubleBufferSwitch ? m_inputBufferA : m_inputBufferB;

		// For each ASIO channel this node handles
		for (size_t i = 0; i < m_asioChannelIndices.size(); i++)
		{
			// Get ASIO buffer pointer for this channel
			long channelIndex = m_asioChannelIndices[i];
			void *asioBuffer = asioBuffers[channelIndex];

			if (!asioBuffer)
			{
				logMessage("Null ASIO buffer for channel " + std::to_string(channelIndex), true);
				continue;
			}

			// Get source buffer pointer from AudioBuffer
			uint8_t *sourceBuffer = currentBuffer->getChannelData(i);

			if (!sourceBuffer)
			{
				logMessage("Null source buffer for channel " + std::to_string(i), true);
				continue;
			}

			// Convert from AudioBuffer's format to ASIO format
			convertAudioBufferToAsio(sourceBuffer, asioBuffer, m_bufferSize,
									 currentBuffer->getFormat(), m_asioManager->getSampleType());
		}

		return true;
	}

	void AsioSinkNode::convertAudioBufferToAsio(uint8_t *sourceBuffer, void *asioBuffer, long frames,
												AVSampleFormat format, int asioType)
	{
		// This function handles converting from FFmpeg's format to ASIO's sample format

		// Handle different ASIO sample types
		switch (asioType)
		{
		case 0: // Int16 LSB
		{
			int16_t *dest = static_cast<int16_t *>(asioBuffer);

			// Convert based on source format
			if (format == AV_SAMPLE_FMT_S16)
			{
				// Direct copy for same format
				memcpy(dest, sourceBuffer, frames * sizeof(int16_t));
			}
			else if (format == AV_SAMPLE_FMT_FLT)
			{
				// Convert float to int16
				float *src = reinterpret_cast<float *>(sourceBuffer);
				for (long i = 0; i < frames; i++)
				{
					float sample = std::max(-1.0f, std::min(1.0f, src[i]));
					dest[i] = static_cast<int16_t>(sample * 32767.0f);
				}
			}
			else if (format == AV_SAMPLE_FMT_DBL)
			{
				// Convert double to int16
				double *src = reinterpret_cast<double *>(sourceBuffer);
				for (long i = 0; i < frames; i++)
				{
					double sample = std::max(-1.0, std::min(1.0, src[i]));
					dest[i] = static_cast<int16_t>(sample * 32767.0);
				}
			}
			else if (format == AV_SAMPLE_FMT_S32)
			{
				// Convert int32 to int16
				int32_t *src = reinterpret_cast<int32_t *>(sourceBuffer);
				for (long i = 0; i < frames; i++)
				{
					dest[i] = static_cast<int16_t>(src[i] >> 16);
				}
			}
			else
			{
				// Just fill with silence if unsupported
				memset(dest, 0, frames * sizeof(int16_t));
				logMessage("Unsupported conversion from format " + std::to_string(format) +
							   " to Int16",
						   true);
			}
			break;
		}

		case 1: // Int24 LSB
		{
			int32_t *dest = static_cast<int32_t *>(asioBuffer); // Actually int24 in low 3 bytes

			if (format == AV_SAMPLE_FMT_S32)
			{
				// Convert int32 to int24 by shifting right 8 bits
				int32_t *src = reinterpret_cast<int32_t *>(sourceBuffer);
				for (long i = 0; i < frames; i++)
				{
					dest[i] = src[i] >> 8;
				}
			}
			else if (format == AV_SAMPLE_FMT_FLT)
			{
				// Convert float to int24
				float *src = reinterpret_cast<float *>(sourceBuffer);
				const float scale = 8388607.0f; // 2^23 - 1
				for (long i = 0; i < frames; i++)
				{
					float sample = std::max(-1.0f, std::min(1.0f, src[i]));
					dest[i] = static_cast<int32_t>(sample * scale);
				}
			}
			else if (format == AV_SAMPLE_FMT_DBL)
			{
				// Convert double to int24
				double *src = reinterpret_cast<double *>(sourceBuffer);
				const double scale = 8388607.0; // 2^23 - 1
				for (long i = 0; i < frames; i++)
				{
					double sample = std::max(-1.0, std::min(1.0, src[i]));
					dest[i] = static_cast<int32_t>(sample * scale);
				}
			}
			else
			{
				// Fill with silence if unsupported
				memset(dest, 0, frames * sizeof(int32_t));
				logMessage("Unsupported conversion from format " + std::to_string(format) +
							   " to Int24",
						   true);
			}
			break;
		}

		case 2: // Int32 LSB
		{
			int32_t *dest = static_cast<int32_t *>(asioBuffer);

			if (format == AV_SAMPLE_FMT_S32)
			{
				// Direct copy for same format
				memcpy(dest, sourceBuffer, frames * sizeof(int32_t));
			}
			else if (format == AV_SAMPLE_FMT_FLT)
			{
				// Convert float to int32
				float *src = reinterpret_cast<float *>(sourceBuffer);
				const float scale = 2147483647.0f; // 2^31 - 1
				for (long i = 0; i < frames; i++)
				{
					float sample = std::max(-1.0f, std::min(1.0f, src[i]));
					dest[i] = static_cast<int32_t>(sample * scale);
				}
			}
			else if (format == AV_SAMPLE_FMT_DBL)
			{
				// Convert double to int32
				double *src = reinterpret_cast<double *>(sourceBuffer);
				const double scale = 2147483647.0; // 2^31 - 1
				for (long i = 0; i < frames; i++)
				{
					double sample = std::max(-1.0, std::min(1.0, src[i]));
					dest[i] = static_cast<int32_t>(sample * scale);
				}
			}
			else
			{
				// Fill with silence if unsupported
				memset(dest, 0, frames * sizeof(int32_t));
				logMessage("Unsupported conversion from format " + std::to_string(format) +
							   " to Int32",
						   true);
			}
			break;
		}

		case 3: // Float32 LSB
		{
			float *dest = static_cast<float *>(asioBuffer);

			if (format == AV_SAMPLE_FMT_FLT)
			{
				// Direct copy for same format
				memcpy(dest, sourceBuffer, frames * sizeof(float));
			}
			else if (format == AV_SAMPLE_FMT_DBL)
			{
				// Convert double to float
				double *src = reinterpret_cast<double *>(sourceBuffer);
				for (long i = 0; i < frames; i++)
				{
					dest[i] = static_cast<float>(src[i]);
				}
			}
			else if (format == AV_SAMPLE_FMT_S32)
			{
				// Convert int32 to float
				int32_t *src = reinterpret_cast<int32_t *>(sourceBuffer);
				const float scale = 1.0f / 2147483648.0f; // 1/2^31
				for (long i = 0; i < frames; i++)
				{
					dest[i] = src[i] * scale;
				}
			}
			else if (format == AV_SAMPLE_FMT_S16)
			{
				// Convert int16 to float
				int16_t *src = reinterpret_cast<int16_t *>(sourceBuffer);
				const float scale = 1.0f / 32768.0f; // 1/2^15
				for (long i = 0; i < frames; i++)
				{
					dest[i] = src[i] * scale;
				}
			}
			else
			{
				// Fill with silence if unsupported
				memset(dest, 0, frames * sizeof(float));
				logMessage("Unsupported conversion from format " + std::to_string(format) +
							   " to Float32",
						   true);
			}
			break;
		}

		default:
			logMessage("Unsupported ASIO sample type: " + std::to_string(asioType), true);
			break;
		}
	}

} // namespace AudioEngine
