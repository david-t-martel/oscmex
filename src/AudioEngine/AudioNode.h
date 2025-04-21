#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include "AudioBuffer.h"

namespace AudioEngine
{

	// Forward declaration
	class AudioEngine;

	/**
	 * @brief Node types enumeration
	 */
	enum class NodeType
	{
		UNKNOWN,
		FILE_SOURCE,
		FILE_SINK,
		ASIO_SOURCE, // New node type for ASIO input
		ASIO_SINK,	 // New node type for ASIO output
		FFMPEG_PROCESSOR
	};

	/**
	 * @brief Base class for all audio processing nodes
	 */
	class AudioNode
	{
	public:
		/**
		 * @brief Create a new audio node
		 *
		 * @param name Node name
		 * @param type Node type
		 * @param engine Reference to the audio engine
		 */
		AudioNode(const std::string &name, NodeType type, AudioEngine *engine);

		/**
		 * @brief Destroy the audio node
		 */
		virtual ~AudioNode();

		/**
		 * @brief Configure the node
		 *
		 * @param params Configuration parameters
		 * @param sampleRate Sample rate in Hz
		 * @param bufferSize Buffer size in frames
		 * @param format Sample format
		 * @param layout Channel layout
		 * @return true if configuration was successful
		 */
		virtual bool configure(const std::map<std::string, std::string> &params,
							   double sampleRate,
							   long bufferSize,
							   AVSampleFormat format,
							   const AVChannelLayout &layout) = 0;

		/**
		 * @brief Start node processing
		 *
		 * @return true if start was successful
		 */
		virtual bool start() = 0;

		/**
		 * @brief Process audio data
		 *
		 * @return true if processing was successful
		 */
		virtual bool process() = 0;

		/**
		 * @brief Stop node processing
		 */
		virtual void stop() = 0;

		/**
		 * @brief Get node name
		 *
		 * @return Node name
		 */
		std::string getName() const { return m_name; }

		/**
		 * @brief Get node type
		 *
		 * @return Node type
		 */
		NodeType getType() const { return m_type; }

		/**
		 * @brief Get output buffer from specified pad
		 *
		 * @param padIndex Output pad index
		 * @return Shared pointer to the audio buffer
		 */
		virtual std::shared_ptr<AudioBuffer> getOutputBuffer(int padIndex) = 0;

		/**
		 * @brief Set input buffer for specified pad
		 *
		 * @param buffer Input audio buffer
		 * @param padIndex Input pad index
		 * @return true if buffer was accepted
		 */
		virtual bool setInputBuffer(std::shared_ptr<AudioBuffer> buffer, int padIndex) = 0;

		/**
		 * @brief Get number of input pads
		 *
		 * @return Input pad count
		 */
		virtual int getInputPadCount() const = 0;

		/**
		 * @brief Get number of output pads
		 *
		 * @return Output pad count
		 */
		virtual int getOutputPadCount() const = 0;

		/**
		 * @brief Update a node parameter
		 *
		 * Dynamic parameter updates after node has been configured
		 *
		 * @param paramName Parameter name
		 * @param paramValue Parameter value
		 * @return true if parameter was updated successfully
		 */
		virtual bool updateParameter(const std::string &paramName, const std::string &paramValue);

		/**
		 * @brief Check if the node is configured
		 *
		 * @return true if node is configured
		 */
		bool isConfigured() const { return m_configured; }

		/**
		 * @brief Check if the node is running
		 *
		 * @return true if node is running
		 */
		bool isRunning() const { return m_running; }

		/**
		 * @brief Check if a buffer format is compatible with this node
		 *
		 * @param buffer Buffer to check compatibility with
		 * @return true if buffer is compatible
		 */
		bool isBufferFormatCompatible(const std::shared_ptr<AudioBuffer> &buffer) const;

		/**
		 * @brief Get the last error message
		 *
		 * @return Last error message
		 */
		std::string getLastError() const { return m_lastError; }

	protected:
		std::string m_name;	   // Node name
		NodeType m_type;	   // Node type
		AudioEngine *m_engine; // Reference to the audio engine

		double m_sampleRate;			 // Sample rate in Hz
		long m_bufferSize;				 // Buffer size in frames
		AVSampleFormat m_format;		 // Sample format
		AVChannelLayout m_channelLayout; // Channel layout

		bool m_configured; // Configuration state
		bool m_running;	   // Running state

		// Helper methods
		bool checkConfigure(double sampleRate, long bufferSize,
							AVSampleFormat format, const AVChannelLayout &layout);

		void logMessage(const std::string &message, bool isError);
		static std::string nodeTypeToString(NodeType type);

		// Error reporting
		std::string m_lastError;
	};

} // namespace AudioEngine
