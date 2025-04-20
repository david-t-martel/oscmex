#include "AudioBuffer.h"
#include <algorithm>
#include <iostream>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <ipp.h>

// Update to use local FFmpeg source headers
extern "C"
{
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <libavutil/samplefmt.h>
#include <libavutil/error.h>
}

namespace AudioEngine
{

	AudioBuffer::AudioBuffer()
		: frames(0), sampleRate(0), format(AV_SAMPLE_FMT_NONE), m_refCount(1)
	{
		// Initialize an empty channel layout
		av_channel_layout_default(&channelLayout, 0);
	}

	AudioBuffer::AudioBuffer(long numFrames, double sRate, AVSampleFormat fmt, const AVChannelLayout &layout)
		: frames(0), sampleRate(0), format(AV_SAMPLE_FMT_NONE), m_refCount(1)
	{
		// Initialize an empty channel layout
		av_channel_layout_default(&channelLayout, 0);
		// Call allocate to set up the buffer
		allocate(numFrames, sRate, fmt, layout);
	}

	AudioBuffer::~AudioBuffer()
	{
		free();
		av_channel_layout_uninit(&channelLayout);
	}

	bool AudioBuffer::allocate(long numFrames, double sRate, AVSampleFormat fmt, const AVChannelLayout &layout)
	{
		// Protect against concurrent access during allocation
		std::lock_guard<std::mutex> lock(m_mutex);

		// Clean up any existing data first
		free();

		if (numFrames <= 0)
		{
			std::cerr << "Error: Cannot allocate audio buffer with " << numFrames << " frames" << std::endl;
			return false;
		}

		// Copy properties
		frames = numFrames;
		sampleRate = sRate;
		format = fmt;

		// Copy channel layout
		av_channel_layout_uninit(&channelLayout);
		if (av_channel_layout_copy(&channelLayout, &layout) < 0)
		{
			std::cerr << "Error: Failed to copy channel layout" << std::endl;
			return false;
		}

		// Calculate number of channels and buffer sizes
		int numChannels = channelLayout.nb_channels;
		if (numChannels <= 0)
		{
			std::cerr << "Error: Invalid channel count: " << numChannels << std::endl;
			free();
			return false;
		}

		// Determine if format is planar or interleaved
		bool isPlanar = av_sample_fmt_is_planar(format);
		int bytesPerSample = av_get_bytes_per_sample(format);

		// Resize data and linesize vectors
		data.resize(isPlanar ? numChannels : 1);
		linesize.resize(isPlanar ? numChannels : 1);

		// Allocate memory for each channel or interleaved buffer
		for (int i = 0; i < (isPlanar ? numChannels : 1); i++)
		{
			int bufferSize = isPlanar ? frames * bytesPerSample : frames * bytesPerSample * numChannels;

			data[i] = static_cast<uint8_t *>(av_malloc(bufferSize));
			if (!data[i])
			{
				std::cerr << "Error: Failed to allocate audio buffer memory" << std::endl;
				free();
				return false;
			}

			// Zero the buffer - replace std::memset with IPP
			ippSetZero_8u(data[i], bufferSize);

			// Store linesize for this plane/channel
			linesize[i] = isPlanar ? bytesPerSample : bytesPerSample * numChannels;
		}

		return true;
	}

	void AudioBuffer::free()
	{
		// Avoid locking again if called from within allocate (which already holds the lock)
		std::unique_lock<std::mutex> lock(m_mutex, std::defer_lock);
		if (!lock.owns_lock())
		{
			lock.lock();
		}

		cleanupData();
		frames = 0;
		sampleRate = 0.0;
		format = AV_SAMPLE_FMT_NONE;
	}

	void AudioBuffer::cleanupData()
	{
		// Free all allocated data pointers using IPP
		for (uint8_t *ptr : data)
		{
			if (ptr)
			{
				ippsFree(ptr);
			}
		}
		data.clear();
		linesize.clear();
	}

	bool AudioBuffer::copyFrom(const AudioBuffer &other)
	{
		// Lock both buffers to prevent concurrent modification
		// Use std::lock to avoid potential deadlock
		std::unique_lock<std::mutex> lockThis(m_mutex, std::defer_lock);
		std::unique_lock<std::mutex> lockOther(other.m_mutex, std::defer_lock);
		std::lock(lockThis, lockOther);

		if (!other.isValid())
		{
			return false;
		}

		// Allocate this buffer if needed
		if (!isValid() ||
			frames != other.frames ||
			format != other.format ||
			av_channel_layout_compare(&channelLayout, &other.channelLayout) != 0)
		{
			// We already hold the lock, so use internal allocate method
			if (!allocateInternal(other.frames, other.sampleRate, other.format, other.channelLayout))
			{
				return false;
			}
		}
		else
		{
			// If already allocated with same properties, just update sample rate
			sampleRate = other.sampleRate;
		}

		// Use TBB for parallel copy of large buffers
		if (data.size() > 1 && data.size() == other.data.size() && frames > 1000)
		{
			tbb::parallel_for(tbb::blocked_range<size_t>(0, data.size()),
							  [&](const tbb::blocked_range<size_t> &r)
							  {
								  for (size_t i = r.begin(); i < r.end(); ++i)
								  {
									  if (data[i] && other.data[i])
									  {
										  size_t bufferSize = frames * linesize[i];
										  // For copying - replace std::memcpy with IPP
										  ippsCopy_8u(other.data[i], data[i], bufferSize);
									  }
								  }
							  });
		}
		else
		{
			// Original sequential copy for small buffers
			for (size_t i = 0; i < data.size(); i++)
			{
				if (i < other.data.size() && data[i] && other.data[i])
				{
					size_t bufferSize = frames * linesize[i];
					// For copying - replace std::memcpy with IPP
					ippsCopy_8u(other.data[i], data[i], bufferSize);
				}
			}
		}

		return true;
	}

	// Internal version of allocate that doesn't lock (assumes lock is already held)
	bool AudioBuffer::allocateInternal(long numFrames, double sRate, AVSampleFormat fmt, const AVChannelLayout &layout)
	{
		// Clean up any existing data first
		cleanupData();

		if (numFrames <= 0)
		{
			std::cerr << "Error: Cannot allocate audio buffer with " << numFrames << " frames" << std::endl;
			return false;
		}

		// Copy properties
		frames = numFrames;
		sampleRate = sRate;
		format = fmt;

		// Copy channel layout
		av_channel_layout_uninit(&channelLayout);
		if (av_channel_layout_copy(&channelLayout, &layout) < 0)
		{
			std::cerr << "Error: Failed to copy channel layout" << std::endl;
			return false;
		}

		// Calculate number of channels and buffer sizes
		int numChannels = channelLayout.nb_channels;
		if (numChannels <= 0)
		{
			std::cerr << "Error: Invalid channel count: " << numChannels << std::endl;
			cleanupData();
			return false;
		}

		// Determine if format is planar or interleaved
		bool isPlanar = av_sample_fmt_is_planar(format);
		int bytesPerSample = av_get_bytes_per_sample(format);

		// Resize data and linesize vectors
		data.resize(isPlanar ? numChannels : 1);
		linesize.resize(isPlanar ? numChannels : 1);

		// Use IPP for memory allocation and initialization
		for (int i = 0; i < (isPlanar ? numChannels : 1); i++)
		{
			int bufferSize = isPlanar ? frames * bytesPerSample : frames * bytesPerSample * numChannels;

			// Use IPP's aligned memory allocation
			data[i] = ippsMalloc_8u(bufferSize);
			if (!data[i])
			{
				std::cerr << "Error: Failed to allocate audio buffer memory" << std::endl;
				cleanupData();
				return false;
			}

			// Zero the buffer using IPP
			ippsZero_8u(data[i], bufferSize);

			linesize[i] = isPlanar ? bytesPerSample : bytesPerSample * numChannels;
		}

		return true;
	}

	AudioBuffer::AudioBuffer(AudioBuffer &&other) noexcept
		: data(std::move(other.data)),
		  linesize(std::move(other.linesize)),
		  frames(other.frames),
		  sampleRate(other.sampleRate),
		  format(other.format),
		  m_refCount(1)
	{
		// Copy channel layout
		av_channel_layout_default(&channelLayout, 0);
		av_channel_layout_copy(&channelLayout, &other.channelLayout);

		// Clear the moved-from instance
		av_channel_layout_uninit(&other.channelLayout);
		av_channel_layout_default(&other.channelLayout, 0);
		other.frames = 0;
		other.sampleRate = 0.0;
		other.format = AV_SAMPLE_FMT_NONE;
	}

	AudioBuffer &AudioBuffer::operator=(AudioBuffer &&other) noexcept
	{
		if (this != &other)
		{
			// Lock both buffers to prevent concurrent modification
			std::lock_guard<std::mutex> lockThis(m_mutex);
			std::lock_guard<std::mutex> lockOther(other.m_mutex);

			// Free existing resources
			free();
			av_channel_layout_uninit(&channelLayout);

			// Move resources from other
			data = std::move(other.data);
			linesize = std::move(other.linesize);
			frames = other.frames;
			sampleRate = other.sampleRate;
			format = other.format;

			// Keep our reference count (don't take the other buffer's count)

			// Copy channel layout
			av_channel_layout_default(&channelLayout, 0);
			av_channel_layout_copy(&channelLayout, &other.channelLayout);

			// Clear the moved-from instance
			av_channel_layout_uninit(&other.channelLayout);
			av_channel_layout_default(&other.channelLayout, 0);
			other.frames = 0;
			other.sampleRate = 0.0;
			other.format = AV_SAMPLE_FMT_NONE;
		}

		return *this;
	}

	uint8_t *AudioBuffer::getChannelData(int channel) const
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		// Check if buffer is valid
		if (!isValid())
		{
			return nullptr;
		}

		// Check if channel is in range
		bool isPlanar = av_sample_fmt_is_planar(format);
		if (isPlanar)
		{
			if (channel < 0 || static_cast<size_t>(channel) >= data.size())
			{
				return nullptr;
			}
			return data[channel];
		}
		else
		{
			// For interleaved, there's only one buffer
			if (channel < 0 || channel >= channelLayout.nb_channels)
			{
				return nullptr;
			}
			return data[0] + channel * av_get_bytes_per_sample(format);
		}
	}

	uint8_t *AudioBuffer::getPlaneData(int plane) const
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		if (!isValid() || plane < 0 || static_cast<size_t>(plane) >= data.size())
		{
			return nullptr;
		}

		return data[plane];
	}

	int AudioBuffer::getPlaneSamples(int plane) const
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		if (!isValid() || plane < 0 || static_cast<size_t>(plane) >= data.size())
		{
			return 0;
		}

		bool isPlanar = av_sample_fmt_is_planar(format);
		if (isPlanar)
		{
			return frames;
		}
		else
		{
			// For interleaved format, all samples are in the first plane
			return frames * channelLayout.nb_channels;
		}
	}

	int AudioBuffer::getChannelCount() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return channelLayout.nb_channels;
	}

	int AudioBuffer::getBytesPerSample() const
	{
		// No need to lock for this
		return av_get_bytes_per_sample(format);
	}

	int AudioBuffer::getPlaneCount() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return data.size();
	}

	bool AudioBuffer::isValid() const
	{
		// No need to lock since this is a quick check
		return (frames > 0 && !data.empty() && data[0] != nullptr);
	}

	bool AudioBuffer::isPlanar() const
	{
		// No need to lock for this
		return av_sample_fmt_is_planar(format);
	}

	AVSampleFormat AudioBuffer::getFormat() const
	{
		// No need to lock for this constant value
		return format;
	}

	AVChannelLayout AudioBuffer::getChannelLayout() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		AVChannelLayout layout;
		av_channel_layout_default(&layout, 0); // Initialize with empty layout
		av_channel_layout_copy(&layout, &channelLayout);
		return layout;
	}

	long AudioBuffer::getFrames() const
	{
		// No need to lock for this constant value
		return frames;
	}

	double AudioBuffer::getSampleRate() const
	{
		// No need to lock for this constant value
		return sampleRate;
	}

	// Convert this buffer to an AVFrame (useful for FFmpeg processing)
	AVFrame *AudioBuffer::toAVFrame() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		if (!isValid())
		{
			return nullptr;
		}

		AVFrame *frame = av_frame_alloc();
		if (!frame)
		{
			std::cerr << "Failed to allocate AVFrame" << std::endl;
			return nullptr;
		}

		// Set frame parameters
		frame->nb_samples = frames;
		frame->format = format;
		if (av_channel_layout_copy(&frame->ch_layout, &channelLayout) < 0)
		{
			std::cerr << "Failed to copy channel layout to AVFrame" << std::endl;
			av_frame_free(&frame);
			return nullptr;
		}

		frame->sample_rate = static_cast<int>(sampleRate);

		// Set data pointers and linesize
		for (size_t i = 0; i < data.size(); i++)
		{
			frame->data[i] = data[i];
			frame->linesize[i] = linesize[i] * frames;
		}

		// AVFrame needs to be cleaned up by the caller
		frame->buf[0] = nullptr; // Indicate that we don't own the memory

		return frame;
	}

	// Create a new buffer from an AVFrame
	bool AudioBuffer::fromAVFrame(const AVFrame *frame)
	{
		if (!frame)
		{
			return false;
		}

		std::lock_guard<std::mutex> lock(m_mutex);

		// Allocate buffer with frame parameters
		if (!allocateInternal(frame->nb_samples, frame->sample_rate,
							  static_cast<AVSampleFormat>(frame->format), frame->ch_layout))
		{
			return false;
		}

		// Copy data from frame
		for (size_t i = 0; i < data.size(); i++)
		{
			if (frame->data[i])
			{
				int bytesPerSample = av_get_bytes_per_sample(format);
				bool isPlanar = av_sample_fmt_is_planar(format);
				int copySize = isPlanar ? frames * bytesPerSample : frames * bytesPerSample * channelLayout.nb_channels;

				std::memcpy(data[i], frame->data[i], copySize);
			}
		}

		return true;
	}

	// Reference counting support
	std::shared_ptr<AudioBuffer> AudioBuffer::createReference()
	{
		// Increment the reference counter atomically
		m_refCount++;

		// Create a shared_ptr with a custom deleter that decrements the reference count
		return std::shared_ptr<AudioBuffer>(this, [](AudioBuffer *buffer)
											{
			if (buffer->m_refCount.fetch_sub(1) == 1)
			{
				// Last reference is gone, delete the object
				delete buffer;
			} });
	}

	// Static method to create a shared buffer
	std::shared_ptr<AudioBuffer> AudioBuffer::createBuffer(
		long numFrames, double sRate, AVSampleFormat fmt, const AVChannelLayout &layout)
	{
		// Create a new buffer and return it as a shared_ptr with reference counting
		AudioBuffer *buffer = new AudioBuffer();
		if (!buffer->allocate(numFrames, sRate, fmt, layout))
		{
			delete buffer;
			return nullptr;
		}

		// Return as a reference-counted shared_ptr
		return buffer->createReference();
	}

	// Create a copy of this buffer
	std::shared_ptr<AudioBuffer> AudioBuffer::clone() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		if (!isValid())
		{
			return nullptr;
		}

		// Create a new buffer with the same parameters
		std::shared_ptr<AudioBuffer> newBuffer = createBuffer(frames, sampleRate, format, channelLayout);
		if (!newBuffer)
		{
			return nullptr;
		}

		// Copy the data
		for (size_t i = 0; i < data.size() && i < newBuffer->data.size(); i++)
		{
			if (data[i] && newBuffer->data[i])
			{
				size_t bufferSize = frames * linesize[i];
				std::memcpy(newBuffer->data[i], data[i], bufferSize);
			}
		}

		return newBuffer;
	}

} // namespace AudioEngine
