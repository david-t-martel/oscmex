#include "DeviceState.h"
#include <algorithm>

namespace AudioEngine
{

    DeviceState::DeviceState(const std::string &deviceId, const std::string &deviceName)
        : m_deviceId(deviceId), m_deviceName(deviceName), m_active(false), m_sampleRate(44100), m_bufferSize(512), m_inputChannelCount(0), m_outputChannelCount(0), m_hasError(false), m_nextObserverId(1)
    {
    }

    DeviceState::DeviceState(const std::string &deviceId)
        : m_deviceId(deviceId), m_deviceName(""), m_active(false), m_sampleRate(44100), m_bufferSize(512), m_inputChannelCount(0), m_outputChannelCount(0), m_hasError(false), m_nextObserverId(1)
    {
    }

    DeviceState::~DeviceState() = default;

    void DeviceState::setProperty(const std::string &key, const std::string &value)
    {
        std::lock_guard<std::mutex> lock(m_propertiesMutex);
        m_properties[key] = value;
    }

    std::string DeviceState::getProperty(const std::string &key, const std::string &defaultValue) const
    {
        std::lock_guard<std::mutex> lock(m_propertiesMutex);
        auto it = m_properties.find(key);
        if (it != m_properties.end())
        {
            return it->second;
        }
        return defaultValue;
    }

    void DeviceState::setError(bool hasError, const std::string &errorMessage)
    {
        m_hasError = hasError;
        if (hasError)
        {
            m_errorMessage = errorMessage;
        }
        else
        {
            m_errorMessage.clear();
        }
    }

    std::string DeviceState::getDeviceId() const
    {
        return m_deviceId;
    }

    void DeviceState::setDeviceId(const std::string &deviceId)
    {
        m_deviceId = deviceId;
    }

    bool DeviceState::hasParameter(const std::string &path) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_parameters.find(path) != m_parameters.end();
    }

    bool DeviceState::removeParameter(const std::string &path)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_parameters.find(path);
        if (it != m_parameters.end())
        {
            m_parameters.erase(it);
            return true;
        }
        return false;
    }

    std::vector<std::string> DeviceState::getParameterPaths() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::string> paths;
        for (const auto &param : m_parameters)
        {
            paths.push_back(param.first);
        }
        return paths;
    }

    int DeviceState::addObserver(const std::string &path,
                                 std::function<void(const std::string &, const std::any &)> callback)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        int id = m_nextObserverId++;
        m_observers[id] = ObserverInfo{path, callback};
        return id;
    }

    bool DeviceState::removeObserver(int observerId)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_observers.find(observerId);
        if (it != m_observers.end())
        {
            m_observers.erase(it);
            return true;
        }
        return false;
    }

} // namespace AudioEngine
