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
	 * @brief Node for encoding and writing audio to files
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
		 * @brief Flush any remaining audio data to file
		 *
		 * @return true if flush was successful
		 */
		bool flush();

	private:
		// File path
		std::string m_filePath;

		// Encoder settings
		std::string m_encoderName;
		std::string m_format;
		int m_bitrate;

		// FFmpeg structures for file writing
		AVFormatContext *m_formatCtx;
		AVCodecContext *m_codecCtx;
		SwrContext *m_swrCtx;
		AVStream *m_audioStream;

		// Thread for file writing
		std::thread m_writerThread;
		void writerThreadFunc();

		// Queue of input buffers
		std::queue<std::shared_ptr<AudioBuffer>> m_inputQueue;
		std::mutex m_queueMutex;
		std::condition_variable m_queueCondVar;

		// Thread control
		std::atomic<bool> m_stopRequested;

		// Methods for file handling
		bool openFile();
		void closeFile();
		bool writeFrame(AVFrame *frame);
		bool finalizeFile();

		// Buffer management
		int m_inputQueueMaxSize;
		int64_t m_nextPts;
	};

} // namespace AudioEngine
