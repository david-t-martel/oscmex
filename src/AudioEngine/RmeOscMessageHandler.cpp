#include "RmeOscMessageHandler.h"
#include <iostream>

namespace AudioEngine
{
    RmeOscMessageHandler::RmeOscMessageHandler(OscController *controller)
        : m_controller(controller)
    {
    }

    bool RmeOscMessageHandler::handleMessage(const std::string &address, const std::vector<std::any> &args)
    {
        // Check if we have a value in the args
        if (args.empty())
        {
            // This might be a query message, not a value update
            return false;
        }

        // Try to extract a value from the first argument
        float value = 0.0f;
        try
        {
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
            else if (args[0].type() == typeid(bool))
            {
                value = std::any_cast<bool>(args[0]) ? 1.0f : 0.0f;
            }
            else
            {
                // Not a numeric value, we can't handle it
                return false;
            }
        }
        catch (const std::bad_any_cast &e)
        {
            std::cerr << "RmeOscMessageHandler: Error extracting value from message: "
                      << e.what() << std::endl;
            return false;
        }

        // Check if we have any callbacks registered for this address
        bool handled = false;

        {
            std::lock_guard<std::mutex> lock(m_callbackMutex);

            for (const auto &callback : m_addressCallbacks)
            {
                if (std::regex_match(address, callback.pattern))
                {
                    callback.callback(address, value);
                    handled = true;
                    // Don't break, multiple callbacks might be registered for the same pattern
                }
            }
        }

        return handled;
    }

    bool RmeOscMessageHandler::canHandle(const std::string &address) const
    {
        // Basic check: all RME TotalMix OSC messages start with /1/ or /2/
        return address.size() >= 3 &&
               (address.compare(0, 3, "/1/") == 0 ||
                address.compare(0, 3, "/2/") == 0);
    }

    void RmeOscMessageHandler::registerAddressCallback(const std::string &addressPattern, ValueChangedCallback callback)
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);

        try
        {
            AddressCallback callbackEntry;
            callbackEntry.pattern = std::regex(addressPattern);
            callbackEntry.callback = callback;

            m_addressCallbacks.push_back(callbackEntry);

            std::cout << "RmeOscMessageHandler: Registered callback for pattern: "
                      << addressPattern << std::endl;
        }
        catch (const std::regex_error &e)
        {
            std::cerr << "RmeOscMessageHandler: Invalid regex pattern: "
                      << addressPattern << " - " << e.what() << std::endl;
        }
    }

    bool RmeOscMessageHandler::setChannelVolume(int channelNumber, float value)
    {
        if (!m_controller)
        {
            return false;
        }

        // Validate input
        if (channelNumber < 1)
        {
            std::cerr << "RmeOscMessageHandler: Invalid channel number: " << channelNumber << std::endl;
            return false;
        }

        // Clamp value to [0.0, 1.0]
        value = std::max(0.0f, std::min(1.0f, value));

        // Format OSC address - follows TotalMix FX OSC protocol
        std::string address = "/1/volume/" + std::to_string(channelNumber);

        // Send command
        return m_controller->sendCommand(address, {value});
    }

    bool RmeOscMessageHandler::setChannelMute(int channelNumber, bool muted)
    {
        if (!m_controller)
        {
            return false;
        }

        // Validate input
        if (channelNumber < 1)
        {
            std::cerr << "RmeOscMessageHandler: Invalid channel number: " << channelNumber << std::endl;
            return false;
        }

        // Format OSC address - follows TotalMix FX OSC protocol
        std::string address = "/1/mute/" + std::to_string(channelNumber);

        // Send command
        return m_controller->sendCommand(address, {muted ? 1.0f : 0.0f});
    }

    bool RmeOscMessageHandler::setChannelPan(int channelNumber, float value)
    {
        if (!m_controller)
        {
            return false;
        }

        // Validate input
        if (channelNumber < 1)
        {
            std::cerr << "RmeOscMessageHandler: Invalid channel number: " << channelNumber << std::endl;
            return false;
        }

        // Clamp value to [-1.0, 1.0]
        value = std::max(-1.0f, std::min(1.0f, value));

        // Convert from [-1.0, 1.0] to [0.0, 1.0] for RME (0.5 is center)
        float rmeValue = (value + 1.0f) * 0.5f;

        // Format OSC address - follows TotalMix FX OSC protocol
        std::string address = "/1/pan/" + std::to_string(channelNumber);

        // Send command
        return m_controller->sendCommand(address, {rmeValue});
    }

    bool RmeOscMessageHandler::setEqParameter(int channelNumber, int band, const std::string &paramType, float value)
    {
        if (!m_controller)
        {
            return false;
        }

        // Validate input
        if (channelNumber < 1)
        {
            std::cerr << "RmeOscMessageHandler: Invalid channel number: " << channelNumber << std::endl;
            return false;
        }

        if (band < 1 || band > 4)
        {
            std::cerr << "RmeOscMessageHandler: Invalid EQ band: " << band << std::endl;
            return false;
        }

        // Validate param type
        if (paramType != "gain" && paramType != "freq" && paramType != "q")
        {
            std::cerr << "RmeOscMessageHandler: Invalid EQ parameter type: " << paramType << std::endl;
            return false;
        }

        // Format OSC address - follows TotalMix FX OSC protocol
        std::string address = "/1/eq/" + std::to_string(channelNumber) + "/" +
                              std::to_string(band) + "/" + paramType;

        // Apply value clamping depending on parameter type
        float clampedValue = value;

        if (paramType == "gain")
        {
            // Gain typically between -20 and +20 dB, normalized to [0.0, 1.0]
            clampedValue = std::max(0.0f, std::min(1.0f, value));
        }
        else if (paramType == "freq")
        {
            // Frequency normalized to [0.0, 1.0]
            clampedValue = std::max(0.0f, std::min(1.0f, value));
        }
        else if (paramType == "q")
        {
            // Q factor normalized to [0.0, 1.0]
            clampedValue = std::max(0.0f, std::min(1.0f, value));
        }

        // Send command
        return m_controller->sendCommand(address, {clampedValue});
    }

    bool RmeOscMessageHandler::setRoutingMatrix(int sourceChannel, int destinationChannel, bool enabled)
    {
        if (!m_controller)
        {
            return false;
        }

        // Validate input
        if (sourceChannel < 1 || destinationChannel < 1)
        {
            std::cerr << "RmeOscMessageHandler: Invalid channel numbers: source="
                      << sourceChannel << ", destination=" << destinationChannel << std::endl;
            return false;
        }

        // Format OSC address - follows TotalMix FX OSC protocol
        std::string address = "/1/matrix/" + std::to_string(sourceChannel) +
                              "/" + std::to_string(destinationChannel);

        // Send command
        return m_controller->sendCommand(address, {enabled ? 1.0f : 0.0f});
    }

} // namespace AudioEngine
