#pragma once

#include <string>
#include <vector>
#include <map>
#include <any>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp> // Updated to use nlohmann/json directly
/**
 * @file Configuration.h
 * @brief Configuration system for the audio engine
 *
 * The Configuration class defines how the audio engine is set up, including:
 * - Device settings (ASIO device, sample rate, buffer size)
 * - Audio processing node definitions
 * - Node connection specifications
 * - External control commands
 *
 * A Configuration object represents the *desired state* of the system,
 * not necessarily its current state. The DeviceStateManager is responsible
 * for applying configuration to devices and tracking their current state.
 */
namespace AudioEngine
{
	// Forward declaration for classes that will be referenced
	class OscController;
	class DeviceStateManager; // Forward declaration for DeviceStateManager
	class AsioManager;

	/**
	 * @brief Enum for device types supported by the configuration
	 */
	enum class DeviceType
	{
		RME_TOTALMIX, // RME TotalMix FX
		GENERIC_OSC,  // Generic OSC device
		ASIO,		  // ASIO device
					  // Add more device types as needed
	};

	/**
	 * @brief Convert DeviceType to string representation
	 * @param type The device type to convert
	 * @return std::string The string representation
	 */
	inline std::string deviceTypeToString(DeviceType type)
	{
		switch (type)
		{
		case DeviceType::RME_TOTALMIX:
			return "RME_TOTALMIX";
		case DeviceType::GENERIC_OSC:
			return "GENERIC_OSC";
		case DeviceType::ASIO:
			return "ASIO";
		default:
			return "UNKNOWN";
		}
	}

	/**
	 * @brief Convert string to DeviceType
	 * @param typeStr The string to convert
	 * @return DeviceType The device type
	 */
	inline DeviceType stringToDeviceType(const std::string &typeStr)
	{
		if (typeStr == "RME_TOTALMIX")
			return DeviceType::RME_TOTALMIX;
		else if (typeStr == "GENERIC_OSC")
			return DeviceType::GENERIC_OSC;
		else if (typeStr == "ASIO")
			return DeviceType::ASIO;
		else
			return DeviceType::GENERIC_OSC; // Default
	}

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
		std::string type;						   // Node type (e.g., "asio_source", "ffmpeg_processor")
		std::map<std::string, std::string> params; // Node parameters

		// New fields
		int inputPads = 1;		 // Number of input pads (0 for source nodes)
		int outputPads = 1;		 // Number of output pads (0 for sink nodes)
		std::string description; // Human-readable description

		// For FFmpeg processor nodes
		std::string filterGraph; // FFmpeg filter graph definition

		// For file nodes
		std::string filePath;	// Path for file sources/sinks
		std::string fileFormat; // File format specification

		// For ASIO nodes
		std::vector<long> channelIndices; // ASIO channel indices to use
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

		// New fields
		bool formatConversion = true; // Allow format conversion if needed
		std::string bufferPolicy;	  // Buffer policy: "copy", "reference", "auto"
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

		/**
		 * @brief Get whether to use auto-configuration for ASIO
		 *
		 * @return true if auto-configuration should be used
		 */
		bool useAsioAutoConfig() const { return m_useAsioAutoConfig; }

		/**
		 * @brief Set whether to use auto-configuration for ASIO
		 *
		 * @param value true to enable auto-configuration
		 */
		void setUseAsioAutoConfig(bool value) { m_useAsioAutoConfig = value; }

		/**
		 * @brief Get the name of the ASIO device to use
		 *
		 * @return ASIO device name
		 */
		std::string getAsioDeviceName() const { return m_asioDeviceName; }

		/**
		 * @brief Set the name of the ASIO device to use
		 *
		 * @param name ASIO device name
		 */
		void setAsioDeviceName(const std::string &name) { m_asioDeviceName = name; }

		/**
		 * @brief Run auto-discovery to populate configuration based on available hardware
		 *
		 * @param asioManager Pointer to AsioManager for device querying
		 * @param createDefaultGraph If true, creates a default graph based on discovered hardware
		 * @return bool True if auto-discovery succeeded
		 */
		bool autoDiscover(AsioManager *asioManager, bool createDefaultGraph = true);

		/**
		 * @brief Create a default processing graph based on device capabilities
		 *
		 * @param inputChannels Number of input channels to use (0 = all available)
		 * @param outputChannels Number of output channels to use (0 = all available)
		 * @param addProcessor Whether to add a default processor node
		 * @return bool True if graph creation succeeded
		 */
		bool createDefaultGraph(int inputChannels = 2, int outputChannels = 2, bool addProcessor = true);

		/**
		 * @brief Set internal processing format
		 *
		 * @param format Format identifier (e.g., "f32", "s16")
		 */
		void setInternalFormat(const std::string &format);

		/**
		 * @brief Get internal processing format
		 *
		 * @return std::string Format identifier
		 */
		std::string getInternalFormat() const { return m_internalFormat; }

		/**
		 * @brief Set internal channel layout
		 *
		 * @param layout Channel layout specification (e.g., "stereo", "5.1")
		 */
		void setInternalLayout(const std::string &layout);

		/**
		 * @brief Get internal channel layout
		 *
		 * @return std::string Channel layout specification
		 */
		std::string getInternalLayout() const { return m_internalLayout; }

		/**
		 * @brief Create an ASIO input node configuration
		 *
		 * @param name Node name
		 * @param channels Channel indices to use
		 * @return NodeConfig Configured node
		 */
		static NodeConfig createAsioInputNode(const std::string &name, const std::vector<long> &channels);

		/**
		 * @brief Create an ASIO output node configuration
		 *
		 * @param name Node name
		 * @param channels Channel indices to use
		 * @return NodeConfig Configured node
		 */
		static NodeConfig createAsioOutputNode(const std::string &name, const std::vector<long> &channels);

		/**
		 * @brief Create a processor node configuration
		 *
		 * @param name Node name
		 * @param filterGraph FFmpeg filter graph
		 * @return NodeConfig Configured node
		 */
		static NodeConfig createProcessorNode(const std::string &name, const std::string &filterGraph);

		/**
		 * @brief Create a file source node configuration
		 *
		 * @param name Node name
		 * @param filePath Path to the audio file
		 * @param format Format specification (optional)
		 * @return NodeConfig Configured node
		 */
		static NodeConfig createFileSourceNode(const std::string &name,
											   const std::string &filePath,
											   const std::string &format = "");

		// Convert to/from DeviceState
		DeviceState toDeviceState() const;
		static Configuration fromDeviceState(const DeviceState &state);

		// Apply configuration to a device via DeviceStateManager
		bool applyToDevice(DeviceStateManager &manager, std::function<void(bool)> callback);

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
		bool m_useAsioAutoConfig = false;
		std::string m_internalFormat = "f32";	 // Default to float
		std::string m_internalLayout = "stereo"; // Default to stereo
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

	// DeviceStateManager has been moved to its own file (DeviceStateManager.h)
}
