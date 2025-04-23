#include "DeviceState.h"
#include <stdexcept>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <iostream>

namespace AudioEngine
{
    DeviceState::DeviceState(const std::string &name, DeviceType type)
        : m_name(name), m_type(type)
    {
    }

    DeviceState::~DeviceState()
    {
        // Clean up any resources if needed
    }

    void DeviceState::setProperty(const std::string &key, const std::string &value)
    {
        m_properties[key] = value;
        notifyStateChanged(key, std::any(value));
    }

    std::string DeviceState::getProperty(const std::string &key, const std::string &defaultValue) const
    {
        auto it = m_properties.find(key);
        return (it != m_properties.end()) ? it->second : defaultValue;
    }

    bool DeviceState::hasProperty(const std::string &key) const
    {
        return m_properties.find(key) != m_properties.end();
    }

    void DeviceState::setParameter(const std::string &key, const std::any &value, const std::string &paramType)
    {
        m_parameters[key] = value;
        if (!paramType.empty())
        {
            m_parameterTypes[key] = paramType;
        }
        notifyStateChanged(key, value);
    }

    std::any DeviceState::getParameter(const std::string &key, const std::any &defaultValue) const
    {
        auto it = m_parameters.find(key);
        return (it != m_parameters.end()) ? it->second : defaultValue;
    }

    bool DeviceState::hasParameter(const std::string &key) const
    {
        return m_parameters.find(key) != m_parameters.end();
    }

    std::string DeviceState::getParameterType(const std::string &key) const
    {
        auto it = m_parameterTypes.find(key);
        return (it != m_parameterTypes.end()) ? it->second : "";
    }

    int DeviceState::addStateChangedCallback(std::function<void(const std::string &, const std::any &)> callback)
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        int id = m_nextCallbackId++;
        m_callbacks[id] = callback;
        return id;
    }

    bool DeviceState::removeStateChangedCallback(int callbackId)
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        auto it = m_callbacks.find(callbackId);
        if (it != m_callbacks.end())
        {
            m_callbacks.erase(it);
            return true;
        }
        return false;
    }

    void DeviceState::notifyStateChanged(const std::string &param, const std::any &value)
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        for (const auto &[id, callback] : m_callbacks)
        {
            try
            {
                callback(param, value);
            }
            catch (const std::exception &e)
            {
                std::cerr << "Exception in state change callback: " << e.what() << std::endl;
            }
        }
    }

    std::map<std::string, std::any> DeviceState::diffFrom(const DeviceState &other) const
    {
        std::map<std::string, std::any> differences;

        // Compare basic properties
        if (m_type != other.m_type)
        {
            differences["type"] = std::any(static_cast<int>(other.m_type));
        }
        if (m_status != other.m_status)
        {
            differences["status"] = std::any(static_cast<int>(other.m_status));
        }
        if (m_inputChannelCount != other.m_inputChannelCount)
        {
            differences["inputChannelCount"] = std::any(other.m_inputChannelCount);
        }
        if (m_outputChannelCount != other.m_outputChannelCount)
        {
            differences["outputChannelCount"] = std::any(other.m_outputChannelCount);
        }
        if (m_sampleRate != other.m_sampleRate)
        {
            differences["sampleRate"] = std::any(other.m_sampleRate);
        }
        if (m_bufferSize != other.m_bufferSize)
        {
            differences["bufferSize"] = std::any(other.m_bufferSize);
        }

        // Compare properties
        for (const auto &[key, value] : other.m_properties)
        {
            auto it = m_properties.find(key);
            if (it == m_properties.end() || it->second != value)
            {
                differences["property." + key] = std::any(value);
            }
        }

        // Compare parameters
        for (const auto &[key, value] : other.m_parameters)
        {
            auto it = m_parameters.find(key);
            if (it == m_parameters.end())
            {
                differences["parameter." + key] = value;
            }
            else
            {
                // For simplicity, always add parameter differences
                // A more sophisticated implementation would compare based on types
                differences["parameter." + key] = value;
            }
        }

        return differences;
    }

    bool DeviceState::isCompatibleWith(const DeviceState &other) const
    {
        // Devices are compatible if they are the same type
        if (m_type != other.m_type)
        {
            return false;
        }

        // Names should match
        if (m_name != other.m_name)
        {
            return false;
        }

        // Other compatibility checks can be added here
        return true;
    }

    void DeviceState::updateFromConfiguration(const Configuration &config)
    {
        // Update device properties from configuration
        if (config.getSampleRate() > 0)
        {
            m_sampleRate = config.getSampleRate();
        }

        if (config.getBufferSize() > 0)
        {
            m_bufferSize = static_cast<int>(config.getBufferSize());
        }

        // Map Configuration DeviceType to internal DeviceType
        switch (config.getDeviceType())
        {
        case Configuration::DeviceType::ASIO:
            m_type = DeviceType::ASIO;
            break;
        // Map other device types as needed
        default:
            // Leave type unchanged if not recognized
            break;
        }

        // Store connection parameters as properties
        setProperty("target_ip", config.getTargetIp());
        setProperty("target_port", std::to_string(config.getTargetPort()));
        setProperty("receive_port", std::to_string(config.getReceivePort()));

        // Store ASIO device name
        if (!config.getAsioDeviceName().empty())
        {
            setProperty("asio_device_name", config.getAsioDeviceName());
        }

        // Store additional node and connection configuration details
        // These could be useful for debugging or for creating derived configurations
        if (!config.getNodes().empty())
        {
            setProperty("has_node_config", "true");
            setProperty("node_count", std::to_string(config.getNodes().size()));
        }

        if (!config.getConnections().empty())
        {
            setProperty("has_connection_config", "true");
            setProperty("connection_count", std::to_string(config.getConnections().size()));
        }
    }

    Configuration DeviceState::toConfiguration() const
    {
        Configuration config;

        // Set basic parameters
        config.setSampleRate(m_sampleRate);
        config.setBufferSize(m_bufferSize);

        // Map internal DeviceType to Configuration DeviceType
        switch (m_type)
        {
        case DeviceType::ASIO:
            config.setDeviceType(Configuration::DeviceType::ASIO);
            break;
        // Map other device types as needed
        default:
            config.setDeviceType(Configuration::DeviceType::GENERIC_OSC);
            break;
        }

        // Set connection parameters from properties
        std::string ip = getProperty("target_ip", "127.0.0.1");
        int targetPort = std::stoi(getProperty("target_port", "9000"));
        int receivePort = std::stoi(getProperty("receive_port", "8000"));
        config.setConnectionParams(ip, targetPort, receivePort);

        // Set ASIO device name
        std::string asioDeviceName = getProperty("asio_device_name", "");
        if (!asioDeviceName.empty())
        {
            config.setAsioDeviceName(asioDeviceName);
        }

        // Note: This doesn't preserve node and connection configurations
        // Those would typically be retrieved from a saved configuration file or template

        return config;
    }

    std::string DeviceState::toJson() const
    {
        nlohmann::json j;

        // Basic device information
        j["name"] = m_name;
        j["type"] = static_cast<int>(m_type);
        j["status"] = static_cast<int>(m_status);
        j["inputChannelCount"] = m_inputChannelCount;
        j["outputChannelCount"] = m_outputChannelCount;
        j["sampleRate"] = m_sampleRate;
        j["bufferSize"] = m_bufferSize;

        // Properties
        nlohmann::json props = nlohmann::json::object();
        for (const auto &[key, value] : m_properties)
        {
            props[key] = value;
        }
        j["properties"] = props;

        // Parameters - only include simple types that can be serialized
        nlohmann::json params = nlohmann::json::object();
        for (const auto &[key, value] : m_parameters)
        {
            try
            {
                if (value.type() == typeid(int))
                {
                    params[key] = std::any_cast<int>(value);
                }
                else if (value.type() == typeid(float))
                {
                    params[key] = std::any_cast<float>(value);
                }
                else if (value.type() == typeid(double))
                {
                    params[key] = std::any_cast<double>(value);
                }
                else if (value.type() == typeid(bool))
                {
                    params[key] = std::any_cast<bool>(value);
                }
                else if (value.type() == typeid(std::string))
                {
                    params[key] = std::any_cast<std::string>(value);
                }
                // Skip other types that can't be easily serialized
            }
            catch (const std::exception &)
            {
                // Skip values that can't be cast
            }
        }
        j["parameters"] = params;

        // Parameter types
        nlohmann::json paramTypes = nlohmann::json::object();
        for (const auto &[key, type] : m_parameterTypes)
        {
            paramTypes[key] = type;
        }
        j["parameterTypes"] = paramTypes;

        return j.dump(2); // Pretty print with 2-space indent
    }

    DeviceState DeviceState::fromJson(const std::string &json)
    {
        try
        {
            nlohmann::json j = nlohmann::json::parse(json);

            // Extract basic device information
            std::string name = j.contains("name") ? j["name"].get<std::string>() : "Unknown Device";
            DeviceType type = j.contains("type") ? static_cast<DeviceType>(j["type"].get<int>()) : DeviceType::Unknown;

            DeviceState state(name, type);

            if (j.contains("status"))
            {
                state.setStatus(static_cast<Status>(j["status"].get<int>()));
            }
            if (j.contains("inputChannelCount"))
            {
                state.setInputChannelCount(j["inputChannelCount"].get<int>());
            }
            if (j.contains("outputChannelCount"))
            {
                state.setOutputChannelCount(j["outputChannelCount"].get<int>());
            }
            if (j.contains("sampleRate"))
            {
                state.setSampleRate(j["sampleRate"].get<double>());
            }
            if (j.contains("bufferSize"))
            {
                state.setBufferSize(j["bufferSize"].get<int>());
            }

            // Extract properties
            if (j.contains("properties") && j["properties"].is_object())
            {
                for (auto it = j["properties"].begin(); it != j["properties"].end(); ++it)
                {
                    state.setProperty(it.key(), it.value().get<std::string>());
                }
            }

            // Extract parameters
            if (j.contains("parameters") && j["parameters"].is_object())
            {
                for (auto it = j["parameters"].begin(); it != j["parameters"].end(); ++it)
                {
                    std::string key = it.key();

                    // Determine parameter type
                    std::string paramType;
                    if (j.contains("parameterTypes") &&
                        j["parameterTypes"].is_object() &&
                        j["parameterTypes"].contains(key))
                    {
                        paramType = j["parameterTypes"][key].get<std::string>();
                    }

                    // Set parameter based on JSON type
                    if (it.value().is_number_integer())
                    {
                        state.setParameter(key, std::any(it.value().get<int>()), paramType);
                    }
                    else if (it.value().is_number_float())
                    {
                        state.setParameter(key, std::any(it.value().get<double>()), paramType);
                    }
                    else if (it.value().is_boolean())
                    {
                        state.setParameter(key, std::any(it.value().get<bool>()), paramType);
                    }
                    else if (it.value().is_string())
                    {
                        state.setParameter(key, std::any(it.value().get<std::string>()), paramType);
                    }
                }
            }

            return state;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error parsing device state JSON: " << e.what() << std::endl;
            return DeviceState("Invalid Device", DeviceType::Unknown);
        }
    }

    bool DeviceState::isHealthy() const
    {
        // Check if device is in an operational state
        if (m_status == Status::Error)
        {
            return false;
        }

        // Check for valid sample rate and buffer size
        if (m_sampleRate <= 0 || m_bufferSize <= 0)
        {
            return false;
        }

        // For ASIO devices, check if channels are valid
        if (m_type == DeviceType::ASIO && (m_inputChannelCount <= 0 && m_outputChannelCount <= 0))
        {
            return false;
        }

        // Additional health checks can be added here

        return true;
    }

    bool DeviceState::attemptRecovery()
    {
        // Simple recovery logic - reset status to allow reconnection
        if (m_status == Status::Error)
        {
            m_status = Status::Disconnected;
            return true;
        }
        return false; // No recovery needed or unable to recover
    }

} // namespace AudioEngine
