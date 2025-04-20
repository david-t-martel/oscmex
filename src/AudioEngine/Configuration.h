#pragma once

#include <string>
#include <vector>
#include <map>
#include <any>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp> // Updated to use nlohmann/json directly

namespace AudioEngine
{
	// Forward declaration for classes that will be referenced
	class OscController;

	/**
	 * @brief Enum for device types supported by the configuration
	 */
	enum class DeviceType
	{
		RME_TOTALMIX, // RME TotalMix FX
		GENERIC_OSC,  // Generic OSC device
					  // Add more device types as needed
	};

	/**
	 * @brief Convert DeviceType to string representation
	 * @param type The device type to convert
	 * @return std::string The string representation
	 */
	std::string deviceTypeToString(DeviceType type);

	/**
	 * @brief Convert string to DeviceType
	 * @param typeStr The string to convert
	 * @return DeviceType The device type
	 */
	DeviceType stringToDeviceType(const std::string &typeStr);

	/**
	 * @brief Structure to represent a single OSC command
	 */
	struct OscCommandConfig
	{
		std::string address;		// OSC address (e.g., "/1/input/1/volume")
		std::vector<std::any> args; // Command arguments (float, int, bool, string)

		// Constructors for convenience
		OscCommandConfig() = default;
		OscCommandConfig(const std::string &addr, const std::vector<std::any> &arguments)
			: address(addr), args(arguments) {}
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
	 * @brief Class for managing device configuration
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
		~Configuration() = default;

		/**
		 * @brief Load configuration from a JSON file
		 *
		 * @param filePath Path to the JSON configuration file
		 * @param success Optional output parameter to indicate success/failure
		 * @return Configuration object
		 */
		static Configuration loadFromFile(const std::string &filePath, bool &success);

		/**
		 * @brief Load configuration from a JSON file (overload without success flag)
		 *
		 * @param filePath Path to the JSON configuration file
		 * @return true if loading succeeded, false otherwise
		 */
		bool loadFromFile(const std::string &filePath);

		/**
		 * @brief Save configuration to a JSON file
		 *
		 * @param filePath Path to save the JSON configuration file
		 * @return true if saving succeeded, false otherwise
		 */
		bool saveToFile(const std::string &filePath) const;

		/**
		 * @brief Convert configuration to JSON string
		 *
		 * @return std::string JSON representation of the configuration
		 */
		std::string toJsonString() const;

		/**
		 * @brief Create configuration from JSON string
		 *
		 * @param jsonStr JSON string to parse
		 * @return Configuration object
		 */
		static Configuration fromJsonString(const std::string &jsonStr);

		/**
		 * @brief Set device type for this configuration
		 *
		 * @param type The device type
		 */
		void setDeviceType(DeviceType type) { m_deviceType = type; }

		/**
		 * @brief Get device type
		 *
		 * @return DeviceType
		 */
		DeviceType getDeviceType() const { return m_deviceType; }

		/**
		 * @brief Set OSC connection parameters
		 *
		 * @param ip Target IP address (e.g., "127.0.0.1")
		 * @param sendPort Port to send OSC commands to
		 * @param receivePort Port to receive OSC responses on (optional)
		 */
		void setConnectionParams(const std::string &ip, int sendPort, int receivePort = 0);

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
		 * @brief Add a matrix crosspoint gain setting (RME specific)
		 *
		 * @param input Input channel (1-based)
		 * @param output Output channel (1-based)
		 * @param gainDb Gain in decibels
		 */
		void setMatrixCrosspointGain(int input, int output, float gainDb);

		/**
		 * @brief Add a channel mute setting (RME specific)
		 *
		 * @param channelType Channel type (1=input, 2=playback, 3=output)
		 * @param channel Channel number (1-based)
		 * @param mute Mute state
		 */
		void setChannelMute(int channelType, int channel, bool mute);

		/**
		 * @brief Add a channel volume setting (RME specific)
		 *
		 * @param channelType Channel type (1=input, 2=playback, 3=output)
		 * @param channel Channel number (1-based)
		 * @param volumeDb Volume in decibels
		 */
		void setChannelVolume(int channelType, int channel, float volumeDb);

		/**
		 * @brief Get all configured OSC commands
		 *
		 * @return const std::vector<OscCommandConfig>& Vector of command configurations
		 */
		const std::vector<OscCommandConfig> &getCommands() const { return m_commands; }

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

		/**
		 * @brief Get a node configuration by name
		 *
		 * @param name Name of the node to find
		 * @return const NodeConfig* Pointer to the found node, or nullptr if not found
		 */
		const NodeConfig *getNodeConfig(const std::string &name) const;

		/**
		 * @brief Add a node configuration
		 *
		 * @param node Node configuration to add
		 */
		void addNodeConfig(const NodeConfig &node);

		/**
		 * @brief Add a connection configuration
		 *
		 * @param connection Connection configuration to add
		 */
		void addConnectionConfig(const ConnectionConfig &connection);

		/**
		 * @brief Get all node configurations
		 *
		 * @return const std::vector<NodeConfig>& Vector of node configurations
		 */
		const std::vector<NodeConfig> &getNodes() const { return m_nodes; }

		/**
		 * @brief Get all connection configurations
		 *
		 * @return const std::vector<ConnectionConfig>& Vector of connection configurations
		 */
		const std::vector<ConnectionConfig> &getConnections() const { return m_connections; }

		/**
		 * @brief Set ASIO device name
		 *
		 * @param name Name of the ASIO device
		 */
		void setAsioDeviceName(const std::string &name) { m_asioDeviceName = name; }

		/**
		 * @brief Get ASIO device name
		 *
		 * @return std::string The ASIO device name
		 */
		std::string getAsioDeviceName() const { return m_asioDeviceName; }

		/**
		 * @brief Set sample rate
		 *
		 * @param rate Sample rate in Hz
		 */
		void setSampleRate(double rate) { m_sampleRate = rate; }

		/**
		 * @brief Get sample rate
		 *
		 * @return double Sample rate in Hz
		 */
		double getSampleRate() const { return m_sampleRate; }

		/**
		 * @brief Set buffer size
		 *
		 * @param size Buffer size in samples
		 */
		void setBufferSize(long size) { m_bufferSize = size; }

		/**
		 * @brief Get buffer size
		 *
		 * @return long Buffer size in samples
		 */
		long getBufferSize() const { return m_bufferSize; }

	private:
		DeviceType m_deviceType;					 // Type of device for this configuration
		std::string m_targetIp;						 // Target IP address
		int m_targetPort;							 // Target port for sending
		int m_receivePort;							 // Port for receiving
		std::vector<OscCommandConfig> m_commands;	 // List of OSC commands
		std::string m_asioDeviceName;				 // ASIO device name
		double m_sampleRate;						 // Sample rate in Hz
		long m_bufferSize;							 // Buffer size in samples
		std::vector<NodeConfig> m_nodes;			 // Audio processing nodes
		std::vector<ConnectionConfig> m_connections; // Connections between nodes
	};

	/**
	 * @brief Class for parsing configuration from command line
	 */
	class ConfigurationParser
	{
	public:
		/**
		 * @brief Parse configuration from command line arguments
		 *
		 * @param argc Argument count
		 * @param argv Argument values
		 * @param config Configuration to update
		 * @return true if parsing succeeded
		 * @return false if parsing failed
		 */
		static bool parse(int argc, char *argv[], Configuration &config);

	private:
		/**
		 * @brief Parse configuration from a file
		 *
		 * @param filePath Path to the configuration file
		 * @param config Configuration to update
		 * @return true if parsing succeeded
		 * @return false if parsing failed
		 */
		static bool parseFromFile(const std::string &filePath, Configuration &config);
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
