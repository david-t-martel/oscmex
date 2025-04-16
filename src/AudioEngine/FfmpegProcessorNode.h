#pragma once

#include "AudioNode.h"
#include "FfmpegFilter.h"
#include <mutex>

extern "C"
{
#include <libavutil/frame.h>
}

namespace AudioEngine
{

	/**
	 * @brief Node for processing audio through FFmpeg filters
	 */
	class FfmpegProcessorNode : public AudioNode
	{
	public:
		/**
		 * @brief Create a new FFmpeg processor node
		 *
		 * @param name Node name
		 * @param engine Reference to the audio engine
		 */
		FfmpegProcessorNode(const std::string &name, AudioEngine *engine);

		/**
		 * @brief Destroy the FFmpeg processor node
		 */
		~FfmpegProcessorNode();

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
		int getOutputPadCount() const override { return 1; } // One output

		/**
		 * @brief Update a filter parameter
		 *
		 * @param filterName Name of the filter
		 * @param paramName Parameter name
		 * @param value New parameter value
		 * @return true if update was successful
		 */
		bool updateParameter(const std::string &filterName,
							 const std::string &paramName,
							 const std::string &value);

	private:
		// FFmpeg filter
		FfmpegFilter m_ffmpegFilter;
		std::string m_filterDescription;

		// Input/output frames and buffers
		AVFrame *m_inputFrame;
		AVFrame *m_outputFrame;
		std::shared_ptr<AudioBuffer> m_inputBuffer;
		std::shared_ptr<AudioBuffer> m_outputBuffer;

		// Thread synchronization
		std::mutex m_processMutex;

		// Methods for frame handling
		bool initFrames();
		void freeFrames();
		bool copyBufferToFrame(std::shared_ptr<AudioBuffer> buffer, AVFrame *frame);
		bool copyFrameToBuffer(AVFrame *frame, std::shared_ptr<AudioBuffer> buffer);
	};

} // namespace AudioEngine
