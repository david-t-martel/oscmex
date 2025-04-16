#pragma once

#include "AudioBuffer.h"
#include <string>
#include <map>
#include <optional>
#include <memory>

namespace AudioEngine
{

	// Forward declaration
	class AudioEngine;

	/**
	 * @brief Base class for all audio processing nodes
	 *
	 * Defines the common interface for sources, sinks, and processors
	 * in the modular audio processing graph.
	 */
	class AudioNode
	{
	public:
		/**
		 * @brief Node type enumeration
		 */
		enum class NodeType
		{
			SOURCE,	  // Node that generates audio (e.g., ASIO input, file reader)
			SINK,	  // Node that consumes audio (e.g., ASIO output, file writer)
			PROCESSOR // Node that processes audio (e.g., FFmpeg filter chain)
		};

		/**
		 * @brief Construct an AudioNode
		 *
		 * @param name Unique node name
		 * @param type Node type
		 * @param engine Pointer to parent engine
		 */
		AudioNode(std::string name, NodeType type, AudioEngine *engine);

		/**
		 * @brief Virtual destructor
		 */
		virtual ~AudioNode() = default;

		/**
		 * @brief Configure node based on parsed parameters
		 *
		 * @param params Configuration parameters map
		 * @param sampleRate System sample rate
		 * @param bufferSize System buffer size
		 * @param internalFormat Internal sample format
		 * @param internalLayout Internal channel layout
		 * @return true if configuration succeeded
		 * @return false if configuration failed
		 */
		virtual bool configure(
			const std::map<std::string, std::string> &params,
			double sampleRate,
			long bufferSize,
			AVSampleFormat internalFormat,
			const AVChannelLayout &internalLayout) = 0;

		/**
		 * @brief Prepare node for processing
		 *
		 * @return true if start succeeded
		 * @return false if start failed
		 */
		virtual bool start() = 0;

		/**
		 * @brief Process a block of audio
		 *
		 * Pulls from inputs, processes, pushes to outputs as appropriate.
		 */
		virtual void process() = 0;

		/**
		 * @brief Stop processing
		 */
		virtual void stop() = 0;

		/**
		 * @brief Clean up resources
		 */
		virtual void cleanup() = 0;

		/**
		 * @brief Get output buffer from a pad
		 *
		 * @param padIndex Output pad index (0-based)
		 * @return std::optional<AudioBuffer> Buffer if available, empty optional if not
		 */
		virtual std::optional<AudioBuffer> getOutputBuffer(int padIndex = 0) { return std::nullopt; }

		/**
		 * @brief Set input buffer for a pad
		 *
		 * @param buffer Buffer to process
		 * @param padIndex Input pad index (0-based)
		 * @return true if buffer was accepted
		 * @return false if buffer was rejected
		 */
		virtual bool setInputBuffer(const AudioBuffer &buffer, int padIndex = 0) { return false; }

		/**
		 * @brief Get node name
		 */
		const std::string &getName() const { return m_name; }

		/**
		 * @brief Get node type
		 */
		NodeType getType() const { return m_type; }

	protected:
		std::string m_name;									  // Unique node name
		NodeType m_type;									  // Node type (source, sink, processor)
		AudioEngine *m_engine;								  // Pointer to parent engine
		double m_sampleRate = 0.0;							  // Current sample rate
		long m_bufferSize = 0;								  // Buffer size in frames
		AVSampleFormat m_internalFormat = AV_SAMPLE_FMT_NONE; // Target format for connections
		AVChannelLayout m_internalLayout;					  // Target channel layout

	private:
		// Prevent copying
		AudioNode(const AudioNode &) = delete;
		AudioNode &operator=(const AudioNode &) = delete;
	};

} // namespace AudioEngine
