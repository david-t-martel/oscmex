#include "ConfigurationParser.h"
#include "Configuration.h"
#include <string>
#include <nlohmann/json.hpp>

bool ConfigurationParser::parseCommandLine(int argc, char *argv[], Configuration &config)
{
    // ...existing code...

    // Parse ASIO auto-configuration options
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];

        if (arg == "--asio-auto-config")
        {
            config.setUseAsioAutoConfig(true);
        }
        else if (arg == "--asio-device" && i + 1 < argc)
        {
            config.setAsioDeviceName(argv[++i]);
        }
        // ...other parsing code...
    }

    // ...existing code...
}

bool ConfigurationParser::parseJsonFile(const std::string &filePath, Configuration &config)
{
    // ...existing code...

    // Parse ASIO auto-configuration options
    nlohmann::json json;
    std::ifstream file(filePath);
    if (file.is_open())
    {
        file >> json;
        file.close();
    }

    if (json.contains("asioAutoConfig"))
    {
        config.setUseAsioAutoConfig(json["asioAutoConfig"].get<bool>());
    }

    if (json.contains("asioDeviceName"))
    {
        config.setAsioDeviceName(json["asioDeviceName"].get<std::string>());
    }

    // ...existing code...
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
            NodeConfig sourceNode;
            sourceNode.name = "asio_input";
            sourceNode.type = "asio_source";
            sourceNode.inputPads = 0; // Source has no inputs
            sourceNode.outputPads = 1;
            sourceNode.description = "ASIO Hardware Input";

            // Use first 2 channels by default (or whatever is available)
            std::string channelIndices;
            for (long i = 0; i < std::min(2L, inputChannels); i++)
            {
                if (i > 0)
                    channelIndices += ",";
                channelIndices += std::to_string(i);
            }
            sourceNode.params["channels"] = channelIndices;

            config.addNodeConfig(sourceNode);
        }

        // Create processor node if we have both inputs and outputs
        if (inputChannels > 0 && outputChannels > 0)
        {
            NodeConfig procNode;
            procNode.name = "main_processor";
            procNode.type = "ffmpeg_processor";
            procNode.inputPads = 1;
            procNode.outputPads = 1;
            procNode.description = "Main Processor";
            procNode.filterGraph = "volume=0dB"; // Identity filter by default

            config.addNodeConfig(procNode);
        }

        // Create sink node if we have outputs
        if (outputChannels > 0)
        {
            NodeConfig sinkNode;
            sinkNode.name = "asio_output";
            sinkNode.type = "asio_sink";
            sinkNode.inputPads = 1;
            sinkNode.outputPads = 0; // Sink has no outputs
            sinkNode.description = "ASIO Hardware Output";

            // Use first 2 channels by default (or whatever is available)
            std::string channelIndices;
            for (long i = 0; i < std::min(2L, outputChannels); i++)
            {
                if (i > 0)
                    channelIndices += ",";
                channelIndices += std::to_string(i);
            }
            sinkNode.params["channels"] = channelIndices;

            config.addNodeConfig(sinkNode);
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
