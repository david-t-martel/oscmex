#include "AsioSourceNode.h"
#include "AsioManager.h"
#include "AudioEngine.h"
#include <iostream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <string>

extern "C"
{
#include <libavutil/opt.h>
}

namespace AudioEngine
{

	AsioSourceNode::AsioSourceNode(const std::string &name, AudioEngine *engine, AsioManager *asioManager)
		: AudioNode(name, NodeType::ASIO_SOURCE, engine),
		  m_asioManager(asioManager),
		  m_doubleBufferSwitch(false),
		  m_outputBuffer(nullptr),
		  m_outputBufferA(nullptr),
		  m_outputBufferB(nullptr)
	{
		if (!m_asioManager)
		{
			logMessage("Invalid AsioManager pointer", true);
		}
	}

	AsioSourceNode::~AsioSourceNode()
	{
		stop();
	}

	bool AsioSourceNode::configure(const std::map<std::string, std::string> &params,
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

		// Create output buffers
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

	bool AsioSourceNode::createBuffers()
	{
		// Create two buffers for double-buffering
		try
		{
			m_outputBufferA = std::make_shared<AudioBuffer>(m_bufferSize, m_sampleRate,
															m_format, m_channelLayout);

			m_outputBufferB = std::make_shared<AudioBuffer>(m_bufferSize, m_sampleRate,
															m_format, m_channelLayout);

			if (!m_outputBufferA->isValid() || !m_outputBufferB->isValid())
			{
				logMessage("Failed to create valid output buffers", true);
				return false;
			}

			// Start with buffer A
			m_outputBuffer = m_outputBufferA;
			m_doubleBufferSwitch = false;

			return true;
		}
		catch (const std::exception &e)
		{
			logMessage("Exception creating buffers: " + std::string(e.what()), true);
			return false;
		}
	}

	bool AsioSourceNode::start()
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
		m_outputBuffer = m_outputBufferA;

		m_running = true;
		logMessage("Started", false);
		return true;
	}

	void AsioSourceNode::stop()
	{
		std::lock_guard<std::mutex> lock(m_bufferMutex);

		if (!m_running)
		{
			return;
		}

		m_running = false;
		logMessage("Stopped", false);
	}

	bool AsioSourceNode::process()
	{
		// No active processing needed - data is received via receiveAsioData
		return true;
	}

	std::shared_ptr<AudioBuffer> AsioSourceNode::getOutputBuffer(int padIndex)
	{
		if (padIndex != 0)
		{
			logMessage("Invalid output pad index: " + std::to_string(padIndex), true);
			return nullptr;
		}

		std::lock_guard<std::mutex> lock(m_bufferMutex);

		if (!m_running || !m_outputBuffer)
		{
			return nullptr;
		}

		// Return current buffer
		return m_outputBuffer;
	}

	bool AsioSourceNode::setInputBuffer(std::shared_ptr<AudioBuffer> buffer, int padIndex)
	{
		logMessage("Cannot set input buffer - this is a source node", true);
		return false;
	}

	bool AsioSourceNode::receiveAsioData(long doubleBufferIndex, void **asioBuffers)
	{
		std::lock_guard<std::mutex> lock(m_bufferMutex);

		if (!m_running)
		{
			return false;
		}

		// Get the current buffer to fill based on double-buffering
		std::shared_ptr<AudioBuffer> currentBuffer = m_doubleBufferSwitch ? m_outputBufferB : m_outputBufferA;

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

			// Get destination buffer pointer from AudioBuffer
			uint8_t *destBuffer = currentBuffer->getChannelData(i);

			if (!destBuffer)
			{
				logMessage("Null destination buffer for channel " + std::to_string(i), true);
				continue;
			}

			// Convert from ASIO format to AudioBuffer's format
			convertAsioToAudioBuffer(asioBuffer, destBuffer, m_bufferSize,
									 m_asioManager->getSampleType(), currentBuffer->getFormat());
		}

		// Toggle double buffer for next time and set current output buffer
		m_doubleBufferSwitch = !m_doubleBufferSwitch;
		m_outputBuffer = currentBuffer;

		return true;
	}

	void AsioSourceNode::convertAsioToAudioBuffer(void *asioBuffer, uint8_t *destBuffer, long frames,
												  int asioType, AVSampleFormat format)
	{
		// This function handles converting from ASIO's sample format to FFmpeg's format

		// Handle different ASIO sample types
		switch (asioType)
		{
		case 0: // Int16 LSB
		{
			int16_t *src = static_cast<int16_t *>(asioBuffer);

			// Convert based on destination format
			if (format == AV_SAMPLE_FMT_S16)
			{
				// Direct copy for same format
				memcpy(destBuffer, src, frames * sizeof(int16_t));
			}
			else if (format == AV_SAMPLE_FMT_FLT)
			{
				// Convert int16 to float
				float *dest = reinterpret_cast<float *>(destBuffer);
				for (long i = 0; i < frames; i++)
				{
					dest[i] = src[i] / 32768.0f;
				}
			}
			else if (format == AV_SAMPLE_FMT_DBL)
			{
				// Convert int16 to double
				double *dest = reinterpret_cast<double *>(destBuffer);
				for (long i = 0; i < frames; i++)
				{
					dest[i] = src[i] / 32768.0;
				}
			}
			else
			{
				logMessage("Unsupported conversion from Int16 to format " +
							   std::to_string(format),
						   true);
			}
			break;
		}

		case 1: // Int24 LSB
		{
			int32_t *src = static_cast<int32_t *>(asioBuffer); // Actually int24 in low 3 bytes

			if (format == AV_SAMPLE_FMT_S32)
			{
				// Convert int24 to int32 by shifting left 8 bits
				int32_t *dest = reinterpret_cast<int32_t *>(destBuffer);
				for (long i = 0; i < frames; i++)
				{
					dest[i] = src[i] << 8;
				}
			}
			else if (format == AV_SAMPLE_FMT_FLT)
			{
				// Convert int24 to float
				float *dest = reinterpret_cast<float *>(destBuffer);
				const float scale = 1.0f / 8388608.0f; // 2^23
				for (long i = 0; i < frames; i++)
				{
					dest[i] = src[i] * scale;
				}
			}
			else if (format == AV_SAMPLE_FMT_DBL)
			{
				// Convert int24 to double
				double *dest = reinterpret_cast<double *>(destBuffer);
				const double scale = 1.0 / 8388608.0; // 2^23
				for (long i = 0; i < frames; i++)
				{
					dest[i] = src[i] * scale;
				}
			}
			else
			{
				logMessage("Unsupported conversion from Int24 to format " +
							   std::to_string(format),
						   true);
			}
			break;
		}

		case 2: // Int32 LSB
		{
			int32_t *src = static_cast<int32_t *>(asioBuffer);

			if (format == AV_SAMPLE_FMT_S32)
			{
				// Direct copy for same format
				memcpy(destBuffer, src, frames * sizeof(int32_t));
			}
			else if (format == AV_SAMPLE_FMT_FLT)
			{
				// Convert int32 to float
				float *dest = reinterpret_cast<float *>(destBuffer);
				const float scale = 1.0f / 2147483648.0f; // 2^31
				for (long i = 0; i < frames; i++)
				{
					dest[i] = src[i] * scale;
				}
			}
			else if (format == AV_SAMPLE_FMT_DBL)
			{
				// Convert int32 to double
				double *dest = reinterpret_cast<double *>(destBuffer);
				const double scale = 1.0 / 2147483648.0; // 2^31
				for (long i = 0; i < frames; i++)
				{
					dest[i] = src[i] * scale;
				}
			}
			else
			{
				logMessage("Unsupported conversion from Int32 to format " +
							   std::to_string(format),
						   true);
			}
			break;
		}

		case 3: // Float32 LSB
		{
			float *src = static_cast<float *>(asioBuffer);

			if (format == AV_SAMPLE_FMT_FLT)
			{
				// Direct copy for same format
				memcpy(destBuffer, src, frames * sizeof(float));
			}
			else if (format == AV_SAMPLE_FMT_DBL)
			{
				// Convert float to double
				double *dest = reinterpret_cast<double *>(destBuffer);
				for (long i = 0; i < frames; i++)
				{
					dest[i] = src[i];
				}
			}
			else if (format == AV_SAMPLE_FMT_S32)
			{
				// Convert float to int32
				int32_t *dest = reinterpret_cast<int32_t *>(destBuffer);
				for (long i = 0; i < frames; i++)
				{
					float sample = std::max(-1.0f, std::min(1.0f, src[i]));
					dest[i] = static_cast<int32_t>(sample * 2147483647.0f);
				}
			}
			else if (format == AV_SAMPLE_FMT_S16)
			{
				// Convert float to int16
				int16_t *dest = reinterpret_cast<int16_t *>(destBuffer);
				for (long i = 0; i < frames; i++)
				{
					float sample = std::max(-1.0f, std::min(1.0f, src[i]));
					dest[i] = static_cast<int16_t>(sample * 32767.0f);
				}
			}
			else
			{
				logMessage("Unsupported conversion from Float32 to format " +
							   std::to_string(format),
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
