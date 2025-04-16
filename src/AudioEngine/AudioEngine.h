#pragma once

#include "Configuration.h"
#include "AsioManager.h"
#include "RmeOscController.h"
#include "AudioNode.h"

#include <memory>
#include <map>
#include <atomic>
#include <string>
#include <vector>

extern "C"
{
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
}

namespace AudioEngine
{

	/**
	 * @brief Central coordinator for the audio processing system
	 *
	 * Manages nodes, connections, ASIO interface, and OSC communication.
	 * Handles initialization, audio processing loop, and resource cleanup.
	 */
	class AudioEngine
	{
	public:
		/**
		 * @brief Construct a new AudioEngine
		 */
		AudioEngine();

		/**
		 * @brief Destroy the AudioEngine and clean up resources
		 */
		~AudioEngine();

		/**
		 * @brief Initialize the engine with a configuration
		 *
		 * @param config Engine configuration
		 * @return true if initialization succeeded
		 * @return false if initialization failed
		 */
		bool initialize(Configuration config);

		/**
		 * @brief Start the audio processing loop
		 *
		 * Blocking if file-only, non-blocking if ASIO.
		 */
		void run();

		/**
		 * @brief Stop the audio processing
		 */
		void stop();

		/**
		 * @brief Clean up all resources
		 */
		void cleanup();

		/**
		 * @brief Process one block of audio data (called by ASIO callback)
		 *
		 * @param doubleBufferIndex Current ASIO buffer index
		 */
		void processAsioBlock(long doubleBufferIndex);

		/**
		 * @brief Get the AsioManager instance
		 *
		 * @return AsioManager* Pointer to ASIO manager
		 */
		AsioManager *getAsioManager() { return m_asioManager.get(); }

		/**
		 * @brief Get the RmeController instance
		 *
		 * @return RmeOscController* Pointer to RME OSC controller
		 */
		RmeOscController *getRmeController() { return m_rmeController.get(); }

		/**
		 * @brief Get the current sample rate
		 *
		 * @return double Sample rate in Hz
		 */
		double getSampleRate() const { return m_sampleRate; }

		/**
		 * @brief Get the current buffer size
		 *
		 * @return long Buffer size in samples
		 */
		long getBufferSize() const { return m_bufferSize; }

		/**
		 * @brief Get the internal channel layout
		 *
		 * @return const AVChannelLayout& Channel layout
		 */
		const AVChannelLayout &getInternalLayout() const { return m_internalLayout; }

		/**
		 * @brief Get the internal sample format
		 *
		 * @return AVSampleFormat Sample format
		 */
		AVSampleFormat getInternalFormat() const { return m_internalFormat; }

	private:
		/**
		 * @brief Set up ASIO interface
		 *
		 * @return true if setup succeeded
		 * @return false if setup failed
		 */
		bool setupAsio();

		/**
		 * @brief Create and configure nodes based on configuration
		 *
		 * @return true if node creation succeeded
		 * @return false if node creation failed
		 */
		bool createAndConfigureNodes();

		/**
		 * @brief Set up connections between nodes
		 *
		 * @return true if connection setup succeeded
		 * @return false if connection setup failed
		 */
		bool setupConnections();

		/**
		 * @brief Send initial OSC commands to RME device
		 *
		 * @return true if commands were sent successfully
		 * @return false if commands failed
		 */
		bool sendRmeCommands();

		/**
		 * @brief Run processing loop for file-only mode
		 */
		void runFileProcessingLoop();

		// Configuration and manager instances
		Configuration m_config;							   // Engine configuration
		std::unique_ptr<AsioManager> m_asioManager;		   // ASIO interface
		std::unique_ptr<RmeOscController> m_rmeController; // RME OSC controller

		// Node management
		std::vector<std::shared_ptr<AudioNode>> m_nodes;			 // All nodes
		std::map<std::string, std::shared_ptr<AudioNode>> m_nodeMap; // Name->node lookup

		// Connection management
		struct Connection
		{
			std::shared_ptr<AudioNode> sourceNode; // Source node
			int sourcePad;						   // Output pad index on source
			std::shared_ptr<AudioNode> sinkNode;   // Sink node
			int sinkPad;						   // Input pad index on sink
		};

		std::vector<Connection> m_connections;					   // All connections
		std::vector<std::shared_ptr<AudioNode>> m_processingOrder; // Node processing order

		// Runtime state
		std::atomic<bool> m_isRunning{false}; // Is the engine running?
		bool m_useAsio = false;				  // Use ASIO for processing?
		double m_sampleRate = 0.0;			  // Current sample rate
		long m_bufferSize = 0;				  // Current buffer size

		// Internal audio format
		AVSampleFormat m_internalFormat = AV_SAMPLE_FMT_FLTP; // Float planar format for processing
		AVChannelLayout m_internalLayout;					  // Internal channel layout
	};

} // namespace AudioEngine
