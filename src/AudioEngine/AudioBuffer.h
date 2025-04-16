#pragma once

#include <vector>
#include <memory>
#include <atomic>
#include <mutex>

extern "C"
{
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
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
		 * @brief Create a new audio buffer
		 */
		AudioBuffer();

		/**
		 * @brief Create a new audio buffer with specified parameters
		 *
		 * @param frames Number of audio frames
		 * @param sampleRate Sample rate in Hz
		 * @param format Sample format
		 * @param layout Channel layout
		 */
		AudioBuffer(long frames, double sampleRate,
					AVSampleFormat format, const AVChannelLayout &layout);

		/**
		 * @brief Destructor
		 */
		~AudioBuffer();

		/**
		 * @brief Allocate memory for the buffer
		 *
		 * @param frames Number of audio frames
		 * @param sampleRate Sample rate in Hz
		 * @param format Sample format
		 * @param layout Channel layout
		 * @return true if allocation was successful
		 */
		bool allocate(long frames, double sampleRate,
					  AVSampleFormat format, const AVChannelLayout &layout);

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
		 * @brief Get a data pointer for a specific plane
		 *
		 * @param plane Plane index (usually channel index for planar formats)
		 * @return Pointer to the data
		 */
		uint8_t *getPlaneData(int plane);

		/**
		 * @brief Get a read-only data pointer for a specific plane
		 *
		 * @param plane Plane index (usually channel index for planar formats)
		 * @return Const pointer to the data
		 */
		const uint8_t *getPlaneData(int plane) const;

		/**
		 * @brief Get the buffer's frame count
		 *
		 * @return Number of frames
		 */
		long getFrames() const { return m_frames; }

		/**
		 * @brief Get the buffer's sample rate
		 *
		 * @return Sample rate in Hz
		 */
		double getSampleRate() const { return m_sampleRate; }

		/**
		 * @brief Get the buffer's sample format
		 *
		 * @return AVSampleFormat
		 */
		AVSampleFormat getFormat() const { return m_format; }

		/**
		 * @brief Get the buffer's channel layout
		 *
		 * @return AVChannelLayout
		 */
		const AVChannelLayout &getChannelLayout() const { return m_channelLayout; }

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
		 * @brief Create a reference to this buffer (reference counting)
		 *
		 * @return Shared pointer to this buffer
		 */
		std::shared_ptr<AudioBuffer> createReference();

	private:
		std::vector<std::vector<uint8_t>> m_data; // Data planes
		long m_frames;							  // Number of frames
		double m_sampleRate;					  // Sample rate in Hz
		AVSampleFormat m_format;				  // Sample format
		AVChannelLayout m_channelLayout;		  // Channel layout

		std::atomic<int> m_refCount; // Reference counter
		mutable std::mutex m_mutex;	 // Mutex for thread safety
	};

} // namespace AudioEngine
