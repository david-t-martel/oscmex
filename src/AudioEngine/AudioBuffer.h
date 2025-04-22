#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

// FFmpeg includes
extern "C"
{
#include "libavutil / samplefmt.h"
#include "libavutil / channel_layout.h"
}

namespace AudioEngine
{

	/**
	 * @brief Thread-safe audio buffer with reference counting
	 *
	 * Manages audio data with format information and reference counting
	 * to optimize buffer transfers between AudioNodes
	 */
	class AudioBuffer
	{
	public:
		/**
		 * @brief Create a new empty audio buffer
		 */
		AudioBuffer();

		/**
		 * @brief Create a new audio buffer with specified parameters
		 *
		 * @param numFrames Number of audio frames
		 * @param sRate Sample rate in Hz
		 * @param fmt Sample format (from FFmpeg)
		 * @param layout Channel layout (from FFmpeg)
		 */
		AudioBuffer(long numFrames, double sRate, AVSampleFormat fmt, const AVChannelLayout &layout);

		/**
		 * @brief Destructor
		 */
		~AudioBuffer();

		/**
		 * @brief Move constructor
		 */
		AudioBuffer(AudioBuffer &&other) noexcept;

		/**
		 * @brief Move assignment operator
		 */
		AudioBuffer &operator=(AudioBuffer &&other) noexcept;

		// Disable copy constructor and copy assignment (use copyFrom method instead)
		AudioBuffer(const AudioBuffer &) = delete;
		AudioBuffer &operator=(const AudioBuffer &) = delete;

		/**
		 * @brief Allocate memory for the buffer
		 *
		 * @param numFrames Number of audio frames
		 * @param sRate Sample rate in Hz
		 * @param fmt Sample format
		 * @param layout Channel layout
		 * @return true if allocation was successful
		 */
		bool allocate(long numFrames, double sRate, AVSampleFormat fmt, const AVChannelLayout &layout);

		/**
		 * @brief Free the buffer memory
		 */
		void free();

		/**
		 * @brief Copy data from another buffer
		 *
		 * @param other Source buffer to copy from
		 * @return true if copy was successful
		 */
		bool copyFrom(const AudioBuffer &other);

		/**
		 * @brief Get a data pointer for a specific channel
		 *
		 * Handles both planar and interleaved formats correctly
		 *
		 * @param channel Channel index
		 * @return Pointer to the channel data, or nullptr if channel is invalid
		 */
		uint8_t *getChannelData(int channel) const;

		/**
		 * @brief Get a data pointer for a specific plane
		 *
		 * @param plane Plane index (usually channel index for planar formats)
		 * @return Pointer to the plane data, or nullptr if plane is invalid
		 */
		uint8_t *getPlaneData(int plane) const;

		/**
		 * @brief Get the number of samples in a plane
		 *
		 * @param plane Plane index
		 * @return Number of samples in the plane (frames * channels for interleaved format)
		 */
		int getPlaneSamples(int plane) const;

		/**
		 * @brief Get the number of channels
		 *
		 * @return Channel count
		 */
		int getChannelCount() const;

		/**
		 * @brief Get the bytes per sample for this format
		 *
		 * @return Bytes per sample
		 */
		int getBytesPerSample() const;

		/**
		 * @brief Get the number of planes (channels for planar, 1 for interleaved)
		 *
		 * @return Number of planes
		 */
		int getPlaneCount() const;

		/**
		 * @brief Check if the buffer is valid (allocated and has data)
		 *
		 * @return true if buffer is valid
		 */
		bool isValid() const;

		/**
		 * @brief Check if the buffer format is planar
		 *
		 * @return true if format is planar, false if interleaved
		 */
		bool isPlanar() const;

		/**
		 * @brief Get the buffer's sample format
		 *
		 * @return AVSampleFormat
		 */
		AVSampleFormat getFormat() const;

		/**
		 * @brief Get the buffer's channel layout
		 *
		 * @return Copy of the AVChannelLayout
		 */
		AVChannelLayout getChannelLayout() const;

		/**
		 * @brief Get the buffer's frame count
		 *
		 * @return Number of frames
		 */
		long getFrames() const;

		/**
		 * @brief Get the buffer's sample rate
		 *
		 * @return Sample rate in Hz
		 */
		double getSampleRate() const;

		/**
		 * @brief Convert buffer to an AVFrame for FFmpeg processing
		 *
		 * Note: The returned AVFrame points to this buffer's data and must be
		 * freed by the caller (but the caller should not free the data buffers).
		 *
		 * @return AVFrame pointer, or nullptr on error
		 */
		AVFrame *toAVFrame() const;

		/**
		 * @brief Initialize buffer from an AVFrame
		 *
		 * @param frame AVFrame to copy from
		 * @return true if successful
		 */
		bool fromAVFrame(const AVFrame *frame);

		/**
		 * @brief Create a reference to this buffer (reference counting)
		 *
		 * Use this instead of creating copies to improve efficiency.
		 *
		 * @return Shared pointer to this buffer
		 */
		std::shared_ptr<AudioBuffer> createReference();

		/**
		 * @brief Create a new shared buffer
		 *
		 * Static factory method for creating reference-counted buffers
		 *
		 * @param numFrames Number of frames
		 * @param sRate Sample rate
		 * @param fmt Format
		 * @param layout Channel layout
		 * @return Shared pointer to new buffer
		 */
		static std::shared_ptr<AudioBuffer> createBuffer(
			long numFrames, double sRate, AVSampleFormat fmt, const AVChannelLayout &layout);

		/**
		 * @brief Create a deep copy of this buffer
		 *
		 * @return Shared pointer to the new buffer
		 */
		std::shared_ptr<AudioBuffer> clone() const;

	private:
		// Buffer properties
		long frames;				   // Number of frames
		double sampleRate;			   // Sample rate in Hz
		AVSampleFormat format;		   // Sample format
		AVChannelLayout channelLayout; // Channel layout

		// Buffer data storage
		std::vector<uint8_t *> data; // Data pointers (one per plane)
		std::vector<int> linesize;	 // Bytes per sample or bytes per frame (for interleaved)

		// Reference counting
		std::atomic<int> m_refCount; // Reference counter

		// Thread safety
		mutable std::mutex m_mutex; // Mutex for thread safety

		// Helper methods
		void cleanupData(); // Free data buffers

		// Internal version of allocate that doesn't lock
		bool allocateInternal(long numFrames, double sRate, AVSampleFormat fmt, const AVChannelLayout &layout);
	};

} // namespace AudioEngine
