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
	 * @brief Node for reading audio from a file
	 */
	class FileSourceNode : public AudioNode
	{
	public:
		/**
		 * @brief Create a new file source node
		 *
		 * @param name Node name
		 * @param engine Reference to the audio engine
		 */
		FileSourceNode(const std::string &name, AudioEngine *engine);

		/**
		 * @brief Destroy the file source node
		 */
		~FileSourceNode();

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

		int getInputPadCount() const override { return 0; }	 // No inputs
		int getOutputPadCount() const override { return 1; } // One output

		/**
		 * @brief Seek to a specific position in the file
		 *
		 * @param position Position in seconds
		 * @return true if seek was successful
		 */
		bool seekTo(double position);

		/**
		 * @brief Get the current playback position
		 *
		 * @return Current position in seconds
		 */
		double getCurrentPosition() const;

		/**
		 * @brief Get the file duration
		 *
		 * @return Duration in seconds
		 */
		double getDuration() const;

		/**
		 * @brief Get the file path
		 *
		 * @return File path
		 */
		std::string getFilePath() const { return m_filePath; }

	private:
		// File information
		std::string m_filePath;
		double m_duration;

		// FFmpeg objects
		AVFormatContext *m_formatContext;
		AVCodecContext *m_codecContext;
		SwrContext *m_swrContext;
		int m_audioStreamIndex;

		// Decoder state
		AVPacket *m_packet;
		AVFrame *m_decodedFrame;
		AVFrame *m_convertedFrame;

		// Reader thread
		std::thread m_readerThread;
		std::atomic<bool> m_stopThread;

		// Output buffer queue
		std::queue<std::shared_ptr<AudioBuffer>> m_outputQueue;
		std::mutex m_queueMutex;
		std::condition_variable m_queueCondVar;

		// Current position tracking
		std::atomic<double> m_currentPosition;

		// Maximum buffer queue size
		const int MAX_QUEUE_SIZE = 4;

		// Helper methods
		void readerThreadFunc();
		bool openFile();
		void closeFile();
		bool readNextFrame();
		bool decodePacket();
		bool resampleFrame();
		std::shared_ptr<AudioBuffer> createBufferFromFrame();

		// Tracks when we're at the end of file but still have buffers queued
		bool m_endOfFile;

		// Timing information
		AVRational m_timeBase;
		double m_startTime;
	};

} // namespace AudioEngine
