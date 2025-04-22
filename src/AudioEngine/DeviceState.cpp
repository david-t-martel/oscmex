#include "DeviceState.h"
#include <stdexcept>
#include <algorithm>

namespace AudioEngine
{

    DeviceState::DeviceState(const std::string &id) : m_id(id)
    {
        // Initialize with default values
    }

    DeviceState::DeviceState(const std::string &deviceId, const std::string &deviceName)
        : m_deviceId(deviceId), m_deviceName(deviceName)
    {
        // Initialize with default values
    }

    DeviceState::~DeviceState()
    {
        // Clean up any resources if needed
    }

    const std::string &DeviceState::getId() const
    {
        return m_id;
    }

    const std::string &DeviceState::getDeviceId() const
    {
        return m_deviceId;
    }

    bool DeviceState::hasParameter(const std::string &address) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_parameters.find(address) != m_parameters.end();
    }

    bool DeviceState::removeParameter(const std::string &address)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_parameters.find(address);
        if (it != m_parameters.end())
        {
            m_parameters.erase(it);
            m_parameterTypes.erase(address);
            return true;
        }

        return false;
    }

    std::vector<std::string> DeviceState::getParameterAddresses() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::vector<std::string> addresses;
        addresses.reserve(m_parameters.size());

        for (const auto &pair : m_parameters)
        {
            addresses.push_back(pair.first);
        }

        return addresses;
    }

    nlohmann::json DeviceState::toJson() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        nlohmann::json json;
        json["id"] = m_id;
        json["deviceId"] = m_deviceId;
        json["deviceName"] = m_deviceName;
        json["active"] = m_active;
        json["inputChannelCount"] = m_inputChannelCount;
        json["outputChannelCount"] = m_outputChannelCount;
        json["sampleRate"] = m_sampleRate;
        json["bufferSize"] = m_bufferSize;

        // Convert parameters to JSON
        nlohmann::json parametersJson = nlohmann::json::object();
        for (const auto &pair : m_parameters)
        {
            auto typeIt = m_parameterTypes.find(pair.first);
            if (typeIt != m_parameterTypes.end())
            {
                parametersJson[pair.first] = anyToJson(pair.second, typeIt->second);
            }
        }
        json["parameters"] = parametersJson;

        // Convert properties to JSON
        nlohmann::json propertiesJson = nlohmann::json::object();
        for (const auto &pair : m_properties)
        {
            propertiesJson[pair.first] = pair.second;
        }
        json["properties"] = propertiesJson;

        return json;
    }

    bool DeviceState::fromJson(const nlohmann::json &json)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        try
        {
            // Basic properties
            if (json.contains("id"))
                m_id = json["id"].get<std::string>();
            if (json.contains("deviceId"))
                m_deviceId = json["deviceId"].get<std::string>();
            if (json.contains("deviceName"))
                m_deviceName = json["deviceName"].get<std::string>();
            if (json.contains("active"))
                m_active = json["active"].get<bool>();
            if (json.contains("inputChannelCount"))
                m_inputChannelCount = json["inputChannelCount"].get<int>();
            if (json.contains("outputChannelCount"))
                m_outputChannelCount = json["outputChannelCount"].get<int>();
            if (json.contains("sampleRate"))
                m_sampleRate = json["sampleRate"].get<double>();
            if (json.contains("bufferSize"))
                m_bufferSize = json["bufferSize"].get<int>();

            // Load parameters
            if (json.contains("parameters") && json["parameters"].is_object())
            {
                const auto &parametersJson = json["parameters"];
                for (auto it = parametersJson.begin(); it != parametersJson.end(); ++it)
                {
                    std::any value;
                    std::type_index type(typeid(void));
                    jsonToAny(it.value(), value, type);

                    m_parameters[it.key()] = value;
                    m_parameterTypes[it.key()] = type;
                }
            }

            // Load properties
            if (json.contains("properties") && json["properties"].is_object())
            {
                const auto &propertiesJson = json["properties"];
                for (auto it = propertiesJson.begin(); it != propertiesJson.end(); ++it)
                {
                    m_properties[it.key()] = it.value().get<std::string>();
                }
            }

            return true;
        }
        catch (const std::exception &e)
        {
            // Log error
            return false;
        }
    }

    void DeviceState::jsonToAny(const nlohmann::json &json, std::any &value, std::type_index &type)
    {
        if (json.is_null())
        {
            value = std::any();
            type = std::type_index(typeid(void));
        }
        else if (json.is_boolean())
        {
            value = json.get<bool>();
            type = std::type_index(typeid(bool));
        }
        else if (json.is_number_integer())
        {
            value = json.get<int64_t>();
            type = std::type_index(typeid(int64_t));
        }
        else if (json.is_number_float())
        {
            value = json.get<double>();
            type = std::type_index(typeid(double));
        }
        else if (json.is_string())
        {
            value = json.get<std::string>();
            type = std::type_index(typeid(std::string));
        }
        else if (json.is_array())
        {
            // Handle arrays as needed
            value = std::any();
            type = std::type_index(typeid(void));
        }
        else if (json.is_object())
        {
            // Handle objects as needed
            value = std::any();
            type = std::type_index(typeid(void));
        }
    }

    nlohmann::json DeviceState::anyToJson(const std::any &value, const std::type_index &type) const
    {
        if (type == std::type_index(typeid(bool)))
        {
            return std::any_cast<bool>(value);
        }
        else if (type == std::type_index(typeid(int)) || type == std::type_index(typeid(int32_t)))
        {
            return std::any_cast<int>(value);
        }
        else if (type == std::type_index(typeid(int64_t)))
        {
            return std::any_cast<int64_t>(value);
        }
        else if (type == std::type_index(typeid(float)))
        {
            return std::any_cast<float>(value);
        }
        else if (type == std::type_index(typeid(double)))
        {
            return std::any_cast<double>(value);
        }
        else if (type == std::type_index(typeid(std::string)))
        {
            return std::any_cast<std::string>(value);
        }

        // Default case
        return nullptr;
    }

    bool DeviceState::setSampleRate(double sampleRate)
    {
        if (sampleRate >= m_minSampleRate && sampleRate <= m_maxSampleRate)
        {
            m_sampleRate = sampleRate;
            return true;
        }
        return false;
    }

    bool DeviceState::setBufferSize(int bufferSize)
    {
        if (bufferSize >= m_minBufferSize && bufferSize <= m_maxBufferSize)
        {
            m_bufferSize = bufferSize;
            return true;
        }
        return false;
    }

    void DeviceState::setProperty(const std::string &name, const std::string &value)
    {
        m_properties[name] = value;
    }

    std::string DeviceState::getProperty(const std::string &name, const std::string &defaultValue) const
    {
        auto it = m_properties.find(name);
        if (it != m_properties.end())
        {
            return it->second;
        }
        return defaultValue;
    }

} // namespace AudioEngine
