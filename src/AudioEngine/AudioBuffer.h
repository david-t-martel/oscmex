#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

// FFmpeg includes
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

		/**
		 * @brief Create a new audio buffer
		 *
		 * @param numSamples Number of samples per channel
		 * @param format Sample format (e.g., AV_SAMPLE_FMT_FLTP)
		 * @param channelLayout Channel layout
		 * @return std::shared_ptr<AudioBuffer> Shared pointer to the new buffer
		 */
		static std::shared_ptr<AudioBuffer> create(int numSamples, AVSampleFormat format, AVChannelLayout channelLayout);

		/**
		 * @brief Create a new audio buffer as a view into another buffer
		 *
		 * @param sourceBuffer Source buffer to create view from
		 * @param startSample Starting sample index
		 * @param numSamples Number of samples to include in the view
		 * @return std::shared_ptr<AudioBuffer> Shared pointer to the new buffer view
		 */
		static std::shared_ptr<AudioBuffer> createView(std::shared_ptr<AudioBuffer> sourceBuffer, int startSample, int numSamples);

		/**
		 * @brief Create a new audio buffer as a copy of another buffer
		 *
		 * @param sourceBuffer Source buffer to copy
		 * @param startSample Starting sample index
		 * @param numSamples Number of samples to copy
		 * @return std::shared_ptr<AudioBuffer> Shared pointer to the new buffer copy
		 */
		static std::shared_ptr<AudioBuffer> createCopy(std::shared_ptr<AudioBuffer> sourceBuffer, int startSample, int numSamples);

		/**
		 * @brief Create a new audio buffer with a different format/layout than the source
		 *
		 * @param sourceBuffer Source buffer to convert
		 * @param format Target sample format
		 * @param channelLayout Target channel layout
		 * @return std::shared_ptr<AudioBuffer> Shared pointer to the new buffer
		 */
		static std::shared_ptr<AudioBuffer> createConverted(std::shared_ptr<AudioBuffer> sourceBuffer,
															AVSampleFormat format, AVChannelLayout channelLayout);

		/**
		 * @brief Get the number of samples per channel
		 *
		 * @return int Number of samples
		 */
		int getNumSamples() const { return m_numSamples; }

		/**
		 * @brief Get the number of channels
		 *
		 * @return int Number of channels
		 */
		int getNumChannels() const { return m_numChannels; }

		/**
		 * @brief Get the sample format
		 *
		 * @return AVSampleFormat Sample format
		 */
		AVSampleFormat getFormat() const { return m_format; }

		/**
		 * @brief Get the channel layout
		 *
		 * @return const AVChannelLayout& Channel layout
		 */
		const AVChannelLayout &getChannelLayout() const { return m_channelLayout; }

		/**
		 * @brief Get a pointer to the data for a specific channel
		 *
		 * For planar formats, this returns a pointer to the specific channel's data.
		 * For interleaved formats, this returns a pointer to the start of the interleaved data
		 * (only valid for channel 0).
		 *
		 * @param channel Channel index
		 * @return void* Pointer to the channel data, or nullptr if invalid channel
		 */
		void *getChannelData(int channel);

		/**
		 * @brief Get a const pointer to the data for a specific channel
		 *
		 * @param channel Channel index
		 * @return const void* Pointer to the channel data, or nullptr if invalid channel
		 */
		const void *getChannelData(int channel) const;

		/**
		 * @brief Get pointers to all channel data
		 *
		 * @param outPointers Array to fill with pointers (must be at least getNumChannels() in size)
		 */
		void getChannelPointers(void **outPointers);

		/**
		 * @brief Clear the buffer (fill with zeros)
		 */
		void clear();

		/**
		 * @brief Check if this buffer is a view of another buffer
		 *
		 * @return bool True if this is a view
		 */
		bool isView() const { return m_isView; }

		/**
		 * @brief Get the source buffer if this is a view
		 *
		 * @return std::shared_ptr<AudioBuffer> Source buffer, or nullptr if this is not a view
		 */
		std::shared_ptr<AudioBuffer> getSourceBuffer() const { return m_sourceBuffer; }

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

		/**
		 * @brief Constructor for creating a new buffer
		 *
		 * @param numSamples Number of samples per channel
		 * @param format Sample format
		 * @param channelLayout Channel layout
		 */
		AudioBuffer(int numSamples, AVSampleFormat format, AVChannelLayout channelLayout);

		/**
		 * @brief Constructor for creating a view into another buffer
		 *
		 * @param sourceBuffer Source buffer
		 * @param startSample Starting sample
		 * @param numSamples Number of samples
		 */
		AudioBuffer(std::shared_ptr<AudioBuffer> sourceBuffer, int startSample, int numSamples);

		/**
		 * @brief Allocate memory for the buffer
		 */
		void allocateMemory();

		/**
		 * @brief Calculate the offset for a channel in an interleaved buffer
		 *
		 * @param sample Sample index
		 * @param channel Channel index
		 * @return int Offset in bytes
		 */
		int calculateInterleavedOffset(int sample, int channel) const;

		/**
		 * @brief Get the size of a single sample in bytes
		 *
		 * @return int Sample size in bytes
		 */
		int getSampleSize() const;

		int m_numSamples;							 // Number of samples per channel
		int m_numChannels;							 // Number of channels
		AVSampleFormat m_format;					 // Sample format
		AVChannelLayout m_channelLayout;			 // Channel layout
		bool m_isView;								 // Whether this is a view into another buffer
		int m_startSample;							 // Starting sample for views
		std::shared_ptr<AudioBuffer> m_sourceBuffer; // Source buffer for views

		// Data storage - use either data pointers (planar) or a single buffer (interleaved)
		std::vector<uint8_t *> m_dataPointers; // For planar formats, pointers to each channel
		std::vector<uint8_t> m_data;		   // For interleaved formats, a single buffer
	};

} // namespace AudioEngine
