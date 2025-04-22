#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

**
 * @file ConfigurationParser.h
 * @brief Parser for configuration from various sources
 *
 * The ConfigurationParser provides methods to load and parse configuration
 * data from various sources including command line arguments and JSON files.
 * It is responsible for:
 * - Converting raw input into Configuration objects
 * - Validating configuration data
 * - Providing auto-configuration capabilities
 *
 * This class does not maintain state itself and all methods are static.
 */

namespace AudioEngine
{
    // Forward declarations
    class Configuration;
    class AsioManager;

    /**
     * @brief Parser for engine configuration from different sources
     *
     * This class provides methods to parse configuration from command line arguments,
     * JSON files, and can auto-configure ASIO settings based on hardware capabilities.
     */
    class ConfigurationParser
    {
    public:
        /**
         * @brief Parse configuration from command line arguments
         *
         * @param argc Argument count
         * @param argv Argument values
         * @param config Configuration to fill
         * @return bool True if parsing was successful
         */
        static bool parseCommandLine(int argc, char *argv[], Configuration &config);

        /**
         * @brief Parse configuration from a JSON file
         *
         * @param filePath Path to the JSON file
         * @param config Configuration to fill
         * @return bool True if parsing was successful
         */
        static bool parseJsonFile(const std::string &filePath, Configuration &config);

        /**
         * @brief Parse configuration from a JSON string
         *
         * @param jsonContent JSON content as string
         * @param config Configuration to fill
         * @return bool True if parsing was successful
         */
        static bool parseJsonString(const std::string &jsonContent, Configuration &config);

        /**
         * @brief Auto-configure ASIO settings based on hardware capabilities
         *
         * @param config Configuration to update
         * @param asioManager Instance of AsioManager to query capabilities
         * @return bool True if auto-configuration succeeded
         */
        static bool autoConfigureAsio(Configuration &config, AsioManager *asioManager);

        /**
         * @brief Parse configuration from JSON content
         *
         * Core JSON parsing logic used by all JSON-related methods
         *
         * @param jsonContent JSON content as string
         * @param config Configuration to populate
         * @return bool True if parsing was successful
         */
        static bool parseJsonString(const std::string &jsonContent, Configuration &config);

    private:
        /**
         * @brief Parse a node configuration from JSON
         *
         * @param nodeJson JSON object containing node configuration
         * @param config Configuration to update
         * @return bool True if parsing was successful
         */
        static bool parseNodeConfig(const nlohmann::json &nodeJson, Configuration &config);

        /**
         * @brief Parse a connection configuration from JSON
         *
         * @param connectionJson JSON object containing connection configuration
         * @param config Configuration to update
         * @return bool True if parsing was successful
         */
        static bool parseConnectionConfig(const nlohmann::json &connectionJson, Configuration &config);

        /**
         * @brief Parse external command configurations from JSON
         *
         * @param commandsJson JSON array containing external command configurations
         * @param config Configuration to update
         * @return bool True if parsing was successful
         */
        static bool parseExternalCommands(const nlohmann::json &commandsJson, Configuration &config);

        /**
         * @brief Parse basic configuration settings from JSON
         */
        static bool parseBasicSettings(const nlohmann::json &json, Configuration &config);

        /**
         * @brief Parse node configurations from JSON
         */
        static bool parseNodeConfigs(const nlohmann::json &json, Configuration &config);

        /**
         * @brief Parse connection configurations from JSON
         */
        static bool parseConnectionConfigs(const nlohmann::json &json, Configuration &config);

        /**
         * @brief Parse command configurations from JSON
         */
        static bool parseCommandConfigs(const nlohmann::json &json, Configuration &config);

        /**
         * @brief Command line option definition
         */
        struct CommandOption
        {
            std::string flag;                                                  // Command line flag (e.g., "--asio-device")
            bool hasArg;                                                       // Whether the option requires an argument
            std::string description;                                           // Description for help text
            std::function<bool(Configuration &, const std::string &)> handler; // Handler function
        };

        /**
         * @brief Get registered command line options
         * @return std::vector<CommandOption> List of command options
         */
        static const std::vector<CommandOption> &getCommandOptions();

        /**
         * @brief Print help text for command line options
         * @param programName Name of the program (argv[0])
         */
        static void printHelp(const std::string &programName);
    };

} // namespace AudioEngine
