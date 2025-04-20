#include "Configuration.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <nlohmann/json.hpp>

// For command line parsing
#include <cstring>

namespace AudioEngine
{
    Configuration::Configuration()
        : asioDeviceName("ASIO4ALL v2"),
          rmeOscIp("127.0.0.1"),
          rmeOscPort(8000),
          sampleRate(48000.0),
          bufferSize(512)
    {
        // Default empty configuration - nodes, connections and commands
        // will be populated during parsing
    }

    const NodeConfig *Configuration::getNodeConfig(const std::string &name) const
    {
        auto it = std::find_if(nodes.begin(), nodes.end(),
                               [&name](const NodeConfig &config)
                               { return config.name == name; });

        if (it != nodes.end())
        {
            return &(*it);
        }

        return nullptr;
    }

    bool ConfigurationParser::parse(int argc, char *argv[], Configuration &config)
    {
        // Parse command line arguments
        for (int i = 1; i < argc; i++)
        {
            std::string arg = argv[i];

            if (arg == "--config" || arg == "-c")
            {
                if (i + 1 < argc)
                {
                    std::string configFile = argv[++i];
                    if (!parseFromFile(configFile, config))
                    {
                        std::cerr << "Failed to parse config file: " << configFile << std::endl;
                        return false;
                    }
                }
                else
                {
                    std::cerr << "Missing config file path after --config" << std::endl;
                    return false;
                }
            }
            else if (arg == "--asio" || arg == "-a")
            {
                if (i + 1 < argc)
                {
                    config.asioDeviceName = argv[++i];
                }
                else
                {
                    std::cerr << "Missing device name after --asio" << std::endl;
                    return false;
                }
            }
            else if (arg == "--ip" || arg == "-i")
            {
                if (i + 1 < argc)
                {
                    config.rmeOscIp = argv[++i];
                }
                else
                {
                    std::cerr << "Missing IP after --ip" << std::endl;
                    return false;
                }
            }
            else if (arg == "--port" || arg == "-p")
            {
                if (i + 1 < argc)
                {
                    try
                    {
                        config.rmeOscPort = std::stoi(argv[++i]);
                    }
                    catch (const std::exception &e)
                    {
                        std::cerr << "Invalid port number: " << argv[i] << std::endl;
                        return false;
                    }
                }
                else
                {
                    std::cerr << "Missing port number after --port" << std::endl;
                    return false;
                }
            }
            else if (arg == "--rate" || arg == "-r")
            {
                if (i + 1 < argc)
                {
                    try
                    {
                        config.sampleRate = std::stod(argv[++i]);
                    }
                    catch (const std::exception &e)
                    {
                        std::cerr << "Invalid sample rate: " << argv[i] << std::endl;
                        return false;
                    }
                }
                else
                {
                    std::cerr << "Missing sample rate after --rate" << std::endl;
                    return false;
                }
            }
            else if (arg == "--buffer" || arg == "-b")
            {
                if (i + 1 < argc)
                {
                    try
                    {
                        config.bufferSize = std::stol(argv[++i]);
                    }
                    catch (const std::exception &e)
                    {
                        std::cerr << "Invalid buffer size: " << argv[i] << std::endl;
                        return false;
                    }
                }
                else
                {
                    std::cerr << "Missing buffer size after --buffer" << std::endl;
                    return false;
                }
            }
            else if (arg == "--help" || arg == "-h")
            {
                std::cout << "Audio Engine Configuration Options:\n"
                          << "  --config, -c <file>     Configuration file path\n"
                          << "  --asio, -a <name>       ASIO device name\n"
                          << "  --ip, -i <address>      OSC target IP address\n"
                          << "  --port, -p <number>     OSC target port\n"
                          << "  --rate, -r <number>     Sample rate in Hz\n"
                          << "  --buffer, -b <number>   Buffer size in frames\n"
                          << "  --help, -h              Show this help\n";
                return false; // Return false to indicate that we should exit after showing help
            }
        }

        return true;
    }

    bool ConfigurationParser::parseFromFile(const std::string &filePath, Configuration &config)
    {
        try
        {
            // Check if file exists
            if (!std::filesystem::exists(filePath))
            {
                std::cerr << "Config file not found: " << filePath << std::endl;
                return false;
            }

            // Read file content
            std::ifstream file(filePath);
            if (!file.is_open())
            {
                std::cerr << "Failed to open config file: " << filePath << std::endl;
                return false;
            }

            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string content = buffer.str();
            file.close();

            // Determine file type based on extension
            std::string extension = std::filesystem::path(filePath).extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                           [](unsigned char c)
                           { return std::tolower(c); });

            if (extension == ".json")
            {
                return parseJson(content, config);
            }
            else
            {
                std::cerr << "Unsupported config file format: " << extension << std::endl;
                return false;
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error parsing config file: " << e.what() << std::endl;
            return false;
        }
    }

    bool ConfigurationParser::parseJson(const std::string &content, Configuration &config)
    {
        try
        {
            // Parse JSON
            nlohmann::json json = nlohmann::json::parse(content);

            // Parse global settings
            if (json.contains("asioDeviceName") && json["asioDeviceName"].is_string())
            {
                config.asioDeviceName = json["asioDeviceName"].get<std::string>();
            }

            if (json.contains("rmeOscIp") && json["rmeOscIp"].is_string())
            {
                config.rmeOscIp = json["rmeOscIp"].get<std::string>();
            }

            if (json.contains("rmeOscPort") && json["rmeOscPort"].is_number())
            {
                config.rmeOscPort = json["rmeOscPort"].get<int>();
            }

            if (json.contains("sampleRate") && json["sampleRate"].is_number())
            {
                config.sampleRate = json["sampleRate"].get<double>();
            }

            if (json.contains("bufferSize") && json["bufferSize"].is_number())
            {
                config.bufferSize = json["bufferSize"].get<long>();
            }

            // Parse nodes
            if (json.contains("nodes") && json["nodes"].is_array())
            {
                for (const auto &nodeJson : json["nodes"])
                {
                    if (nodeJson.contains("name") && nodeJson["name"].is_string() &&
                        nodeJson.contains("type") && nodeJson["type"].is_string())
                    {
                        NodeConfig node;
                        node.name = nodeJson["name"].get<std::string>();
                        node.type = nodeJson["type"].get<std::string>();

                        // Parse parameters
                        if (nodeJson.contains("params") && nodeJson["params"].is_object())
                        {
                            for (auto it = nodeJson["params"].begin(); it != nodeJson["params"].end(); ++it)
                            {
                                node.params[it.key()] = it.value().is_string() ? it.value().get<std::string>() : it.value().dump();
                            }
                        }

                        config.nodes.push_back(node);
                    }
                    else
                    {
                        std::cerr << "Warning: Skipping invalid node configuration" << std::endl;
                    }
                }
            }

            // Parse connections
            if (json.contains("connections") && json["connections"].is_array())
            {
                for (const auto &connJson : json["connections"])
                {
                    if (connJson.contains("sourceName") && connJson["sourceName"].is_string() &&
                        connJson.contains("sourcePad") && connJson["sourcePad"].is_number() &&
                        connJson.contains("sinkName") && connJson["sinkName"].is_string() &&
                        connJson.contains("sinkPad") && connJson["sinkPad"].is_number())
                    {
                        ConnectionConfig conn;
                        conn.sourceName = connJson["sourceName"].get<std::string>();
                        conn.sourcePad = connJson["sourcePad"].get<int>();
                        conn.sinkName = connJson["sinkName"].get<std::string>();
                        conn.sinkPad = connJson["sinkPad"].get<int>();

                        config.connections.push_back(conn);
                    }
                    else
                    {
                        std::cerr << "Warning: Skipping invalid connection configuration" << std::endl;
                    }
                }
            }

            // Parse OSC commands
            if (json.contains("rmeCommands") && json["rmeCommands"].is_array())
            {
                for (const auto &cmdJson : json["rmeCommands"])
                {
                    if (cmdJson.contains("address") && cmdJson["address"].is_string() &&
                        cmdJson.contains("args") && cmdJson["args"].is_array())
                    {
                        RmeOscCommandConfig cmd;
                        cmd.address = cmdJson["address"].get<std::string>();

                        // Parse arguments with their types
                        for (const auto &argJson : cmdJson["args"])
                        {
                            if (argJson.is_boolean())
                            {
                                cmd.args.push_back(std::any(argJson.get<bool>()));
                            }
                            else if (argJson.is_number_integer())
                            {
                                cmd.args.push_back(std::any(argJson.get<int>()));
                            }
                            else if (argJson.is_number_float())
                            {
                                cmd.args.push_back(std::any(argJson.get<float>()));
                            }
                            else if (argJson.is_string())
                            {
                                cmd.args.push_back(std::any(argJson.get<std::string>()));
                            }
                            else
                            {
                                std::cerr << "Warning: Skipping unsupported argument type" << std::endl;
                            }
                        }

                        config.rmeCommands.push_back(cmd);
                    }
                    else
                    {
                        std::cerr << "Warning: Skipping invalid OSC command configuration" << std::endl;
                    }
                }
            }

            return true;
        }
        catch (const nlohmann::json::exception &e)
        {
            std::cerr << "JSON parsing error: " << e.what() << std::endl;
            return false;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error parsing JSON: " << e.what() << std::endl;
            return false;
        }
    }

} // namespace AudioEngine
