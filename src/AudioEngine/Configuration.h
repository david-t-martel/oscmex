#pragma once

#include <string>
#include <vector>
#include <map>
#include <any>

namespace AudioEngine
{

	/**
	 * @brief Configuration structure for a single node
	 */
	struct NodeConfig
	{
		std::string name;						   // Unique node identifier
		std::string type;						   // "asio_source", "file_sink", "ffmpeg_processor", etc.
		std::map<std::string, std::string> params; // Type-specific parameters
	};

	/**
	 * @brief Configuration for a connection between node pads
	 */
	struct ConnectionConfig
	{
		std::string sourceNode; // Name of source node
		int sourcePad = 0;		// Output pad index on source node
		std::string sinkNode;	// Name of sink node
		int sinkPad = 0;		// Input pad index on sink node
	};

	/**
	 * @brief Configuration for an OSC command to RME device
	 */
	struct RmeOscCommandConfig
	{
		std::string address;		// OSC address (e.g., "/1/matrix/1/1/gain")
		std::vector<std::any> args; // Command arguments (float, int, string)
	};

	/**
	 * @brief Complete configuration for the audio engine
	 */
	struct Configuration
	{
		// Global settings
		std::string asioDeviceName;			// ASIO device to use (e.g., "ASIO Fireface UCX II")
		std::string rmeOscIp = "127.0.0.1"; // IP address for RME OSC control
		int rmeOscPort = 9000;				// Port for RME OSC control
		double sampleRate = 48000.0;		// Default or requested sample rate
		long bufferSize = 512;				// Default or requested buffer size

		// Components
		std::vector<NodeConfig> nodes;				  // Nodes to create
		std::vector<ConnectionConfig> connections;	  // How to connect nodes
		std::vector<RmeOscCommandConfig> rmeCommands; // OSC commands to send
	};

	/**
	 * @brief Parses configuration from command line or file
	 */
	class ConfigurationParser
	{
	public:
		/**
		 * @brief Parse configuration from command line arguments
		 *
		 * @param argc Argument count from main()
		 * @param argv Argument values from main()
		 * @param outConfig Configuration to populate
		 * @return true if parsing succeeded
		 * @return false if parsing failed
		 */
		bool parse(int argc, char *argv[], Configuration &outConfig);

		/**
		 * @brief Parse configuration from a file
		 *
		 * @param filePath Path to configuration file
		 * @param outConfig Configuration to populate
		 * @return true if parsing succeeded
		 * @return false if parsing failed
		 */
		bool parseFromFile(const std::string &filePath, Configuration &outConfig);

	private:
		// Helper methods for parsing different parts of configuration
		bool parseNodes(const std::string &jsonNodes, Configuration &outConfig);
		bool parseConnections(const std::string &jsonConnections, Configuration &outConfig);
		bool parseRmeCommands(const std::string &jsonCommands, Configuration &outConfig);
	};

} // namespace AudioEngine
