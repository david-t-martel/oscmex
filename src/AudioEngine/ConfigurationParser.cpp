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
