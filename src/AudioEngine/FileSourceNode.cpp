#include "FileSourceNode.h"
#include "AudioEngine.h"
#include <iostream>
#include <iomanip>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

namespace AudioEngine
{

	FileSourceNode::FileSourceNode(const std::string &name, AudioEngine *engine)
		: AudioNode(name, NodeType::FILE_SOURCE, engine),
		  m_formatContext(nullptr),
		  m_codecContext(nullptr),
		  m_swrContext(nullptr),
		  m_audioStreamIndex(-1),
		  m_packet(nullptr),
		  m_decodedFrame(nullptr),
		  m_convertedFrame(nullptr),
		  m_stopThread(false),
		  m_currentPosition(0.0),
		  m_duration(0.0),
		  m_endOfFile(false),
		  m_startTime(0.0)
	{
		// Initialize time base to a default value
		m_timeBase.num = 1;
		m_timeBase.den = 1;
	}

	FileSourceNode::~FileSourceNode()
	{
		stop();
		closeFile();
	}

	bool FileSourceNode::configure(const std::map<std::string, std::string> &params,
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

		// Get file path
		auto it = params.find("file_path");
		if (it == params.end())
		{
			logMessage("Missing required 'file_path' parameter", true);
			return false;
		}
		m_filePath = it->second;

		// Open the file and prepare decoder
		if (!openFile())
		{
			return false;
		}

		m_configured = true;
		logMessage("Configured for file: " + m_filePath + " (Duration: " +
					   std::to_string(m_duration) + "s)",
				   false);

		return true;
	}

	bool FileSourceNode::openFile()
	{
		// Close any previously opened file
		closeFile();

		// Open the input file
		int ret = avformat_open_input(&m_formatContext, m_filePath.c_str(), nullptr, nullptr);
		if (ret < 0)
		{
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			logMessage("Failed to open file: " + std::string(errbuf), true);
			return false;
		}

		// Read stream information
		ret = avformat_find_stream_info(m_formatContext, nullptr);
		if (ret < 0)
		{
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			logMessage("Failed to find stream info: " + std::string(errbuf), true);
			avformat_close_input(&m_formatContext);
			m_formatContext = nullptr;
			return false;
		}

		// Find the audio stream
		m_audioStreamIndex = av_find_best_stream(m_formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
		if (m_audioStreamIndex < 0)
		{
			logMessage("No audio stream found in file", true);
			avformat_close_input(&m_formatContext);
			m_formatContext = nullptr;
			return false;
		}

		// Get the audio stream
		AVStream *stream = m_formatContext->streams[m_audioStreamIndex];
		m_timeBase = stream->time_base;

		// Get the codec parameters
		const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
		if (!codec)
		{
			logMessage("Unsupported codec", true);
			avformat_close_input(&m_formatContext);
			m_formatContext = nullptr;
			return false;
		}

		// Allocate codec context
		m_codecContext = avcodec_alloc_context3(codec);
		if (!m_codecContext)
		{
			logMessage("Failed to allocate codec context", true);
			avformat_close_input(&m_formatContext);
			m_formatContext = nullptr;
			return false;
		}

		// Copy codec parameters to codec context
		ret = avcodec_parameters_to_context(m_codecContext, stream->codecpar);
		if (ret < 0)
		{
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			logMessage("Failed to copy codec parameters: " + std::string(errbuf), true);
			avcodec_free_context(&m_codecContext);
			avformat_close_input(&m_formatContext);
			m_formatContext = nullptr;
			return false;
		}

		// Open codec
		ret = avcodec_open2(m_codecContext, codec, nullptr);
		if (ret < 0)
		{
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			logMessage("Failed to open codec: " + std::string(errbuf), true);
			avcodec_free_context(&m_codecContext);
			avformat_close_input(&m_formatContext);
			m_formatContext = nullptr;
			return false;
		}

		// Create packet and frames
		m_packet = av_packet_alloc();
		m_decodedFrame = av_frame_alloc();
		m_convertedFrame = av_frame_alloc();

		if (!m_packet || !m_decodedFrame || !m_convertedFrame)
		{
			logMessage("Failed to allocate packet or frames", true);
			if (m_packet)
				av_packet_free(&m_packet);
			if (m_decodedFrame)
				av_frame_free(&m_decodedFrame);
			if (m_convertedFrame)
				av_frame_free(&m_convertedFrame);
			avcodec_free_context(&m_codecContext);
			avformat_close_input(&m_formatContext);
			m_formatContext = nullptr;
			m_packet = nullptr;
			m_decodedFrame = nullptr;
			m_convertedFrame = nullptr;
			return false;
		}

		// Prepare the converted frame
		m_convertedFrame->format = m_format;
		m_convertedFrame->nb_samples = m_bufferSize;
		m_convertedFrame->channel_layout = 0; // deprecated, use ch_layout instead
		m_convertedFrame->sample_rate = m_sampleRate;
		ret = av_channel_layout_copy(&m_convertedFrame->ch_layout, &m_channelLayout);
		if (ret < 0)
		{
			logMessage("Failed to copy channel layout", true);
			closeFile();
			return false;
		}

		// Init resampler
		m_swrContext = swr_alloc();
		if (!m_swrContext)
		{
			logMessage("Failed to allocate resampler context", true);
			closeFile();
			return false;
		}

		// Set resampler options
		av_opt_set_int(m_swrContext, "in_channel_count", m_codecContext->ch_layout.nb_channels, 0);
		av_opt_set_int(m_swrContext, "out_channel_count", m_channelLayout.nb_channels, 0);

		av_opt_set_int(m_swrContext, "in_channel_layout", m_codecContext->ch_layout.u.mask, 0);
		av_opt_set_int(m_swrContext, "out_channel_layout", m_channelLayout.u.mask, 0);

		av_opt_set_int(m_swrContext, "in_sample_rate", m_codecContext->sample_rate, 0);
		av_opt_set_int(m_swrContext, "out_sample_rate", m_sampleRate, 0);

		av_opt_set_sample_fmt(m_swrContext, "in_sample_fmt", m_codecContext->sample_fmt, 0);
		av_opt_set_sample_fmt(m_swrContext, "out_sample_fmt", m_format, 0);

		// Initialize resampler
		ret = swr_init(m_swrContext);
		if (ret < 0)
		{
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			logMessage("Failed to initialize resampler: " + std::string(errbuf), true);
			closeFile();
			return false;
		}

		// Get duration
		if (m_formatContext->duration != AV_NOPTS_VALUE)
		{
			m_duration = m_formatContext->duration / (double)AV_TIME_BASE;
		}
		else if (stream->duration != AV_NOPTS_VALUE)
		{
			m_duration = stream->duration * av_q2d(stream->time_base);
		}
		else
		{
			m_duration = 0.0;
			logMessage("Unknown duration", false);
		}

		// Get start time
		if (m_formatContext->start_time != AV_NOPTS_VALUE)
		{
			m_startTime = m_formatContext->start_time / (double)AV_TIME_BASE;
		}
		else if (stream->start_time != AV_NOPTS_VALUE)
		{
			m_startTime = stream->start_time * av_q2d(stream->time_base);
		}
		else
		{
			m_startTime = 0.0;
		}

		m_currentPosition = 0.0;
		m_endOfFile = false;

		logMessage("File opened successfully, format: " +
					   std::string(m_codecContext->ch_layout.nb_channels > 1 ? "stereo" : "mono") +
					   ", " + std::to_string(m_codecContext->sample_rate) + " Hz",
				   false);

		return true;
	}

	void FileSourceNode::closeFile()
	{
		// Cleanup resources
		if (m_swrContext)
		{
			swr_free(&m_swrContext);
			m_swrContext = nullptr;
		}

		if (m_convertedFrame)
		{
			av_frame_free(&m_convertedFrame);
			m_convertedFrame = nullptr;
		}

		if (m_decodedFrame)
		{
			av_frame_free(&m_decodedFrame);
			m_decodedFrame = nullptr;
		}

		if (m_packet)
		{
			av_packet_free(&m_packet);
			m_packet = nullptr;
		}

		if (m_codecContext)
		{
			avcodec_free_context(&m_codecContext);
			m_codecContext = nullptr;
		}

		if (m_formatContext)
		{
			avformat_close_input(&m_formatContext);
			m_formatContext = nullptr;
		}

		m_audioStreamIndex = -1;
	}

	bool FileSourceNode::start()
	{
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

		// Clear the output queue
		{
			std::lock_guard<std::mutex> lock(m_queueMutex);
			while (!m_outputQueue.empty())
			{
				m_outputQueue.pop();
			}
		}

		// Reset state
		m_endOfFile = false;
		m_stopThread = false;

		// Start the reader thread
		try
		{
			m_readerThread = std::thread(&FileSourceNode::readerThreadFunc, this);
			m_running = true;
			logMessage("Started", false);
			return true;
		}
		catch (const std::exception &e)
		{
			logMessage("Failed to start reader thread: " + std::string(e.what()), true);
			return false;
		}
	}

	void FileSourceNode::stop()
	{
		if (!m_running)
		{
			return;
		}

		// Signal the reader thread to stop
		m_stopThread = true;

		// Notify consumer that we're stopping
		{
			std::lock_guard<std::mutex> lock(m_queueMutex);
			m_queueCondVar.notify_all();
		}

		// Wait for the reader thread to finish
		if (m_readerThread.joinable())
		{
			m_readerThread.join();
		}

		m_running = false;
		logMessage("Stopped", false);
	}

	bool FileSourceNode::process()
	{
		// This node doesn't need explicit processing since reading happens in the reader thread
		return true;
	}

	std::shared_ptr<AudioBuffer> FileSourceNode::getOutputBuffer(int padIndex)
	{
		if (padIndex != 0)
		{
			logMessage("Invalid output pad index: " + std::to_string(padIndex), true);
			return nullptr;
		}

		if (!m_running)
		{
			return nullptr;
		}

		// Get the next buffer from the queue
		std::unique_lock<std::mutex> lock(m_queueMutex);

		// If queue is empty and we've reached EOF, return null
		if (m_outputQueue.empty() && m_endOfFile)
		{
			return nullptr;
		}

		// Wait for data if queue is empty (but not at EOF yet)
		if (m_outputQueue.empty())
		{
			// Wait with timeout
			auto waitResult = m_queueCondVar.wait_for(
				lock, std::chrono::milliseconds(500),
				[this]
				{ return !m_outputQueue.empty() || m_stopThread; });

			if (m_stopThread || !waitResult)
			{
				return nullptr;
			}
		}

		if (m_outputQueue.empty())
		{
			return nullptr;
		}

		// Get buffer from the front of the queue
		auto buffer = m_outputQueue.front();
		m_outputQueue.pop();

		// Notify producer thread if it was waiting
		m_queueCondVar.notify_one();

		return buffer;
	}

	bool FileSourceNode::setInputBuffer(std::shared_ptr<AudioBuffer> buffer, int padIndex)
	{
		logMessage("Cannot set input buffer - this is a source node", true);
		return false;
	}

	void FileSourceNode::readerThreadFunc()
	{
		while (!m_stopThread)
		{
			// Check if output queue is full
			{
				std::unique_lock<std::mutex> lock(m_queueMutex);
				if (m_outputQueue.size() >= MAX_QUEUE_SIZE)
				{
					// Queue is full, wait for some buffers to be consumed
					m_queueCondVar.wait(lock, [this]
										{ return m_outputQueue.size() < MAX_QUEUE_SIZE || m_stopThread; });
				}

				if (m_stopThread)
				{
					break;
				}
			}

			// Read and decode the next frame
			if (!readNextFrame())
			{
				if (!m_endOfFile)
				{
					// Mark that we've reached EOF
					m_endOfFile = true;
					logMessage("End of file reached", false);
				}

				// Sleep a bit to avoid spinning if we've reached EOF
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}

			// Process the read frame
			if (!decodePacket())
			{
				continue;
			}
		}
	}

	bool FileSourceNode::readNextFrame()
	{
		if (!m_formatContext || !m_packet)
		{
			return false;
		}

		// Free the previous packet
		av_packet_unref(m_packet);

		// Read a packet
		int ret = av_read_frame(m_formatContext, m_packet);
		if (ret < 0)
		{
			if (ret == AVERROR_EOF)
			{
				return false; // End of file
			}

			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			logMessage("Error reading frame: " + std::string(errbuf), true);
			return false;
		}

		// Check if packet is from audio stream
		if (m_packet->stream_index != m_audioStreamIndex)
		{
			// Skip non-audio packets
			return readNextFrame();
		}

		return true;
	}

	bool FileSourceNode::decodePacket()
	{
		if (!m_codecContext || !m_packet || !m_decodedFrame)
		{
			return false;
		}

		// Send packet to decoder
		int ret = avcodec_send_packet(m_codecContext, m_packet);
		if (ret < 0)
		{
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			logMessage("Error sending packet to decoder: " + std::string(errbuf), true);
			return false;
		}

		// Receive frames from decoder
		bool success = false;
		while (ret >= 0)
		{
			// Reset the frame
			av_frame_unref(m_decodedFrame);

			ret = avcodec_receive_frame(m_codecContext, m_decodedFrame);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			{
				break;
			}
			else if (ret < 0)
			{
				char errbuf[AV_ERROR_MAX_STRING_SIZE];
				av_strerror(ret, errbuf, sizeof(errbuf));
				logMessage("Error receiving frame from decoder: " + std::string(errbuf), true);
				break;
			}

			// Update current position
			if (m_decodedFrame->pts != AV_NOPTS_VALUE)
			{
				m_currentPosition = m_decodedFrame->pts * av_q2d(m_timeBase);
			}

			// Resample the frame
			if (resampleFrame())
			{
				auto buffer = createBufferFromFrame();
				if (buffer)
				{
					// Add buffer to the queue
					std::lock_guard<std::mutex> lock(m_queueMutex);
					m_outputQueue.push(buffer);
					m_queueCondVar.notify_one();
					success = true;
				}
			}
		}

		return success;
	}

	bool FileSourceNode::resampleFrame()
	{
		if (!m_swrContext || !m_decodedFrame || !m_convertedFrame)
		{
			return false;
		}

		// Allocate frame data if needed
		int ret = av_frame_get_buffer(m_convertedFrame, 0);
		if (ret < 0)
		{
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			logMessage("Error allocating frame data: " + std::string(errbuf), true);
			return false;
		}

		// Make sure the frame data is writable
		ret = av_frame_make_writable(m_convertedFrame);
		if (ret < 0)
		{
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			logMessage("Error making frame writable: " + std::string(errbuf), true);
			return false;
		}

		// Convert
		ret = swr_convert(m_swrContext,
						  m_convertedFrame->data, m_convertedFrame->nb_samples,
						  const_cast<const uint8_t **>(m_decodedFrame->data), m_decodedFrame->nb_samples);
		if (ret < 0)
		{
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			logMessage("Error resampling audio: " + std::string(errbuf), true);
			return false;
		}

		// Set properties
		m_convertedFrame->pts = m_decodedFrame->pts;
		m_convertedFrame->best_effort_timestamp = m_decodedFrame->best_effort_timestamp;
		m_convertedFrame->pkt_pos = m_decodedFrame->pkt_pos;
		m_convertedFrame->pkt_duration = m_decodedFrame->pkt_duration;
		m_convertedFrame->time_base = m_decodedFrame->time_base;

		return true;
	}

	std::shared_ptr<AudioBuffer> FileSourceNode::createBufferFromFrame()
	{
		if (!m_convertedFrame)
		{
			return nullptr;
		}

		// Create a new buffer
		auto buffer = std::make_shared<AudioBuffer>(m_convertedFrame->nb_samples, m_sampleRate,
													m_format, m_channelLayout);

		if (!buffer->isValid())
		{
			logMessage("Failed to create valid buffer", true);
			return nullptr;
		}

		// Copy data from frame to buffer
		for (int i = 0; i < buffer->getPlaneCount() && i < AV_NUM_DATA_POINTERS; i++)
		{
			if (m_convertedFrame->data[i] && buffer->getPlaneData(i))
			{
				int linesize = buffer->isPlanar() ? buffer->getBytesPerSample() : buffer->getBytesPerSample() * buffer->getChannelCount();

				int size = m_convertedFrame->nb_samples * linesize;
				memcpy(buffer->getPlaneData(i), m_convertedFrame->data[i], size);
			}
		}

		return buffer;
	}

	bool FileSourceNode::seekTo(double position)
	{
		if (!m_running || !m_formatContext)
		{
			return false;
		}

		// Calculate timestamp in stream timebase
		int64_t timestamp = static_cast<int64_t>(position / av_q2d(m_timeBase));

		int flags = AVSEEK_FLAG_BACKWARD; // Seek to keyframe before timestamp

		// Seek to the specified position
		int ret = av_seek_frame(m_formatContext, m_audioStreamIndex, timestamp, flags);
		if (ret < 0)
		{
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			logMessage("Error seeking: " + std::string(errbuf), true);
			return false;
		}

		// Flush codec buffers
		avcodec_flush_buffers(m_codecContext);

		// Clear the output queue
		{
			std::lock_guard<std::mutex> lock(m_queueMutex);
			while (!m_outputQueue.empty())
			{
				m_outputQueue.pop();
			}
		}

		// Update position
		m_currentPosition = position;
		m_endOfFile = false;

		logMessage("Seeked to position: " + std::to_string(position) + "s", false);

		return true;
	}

	double FileSourceNode::getCurrentPosition() const
	{
		return m_currentPosition;
	}

	double FileSourceNode::getDuration() const
	{
		return m_duration;
	}

} // namespace AudioEngine
