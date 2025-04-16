#include "FileSinkNode.h"
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

	FileSinkNode::FileSinkNode(const std::string &name, AudioEngine *engine)
		: AudioNode(name, NodeType::FILE_SINK, engine),
		  m_formatContext(nullptr),
		  m_codecContext(nullptr),
		  m_swrContext(nullptr),
		  m_stream(nullptr),
		  m_frame(nullptr),
		  m_packet(nullptr),
		  m_stopThread(false),
		  m_frameCount(0),
		  m_duration(0.0),
		  m_startPts(0),
		  m_lastPts(0),
		  m_bitrate(192000),
		  m_quality(5),
		  m_useCompression(true)
	{
		// Default to mp3 format if not specified
		m_format = "mp3";
		m_codec = "libmp3lame";
	}

	FileSinkNode::~FileSinkNode()
	{
		stop();
		closeFile();
	}

	bool FileSinkNode::configure(const std::map<std::string, std::string> &params,
								 double sampleRate, long bufferSize,
								 AVSampleFormat format, const AVChannelLayout &layout)
	{
		if (m_running)
		{
			logMessage("Cannot configure while running", true);
			return false;
		}

		// Set the base audio parameters
		m_sampleRate = sampleRate;
		m_bufferSize = bufferSize;
		m_format = format;
		int ret = av_channel_layout_copy(&m_channelLayout, &layout);
		if (ret < 0)
		{
			logMessage("Failed to copy channel layout", true);
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

		// Get optional format
		it = params.find("format");
		if (it != params.end())
		{
			m_format = it->second;
		}

		// Get optional codec
		it = params.find("codec");
		if (it != params.end())
		{
			m_codec = it->second;
		}

		// Get optional bitrate
		it = params.find("bitrate");
		if (it != params.end())
		{
			try
			{
				m_bitrate = std::stoi(it->second);
			}
			catch (const std::exception &e)
			{
				logMessage("Invalid bitrate value: " + it->second, true);
				return false;
			}
		}

		// Get optional quality (for VBR codecs)
		it = params.find("quality");
		if (it != params.end())
		{
			try
			{
				m_quality = std::stoi(it->second);
			}
			catch (const std::exception &e)
			{
				logMessage("Invalid quality value: " + it->second, true);
				return false;
			}
		}

		// Get optional compression flag
		it = params.find("compression");
		if (it != params.end())
		{
			m_useCompression = (it->second == "true" || it->second == "1" || it->second == "yes");
		}

		// File will be opened when start() is called
		m_configured = true;
		logMessage("Configured for file: " + m_filePath + " (Format: " + m_format +
					   ", Codec: " + m_codec + ")",
				   false);

		return true;
	}

	bool FileSinkNode::openFile()
	{
		// Close any previously opened file
		closeFile();

		// Allocate format context
		int ret = avformat_alloc_output_context2(&m_formatContext, NULL,
												 m_format.c_str(), m_filePath.c_str());
		if (ret < 0 || !m_formatContext)
		{
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			logMessage("Failed to allocate output format context: " + std::string(errbuf), true);
			return false;
		}

		// Find encoder
		const AVCodec *codec = avcodec_find_encoder_by_name(m_codec.c_str());
		if (!codec)
		{
			logMessage("Codec not found: " + m_codec, true);
			closeFile();
			return false;
		}

		// Create audio stream
		m_stream = avformat_new_stream(m_formatContext, codec);
		if (!m_stream)
		{
			logMessage("Failed to create output stream", true);
			closeFile();
			return false;
		}
		m_stream->id = m_formatContext->nb_streams - 1;
		m_stream->time_base = (AVRational){1, (int)m_sampleRate};

		// Allocate codec context
		m_codecContext = avcodec_alloc_context3(codec);
		if (!m_codecContext)
		{
			logMessage("Failed to allocate codec context", true);
			closeFile();
			return false;
		}

		// Set codec parameters
		m_codecContext->sample_fmt = m_format;
		m_codecContext->bit_rate = m_bitrate;
		m_codecContext->sample_rate = m_sampleRate;

		// Set channel layout
		ret = av_channel_layout_copy(&m_codecContext->ch_layout, &m_channelLayout);
		if (ret < 0)
		{
			logMessage("Failed to copy channel layout to codec context", true);
			closeFile();
			return false;
		}

		// Set codec time base
		m_codecContext->time_base = (AVRational){1, (int)m_sampleRate};

		// Some formats want stream headers to be separate
		if (m_formatContext->oformat->flags & AVFMT_GLOBALHEADER)
			m_codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

		// Set quality for VBR codecs
		if (m_useCompression && (m_codec == "libmp3lame" || m_codec == "libvorbis"))
		{
			ret = av_opt_set_int(m_codecContext, "compression_level", m_quality, 0);
			if (ret < 0)
			{
				logMessage("Failed to set compression level", false);
				// Non-fatal, continue
			}
		}

		// Open encoder
		ret = avcodec_open2(m_codecContext, codec, NULL);
		if (ret < 0)
		{
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			logMessage("Failed to open encoder: " + std::string(errbuf), true);
			closeFile();
			return false;
		}

		// Copy stream parameters from codec context
		ret = avcodec_parameters_from_context(m_stream->codecpar, m_codecContext);
		if (ret < 0)
		{
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			logMessage("Failed to copy stream parameters: " + std::string(errbuf), true);
			closeFile();
			return false;
		}

		// Open output file (if not using memory output)
		if (!(m_formatContext->oformat->flags & AVFMT_NOFILE))
		{
			ret = avio_open(&m_formatContext->pb, m_filePath.c_str(), AVIO_FLAG_WRITE);
			if (ret < 0)
			{
				char errbuf[AV_ERROR_MAX_STRING_SIZE];
				av_strerror(ret, errbuf, sizeof(errbuf));
				logMessage("Failed to open output file: " + std::string(errbuf), true);
				closeFile();
				return false;
			}
		}

		// Write header
		ret = avformat_write_header(m_formatContext, NULL);
		if (ret < 0)
		{
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			logMessage("Failed to write header: " + std::string(errbuf), true);
			closeFile();
			return false;
		}

		// Allocate frame and packet
		m_frame = av_frame_alloc();
		m_packet = av_packet_alloc();

		if (!m_frame || !m_packet)
		{
			logMessage("Failed to allocate frame or packet", true);
			closeFile();
			return false;
		}

		// Initialize frame properties
		m_frame->format = m_codecContext->sample_fmt;
		m_frame->nb_samples = m_bufferSize;
		m_frame->channel_layout = 0; // deprecated, use ch_layout
		m_frame->sample_rate = m_sampleRate;

		ret = av_channel_layout_copy(&m_frame->ch_layout, &m_channelLayout);
		if (ret < 0)
		{
			logMessage("Failed to copy channel layout to frame", true);
			closeFile();
			return false;
		}

		// Allocate frame buffers
		ret = av_frame_get_buffer(m_frame, 0);
		if (ret < 0)
		{
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			logMessage("Failed to allocate frame buffers: " + std::string(errbuf), true);
			closeFile();
			return false;
		}

		// Initialize frame counter for PTS calculation
		m_frameCount = 0;
		m_startPts = 0;
		m_lastPts = 0;

		logMessage("File opened successfully: " + m_filePath, false);

		return true;
	}

	void FileSinkNode::closeFile()
	{
		// Write trailer if format context exists
		if (m_formatContext)
		{
			// Flush encoder
			if (m_codecContext)
			{
				writeDelayedPackets();
			}

			// Write trailer
			int ret = av_write_trailer(m_formatContext);
			if (ret < 0)
			{
				char errbuf[AV_ERROR_MAX_STRING_SIZE];
				av_strerror(ret, errbuf, sizeof(errbuf));
				logMessage("Error writing trailer: " + std::string(errbuf), true);
			}
		}

		// Close output file
		if (m_formatContext && !(m_formatContext->oformat->flags & AVFMT_NOFILE) &&
			m_formatContext->pb)
		{
			avio_closep(&m_formatContext->pb);
		}

		// Free packet
		if (m_packet)
		{
			av_packet_free(&m_packet);
			m_packet = nullptr;
		}

		// Free frame
		if (m_frame)
		{
			av_frame_free(&m_frame);
			m_frame = nullptr;
		}

		// Free codec context
		if (m_codecContext)
		{
			avcodec_free_context(&m_codecContext);
			m_codecContext = nullptr;
		}

		// Free format context (also frees streams)
		if (m_formatContext)
		{
			avformat_free_context(m_formatContext);
			m_formatContext = nullptr;
		}

		m_stream = nullptr;
	}

	bool FileSinkNode::start()
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

		// Open the output file
		if (!openFile())
		{
			logMessage("Failed to open output file", true);
			return false;
		}

		// Clear the input queue
		{
			std::lock_guard<std::mutex> lock(m_queueMutex);
			while (!m_inputQueue.empty())
			{
				m_inputQueue.pop();
			}
		}

		// Reset state
		m_stopThread = false;

		// Start the writer thread
		try
		{
			m_writerThread = std::thread(&FileSinkNode::writerThreadFunc, this);
			m_running = true;
			logMessage("Started", false);
			return true;
		}
		catch (const std::exception &e)
		{
			logMessage("Failed to start writer thread: " + std::string(e.what()), true);
			return false;
		}
	}

	void FileSinkNode::stop()
	{
		if (!m_running)
		{
			return;
		}

		// Signal the writer thread to stop
		m_stopThread = true;

		// Notify thread if it's waiting
		{
			std::lock_guard<std::mutex> lock(m_queueMutex);
			m_queueCondVar.notify_all();
		}

		// Wait for the writer thread to finish
		if (m_writerThread.joinable())
		{
			m_writerThread.join();
		}

		// Finalize the file
		flush();
		closeFile();

		m_running = false;
		logMessage("Stopped", false);
	}

	bool FileSinkNode::process()
	{
		// This node doesn't need explicit processing since writing happens in the writer thread
		return true;
	}

	std::shared_ptr<AudioBuffer> FileSinkNode::getOutputBuffer(int padIndex)
	{
		logMessage("Cannot get output buffer - this is a sink node", true);
		return nullptr;
	}

	bool FileSinkNode::setInputBuffer(std::shared_ptr<AudioBuffer> buffer, int padIndex)
	{
		if (padIndex != 0)
		{
			logMessage("Invalid input pad index: " + std::to_string(padIndex), true);
			return false;
		}

		if (!m_running)
		{
			logMessage("Cannot set input buffer - not running", true);
			return false;
		}

		if (!buffer)
		{
			logMessage("Cannot set null buffer", true);
			return false;
		}

		// Add buffer to the queue
		{
			std::unique_lock<std::mutex> lock(m_queueMutex);

			// Wait if queue is full
			if (m_inputQueue.size() >= MAX_QUEUE_SIZE)
			{
				m_queueCondVar.wait(lock, [this]
									{ return m_inputQueue.size() < MAX_QUEUE_SIZE || m_stopThread; });

				if (m_stopThread)
				{
					return false;
				}
			}

			m_inputQueue.push(buffer);
		}

		// Notify writer thread
		m_queueCondVar.notify_one();

		return true;
	}

	void FileSinkNode::writerThreadFunc()
	{
		while (!m_stopThread)
		{
			// Get buffer from the queue
			std::shared_ptr<AudioBuffer> buffer = nullptr;
			{
				std::unique_lock<std::mutex> lock(m_queueMutex);

				if (m_inputQueue.empty())
				{
					// Wait for data
					m_queueCondVar.wait_for(lock, std::chrono::milliseconds(100), [this]
											{ return !m_inputQueue.empty() || m_stopThread; });
				}

				if (m_stopThread)
				{
					break;
				}

				if (!m_inputQueue.empty())
				{
					buffer = m_inputQueue.front();
					m_inputQueue.pop();
					m_queueCondVar.notify_one();
				}
			}

			if (buffer)
			{
				// Process the buffer
				if (!processBuffer(buffer))
				{
					logMessage("Error processing buffer", true);
				}
			}
		}
	}

	bool FileSinkNode::processBuffer(std::shared_ptr<AudioBuffer> buffer)
	{
		if (!m_running || !m_formatContext || !m_codecContext || !m_frame || !m_packet)
		{
			return false;
		}

		// Make sure the frame is writeable
		int ret = av_frame_make_writable(m_frame);
		if (ret < 0)
		{
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			logMessage("Error making frame writable: " + std::string(errbuf), true);
			return false;
		}

		// Copy buffer data to frame
		if (!copyBufferToFrame(buffer))
		{
			logMessage("Error copying buffer to frame", true);
			return false;
		}

		// Set frame PTS
		m_frame->pts = m_frameCount;
		m_lastPts = m_frameCount;
		m_frameCount += m_frame->nb_samples;

		// Encode frame
		return encodeFrame(m_frame);
	}

	bool FileSinkNode::copyBufferToFrame(std::shared_ptr<AudioBuffer> buffer)
	{
		if (!buffer || !m_frame)
		{
			return false;
		}

		// Check if buffer has enough samples
		if (buffer->getFrameCount() != m_frame->nb_samples)
		{
			logMessage("Buffer size mismatch: expected " + std::to_string(m_frame->nb_samples) +
						   " samples, got " + std::to_string(buffer->getFrameCount()),
					   true);
			// TODO: Handle partial buffers or buffer size mismatches
			return false;
		}

		// Copy data from buffer to frame
		for (int i = 0; i < buffer->getPlaneCount() && i < AV_NUM_DATA_POINTERS; i++)
		{
			if (buffer->getPlaneData(i) && m_frame->data[i])
			{
				int linesize = buffer->isPlanar() ? buffer->getBytesPerSample() : buffer->getBytesPerSample() * buffer->getChannelCount();

				int size = buffer->getFrameCount() * linesize;
				memcpy(m_frame->data[i], buffer->getPlaneData(i), size);
			}
		}

		return true;
	}

	bool FileSinkNode::encodeFrame(AVFrame *frame)
	{
		int ret;

		// Send frame to encoder
		ret = avcodec_send_frame(m_codecContext, frame);
		if (ret < 0)
		{
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			logMessage("Error sending frame to encoder: " + std::string(errbuf), true);
			return false;
		}

		// Get all available packets
		while (ret >= 0)
		{
			ret = avcodec_receive_packet(m_codecContext, m_packet);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			{
				// Need more frames or end of stream
				return true;
			}
			else if (ret < 0)
			{
				char errbuf[AV_ERROR_MAX_STRING_SIZE];
				av_strerror(ret, errbuf, sizeof(errbuf));
				logMessage("Error receiving packet from encoder: " + std::string(errbuf), true);
				return false;
			}

			// Rescale packet timestamps
			av_packet_rescale_ts(m_packet, m_codecContext->time_base, m_stream->time_base);
			m_packet->stream_index = m_stream->index;

			// Write packet to file
			ret = av_interleaved_write_frame(m_formatContext, m_packet);
			if (ret < 0)
			{
				char errbuf[AV_ERROR_MAX_STRING_SIZE];
				av_strerror(ret, errbuf, sizeof(errbuf));
				logMessage("Error writing packet to file: " + std::string(errbuf), true);
				return false;
			}

			// Reset packet for next use
			av_packet_unref(m_packet);
		}

		return true;
	}

	bool FileSinkNode::writeDelayedPackets()
	{
		if (!m_codecContext || !m_packet)
		{
			return false;
		}

		// Send NULL frame to signal end of stream
		int ret = avcodec_send_frame(m_codecContext, NULL);
		if (ret < 0)
		{
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			logMessage("Error sending flush frame: " + std::string(errbuf), true);
			return false;
		}

		// Get all delayed packets
		while (true)
		{
			ret = avcodec_receive_packet(m_codecContext, m_packet);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			{
				// No more packets
				break;
			}
			else if (ret < 0)
			{
				char errbuf[AV_ERROR_MAX_STRING_SIZE];
				av_strerror(ret, errbuf, sizeof(errbuf));
				logMessage("Error receiving delayed packet: " + std::string(errbuf), true);
				return false;
			}

			// Rescale packet timestamps
			av_packet_rescale_ts(m_packet, m_codecContext->time_base, m_stream->time_base);
			m_packet->stream_index = m_stream->index;

			// Write packet to file
			ret = av_interleaved_write_frame(m_formatContext, m_packet);
			if (ret < 0)
			{
				char errbuf[AV_ERROR_MAX_STRING_SIZE];
				av_strerror(ret, errbuf, sizeof(errbuf));
				logMessage("Error writing delayed packet: " + std::string(errbuf), true);
				return false;
			}

			// Reset packet for next use
			av_packet_unref(m_packet);
		}

		return true;
	}

	bool FileSinkNode::flush()
	{
		if (!m_running || !m_formatContext || !m_codecContext)
		{
			return false;
		}

		// Flush any frames in the encoder
		return writeDelayedPackets();
	}

	int64_t FileSinkNode::getFileSize() const
	{
		if (!m_formatContext || !m_formatContext->pb)
		{
			return 0;
		}

		return avio_size(m_formatContext->pb);
	}

	double FileSinkNode::getDuration() const
	{
		if (m_sampleRate <= 0)
		{
			return 0.0;
		}

		return m_frameCount / m_sampleRate;
	}

} // namespace AudioEngine
