#pragma once

#include "AudioNode.h"
#include <string>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>
#include <atomic>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}

namespace AudioEngine
{

	/**
	 * @brief Node for writing audio to a file
	 */
	class FileSinkNode : public AudioNode
	{
	public:
		/**
		 * @brief Create a new file sink node
		 *
		 * @param name Node name
		 * @param engine Reference to the audio engine
		 */
		FileSinkNode(const std::string &name, AudioEngine *engine);

		/**
		 * @brief Destroy the file sink node
		 */
		~FileSinkNode();

		// AudioNode interface implementations
		bool configure(const std::map<std::string, std::string> &params,
					   double sampleRate,
					   long bufferSize,
					   AVSampleFormat format,
					   const AVChannelLayout &layout) override;

		bool start() override;
		bool process() override;
		void stop() override;

		std::shared_ptr<AudioBuffer> getOutputBuffer(int padIndex) override;
		bool setInputBuffer(std::shared_ptr<AudioBuffer> buffer, int padIndex) override;

		int getInputPadCount() const override { return 1; }	 // One input
		int getOutputPadCount() const override { return 0; } // No outputs

		/**
		 * @brief Flush any remaining audio data to the file and finalize
		 */
		bool flush();

		/**
		 * @brief Get the file path
		 *
		 * @return File path
		 */
		std::string getFilePath() const { return m_filePath; }

		/**
		 * @brief Get the current file size in bytes
		 *
		 * @return File size in bytes
		 */
		int64_t getFileSize() const;

		/**
		 * @brief Get the current write duration
		 *
		 * @return Duration in seconds
		 */
		double getDuration() const;

	private:
		// File information
		std::string m_filePath;
		std::string m_format;
		std::string m_codec;
		int m_bitrate;

		// FFmpeg objects
		AVFormatContext *m_formatContext;
		AVCodecContext *m_codecContext;
		SwrContext *m_swrContext;
		AVStream *m_stream;

		// Encoder state
		AVFrame *m_frame;
		AVPacket *m_packet;

		// Writer thread
		std::thread m_writerThread;
		std::atomic<bool> m_stopThread;

		// Input buffer queue
		std::queue<std::shared_ptr<AudioBuffer>> m_inputQueue;
		std::mutex m_queueMutex;
		std::condition_variable m_queueCondVar;

		// Duration tracking
		std::atomic<int64_t> m_frameCount;
		double m_duration;
		int64_t m_startPts;
		int64_t m_lastPts;

		// Maximum buffer queue size
		const int MAX_QUEUE_SIZE = 4;

		// Helper methods
		void writerThreadFunc();
		bool openFile();
		void closeFile();
		bool processBuffer(std::shared_ptr<AudioBuffer> buffer);
		bool encodeFrame(AVFrame *frame);

		// Initialize encoding frame
		bool initFrame();

		// Copy buffer to frame
		bool copyBufferToFrame(std::shared_ptr<AudioBuffer> buffer);

		// Write any delayed packets (after flushing encoder)
		bool writeDelayedPackets();

		// Encoding options
		int m_quality;
		bool m_useCompression;
	};

} // namespace AudioEngine
