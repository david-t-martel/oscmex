#pragma once

#include "AudioNode.h"
#include <mutex>
#include <string>

extern "C"
{
// Use the local FFmpeg source headers
#include <libavfilter/avfilter.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
}

namespace AudioEngine
{

	// Forward declaration
	class FfmpegFilter;

	/**
	 * @brief Node for processing audio using FFmpeg filter chains
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
		 * @brief Update a node parameter
		 *
		 * This implementation routes parameter update requests to the
		 * base class updateParameter method.
		 *
		 * @param paramName Name of the parameter to update
		 * @param paramValue New value for the parameter
		 * @return true if update was successful
		 */
		bool updateParameter(const std::string &paramName, const std::string &paramValue) override;

		/**
		 * @brief Update a filter parameter
		 *
		 * Dynamically update a parameter of a specific filter in the chain
		 *
		 * @param filterName Name of the filter to update
		 * @param paramName Name of the parameter to update
		 * @param value New value for the parameter
		 * @return true if update was successful
		 */
		bool updateParameter(const std::string &filterName, const std::string &paramName, const std::string &value);

		/**
		 * @brief Get the filter description
		 *
		 * @return FFmpeg filter description string
		 */
		std::string getFilterDescription() const { return m_filterDescription; }

	private:
		// FFmpeg filter
		std::unique_ptr<FfmpegFilter> m_ffmpegFilter;

		// Filter configuration
		std::string m_filterDescription;

		// FFmpeg frame objects
		AVFrame *m_inputFrame;
		AVFrame *m_outputFrame;

		// Input/output buffers
		std::shared_ptr<AudioBuffer> m_inputBuffer;
		std::shared_ptr<AudioBuffer> m_outputBuffer;

		// Thread safety
		std::mutex m_processMutex;

		// Helper methods
		bool initializeFrames();
		void cleanupFrames();
	};

} // namespace AudioEngine
