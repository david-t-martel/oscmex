#pragma once

#include <string>

namespace AudioEngine
{

	// Forward declarations
	class AudioNode;

	/**
	 * @brief Connection between AudioNodes
	 *
	 * Represents a connection from a source node's output pad
	 * to a destination node's input pad
	 */
	class Connection
	{
	public:
		/**
		 * @brief Create a new connection
		 *
		 * @param sourceNode Source node
		 * @param sourcePad Source node output pad index
		 * @param sinkNode Destination node
		 * @param sinkPad Destination node input pad index
		 */
		Connection(AudioNode *sourceNode, int sourcePad,
				   AudioNode *sinkNode, int sinkPad);

		/**
		 * @brief Get the source node
		 *
		 * @return Source node pointer
		 */
		AudioNode *getSourceNode() const { return m_sourceNode; }

		/**
		 * @brief Get the source pad index
		 *
		 * @return Source pad index
		 */
		int getSourcePad() const { return m_sourcePad; }

		/**
		 * @brief Get the sink node
		 *
		 * @return Sink node pointer
		 */
		AudioNode *getSinkNode() const { return m_sinkNode; }

		/**
		 * @brief Get the sink pad index
		 *
		 * @return Sink pad index
		 */
		int getSinkPad() const { return m_sinkPad; }

		/**
		 * @brief Transfer data from source to sink
		 *
		 * @return true if transfer was successful
		 */
		bool transfer();

		/**
		 * @brief Get a string representation of the connection
		 *
		 * @return Connection as a string
		 */
		std::string toString() const;

	private:
		AudioNode *m_sourceNode; // Source node
		int m_sourcePad;		 // Source node output pad index
		AudioNode *m_sinkNode;	 // Sink node
		int m_sinkPad;			 // Sink node input pad index
	};

} // namespace AudioEngine
