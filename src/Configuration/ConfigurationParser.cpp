#include "ConfigurationParser.h"
#include "Configuration.h"
#include <string>
#include <nlohmann/json.hpp>

const std::vector<ConfigurationParser::CommandOption> &ConfigurationParser::getCommandOptions()
{
    static std::vector<CommandOption> options = {
        {"--asio-auto-config",
         false,
         "Enable automatic configuration of ASIO device",
         [](Configuration &config, const std::string &)
         {
             config.setUseAsioAutoConfig(true);
             return true;
         }},
        {"--no-asio-auto-config",
         false,
         "Disable automatic configuration of ASIO device",
         [](Configuration &config, const std::string &)
         {
             config.setUseAsioAutoConfig(false);
             return true;
         }},
        {"--asio-device",
         true,
         "Specify ASIO device name",
         [](Configuration &config, const std::string &value)
         {
             config.setAsioDeviceName(value);
             return true;
         }},
        {"--ip",
         true,
         "Specify target IP address for OSC communication",
         [](Configuration &config, const std::string &value)
         {
             config.setTargetIp(value);
             return true;
         }},
        {"--port",
         true,
         "Specify target port for OSC communication",
         [](Configuration &config, const std::string &value)
         {
             try
             {
                 config.setTargetPort(std::stoi(value));
                 return true;
             }
             catch (const std::exception &e)
             {
                 std::cerr << "Invalid port number: " << value << std::endl;
                 return false;
             }
         }},
        {"--receive-port",
         true,
         "Specify receive port for OSC communication",
         [](Configuration &config, const std::string &value)
         {
             try
             {
                 config.setReceivePort(std::stoi(value));
                 return true;
             }
             catch (const std::exception &e)
             {
                 std::cerr << "Invalid port number: " << value << std::endl;
                 return false;
             }
         }},
        {"--sample-rate",
         true,
         "Specify sample rate in Hz",
         [](Configuration &config, const std::string &value)
         {
             try
             {
                 config.setSampleRate(std::stod(value));
                 return true;
             }
             catch (const std::exception &e)
             {
                 std::cerr << "Invalid sample rate: " << value << std::endl;
                 return false;
             }
         }},
        {"--buffer-size",
         true,
         "Specify buffer size in samples",
         [](Configuration &config, const std::string &value)
         {
             try
             {
                 config.setBufferSize(std::stol(value));
                 return true;
             }
             catch (const std::exception &e)
             {
                 std::cerr << "Invalid buffer size: " << value << std::endl;
                 return false;
             }
         }},
        {"--config",
         true,
         "Load configuration from file",
         [](Configuration &config, const std::string &value)
         {
             return ConfigurationParser::parseJsonFile(value, config);
         }},
        {"--help",
         false,
         "Show help text",
         [](Configuration &config, const std::string &)
         {
             ConfigurationParser::printHelp("audioEngine");
             return false; // Returning false will stop parsing
         }}};

    return options;
}

bool ConfigurationParser::parseCommandLine(int argc, char *argv[], Configuration &config)
{
    const auto &options = getCommandOptions();

    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        bool optionHandled = false;

        for (const auto &option : options)
        {
            if (arg == option.flag)
            {
                std::string argValue;

                if (option.hasArg)
                {
                    if (i + 1 >= argc)
                    {
                        std::cerr << "Missing argument for option: " << option.flag << std::endl;
                        return false;
                    }
                    argValue = argv[++i];
                }

                if (!option.handler(config, argValue))
                {
                    // Handler returned false, stop parsing
                    return false;
                }

                optionHandled = true;
                break;
            }
        }

        if (!optionHandled)
        {
            std::cerr << "Unknown option: " << arg << std::endl;
            printHelp(argv[0]);
            return false;
        }
    }

    return true;
}

bool ConfigurationParser::parseJsonFile(const std::string &filePath, Configuration &config)
{
    try
    {
        // Open and parse the JSON file
        nlohmann::json json;
        std::ifstream file(filePath);
        if (!file.is_open())
        {
            std::cerr << "Failed to open configuration file: " << filePath << std::endl;
            return false;
        }

        try
        {
            file >> json;
            file.close();
        }
        catch (const nlohmann::json::exception &e)
        {
            std::cerr << "Error parsing JSON file: " << e.what() << std::endl;
            return false;
        }

        // Parse ASIO auto-configuration options
        if (json.contains("asioAutoConfig"))
        {
            config.setUseAsioAutoConfig(json["asioAutoConfig"].get<bool>());
        }

        if (json.contains("asioDeviceName"))
        {
            config.setAsioDeviceName(json["asioDeviceName"].get<std::string>());
        }

        // Use parseJsonString to handle the rest of the JSON parsing
        return parseJsonString(json.dump(), config);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error in parseJsonFile: " << e.what() << std::endl;
        return false;
    }
}

bool ConfigurationParser::parseJsonString(const std::string &jsonContent, Configuration &config)
{
    try
    {
        auto json = nlohmann::json::parse(jsonContent);

        // Parse all sections using helper methods
        bool success = true;
        success &= parseBasicSettings(json, config);
        success &= parseNodeConfigs(json, config);
        success &= parseConnectionConfigs(json, config);
        success &= parseCommandConfigs(json, config);

        return success;
    }
    catch (const nlohmann::json::exception &e)
    {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
        return false;
    }
}

bool ConfigurationParser::parseBasicSettings(const nlohmann::json &json, Configuration &config)
{
    // Parse device type
    if (json.contains("deviceType"))
        config.setDeviceType(stringToDeviceType(json["deviceType"].get<std::string>()));

    // Parse connection info
    if (json.contains("targetIp") && json.contains("targetPort"))
    {
        int receivePort = json.contains("receivePort") ? json["receivePort"].get<int>() : 0;
        config.setConnectionParams(
            json["targetIp"].get<std::string>(),
            json["targetPort"].get<int>(),
            receivePort);
    }

    // Parse ASIO settings
    if (json.contains("asioDeviceName"))
        config.setAsioDeviceName(json["asioDeviceName"].get<std::string>());

    if (json.contains("sampleRate"))
        config.setSampleRate(json["sampleRate"].get<double>());

    if (json.contains("bufferSize"))
        config.setBufferSize(json["bufferSize"].get<long>());

    if (json.contains("useAsioAutoConfig"))
        config.setUseAsioAutoConfig(json["useAsioAutoConfig"].get<bool>());

    // Parse internal format settings
    if (json.contains("internalFormat"))
        config.setInternalFormat(json["internalFormat"].get<std::string>());

    if (json.contains("internalLayout"))
        config.setInternalLayout(json["internalLayout"].get<std::string>());

    return true;
}

/**
 * @brief Auto-configure ASIO settings based on hardware capabilities
 *
 * @param config Configuration to update
 * @param asioManager Instance of AsioManager to query capabilities
 * @return bool True if auto-configuration succeeded
 */
bool ConfigurationParser::autoConfigureAsio(Configuration &config, AsioManager *asioManager)
{
    if (!asioManager || !asioManager->isInitialized())
    {
        return false;
    }

    // Get optimal device settings
    long bufferSize;
    double sampleRate;
    if (!asioManager->getDefaultDeviceConfiguration(bufferSize, sampleRate))
    {
        return false;
    }

    // Update configuration with optimal settings
    config.setBufferSize(bufferSize);
    config.setSampleRate(sampleRate);

    // Get available channels for auto-configuration
    long inputChannels = asioManager->getInputChannelCount();
    long outputChannels = asioManager->getOutputChannelCount();

    // Create default node configurations if none exist
    if (config.getNodes().empty())
    {
        // Create source node if we have inputs
        if (inputChannels > 0)
        {
            std::vector<long> channels;
            for (long i = 0; i < std::min(2L, inputChannels); i++)
            {
                channels.push_back(i);
            }
            config.addNodeConfig(Configuration::createAsioInputNode("asio_input", channels));
        }

        // Create processor node if we have both inputs and outputs
        if (inputChannels > 0 && outputChannels > 0)
        {
            config.addNodeConfig(Configuration::createProcessorNode("main_processor", "volume=0dB"));
        }

        // Create sink node if we have outputs
        if (outputChannels > 0)
        {
            std::vector<long> channels;
            for (long i = 0; i < std::min(2L, outputChannels); i++)
            {
                channels.push_back(i);
            }
            config.addNodeConfig(Configuration::createAsioOutputNode("asio_output", channels));
        }

        // Create connections if we have created nodes
        const auto &nodes = config.getNodes();
        if (nodes.size() >= 2)
        {
            // Simple case: connect source -> sink
            if (nodes.size() == 2)
            {
                ConnectionConfig conn;
                conn.sourceName = nodes[0].name;
                conn.sourcePad = 0;
                conn.sinkName = nodes[1].name;
                conn.sinkPad = 0;
                config.addConnectionConfig(conn);
            }
            // More complex case: connect source -> processor -> sink
            else if (nodes.size() >= 3)
            {
                // Source to processor
                ConnectionConfig conn1;
                conn1.sourceName = "asio_input";
                conn1.sourcePad = 0;
                conn1.sinkName = "main_processor";
                conn1.sinkPad = 0;
                config.addConnectionConfig(conn1);

                // Processor to sink
                ConnectionConfig conn2;
                conn2.sourceName = "main_processor";
                conn2.sourcePad = 0;
                conn2.sinkName = "asio_output";
                conn2.sinkPad = 0;
                config.addConnectionConfig(conn2);
            }
        }
    }

    return true;
}

void ConfigurationParser::printHelp(const std::string &programName)
{
    std::cout << "Usage: " << programName << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;

    const auto &options = getCommandOptions();
    for (const auto &option : options)
    {
        std::cout << "  " << option.flag;
        if (option.hasArg)
        {
            std::cout << " <value>";
        }
        std::cout << std::endl;
        std::cout << "      " << option.description << std::endl;
    }
}
