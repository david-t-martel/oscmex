#pragma once

#include <string>
#include <vector>
#include <map>
#include <any>
#include <functional>
#include <memory>

namespace AudioEngine
{
	// Forward declaration for classes that will be referenced
	class OscController;

	/**
	 * @brief Structure to represent a single OSC command
	 */
	struct RmeOscCommandConfig
	{
		std::string address;		// OSC address (e.g., "/1/input/1/volume")
		std::vector<std::any> args; // Command arguments (float, int, bool, string)
	};

	/**
	 * @brief Configuration structure for audio processing nodes
	 */
	struct NodeConfig
	{
		std::string name;						   // Node name (e.g., "input1")
		std::string type;						   // Node type (e.g., "asio_in", "gain", "route")
		std::map<std::string, std::string> params; // Node parameters
	};

	/**
	 * @brief Configuration structure for connections between nodes
	 */
	struct ConnectionConfig
	{
		std::string sourceName; // Source node name
		int sourcePad;			// Source pad index
		std::string sinkName;	// Sink node name
		int sinkPad;			// Sink pad index
	};

	/**
	 * @brief Callback type for asynchronous device state operations
	 */
	using DeviceStateCallback = std::function<void(bool success, const std::string &message)>;

	/**
	 * @brief Class for managing RME device configuration
	 */
	class Configuration
	{
	public:
		/**
		 * @brief Default constructor
		 */
		Configuration();

		/**
		 * @brief Destructor
		 */
		~Configuration();

		/**
		 * @brief Load configuration from a JSON file
		 *
		 * @param filepath Path to the JSON configuration file
		 * @return true if loading succeeded
		 * @return false if loading failed
		 */
		bool loadFromJson(const std::string &filepath);

		/**
		 * @brief Save configuration to a JSON file
		 *
		 * @param filepath Path to save the JSON configuration file
		 * @return true if saving succeeded
		 * @return false if saving failed
		 */
		bool saveToJson(const std::string &filepath) const;

		/**
		 * @brief Set OSC connection parameters
		 *
		 * @param ip Target IP address (e.g., "127.0.0.1")
		 * @param sendPort Port to send OSC commands to
		 * @param receivePort Port to receive OSC responses on
		 */
		void setConnectionParams(const std::string &ip, int sendPort, int receivePort);

		/**
		 * @brief Get OSC target IP address
		 *
		 * @return std::string The IP address
		 */
		std::string getTargetIp() const { return m_targetIp; }

		/**
		 * @brief Get OSC target port
		 *
		 * @return int The port number
		 */
		int getTargetPort() const { return m_targetPort; }

		/**
		 * @brief Get OSC receive port
		 *
		 * @return int The port number
		 */
		int getReceivePort() const { return m_receivePort; }

		/**
		 * @brief Add a matrix crosspoint gain setting
		 *
		 * @param input Input channel (1-based)
		 * @param output Output channel (1-based)
		 * @param gainDb Gain in decibels
		 */
		void setMatrixCrosspointGain(int input, int output, float gainDb);

		/**
		 * @brief Add a channel mute setting
		 *
		 * @param channelType Channel type (input, playback, output)
		 * @param channel Channel number (1-based)
		 * @param mute Mute state
		 */
		void setChannelMute(int channelType, int channel, bool mute);

		/**
		 * @brief Add a channel volume setting
		 *
		 * @param channelType Channel type (input, playback, output)
		 * @param channel Channel number (1-based)
		 * @param volumeDb Volume in decibels
		 */
		void setChannelVolume(int channelType, int channel, float volumeDb);

		/**
		 * @brief Get all configured OSC commands
		 *
		 * @return const std::vector<RmeOscCommandConfig>& Vector of command configurations
		 */
		const std::vector<RmeOscCommandConfig> &getCommands() const { return m_commands; }

		/**
		 * @brief Add a raw OSC command to the configuration
		 *
		 * @param address OSC address path
		 * @param args Command arguments
		 */
		void addCommand(const std::string &address, const std::vector<std::any> &args);

		/**
		 * @brief Clear all commands in the configuration
		 */
		void clearCommands();

	private:
		std::string m_targetIp;						 // Target IP address
		int m_targetPort;							 // Target port for sending
		int m_receivePort;							 // Port for receiving
		std::vector<RmeOscCommandConfig> m_commands; // List of OSC commands
	};

	/**
	 * @brief Class for reading and managing device state
	 */
	class DeviceStateManager
	{
	public:
		/**
		 * @brief Device parameter query callback type
		 *
		 * Called when a parameter query has completed
		 */
		using QueryCallback = std::function<void(bool success, float value)>;

		/**
		 * @brief Query completion callback
		 *
		 * Called when all queries in a batch have completed
		 */
		using BatchCompletionCallback = std::function<void(bool allSucceeded)>;

		/**
		 * @brief Construct a new Device State Manager
		 *
		 * @param controller Pointer to the OscController to use
		 */
		DeviceStateManager(OscController *controller);

		/**
		 * @brief Destructor
		 */
		~DeviceStateManager();

		/**
		 * @brief Query the current state of the device and build a configuration
		 *
		 * @param callback Callback to call when the query completes
		 * @param channelCount Number of channels to query (default: 32)
		 */
		void queryDeviceState(std::function<void(const Configuration &)> callback, int channelCount = 32);

		/**
		 * @brief Query a single parameter from the device
		 *
		 * @param address OSC address to query
		 * @param callback Callback to call with the result
		 */
		void queryParameter(const std::string &address, QueryCallback callback);

		/**
		 * @brief Query multiple parameters from the device
		 *
		 * @param addresses List of OSC addresses to query
		 * @param callback Callback to call when all queries complete
		 */
		void queryParameters(const std::vector<std::string> &addresses,
							 std::function<void(const std::map<std::string, float> &)> callback);

	private:
		OscController *m_controller;					  // Pointer to the OscController
		std::map<std::string, float> m_queryResults;	  // Results of parameter queries
		int m_pendingQueries;							  // Count of pending queries
		std::map<std::string, QueryCallback> m_callbacks; // Callbacks for individual queries
	};

}
