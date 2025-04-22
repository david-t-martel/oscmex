#include "RmeOscCommands.h"
#include <iostream>
#include <sstream>
#include <cmath>

namespace AudioEngine
{
    RmeOscCommands::RmeOscCommands(OscController *controller)
        : m_controller(controller)
    {
        if (!controller)
        {
            throw std::invalid_argument("OscController cannot be null");
        }
    }

    bool RmeOscCommands::setChannelVolume(int channelType, int channel, float dB)
    {
        if (!m_controller || channel < 1)
        {
            return false;
        }

        std::string typeStr;
        switch (channelType)
        {
        case 0:
            typeStr = "input";
            break;
        case 1:
            typeStr = "playback";
            break;
        case 2:
            typeStr = "output";
            break;
        default:
            return false;
        }

        std::string address = "/" + std::to_string(channel) + "/" + typeStr + "/volume";
        float normalized = dbToNormalized(dB);

        return m_controller->setParameter(address, {normalized});
    }

    bool RmeOscCommands::setChannelMute(int channelType, int channel, bool mute)
    {
        if (!m_controller || channel < 1)
        {
            return false;
        }

        std::string typeStr;
        switch (channelType)
        {
        case 0:
            typeStr = "input";
            break;
        case 1:
            typeStr = "playback";
            break;
        case 2:
            typeStr = "output";
            break;
        default:
            return false;
        }

        std::string address = "/" + std::to_string(channel) + "/" + typeStr + "/mute";

        return m_controller->setParameter(address, {mute ? 1.0f : 0.0f});
    }

    bool RmeOscCommands::setChannelSolo(int channelType, int channel, bool solo)
    {
        if (!m_controller || channel < 1)
        {
            return false;
        }

        // Solo is not available for output channels in TotalMix FX
        if (channelType == 2)
        {
            return false;
        }

        std::string typeStr;
        switch (channelType)
        {
        case 0:
            typeStr = "input";
            break;
        case 1:
            typeStr = "playback";
            break;
        default:
            return false;
        }

        std::string address = "/" + std::to_string(channel) + "/" + typeStr + "/solo";

        return m_controller->setParameter(address, {solo ? 1.0f : 0.0f});
    }

    bool RmeOscCommands::setChannelPan(int channelType, int channel, float pan)
    {
        if (!m_controller || channel < 1)
        {
            return false;
        }

        std::string typeStr;
        switch (channelType)
        {
        case 0:
            typeStr = "input";
            break;
        case 1:
            typeStr = "playback";
            break;
        case 2:
            typeStr = "output";
            break;
        default:
            return false;
        }

        // Clamp pan to [-1, 1]
        pan = std::min(1.0f, std::max(-1.0f, pan));

        // Convert to RME's 0-1 range (0.5 is center)
        float normalized = (pan + 1.0f) * 0.5f;

        std::string address = "/" + std::to_string(channel) + "/" + typeStr + "/pan";

        return m_controller->setParameter(address, {normalized});
    }

    bool RmeOscCommands::setPhantomPower(int channel, bool enabled)
    {
        if (!m_controller || channel < 1)
        {
            return false;
        }

        std::string address = "/" + std::to_string(channel) + "/input/phantom";

        return m_controller->setParameter(address, {enabled ? 1.0f : 0.0f});
    }

    bool RmeOscCommands::setMatrixGain(int sourceChannel, int destChannel, float dB)
    {
        if (!m_controller || sourceChannel < 1 || destChannel < 1)
        {
            return false;
        }

        // RME matrix routing uses different formats depending on matrix size
        // For typical TotalMix FX, we use the volA format
        std::string address = "/matrix/volA/" + std::to_string(sourceChannel) + "/" + std::to_string(destChannel);
        float normalized = dbToNormalized(dB);

        return m_controller->setParameter(address, {normalized});
    }

    bool RmeOscCommands::setMainVolume(float dB)
    {
        if (!m_controller)
        {
            return false;
        }

        std::string address = "/main/volume";
        float normalized = dbToNormalized(dB);

        return m_controller->setParameter(address, {normalized});
    }

    bool RmeOscCommands::setMainMute(bool mute)
    {
        if (!m_controller)
        {
            return false;
        }

        std::string address = "/main/mute";

        return m_controller->setParameter(address, {mute ? 1.0f : 0.0f});
    }

    bool RmeOscCommands::setDim(bool enabled)
    {
        if (!m_controller)
        {
            return false;
        }

        std::string address = "/main/dim";

        return m_controller->setParameter(address, {enabled ? 1.0f : 0.0f});
    }

    bool RmeOscCommands::setMono(bool enabled)
    {
        if (!m_controller)
        {
            return false;
        }

        std::string address = "/main/mono";

        return m_controller->setParameter(address, {enabled ? 1.0f : 0.0f});
    }

    bool RmeOscCommands::setEqBand(int channelType, int channel, int band, float gain, float freq, float q)
    {
        if (!m_controller || channel < 1 || band < 1 || band > 4)
        {
            return false;
        }

        // EQ is not available for playback channels in TotalMix FX
        if (channelType == 1)
        {
            return false;
        }

        std::string typeStr;
        switch (channelType)
        {
        case 0:
            typeStr = "input";
            break;
        case 2:
            typeStr = "output";
            break;
        default:
            return false;
        }

        // Base address for this channel's EQ
        std::string baseAddress = "/" + std::to_string(channel) + "/" + typeStr + "/eq/" + std::to_string(band);

        // Set gain (normalized from -20dB to +20dB to 0.0-1.0 range)
        float normalizedGain = (gain + 20.0f) / 40.0f;
        normalizedGain = std::min(1.0f, std::max(0.0f, normalizedGain));
        bool gainResult = m_controller->setParameter(baseAddress + "/gain", {normalizedGain});

        // Set frequency (depends on band, requires device-specific calibration)
        // Here we map roughly based on RME's typical EQ frequency ranges
        float normalizedFreq;
        switch (band)
        {
        case 1: // Low band (usually 20Hz - 500Hz)
            normalizedFreq = (freq - 20.0f) / 480.0f;
            break;
        case 2: // Low-mid band (usually 100Hz - 2kHz)
            normalizedFreq = (freq - 100.0f) / 1900.0f;
            break;
        case 3: // High-mid band (usually 500Hz - 8kHz)
            normalizedFreq = (freq - 500.0f) / 7500.0f;
            break;
        case 4: // High band (usually 2kHz - 20kHz)
            normalizedFreq = (freq - 2000.0f) / 18000.0f;
            break;
        default:
            normalizedFreq = 0.5f;
        }
        normalizedFreq = std::min(1.0f, std::max(0.0f, normalizedFreq));
        bool freqResult = m_controller->setParameter(baseAddress + "/freq", {normalizedFreq});

        // Set Q (normalized from 0.5 to 8.0 to 0.0-1.0 range)
        float normalizedQ = (q - 0.5f) / 7.5f;
        normalizedQ = std::min(1.0f, std::max(0.0f, normalizedQ));
        bool qResult = m_controller->setParameter(baseAddress + "/q", {normalizedQ});

        // Return true only if all parameters were set successfully
        return gainResult && freqResult && qResult;
    }

    bool RmeOscCommands::setEqEnabled(int channelType, int channel, bool enabled)
    {
        if (!m_controller || channel < 1)
        {
            return false;
        }

        // EQ is not available for playback channels in TotalMix FX
        if (channelType == 1)
        {
            return false;
        }

        std::string typeStr;
        switch (channelType)
        {
        case 0:
            typeStr = "input";
            break;
        case 2:
            typeStr = "output";
            break;
        default:
            return false;
        }

        std::string address = "/" + std::to_string(channel) + "/" + typeStr + "/eq/on";

        return m_controller->setParameter(address, {enabled ? 1.0f : 0.0f});
    }

    float RmeOscCommands::dbToNormalized(float dB)
    {
        // RME's volume scale is approximately -65dB to 0dB mapped to 0.0-1.0
        // Clamp dB to valid range
        dB = std::min(0.0f, std::max(-65.0f, dB));

        // Convert to normalized value
        return (dB + 65.0f) / 65.0f;
    }

    float RmeOscCommands::normalizedToDB(float normalized)
    {
        // Clamp normalized value to 0.0-1.0
        normalized = std::min(1.0f, std::max(0.0f, normalized));

        // Convert to dB value
        return (normalized * 65.0f) - 65.0f;
    }

    bool RmeOscCommands::requestRefresh()
    {
        if (!m_controller)
        {
            return false;
        }

        // Send refresh command to TotalMix
        return m_controller->setParameter("/refresh", {});
    }

    bool RmeOscCommands::recallPreset(int presetNumber)
    {
        if (!m_controller || presetNumber < 1 || presetNumber > 8)
        {
            return false;
        }

        // TotalMix FX has 8 preset slots, referenced by /preset/1 through /preset/8
        std::string address = "/preset/" + std::to_string(presetNumber);

        return m_controller->setParameter(address, {1.0f});
    }

    std::string RmeOscCommands::channelAddressPrefix(int channelType, int channel)
    {
        std::string typeStr;
        switch (channelType)
        {
        case 0:
            typeStr = "input";
            break;
        case 1:
            typeStr = "playback";
            break;
        case 2:
            typeStr = "output";
            break;
        default:
            return "";
        }

        return "/" + std::to_string(channel) + "/" + typeStr;
    }

} // namespace AudioEngine
