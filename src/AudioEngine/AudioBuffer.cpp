#include "AudioBuffer.h"
#include <algorithm>
#include <iostream>

extern "C"
{
#include <libavutil/frame.h>
#include <libavutil/mem.h>
}

namespace AudioEngine
{

	AudioBuffer::AudioBuffer()
	{
		// Initialize an empty channel layout
		av_channel_layout_default(&channelLayout, 0);
	}

	AudioBuffer::AudioBuffer(long numFrames, double sRate, AVSampleFormat fmt, const AVChannelLayout &layout)
		: frames(0), sampleRate(0), format(AV_SAMPLE_FMT_NONE)
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

			// Zero the buffer
			std::memset(data[i], 0, bufferSize);

			// Store linesize for this plane/channel
			linesize[i] = isPlanar ? bytesPerSample : bytesPerSample * numChannels;
		}

		return true;
	}

	void AudioBuffer::free()
	{
		cleanupData();
		frames = 0;
		sampleRate = 0.0;
		format = AV_SAMPLE_FMT_NONE;
	}

	void AudioBuffer::cleanupData()
	{
		// Free all allocated data pointers
		for (uint8_t *ptr : data)
		{
			if (ptr)
			{
				av_free(ptr);
			}
		}
		data.clear();
		linesize.clear();
	}

	bool AudioBuffer::copyFrom(const AudioBuffer &other)
	{
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

			if (!allocate(other.frames, other.sampleRate, other.format, other.channelLayout))
			{
				return false;
			}
		}
		else
		{
			// If already allocated with same properties, just update sample rate
			sampleRate = other.sampleRate;
		}

		// Copy data
		for (size_t i = 0; i < data.size(); i++)
		{
			if (i < other.data.size() && data[i] && other.data[i])
			{
				size_t bufferSize = frames * linesize[i];
				std::memcpy(data[i], other.data[i], bufferSize);
			}
		}

		return true;
	}

	AudioBuffer::AudioBuffer(AudioBuffer &&other) noexcept
		: data(std::move(other.data)),
		  linesize(std::move(other.linesize)),
		  frames(other.frames),
		  sampleRate(other.sampleRate),
		  format(other.format)
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
			// Free existing resources
			free();
			av_channel_layout_uninit(&channelLayout);

			// Move resources from other
			data = std::move(other.data);
			linesize = std::move(other.linesize);
			frames = other.frames;
			sampleRate = other.sampleRate;
			format = other.format;

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

} // namespace AudioEngine
