#include "RmeOscController.h"

#include <iostream>
#include <cmath>

// Include liblo for OSC communication
extern "C"
{
#include "lo/lo.h"
}

namespace AudioEngine
{

	RmeOscController::RmeOscController()
		: m_oscAddress(nullptr)
	{
		// Constructor is minimal, initialization happens in configure()
	}

	RmeOscController::~RmeOscController()
	{
		// Clean up liblo resources
		if (m_oscAddress)
		{
			lo_address_free(m_oscAddress);
			m_oscAddress = nullptr;
		}
	}

	bool RmeOscController::configure(const std::string &ip, int port)
	{
		m_rmeIp = ip;
		m_rmePort = port;

		// Create OSC address
		m_oscAddress = lo_address_new(ip.c_str(), std::to_string(port).c_str());
		if (!m_oscAddress)
		{
			std::cerr << "RmeOscController: Failed to create OSC address for " << ip << ":" << port << std::endl;
			return false;
		}

		m_configured = true;
		std::cout << "RME OSC Controller configured for " << ip << ":" << port << std::endl;
		return true;
	}

	bool RmeOscController::sendCommand(const std::string &address, const std::vector<std::any> &args)
	{
		if (!m_configured || !m_oscAddress)
		{
			std::cerr << "RmeOscController: Not configured or no OSC address" << std::endl;
			return false;
		}

		std::cout << "OSC SEND -> " << m_rmeIp << ":" << m_rmePort << " " << address; // Debug

		// Create OSC message
		lo_message msg = lo_message_new();
		if (!msg)
		{
			std::cerr << "RmeOscController: Failed to create OSC message" << std::endl;
			return false;
		}

		// Add arguments to message
		for (const auto &arg : args)
		{
			if (arg.type() == typeid(float))
			{
				lo_message_add_float(msg, std::any_cast<float>(arg));
				std::cout << " float:" << std::any_cast<float>(arg);
			}
			else if (arg.type() == typeid(int))
			{
				lo_message_add_int32(msg, std::any_cast<int>(arg));
				std::cout << " int:" << std::any_cast<int>(arg);
			}
			else if (arg.type() == typeid(bool))
			{
				// Booleans are sent as integers (0/1) in OSC
				lo_message_add_int32(msg, std::any_cast<bool>(arg) ? 1 : 0);
				std::cout << " bool:" << (std::any_cast<bool>(arg) ? "true" : "false");
			}
			else if (arg.type() == typeid(std::string))
			{
				lo_message_add_string(msg, std::any_cast<std::string>(arg).c_str());
				std::cout << " string:" << std::any_cast<std::string>(arg);
			}
			else
			{
				std::cerr << "RmeOscController: Unsupported argument type" << std::endl;
				lo_message_free(msg);
				return false;
			}
		}

		// Send the message
		int result = lo_send_message(m_oscAddress, address.c_str(), msg);
		lo_message_free(msg);

		if (result == -1)
		{
			std::cerr << "RmeOscController: Failed to send OSC message: "
					  << lo_address_errstr(m_oscAddress) << std::endl;
			return false;
		}

		std::cout << " (Sent)" << std::endl;
		return true;
	}

	bool RmeOscController::setMatrixCrosspointGain(int hw_input, int hw_output, float gain_db)
	{
		// IMPORTANT: The exact address format depends on the RME OSC implementation
		// Consult the RME TotalMix FX OSC documentation for the correct format

		// Example address: /1/matrix/{input}/{output}/gain
		std::string address = "/1/matrix/" + std::to_string(hw_input) + "/" + std::to_string(hw_output) + "/gain";

		// Convert dB to linear gain (0-1 range for RME)
		// This conversion may need adjustment based on RME's expected range
		float gain_linear = std::pow(10.0f, gain_db / 20.0f);
		gain_linear = std::max(0.0f, std::min(1.0f, gain_linear)); // Clamp to 0-1

		return sendCommand(address, {std::any(gain_linear)});
	}

	bool RmeOscController::setChannelMute(int channel, bool mute)
	{
		// Example address: /1/channel/{ch}/mute
		std::string address = "/1/channel/" + std::to_string(channel) + "/mute";

		// Boolean values are sent as integers in OSC
		int muteVal = mute ? 1 : 0;

		return sendCommand(address, {std::any(muteVal)});
	}

	bool RmeOscController::setChannelVolume(int channel, float volume_db)
	{
		// Example address: /1/channel/{ch}/volume
		std::string address = "/1/channel/" + std::to_string(channel) + "/volume";

		// Convert dB to normalized range (0-1)
		// This conversion may need adjustment based on RME's expected range
		// Assuming RME expects 0-1 range where 0 = -inf dB, 1 = 0dB
		float volume_norm;
		if (volume_db <= -96.0f)
		{
			volume_norm = 0.0f; // Minimum value (-inf dB)
		}
		else
		{
			// Map from typical dB range to 0-1
			// This is a simplified mapping that may need adjustment
			volume_norm = (volume_db + 96.0f) / 96.0f;
			volume_norm = std::max(0.0f, std::min(1.0f, volume_norm)); // Clamp to 0-1
		}

		return sendCommand(address, {std::any(volume_norm)});
	}

} // namespace AudioEngine
