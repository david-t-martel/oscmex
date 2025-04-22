#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>
#include <any>

#include "Configuration.h"
#include "IExternalControl.h"

// Forward declarations
extern "C"
{
	struct AVSampleFormat;
	struct AVChannelLayout;
}

namespace AudioEngine
{
	// Forward declarations
	class AsioManager;
	class AudioNode;
	class Connection;
	class DeviceStateManager;

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
		 * @param externalControl Optional external control interface (can be nullptr)
		 * @return true if initialization was successful
		 */
		bool initialize(Configuration config, std::shared_ptr<IExternalControl> externalControl = nullptr);

		/**
		 * @brief Initialize the engine from a JSON configuration file
		 *
		 * @param jsonFilePath Path to the JSON configuration file
		 * @param externalControl Optional external control interface (can be nullptr)
		 * @return true if initialization was successful
		 */
		bool initializeFromJson(const std::string &jsonFilePath, std::shared_ptr<IExternalControl> externalControl = nullptr);

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
		bool processAsioBlock(long doubleBufferIndex, bool directProcess = false);

		/**
		 * @brief Get a node by name
		 *
		 * @param name Name of the node
		 * @return AudioNode* Pointer to the node or nullptr if not found
		 */
		AudioNode *getNodeByName(const std::string &name);

		/**
		 * @brief Get the external control interface
		 *
		 * @return std::shared_ptr<IExternalControl> The external control interface or nullptr if not set
		 */
		std::shared_ptr<IExternalControl> getExternalControl() const { return m_externalControl; }

		/**
		 * @brief Set or replace the external control interface
		 *
		 * @param externalControl The external control implementation or nullptr to remove
		 * @return true if successfully set
		 */
		bool setExternalControl(std::shared_ptr<IExternalControl> externalControl);

		/**
		 * @brief Clear (remove) the external control interface
		 */
		void clearExternalControl() { m_externalControl.reset(); }

		/**
		 * @brief Add a status callback
		 *
		 * @param callback Function to call with status updates (category, message)
		 * @return int Callback ID used to remove the callback later
		 */
		int addStatusCallback(std::function<void(const std::string &, const std::string &)> callback);

		/**
		 * @brief Remove a status callback
		 *
		 * @param callbackId ID of the callback to remove
		 */
		void removeStatusCallback(int callbackId);

		/**
		 * @brief Set the device state manager
		 *
		 * @param manager Unique pointer to the device state manager
		 */
		void setDeviceStateManager(std::unique_ptr<DeviceStateManager> manager);

		/**
		 * @brief Get the device state manager
		 *
		 * @return DeviceStateManager* Pointer to the device state manager
		 */
		DeviceStateManager *getDeviceStateManager() const { return m_deviceStateManager.get(); }

		/**
		 * @brief Query the current state of the device
		 *
		 * @param callback Function to call with the result (success, configuration)
		 * @return true if the query was successful
		 */
		bool queryCurrentState(std::function<void(bool, const Configuration &)> callback);

		/**
		 * @brief Apply a new configuration to the device
		 *
		 * @param config New configuration to apply
		 * @param callback Function to call with the result (success)
		 * @return true if the configuration was applied successfully
		 */
		bool applyConfiguration(const Configuration &config, std::function<void(bool)> callback);

	private:
		// Configuration
		Configuration m_config;
		std::unique_ptr<AsioManager> m_asioManager;
		std::shared_ptr<IExternalControl> m_externalControl; // Optional external control

		// Node management
		std::vector<std::shared_ptr<AudioNode>> m_nodes;
		std::map<std::string, AudioNode *> m_nodeMap;
		std::vector<Connection> m_connections;

		// Status
		std::atomic<bool> m_running;

		// Status callbacks
		std::map<int, std::function<void(const std::string &, const std::string &)>> m_statusCallbacks;
		int m_nextCallbackId;
		std::mutex m_callbackMutex;

		// Helper methods
		bool createAndConfigureNodes();
		bool setupConnections();
		bool sendExternalCommands(); // Changed from sendOscCommands
		void reportStatus(const std::string &category, const std::string &message);

		// Process graph traversal
		std::vector<AudioNode *> m_processOrder;
		bool calculateProcessOrder();

		// Non-ASIO processing thread
		std::thread m_processingThread;
		std::atomic<bool> m_stopProcessingThread;

		/**
		 * @brief Run the file processing loop for non-ASIO operation
		 */
		void runFileProcessingLoop();

		/**
		 * @brief Configure ASIO automatically using driver information
		 *
		 * @return true if auto-configuration succeeded
		 */
		bool autoConfigureAsio();

		/**
		 * @brief Send a control command through the external control interface if available
		 *
		 * @param address Command address
		 * @param args Command arguments
		 * @return true if command was sent successfully or if no external control is available
		 */
		bool sendExternalCommand(const std::string &address, const std::vector<std::any> &args);

		// Device state manager
		std::unique_ptr<DeviceStateManager> m_deviceStateManager;
	};
}
