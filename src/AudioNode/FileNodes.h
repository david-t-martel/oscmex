#pragma once

#include "AudioNode.h"
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <optional>

// Forward declarations for FFmpeg types
extern "C"
{
	struct AVFormatContext;
	struct AVCodecContext;
	struct SwrContext;
	struct AVFrame;
	struct AVPacket;
}

namespace AudioEngine
{

	/**
	 * @brief Node that reads audio from a file
	 *
	 * Uses FFmpeg to decode audio files of various formats
	 * and provides them as a source in the audio graph.
	 */
	class FileSourceNode : public AudioNode
	{
	public:
		/**
		 * @brief Construct a new FileSourceNode
		 *
		 * @param name Unique node name
		 * @param engine Pointer to parent engine
		 */
		FileSourceNode(std::string name, AudioEngine *engine);

		/**
		 * @brief Destroy the FileSourceNode
		 */
		~FileSourceNode();

		/**
		 * @brief Configure the node with parameters
		 *
		 * Expected params:
		 * - "path": Path to the audio file
		 * - "loop" (optional): "true" or "false" to enable looping
		 *
		 * @param params Configuration parameters
		 * @param sampleRate System sample rate
		 * @param bufferSize System buffer size
		 * @param internalFormat Internal sample format
		 * @param internalLayout Internal channel layout
		 * @return true if configuration succeeded
		 * @return false if configuration failed
		 */
		bool configure(
			const std::map<std::string, std::string> &params,
			double sampleRate,
			long bufferSize,
			AVSampleFormat internalFormat,
			const AVChannelLayout &internalLayout) override;

		/**
		 * @brief Start the node
		 *
		 * Launches the reader thread.
		 *
		 * @return true if start succeeded
		 * @return false if start failed
		 */
		bool start() override;

		/**
		 * @brief Process the node (no-op for file source)
		 *
		 * Processing happens in the reader thread.
		 */
		void process() override {}

		/**
		 * @brief Stop the node
		 *
		 * Signals the reader thread to stop.
		 */
		void stop() override;

		/**
		 * @brief Clean up resources
		 */
		void cleanup() override;

		/**
		 * @brief Get output buffer from a pad
		 *
		 * @param padIndex Output pad index
		 * @return std::optional<AudioBuffer> Buffer if available, empty optional if not
		 */
		std::optional<AudioBuffer> getOutputBuffer(int padIndex = 0) override;

	private:
		/**
		 * @brief Reader thread function
		 *
		 * Reads audio data from the file, decodes and resamples it,
		 * then pushes it to the output queue.
		 */
		void readerThreadFunc();

		/**
		 * @brief Open the audio file and set up decoding
		 *
		 * @return true if successful
		 * @return false if failed
		 */
		bool openFile();

		/**
		 * @brief Process one frame from the file
		 *
		 * @return true if a frame was processed
		 * @return false if end of file or error
		 */
		bool processFrame();

		// File path and thread management
		std::string m_filePath;				// Path to audio file
		std::thread m_readerThread;			// Background reader thread
		std::atomic<bool> m_running{false}; // Thread running flag
		bool m_loop = false;				// Loop playback flag

		// FFmpeg resources
		AVFormatContext *m_formatContext = nullptr; // Format context
		AVCodecContext *m_codecContext = nullptr;	// Codec context
		SwrContext *m_swrContext = nullptr;			// Resampler context
		AVPacket *m_packet = nullptr;				// Packet for read operations
		AVFrame *m_frame = nullptr;					// Frame for decoded data
		int m_audioStreamIndex = -1;				// Index of audio stream

		// Buffer management
		std::queue<AudioBuffer> m_outputQueue; // Queue of output buffers
		std::mutex m_queueMutex;			   // Mutex for thread safety
		size_t m_maxQueueSize = 10;			   // Maximum buffer queue size

		// Timing and state
		double m_filePosition = 0.0; // Position in seconds
		bool m_endOfFile = false;	 // End of file flag
	};

	/**
	 * @brief Node that writes audio to a file
	 *
	 * Uses FFmpeg to encode audio to various file formats.
	 */
	class FileSinkNode : public AudioNode
	{
	public:
		/**
		 * @brief Construct a new FileSinkNode
		 *
		 * @param name Unique node name
		 * @param engine Pointer to parent engine
		 */
		FileSinkNode(std::string name, AudioEngine *engine);

		/**
		 * @brief Destroy the FileSinkNode
		 */
		~FileSinkNode();

		/**
		 * @brief Configure the node with parameters
		 *
		 * Expected params:
		 * - "path": Path to output file
		 * - "format" (optional): Output container format
		 * - "codec" (optional): Audio codec to use
		 * - "bitrate" (optional): Encoding bitrate
		 *
		 * @param params Configuration parameters
		 * @param sampleRate System sample rate
		 * @param bufferSize System buffer size
		 * @param internalFormat Internal sample format
		 * @param internalLayout Internal channel layout
		 * @return true if configuration succeeded
		 * @return false if configuration failed
		 */
		bool configure(
			const std::map<std::string, std::string> &params,
			double sampleRate,
			long bufferSize,
			AVSampleFormat internalFormat,
			const AVChannelLayout &internalLayout) override;

		/**
		 * @brief Start the node
		 *
		 * Opens the output file and launches the writer thread.
		 *
		 * @return true if start succeeded
		 * @return false if start failed
		 */
		bool start() override;

		/**
		 * @brief Process the node (no-op for file sink)
		 *
		 * Processing happens in the writer thread.
		 */
		void process() override {}

		/**
		 * @brief Stop the node
		 *
		 * Signals the writer thread to stop, flush encoders, and close the file.
		 */
		void stop() override;

		/**
		 * @brief Clean up resources
		 */
		void cleanup() override;

		/**
		 * @brief Set input buffer for a pad
		 *
		 * @param buffer Buffer to write to file
		 * @param padIndex Input pad index
		 * @return true if buffer was accepted
		 * @return false if buffer was rejected
		 */
		bool setInputBuffer(const AudioBuffer &buffer, int padIndex = 0) override;

	private:
		/**
		 * @brief Writer thread function
		 *
		 * Takes buffers from the input queue, encodes them, and writes to the file.
		 */
		void writerThreadFunc();

		/**
		 * @brief Open the output file and set up encoding
		 *
		 * @return true if successful
		 * @return false if failed
		 */
		bool openFile();

		/**
		 * @brief Encode and write a buffer to the file
		 *
		 * @param buffer Buffer to encode and write
		 * @return true if successful
		 * @return false if failed
		 */
		bool encodeAndWrite(const AudioBuffer &buffer);

		/**
		 * @brief Flush the encoder and finish the file
		 */
		void finishFile();

		// File path and thread management
		std::string m_filePath;				// Path to output file
		std::string m_formatName;			// Output format name
		std::string m_codecName;			// Codec name
		int m_bitrate = 0;					// Encoding bitrate
		std::thread m_writerThread;			// Background writer thread
		std::atomic<bool> m_running{false}; // Thread running flag

		// FFmpeg resources
		AVFormatContext *m_formatContext = nullptr; // Format context
		AVCodecContext *m_codecContext = nullptr;	// Codec context
		SwrContext *m_swrContext = nullptr;			// Resampler context
		AVFrame *m_frame = nullptr;					// Frame for encoding
		AVPacket *m_packet = nullptr;				// Packet for write operations

		// Buffer management
		std::queue<AudioBuffer> m_inputQueue; // Queue of input buffers
		std::mutex m_queueMutex;			  // Mutex for thread safety
		size_t m_maxQueueSize = 10;			  // Maximum buffer queue size

		// State
		bool m_fileOpened = false; // Is file open?
	};

} // namespace AudioEngine
