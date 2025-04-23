#pragma once

#include "OscMessageDispatcher.h"
#include <map>
#include <string>
#include <atomic>
#include <functional>

namespace AudioEngine
{
    /**
     * @brief Handler for parameter control OSC messages
     *
     * This class handles OSC messages related to parameter control, such as:
     * - /parameter/set/<name> - Set a parameter value
     * - /parameter/get/<name> - Get a parameter value (responds with /parameter/value/<name>)
     */
    class ParameterControlHandler : public OscMessageHandler
    {
    public:
        /**
         * @brief Parameter value changed callback type
         */
        using ParameterChangedCallback = std::function<void(const std::string &, float)>;

        /**
         * @brief Construct a new parameter control handler
         *
         * @param controller OSC controller for sending responses
         */
        ParameterControlHandler(OscController *controller);

        /**
         * @brief Destructor
         */
        ~ParameterControlHandler() override = default;

        /**
         * @brief Handle an OSC message
         *
         * @param address OSC address of the message
         * @param args Message arguments
         * @return true if the message was handled
         */
        bool handleMessage(const std::string &address, const std::vector<std::any> &args) override;

        /**
         * @brief Check if this handler can process the given OSC address
         *
         * @param address OSC address to check
         * @return true if this handler can process the address
         */
        bool canHandle(const std::string &address) const override;

        /**
         * @brief Register a parameter
         *
         * @param name Parameter name
         * @param defaultValue Default value
         * @param minValue Minimum value
         * @param maxValue Maximum value
         * @param callback Function to call when the parameter value changes
         */
        void registerParameter(
            const std::string &name,
            float defaultValue,
            float minValue,
            float maxValue,
            ParameterChangedCallback callback);

        /**
         * @brief Set a parameter value
         *
         * @param name Parameter name
         * @param value New value
         * @param notifyCallback Whether to call the parameter's callback
         * @param sendOsc Whether to send an OSC message with the new value
         * @return true if the parameter exists and the value was set
         */
        bool setParameterValue(
            const std::string &name,
            float value,
            bool notifyCallback = true,
            bool sendOsc = false);

        /**
         * @brief Get a parameter value
         *
         * @param name Parameter name
         * @param value Output parameter for the value
         * @return true if the parameter exists and the value was retrieved
         */
        bool getParameterValue(const std::string &name, float &value) const;

        /**
         * @brief Send parameter values as OSC messages
         *
         * @param paramNamePattern Regex pattern for parameter names to send (empty for all)
         */
        void sendParameterValues(const std::string &paramNamePattern = "");

    private:
        /**
         * @brief Parameter descriptor
         */
        struct Parameter
        {
            std::string name;
            std::atomic<float> value;
            float minValue;
            float maxValue;
            ParameterChangedCallback callback;
        };

        // Convert parameter value to normalized range [0,1]
        float normalizeValue(const Parameter &param, float value) const;

        // Convert normalized value [0,1] to parameter range
        float denormalizeValue(const Parameter &param, float normalizedValue) const;

        OscController *m_controller;
        std::map<std::string, Parameter> m_parameters;
        mutable std::mutex m_paramMutex;
    };

} // namespace AudioEngine
