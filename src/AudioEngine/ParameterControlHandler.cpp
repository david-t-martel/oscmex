#include "ParameterControlHandler.h"
#include <iostream>
#include <regex>

namespace AudioEngine
{
    ParameterControlHandler::ParameterControlHandler(OscController *controller)
        : m_controller(controller)
    {
    }

    bool ParameterControlHandler::handleMessage(const std::string &address, const std::vector<std::any> &args)
    {
        // Parse the address and extract command and parameter name
        std::regex setPattern("/parameter/set/(.+)");
        std::regex getPattern("/parameter/get/(.+)");
        std::smatch matches;

        // Handle /parameter/set/<name>
        if (std::regex_match(address, matches, setPattern) && matches.size() > 1)
        {
            std::string paramName = matches[1].str();

            if (args.size() < 1)
            {
                std::cerr << "ParameterControlHandler: Missing value for parameter set: " << paramName << std::endl;
                return true; // Handled (with error)
            }

            try
            {
                float value = 0.0f;

                // Try to extract value from the first argument
                if (args[0].type() == typeid(float))
                {
                    value = std::any_cast<float>(args[0]);
                }
                else if (args[0].type() == typeid(int))
                {
                    value = static_cast<float>(std::any_cast<int>(args[0]));
                }
                else if (args[0].type() == typeid(double))
                {
                    value = static_cast<float>(std::any_cast<double>(args[0]));
                }
                else
                {
                    std::cerr << "ParameterControlHandler: Unsupported argument type for parameter set: "
                              << paramName << std::endl;
                    return true; // Handled (with error)
                }

                // Set the parameter value
                bool success = setParameterValue(paramName, value, true, false);

                if (!success)
                {
                    std::cerr << "ParameterControlHandler: Unknown parameter: " << paramName << std::endl;
                }

                // Send a response with the current value
                if (m_controller && success)
                {
                    m_controller->sendCommand("/parameter/value/" + paramName, {value});
                }

                return true;
            }
            catch (const std::bad_any_cast &e)
            {
                std::cerr << "ParameterControlHandler: Error parsing parameter value: "
                          << e.what() << std::endl;
                return true; // Handled (with error)
            }
        }

        // Handle /parameter/get/<name>
        if (std::regex_match(address, matches, getPattern) && matches.size() > 1)
        {
            std::string paramName = matches[1].str();
            float value = 0.0f;

            // Get the parameter value
            bool success = getParameterValue(paramName, value);

            if (!success)
            {
                std::cerr << "ParameterControlHandler: Unknown parameter: " << paramName << std::endl;
            }

            // Send a response with the current value
            if (m_controller && success)
            {
                m_controller->sendCommand("/parameter/value/" + paramName, {value});
            }

            return true;
        }

        return false; // Not handled
    }

    bool ParameterControlHandler::canHandle(const std::string &address) const
    {
        return address.compare(0, 11, "/parameter/") == 0;
    }

    void ParameterControlHandler::registerParameter(
        const std::string &name,
        float defaultValue,
        float minValue,
        float maxValue,
        ParameterChangedCallback callback)
    {
        std::lock_guard<std::mutex> lock(m_paramMutex);

        // Create the parameter
        Parameter param;
        param.name = name;
        param.value = defaultValue;
        param.minValue = minValue;
        param.maxValue = maxValue;
        param.callback = callback;

        // Add to the map
        m_parameters[name] = param;

        std::cout << "ParameterControlHandler: Registered parameter: " << name
                  << " [" << minValue << " - " << maxValue << "], default = "
                  << defaultValue << std::endl;
    }

    bool ParameterControlHandler::setParameterValue(
        const std::string &name,
        float value,
        bool notifyCallback,
        bool sendOsc)
    {
        std::lock_guard<std::mutex> lock(m_paramMutex);

        // Find the parameter
        auto it = m_parameters.find(name);
        if (it == m_parameters.end())
        {
            return false;
        }

        // Clamp the value to the valid range
        Parameter &param = it->second;
        value = std::max(param.minValue, std::min(param.maxValue, value));

        // Update the value
        param.value = value;

        // Notify the callback
        if (notifyCallback && param.callback)
        {
            param.callback(name, value);
        }

        // Send an OSC message if requested
        if (sendOsc && m_controller)
        {
            m_controller->sendCommand("/parameter/value/" + name, {value});
        }

        return true;
    }

    bool ParameterControlHandler::getParameterValue(const std::string &name, float &value) const
    {
        std::lock_guard<std::mutex> lock(m_paramMutex);

        // Find the parameter
        auto it = m_parameters.find(name);
        if (it == m_parameters.end())
        {
            return false;
        }

        // Get the value
        value = it->second.value;

        return true;
    }

    void ParameterControlHandler::sendParameterValues(const std::string &paramNamePattern)
    {
        if (!m_controller)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(m_paramMutex);

        std::regex pattern;
        bool usePattern = !paramNamePattern.empty();

        if (usePattern)
        {
            try
            {
                pattern = std::regex(paramNamePattern);
            }
            catch (const std::regex_error &e)
            {
                std::cerr << "ParameterControlHandler: Invalid parameter name pattern: "
                          << e.what() << std::endl;
                return;
            }
        }

        // Send values for all matching parameters
        for (const auto &pair : m_parameters)
        {
            const std::string &name = pair.first;
            float value = pair.second.value;

            if (!usePattern || std::regex_match(name, pattern))
            {
                m_controller->sendCommand("/parameter/value/" + name, {value});
            }
        }
    }

    float ParameterControlHandler::normalizeValue(const Parameter &param, float value) const
    {
        // Convert value to range [0,1]
        if (param.maxValue == param.minValue)
        {
            return 0.0f;
        }

        return (value - param.minValue) / (param.maxValue - param.minValue);
    }

    float ParameterControlHandler::denormalizeValue(const Parameter &param, float normalizedValue) const
    {
        // Convert from [0,1] to parameter range
        return param.minValue + normalizedValue * (param.maxValue - param.minValue);
    }

} // namespace AudioEngine
