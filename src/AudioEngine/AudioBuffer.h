#pragma once

#include <vector>
#include <cstdint>
#include <memory>

// FFmpeg includes
extern "C"
{
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
}

namespace AudioEngine
{

	/**
	 * @brief Represents an audio buffer with format information
	 *
	 * Handles memory management for audio data with proper RAII semantics.
	 * Uses FFmpeg types for format and channel layout information.
	 */
	class AudioBuffer
	{
	public:
		/**
		 * @brief Construct an empty audio buffer
		 */
		AudioBuffer();

		/**
		 * @brief Allocate a buffer with the given properties
		 *
		 * @param numFrames Number of audio frames
		 * @param sRate Sample rate in Hz
		 * @param fmt FFmpeg sample format (e.g., AV_SAMPLE_FMT_FLTP)
		 * @param layout Channel layout (e.g., stereo)
		 */
		AudioBuffer(long numFrames, double sRate, AVSampleFormat fmt, const AVChannelLayout &layout);

		/**
		 * @brief Destructor - frees all resources
		 */
		~AudioBuffer();

		/**
		 * @brief Reallocate or create the buffer with new properties
		 *
		 * @param numFrames Number of audio frames
		 * @param sRate Sample rate in Hz
		 * @param fmt FFmpeg sample format
		 * @param layout Channel layout
		 * @return true on success, false on failure
		 */
		bool allocate(long numFrames, double sRate, AVSampleFormat fmt, const AVChannelLayout &layout);

		/**
		 * @brief Free buffer resources
		 */
		void free();

		/**
		 * @brief Copy data from another buffer
		 *
		 * Copies data and properties, reallocating if necessary.
		 * @param other Source buffer
		 * @return true on success, false on failure
		 */
		bool copyFrom(const AudioBuffer &other);

		/**
		 * @brief Move constructor
		 */
		AudioBuffer(AudioBuffer &&other) noexcept;

		/**
		 * @brief Move assignment operator
		 */
		AudioBuffer &operator=(AudioBuffer &&other) noexcept;

		// Delete copy constructor and assignment to prevent accidental copying
		AudioBuffer(const AudioBuffer &) = delete;
		AudioBuffer &operator=(const AudioBuffer &) = delete;

		// Getters for buffer properties
		long getFrames() const { return frames; }
		double getSampleRate() const { return sampleRate; }
		AVSampleFormat getFormat() const { return format; }
		const AVChannelLayout &getChannelLayout() const { return channelLayout; }
		const std::vector<uint8_t *> &getData() const { return data; }
		const std::vector<int> &getLinesize() const { return linesize; }

		/**
		 * @brief Get a pointer to a specific channel's data
		 *
		 * @param channel Channel index (0-based)
		 * @return Pointer to channel data or nullptr if invalid
		 */
		uint8_t *getChannelData(int channel) const;

		/**
		 * @brief Check if the buffer is valid and allocated
		 *
		 * @return true if valid, false if not allocated
		 */
		bool isValid() const { return frames > 0 && !data.empty(); }

	private:
		// Audio properties
		std::vector<uint8_t *> data;				// Pointers to each channel's data
		std::vector<int> linesize;					// Size per channel line
		long frames = 0;							// Number of frames
		double sampleRate = 0.0;					// Sample rate in Hz
		AVSampleFormat format = AV_SAMPLE_FMT_NONE; // FFmpeg sample format
		AVChannelLayout channelLayout;				// FFmpeg channel layout

		// Helper method to clean up data pointers
		void cleanupData();
	};

} // namespace AudioEngine
