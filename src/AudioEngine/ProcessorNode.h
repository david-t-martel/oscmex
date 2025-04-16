#pragma once

#include "AudioNode.h"
#include <memory>
#include <mutex>
#include <string>

// Forward declarations
class FfmpegFilter;

namespace AudioEngine
{

	/**
	 * @brief Node that processes audio using FFmpeg filters
	 *
	 * Wraps an FFmpeg filter graph for audio processing operations
	 * such as EQ, compression, delay, etc.
	 */
	class FfmpegProcessorNode : public AudioNode
	{
	public:
		/**
		 * @brief Construct a new FfmpegProcessorNode
		 *
		 * @param name Unique node name
		 * @param engine Pointer to parent engine
		 */
		FfmpegProcessorNode(std::string name, AudioEngine *engine);

		/**
		 * @brief Destroy the FfmpegProcessorNode
		 */
		~FfmpegProcessorNode();

		/**
		 * @brief Configure the node with parameters
		 *
		 * Expected params:
		 * - "chain": FFmpeg filtergraph description string
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
		 * @return true Always succeeds for processor
		 */
		bool start() override { return true; }

		/**
		 * @brief Process a block of audio
		 *
		 * Applies FFmpeg filters to the input buffer and produces output
		 */
		void process() override;

		/**
		 * @brief Stop the node (no-op for processor)
		 */
		void stop() override {}

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

		/**
		 * @brief Set input buffer for a pad
		 *
		 * @param buffer Buffer to process
		 * @param padIndex Input pad index
		 * @return true if buffer was accepted
		 * @return false if buffer was rejected
		 */
		bool setInputBuffer(const AudioBuffer &buffer, int padIndex = 0) override;

		/**
		 * @brief Update filter parameter dynamically
		 *
		 * @param filterName Name of the filter to update
		 * @param paramName Parameter name to update
		 * @param valueStr New parameter value as string
		 * @return true if update succeeded
		 * @return false if update failed
		 */
		bool updateParameter(const std::string &filterName, const std::string &paramName, const std::string &valueStr);

	private:
		std::unique_ptr<FfmpegFilter> m_ffmpegFilter; // FFmpeg filter wrapper
		std::string m_filterDesc;					  // Filter graph description
		AudioBuffer m_inputBuffer;					  // Input buffer
		AudioBuffer m_outputBuffer;					  // Output buffer
		std::mutex m_bufferMutex;					  // Mutex for thread safety

		// Helper methods for filter graph management could be added here
	};

} // namespace AudioEngine
