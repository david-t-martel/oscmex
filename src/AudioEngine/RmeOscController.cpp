#include "RmeOscController.h"

#include <iostream>
#include <cmath>
#include <thread>
#include <chrono>
#include <functional>

// Include liblo for OSC communication
extern "C"
{
#include "lo/lo.h"
}

namespace AudioEngine
{

	RmeOscController::RmeOscController()
		: m_oscAddress(nullptr), m_oscServer(nullptr), m_oscThread(nullptr)
	{
		// Constructor is minimal, initialization happens in configure()
	}

	RmeOscController::~RmeOscController()
	{
		// Clean up liblo resources
		cleanup();
	}

	void RmeOscController::cleanup()
	{
		stopReceiver();

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

		// Create OSC address for sending
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

	bool RmeOscController::startReceiver(int receivePort)
	{
		if (m_oscServer)
		{
			std::cerr << "RmeOscController: OSC receiver already started" << std::endl;
			return false;
		}

		// Create OSC server for receiving
		m_oscServer = lo_server_thread_new(std::to_string(receivePort).c_str(),
										   [](int num, const char *msg, const char *path)
										   {
											   std::cerr << "OSC error " << num << ": " << msg << " (" << path << ")" << std::endl;
											   return;
										   });

		if (!m_oscServer)
		{
			std::cerr << "RmeOscController: Failed to create OSC server on port " << receivePort << std::endl;
			return false;
		}

		// Set up generic message handler
		lo_server_thread_add_method(m_oscServer, nullptr, nullptr, [](const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data) -> int
									{
										RmeOscController *controller = static_cast<RmeOscController*>(user_data);
										return controller->handleOscMessage(path, types, argv, argc, msg); }, this);

		// Start the server thread
		if (lo_server_thread_start(m_oscServer) < 0)
		{
			std::cerr << "RmeOscController: Failed to start OSC server thread" << std::endl;
			lo_server_thread_free(m_oscServer);
			m_oscServer = nullptr;
			return false;
		}

		std::cout << "RME OSC Receiver started on port " << receivePort << std::endl;
		return true;
	}

	void RmeOscController::stopReceiver()
	{
		if (m_oscServer)
		{
			lo_server_thread_stop(m_oscServer);
			lo_server_thread_free(m_oscServer);
			m_oscServer = nullptr;
		}
	}

	int RmeOscController::handleOscMessage(const char *path, const char *types,
										   lo_arg **argv, int argc, lo_message msg)
	{
		// Get the source address
		lo_address source = lo_message_get_source(msg);
		char *sourceUrl = lo_address_get_url(source);
		std::string sourceStr = sourceUrl ? sourceUrl : "unknown";
		free(sourceUrl);

		std::cout << "OSC RECV <- " << sourceStr << " " << path << " (";

		// Convert arguments to a vector
		std::vector<std::any> args;
		for (int i = 0; i < argc; i++)
		{
			switch (types[i])
			{
			case 'i':
				std::cout << argv[i]->i;
				args.push_back(std::any(argv[i]->i));
				break;
			case 'f':
				std::cout << argv[i]->f;
				args.push_back(std::any(argv[i]->f));
				break;
			case 's':
				std::cout << &(argv[i]->s);
				args.push_back(std::any(std::string(&(argv[i]->s))));
				break;
			// Handle other OSC types as needed
			default:
				std::cout << "?";
				break;
			}

			if (i < argc - 1)
				std::cout << ", ";
		}
		std::cout << ")" << std::endl;

		// Call the registered callback if available
		if (m_messageCallback)
		{
			// Invoke the callback with the parsed message
			m_messageCallback(path, args);
		}

		return 0; // Return 1 to indicate message handled, 0 otherwise
	}

	bool RmeOscController::setMessageCallback(OscMessageCallback callback)
	{
		if (!m_configured)
		{
			std::cerr << "RmeOscController: Not configured" << std::endl;
			return false;
		}

		m_messageCallback = callback;
		return true;
	}

	bool RmeOscController::sendCommand(const std::string &address, const std::vector<std::any> &args)
	{
		if (!m_configured || !m_oscAddress)
		{
			std::cerr << "RmeOscController: Not configured or no OSC address" << std::endl;
			return false;
		}

		std::cout << "OSC SEND -> " << m_rmeIp << ":" << m_rmePort << " " << address;

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

	bool RmeOscController::sendBatch(const std::vector<RmeOscCommandConfig> &commands)
	{
		if (!m_configured || !m_oscAddress)
		{
			std::cerr << "RmeOscController: Not configured or no OSC address" << std::endl;
			return false;
		}

		// Create bundle
		lo_bundle bundle = lo_bundle_new(LO_TT_IMMEDIATE);
		if (!bundle)
		{
			std::cerr << "RmeOscController: Failed to create OSC bundle" << std::endl;
			return false;
		}

		std::cout << "OSC SEND BATCH -> " << m_rmeIp << ":" << m_rmePort << " (" << commands.size() << " commands)" << std::endl;

		// Add all commands to bundle
		for (const auto &cmd : commands)
		{
			lo_message msg = lo_message_new();
			if (!msg)
			{
				std::cerr << "RmeOscController: Failed to create OSC message for batch" << std::endl;
				lo_bundle_free(bundle);
				return false;
			}

			// Add arguments to message
			for (const auto &arg : cmd.args)
			{
				if (arg.type() == typeid(float))
				{
					lo_message_add_float(msg, std::any_cast<float>(arg));
				}
				else if (arg.type() == typeid(int))
				{
					lo_message_add_int32(msg, std::any_cast<int>(arg));
				}
				else if (arg.type() == typeid(bool))
				{
					lo_message_add_int32(msg, std::any_cast<bool>(arg) ? 1 : 0);
				}
				else if (arg.type() == typeid(std::string))
				{
					lo_message_add_string(msg, std::any_cast<std::string>(arg).c_str());
				}
				else
				{
					std::cerr << "RmeOscController: Unsupported argument type in batch" << std::endl;
					lo_message_free(msg);
					lo_bundle_free(bundle);
					return false;
				}
			}

			// Add message to bundle
			lo_bundle_add_message(bundle, cmd.address.c_str(), msg);
			lo_message_free(msg); // Message is copied into bundle
		}

		// Send the bundle
		int result = lo_send_bundle(m_oscAddress, bundle);
		lo_bundle_free(bundle);

		if (result == -1)
		{
			std::cerr << "RmeOscController: Failed to send OSC bundle: "
					  << lo_address_errstr(m_oscAddress) << std::endl;
			return false;
		}

		std::cout << "OSC batch sent successfully" << std::endl;
		return true;
	}

	bool RmeOscController::querySingleValue(const std::string &address, float &outValue, int timeoutMs)
	{
		if (!m_oscAddress)
		{
			std::cerr << "RmeOscController: OSC address not configured" << std::endl;
			return false;
		}

		// Create a temporary server to receive the reply
		lo_server tempServer = lo_server_new(nullptr, nullptr);
		if (!tempServer)
		{
			std::cerr << "RmeOscController: Failed to create temp OSC server" << std::endl;
			return false;
		}

		// Set up a flag and value for the callback
		std::atomic<bool> responseReceived{false};
		float receivedValue = 0.0f;

		// Add a method that matches the reply pattern
		lo_server_add_method(tempServer, address.c_str(), "f", [](const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *data) -> int
							 {
				auto responseData = static_cast<std::pair<std::atomic<bool>*, float*>*>(data);
				if (argc >= 1 && types[0] == 'f')
				{
					*responseData->second = argv[0]->f;
					responseData->first->store(true);
				}
				return 0; }, new std::pair<std::atomic<bool> *, float *>(&responseReceived, &receivedValue));

		// Send an empty message to request current value
		lo_message msg = lo_message_new();
		int result = lo_send_message(m_oscAddress, address.c_str(), msg);
		lo_message_free(msg);

		if (result == -1)
		{
			std::cerr << "RmeOscController: Failed to send query: "
					  << lo_address_errstr(m_oscAddress) << std::endl;
			lo_server_free(tempServer);
			return false;
		}

		// Wait for response with timeout
		auto startTime = std::chrono::steady_clock::now();
		while (!responseReceived.load())
		{
			// Check for incoming messages with a short timeout
			lo_server_recv_noblock(tempServer, 1);

			// Check if timeout has elapsed
			auto elapsed = std::chrono::steady_clock::now() - startTime;
			if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > timeoutMs)
			{
				std::cerr << "RmeOscController: Timeout waiting for response to " << address << std::endl;
				lo_server_free(tempServer);
				return false;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		// Clean up
		lo_server_free(tempServer);

		// Set the output value
		outValue = receivedValue;
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

	bool RmeOscController::refreshConnectionStatus()
	{
		// Send a generic ping command to see if the connection is still alive
		// You might need to adjust this based on RME's specific protocol
		std::string address = "/ping";
		lo_message msg = lo_message_new();
		int result = lo_send_message(m_oscAddress, address.c_str(), msg);
		lo_message_free(msg);

		if (result == -1)
		{
			std::cout << "RME OSC connection is not responding" << std::endl;
			return false;
		}

		std::cout << "RME OSC connection is active" << std::endl;
		return true;
	}

	bool RmeOscController::queryChannelVolume(int channel, float &volume_db)
	{
		std::string address = "/1/channel/" + std::to_string(channel) + "/volume";
		float normalized_volume = 0.0f;

		if (!querySingleValue(address, normalized_volume, 500))
		{
			return false;
		}

		// Convert normalized volume back to dB
		// Assuming the same conversion as in setChannelVolume
		if (normalized_volume <= 0.0f)
		{
			volume_db = -INFINITY;
		}
		else
		{
			volume_db = normalized_volume * 96.0f - 96.0f;
		}

		return true;
	}

} // namespace AudioEngine
