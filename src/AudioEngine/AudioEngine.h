#pragma once

#include "Configuration.h"
#include "Connection.h"
#include "AudioNode.h"
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <mutex>
#include <functional>
#include <atomic>

namespace AudioEngine
{

	// Forward declarations
	class AsioManager;
	class RmeOscController;

	/**
	 * @brief Core audio engine class
	 *
	 * Coordinates audio processing, manages nodes and connections
	 */
	class AudioEngine
	{
	public:
		/**
		 * @brief Create a new audio engine
		 */
		AudioEngine();

		/**
		 * @brief Destroy the audio engine
		 */
		~AudioEngine();

		/**
		 * @brief Initialize the engine with configuration
		 *
		 * @param config Engine configuration
		 * @return true if initialization was successful
		 */
		bool initialize(Configuration config);

		/**
		 * @brief Start audio processing
		 *
		 * @return true if engine was started successfully
		 */
		bool run();

		/**
		 * @brief Stop audio processing
		 */
		void stop();

		/**
		 * @brief Clean up all resources
		 */
		void cleanup();

		/**
		 * @brief Handle ASIO buffer switch
		 *
		 * @param doubleBufferIndex ASIO double buffer index
		 * @param directProcess true if processing is direct
		 * @return true if processing was successful
		 */
		bool processAsioBlock(long doubleBufferIndex, bool directProcess);

		/**
		 * @brief Get the RME OSC controller
		 *
		 * @return Pointer to RME OSC controller
		 */
		RmeOscController *getRmeController() { return m_rmeController.get(); }

		/**
		 * @brief Get a node by name
		 *
		 * @param name Node name
		 * @return Pointer to node or nullptr if not found
		 */
		AudioNode *getNodeByName(const std::string &name);

		/**
		 * @brief Add a status callback
		 *
		 * @param callback Status callback function
		 * @return Callback ID
		 */
		int addStatusCallback(std::function<void(const std::string &, const std::string &)> callback);

		/**
		 * @brief Remove a status callback
		 *
		 * @param callbackId Callback ID
		 */
		void removeStatusCallback(int callbackId);

	private:
		// Configuration
		Configuration m_config;

		// Managers
		std::unique_ptr<AsioManager> m_asioManager;
		std::unique_ptr<RmeOscController> m_rmeController;

		// Nodes and connections
		std::vector<std::unique_ptr<AudioNode>> m_nodes;
		std::vector<Connection> m_connections;
		std::map<std::string, AudioNode *> m_nodeMap;

		// Processing state
		std::atomic<bool> m_running;
		std::mutex m_processMutex;

		// Status callbacks
		std::map<int, std::function<void(const std::string &, const std::string &)>> m_statusCallbacks;
		int m_nextCallbackId;
		std::mutex m_callbackMutex;

		// Helper methods
		bool createAndConfigureNodes();
		bool setupConnections();
		bool sendRmeCommands();
		void reportStatus(const std::string &category, const std::string &message);

		// Process graph traversal
		std::vector<AudioNode *> m_processOrder;
		bool calculateProcessOrder();
	};

} // namespace AudioEngine
