#pragma once

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <functional>
#include "AudioBuffer.h"

extern "C"
{
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
}

namespace AudioEngine
{

	class AudioEngine; // Forward declaration

	/**
	 * @brief Base type for all audio processing nodes
	 *
	 * AudioNode is the abstract base class for all audio processing nodes in the system.
	 * It defines the interface that all nodes must implement and provides common functionality.
	 */
	class AudioNode
	{
	public:
		/**
		 * @brief Enumeration of node types
		 */
		enum class NodeType
		{
			UNKNOWN,
			ASIO_SOURCE,
			ASIO_SINK,
			FILE_SOURCE,
			FILE_SINK,
			FFMPEG_PROCESSOR,
			CUSTOM
		};

		/**
		 * @brief Constructor
		 *
		 * @param name Node name
		 * @param engine Pointer to the engine this node belongs to
		 */
		AudioNode(const std::string &name, AudioEngine *engine);

		/**
		 * @brief Virtual destructor
		 */
		virtual ~AudioNode();

		/**
		 * @brief Get the node name
		 *
		 * @return std::string Node name
		 */
		const std::string &getName() const { return m_name; }

		/**
		 * @brief Get the node type
		 *
		 * @return NodeType The type of this node
		 */
		virtual NodeType getType() const = 0;

		/**
		 * @brief Configure the node with the given parameters
		 *
		 * @param params JSON string of parameters specific to the node type
		 * @param sampleRate Sample rate in Hz
		 * @param bufferSize Buffer size in samples
		 * @param format Sample format (e.g., AV_SAMPLE_FMT_FLTP)
		 * @param channelLayout Channel layout (e.g., AV_CHANNEL_LAYOUT_STEREO)
		 * @return bool True if configuration was successful
		 */
		virtual bool configure(const std::string &params, double sampleRate, long bufferSize,
							   AVSampleFormat format, AVChannelLayout channelLayout) = 0;

		/**
		 * @brief Configure the node with the given parameters from a map
		 *
		 * @param params Map of parameter names to values
		 * @param sampleRate Sample rate in Hz
		 * @param bufferSize Buffer size in samples
		 * @param format Sample format (e.g., AV_SAMPLE_FMT_FLTP)
		 * @param channelLayout Channel layout (e.g., AV_CHANNEL_LAYOUT_STEREO)
		 * @return bool True if configuration was successful
		 */
		virtual bool configure(const std::map<std::string, std::string> &params, double sampleRate, long bufferSize,
							   AVSampleFormat format, AVChannelLayout channelLayout);

		/**
		 * @brief Start the node
		 *
		 * @return bool True if the node was started successfully
		 */
		virtual bool start() = 0;

		/**
		 * @brief Stop the node
		 */
		virtual void stop() = 0;

		/**
		 * @brief Process an audio block
		 *
		 * This method is called by the engine to process an audio block.
		 * Nodes should process their input buffers and produce output buffers.
		 *
		 * @return bool True if processing was successful
		 */
		virtual bool process() = 0;

		/**
		 * @brief Check if the node is running
		 *
		 * @return bool True if the node is running
		 */
		virtual bool isRunning() const = 0;

		/**
		 * @brief Set an input buffer for a specific pad
		 *
		 * @param buffer The input buffer
		 * @param padIndex Index of the input pad
		 * @return bool True if the buffer was set successfully
		 */
		virtual bool setInputBuffer(std::shared_ptr<AudioBuffer> buffer, int padIndex = 0) = 0;

		/**
		 * @brief Get the output buffer for a specific pad
		 *
		 * @param padIndex Index of the output pad
		 * @return std::shared_ptr<AudioBuffer> The output buffer, or nullptr if not available
		 */
		virtual std::shared_ptr<AudioBuffer> getOutputBuffer(int padIndex = 0) = 0;

		/**
		 * @brief Get the number of input pads
		 *
		 * @return int Number of input pads
		 */
		virtual int getInputPadCount() const = 0;

		/**
		 * @brief Get the number of output pads
		 *
		 * @return int Number of output pads
		 */
		virtual int getOutputPadCount() const = 0;

		/**
		 * @brief Reset the node state
		 *
		 * Resets the node to its initial state. This can be used to clear
		 * buffers and state when the node is being reused.
		 */
		virtual void reset() = 0;

		/**
		 * @brief Send a control message to the node
		 *
		 * @param messageType Type of message
		 * @param params Parameters for the message
		 * @return bool True if the message was handled successfully
		 */
		virtual bool sendControlMessage(const std::string &messageType, const std::map<std::string, std::string> &params);

	protected:
		std::string m_name;				 // Node name
		AudioEngine *m_engine;			 // Pointer to the engine
		double m_sampleRate;			 // Sample rate in Hz
		long m_bufferSize;				 // Buffer size in samples
		AVSampleFormat m_format;		 // Sample format
		AVChannelLayout m_channelLayout; // Channel layout
		bool m_configured;				 // Whether the node is configured
		bool m_running;					 // Whether the node is running

		/**
		 * @brief Report a status message to the engine
		 *
		 * @param category Message category (e.g., "Error", "Warning", "Info")
		 * @param message The message text
		 */
		void reportStatus(const std::string &category, const std::string &message);
	};

	// Helper functions for node type conversion
	/**
	 * @brief Convert a node type to string
	 *
	 * @param type Node type
	 * @return std::string String representation
	 */
	std::string nodeTypeToString(AudioNode::NodeType type);

	/**
	 * @brief Convert a string to node type
	 *
	 * @param typeStr String representation
	 * @return AudioNode::NodeType Node type
	 */
	AudioNode::NodeType nodeTypeFromString(const std::string &typeStr);

	/**
	 * @brief Structure to represent a connection between nodes
	 */
	class Connection
	{
	public:
		/**
		 * @brief Constructor
		 *
		 * @param sourceNode Source node
		 * @param sourcePad Source pad index
		 * @param sinkNode Sink node
		 * @param sinkPad Sink pad index
		 */
		Connection(AudioNode *sourceNode, int sourcePad, AudioNode *sinkNode, int sinkPad);

		/**
		 * @brief Get the source node
		 *
		 * @return AudioNode* Source node
		 */
		AudioNode *getSourceNode() const { return m_sourceNode; }

		/**
		 * @brief Get the source pad index
		 *
		 * @return int Source pad index
		 */
		int getSourcePad() const { return m_sourcePad; }

		/**
		 * @brief Get the sink node
		 *
		 * @return AudioNode* Sink node
		 */
		AudioNode *getSinkNode() const { return m_sinkNode; }

		/**
		 * @brief Get the sink pad index
		 *
		 * @return int Sink pad index
		 */
		int getSinkPad() const { return m_sinkPad; }

		/**
		 * @brief Transfer data from source to sink
		 *
		 * @return bool True if the transfer was successful
		 */
		bool transfer();

	private:
		AudioNode *m_sourceNode;	// Source node
		int m_sourcePad;			// Source pad index
		AudioNode *m_sinkNode;		// Sink node
		int m_sinkPad;				// Sink pad index
		bool m_formatConversion;	// Whether format conversion is allowed
		std::string m_bufferPolicy; // Buffer policy
	};

} // namespace AudioEngine
