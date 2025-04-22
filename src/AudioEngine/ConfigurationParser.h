#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

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
    };

} // namespace AudioEngine
