#pragma once

#include <string>
#include <vector>
#include <map>
#include <any>

namespace AudioEngine
{

	/**
	 * @brief Node configuration structure
	 */
	struct NodeConfig
	{
		std::string name;						   // Node name
		std::string type;						   // Node type string
		std::map<std::string, std::string> params; // Node parameters
	};

	/**
	 * @brief Connection configuration structure
	 */
	struct ConnectionConfig
	{
		std::string sourceName; // Source node name
		int sourcePad;			// Source node output pad index
		std::string sinkName;	// Sink node name
		int sinkPad;			// Sink node input pad index
	};

	/**
	 * @brief RME OSC command configuration structure
	 */
	struct RmeOscCommandConfig
	{
		std::string address;		// OSC address
		std::vector<std::any> args; // Command arguments
	};

	/**
	 * @brief Global configuration for the audio engine
	 */
	class Configuration
	{
	public:
		// Global settings
		std::string asioDeviceName; // ASIO device name
		std::string rmeOscIp;		// RME OSC target IP
		int rmeOscPort;				// RME OSC target port
		double sampleRate;			// Sample rate in Hz
		long bufferSize;			// Buffer size in frames

		// Node configurations
		std::vector<NodeConfig> nodes;

		// Connection configurations
		std::vector<ConnectionConfig> connections;

		// RME OSC commands to execute at startup
		std::vector<RmeOscCommandConfig> rmeCommands;

		/**
		 * @brief Create default configuration
		 */
		Configuration();

		/**
		 * @brief Get a node configuration by name
		 *
		 * @param name Node name
		 * @return Pointer to node config or nullptr if not found
		 */
		const NodeConfig *getNodeConfig(const std::string &name) const;
	};

	/**
	 * @brief Configuration parser
	 */
	class ConfigurationParser
	{
	public:
		/**
		 * @brief Parse configuration from command line arguments
		 *
		 * @param argc Argument count
		 * @param argv Argument values
		 * @param config Configuration to populate
		 * @return true if parsing was successful
		 */
		bool parse(int argc, char *argv[], Configuration &config);

		/**
		 * @brief Parse configuration from a file
		 *
		 * @param filePath Path to configuration file
		 * @param config Configuration to populate
		 * @return true if parsing was successful
		 */
		bool parseFromFile(const std::string &filePath, Configuration &config);

	private:
		// Helper methods for parsing different file formats
		bool parseJson(const std::string &content, Configuration &config);
		bool parseYaml(const std::string &content, Configuration &config);
	};

} // namespace AudioEngine
