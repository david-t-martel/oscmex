#pragma once

#include "OscController.h"
#include <string>
#include <vector>
#include <map>
#include <any>
#include <functional>
#include <memory>

namespace AudioEngine
{
    /**
     * @brief RME-specific OSC commands for TotalMix FX control
     *
     * This class provides a specialized interface for controlling RME audio interfaces
     * through their TotalMix FX software using OSC protocol.
     */
    class RmeOscCommands
    {
    public:
        /**
         * @brief Construct a new Rme Osc Commands object
         *
         * @param controller Pointer to an OscController instance
         */
        RmeOscCommands(OscController *controller);

        /**
         * @brief Destroy the Rme Osc Commands object
         */
        ~RmeOscCommands() = default;

        /**
         * @brief Set channel volume
         *
         * @param channelType Channel type (0=input, 1=playback, 2=output)
         * @param channel Channel number (1-based)
         * @param dB Volume in decibels (-65 to 0)
         * @return true if command was sent successfully
         */
        bool setChannelVolume(int channelType, int channel, float dB);

        /**
         * @brief Set channel mute state
         *
         * @param channelType Channel type (0=input, 1=playback, 2=output)
         * @param channel Channel number (1-based)
         * @param mute True to mute, false to unmute
         * @return true if command was sent successfully
         */
        bool setChannelMute(int channelType, int channel, bool mute);

        /**
         * @brief Set channel solo state
         *
         * @param channelType Channel type (0=input, 1=playback, 2=output)
         * @param channel Channel number (1-based)
         * @param solo True to solo, false to unsolo
         * @return true if command was sent successfully
         */
        bool setChannelSolo(int channelType, int channel, bool solo);

        /**
         * @brief Set channel pan position
         *
         * @param channelType Channel type (0=input, 1=playback, 2=output)
         * @param channel Channel number (1-based)
         * @param pan Pan position (-1.0 = left, 0 = center, 1.0 = right)
         * @return true if command was sent successfully
         */
        bool setChannelPan(int channelType, int channel, float pan);

        /**
         * @brief Set phantom power for an input channel
         *
         * @param channel Input channel number (1-based)
         * @param enabled True to enable phantom power, false to disable
         * @return true if command was sent successfully
         */
        bool setPhantomPower(int channel, bool enabled);

        /**
         * @brief Set matrix routing gain
         *
         * @param sourceChannel Source channel number (1-based)
         * @param destChannel Destination channel number (1-based)
         * @param dB Gain in decibels (-65 to 0)
         * @return true if command was sent successfully
         */
        bool setMatrixGain(int sourceChannel, int destChannel, float dB);

        /**
         * @brief Set global main volume
         *
         * @param dB Volume in decibels (-65 to 0)
         * @return true if command was sent successfully
         */
        bool setMainVolume(float dB);

        /**
         * @brief Set global mute state
         *
         * @param mute True to mute, false to unmute
         * @return true if command was sent successfully
         */
        bool setMainMute(bool mute);

        /**
         * @brief Set global dim state
         *
         * @param enabled True to enable dim, false to disable
         * @return true if command was sent successfully
         */
        bool setDim(bool enabled);

        /**
         * @brief Set global mono state
         *
         * @param enabled True to enable mono, false to disable
         * @return true if command was sent successfully
         */
        bool setMono(bool enabled);

        /**
         * @brief Set EQ band parameters
         *
         * @param channelType Channel type (0=input, 2=output)
         * @param channel Channel number (1-based)
         * @param band EQ band (1-4)
         * @param gain Gain in dB (-20 to +20)
         * @param freq Frequency in Hz
         * @param q Q factor
         * @return true if commands were sent successfully
         */
        bool setEqBand(int channelType, int channel, int band, float gain, float freq, float q);

        /**
         * @brief Enable or disable EQ for a channel
         *
         * @param channelType Channel type (0=input, 2=output)
         * @param channel Channel number (1-based)
         * @param enabled True to enable EQ, false to disable
         * @return true if command was sent successfully
         */
        bool setEqEnabled(int channelType, int channel, bool enabled);

        /**
         * @brief Convert decibels to normalized value (0.0-1.0) for RME's scale
         *
         * @param dB Decibel value (-65 to 0)
         * @return float Normalized value (0.0-1.0)
         */
        static float dbToNormalized(float dB);

        /**
         * @brief Convert normalized value (0.0-1.0) to decibels
         *
         * @param normalized Normalized value (0.0-1.0)
         * @return float Decibel value (-65 to 0)
         */
        static float normalizedToDB(float normalized);

        /**
         * @brief Request device refresh
         *
         * @return true if command was sent successfully
         */
        bool requestRefresh();

        /**
         * @brief Recall a TotalMix preset
         *
         * @param presetNumber Preset number (1-based)
         * @return true if command was sent successfully
         */
        bool recallPreset(int presetNumber);

        /**
         * @brief Convert channel type and number to the appropriate string prefix
         *
         * @param channelType Channel type (0=input, 1=playback, 2=output)
         * @param channel Channel number (1-based)
         * @return std::string Address prefix for the specified channel
         */
        static std::string channelAddressPrefix(int channelType, int channel);

    private:
        OscController *m_controller;
    };

} // namespace AudioEngine
