#pragma once

#include "OscMessageDispatcher.h"
#include "OscController.h"
#include <string>
#include <regex>
#include <map>
#include <mutex>

namespace AudioEngine
{
    /**
     * @brief Handler for RME-specific OSC messages
     *
     * This class handles OSC messages specific to RME devices like the Fireface series,
     * implementing the TotalMix FX OSC protocol.
     */
    class RmeOscMessageHandler : public OscMessageHandler
    {
    public:
        /**
         * @brief Value changed callback type
         */
        using ValueChangedCallback = std::function<void(const std::string &, float)>;

        /**
         * @brief Construct a new RME OSC message handler
         *
         * @param controller OSC controller for sending responses
         */
        RmeOscMessageHandler(OscController *controller);

        /**
         * @brief Destructor
         */
        ~RmeOscMessageHandler() override = default;

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
         * @brief Register a callback for a specific OSC address
         *
         * @param addressPattern Regex pattern for the OSC address
         * @param callback Function to call when a matching message is received
         */
        void registerAddressCallback(const std::string &addressPattern, ValueChangedCallback callback);

        /**
         * @brief Send a volume change command to a specific channel
         *
         * @param channelNumber Channel number (1-based)
         * @param value Volume value (0.0 to 1.0)
         * @return true if the command was sent successfully
         */
        bool setChannelVolume(int channelNumber, float value);

        /**
         * @brief Send a mute command to a specific channel
         *
         * @param channelNumber Channel number (1-based)
         * @param muted Whether to mute (true) or unmute (false)
         * @return true if the command was sent successfully
         */
        bool setChannelMute(int channelNumber, bool muted);

        /**
         * @brief Send a pan command to a specific channel
         *
         * @param channelNumber Channel number (1-based)
         * @param value Pan value (-1.0 to 1.0, where 0.0 is center)
         * @return true if the command was sent successfully
         */
        bool setChannelPan(int channelNumber, float value);

        /**
         * @brief Send an EQ parameter command
         *
         * @param channelNumber Channel number (1-based)
         * @param band EQ band (1-4)
         * @param paramType Parameter type (gain, freq, q)
         * @param value Parameter value
         * @return true if the command was sent successfully
         */
        bool setEqParameter(int channelNumber, int band, const std::string &paramType, float value);

        /**
         * @brief Send a routing matrix command
         *
         * @param sourceChannel Source channel number (1-based)
         * @param destinationChannel Destination channel number (1-based)
         * @param enabled Whether to enable or disable the route
         * @return true if the command was sent successfully
         */
        bool setRoutingMatrix(int sourceChannel, int destinationChannel, bool enabled);

    private:
        /**
         * @brief Address callback entry
         */
        struct AddressCallback
        {
            std::regex pattern;
            ValueChangedCallback callback;
        };

        OscController *m_controller;
        std::vector<AddressCallback> m_addressCallbacks;
        mutable std::mutex m_callbackMutex;
    };

} // namespace AudioEngine
