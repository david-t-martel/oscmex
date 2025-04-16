#pragma once

#include "AudioNode.h"
#include <thread>
#include <queue>
#include <mutex>
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
	 * @brief Node for reading and decoding audio files
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
		 * @brief Seek to a position in the file
		 *
		 * @param position Position in seconds
		 * @return true if seek was successful
		 */
		bool seekTo(double position);

		/**
		 * @brief Get the current position in the file
		 *
		 * @return Position in seconds
		 */
		double getCurrentPosition() const;

		/**
		 * @brief Get the duration of the file
		 *
		 * @return Duration in seconds
		 */
		double getDuration() const;

	private:
		// File path
		std::string m_filePath;

		// FFmpeg structures for file reading
		AVFormatContext *m_formatCtx;
		AVCodecContext *m_codecCtx;
		SwrContext *m_swrCtx;
		int m_audioStreamIndex;

		// Thread for file reading
		std::thread m_readerThread;
		void readerThreadFunc();

		// Queue of output buffers
		std::queue<std::shared_ptr<AudioBuffer>> m_outputQueue;
		std::mutex m_queueMutex;
		std::condition_variable m_queueCondVar;

		// Thread control
		std::atomic<bool> m_stopRequested;

		// Buffer management
		std::shared_ptr<AudioBuffer> m_currentOutputBuffer;
		int m_outputQueueMaxSize;

		// File information
		double m_duration;
		std::atomic<double> m_currentPosition;

		// Methods for file handling
		bool openFile();
		void closeFile();
		bool readNextFrame(AVFrame *frame);
	};

} // namespace AudioEngine
