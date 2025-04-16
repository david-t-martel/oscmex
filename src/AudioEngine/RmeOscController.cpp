#include "RmeOscController.h"

#include <iostream>
#include <cmath>
#include <thread>
#include <chrono>
#include <functional>
#include <algorithm>
#include <map>

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

		// Set UDP transmission timeout (default is -1 which can hang indefinitely)
		lo_address_set_ttl(m_oscAddress, 4); // Reasonable TTL for local network

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
		lo_server_thread_add_method(m_oscServer, nullptr, nullptr,
									// Use C-style cast here because we know the argument types match
									(lo_method_handler)[](const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)->int {
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

		return 0; // Return 0 to indicate we handled the message
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

		// Package the data for the callback
		auto responseData = new std::pair<std::atomic<bool> *, float *>(&responseReceived, &receivedValue);

		// Add a method that matches the reply pattern
		lo_server_add_method(tempServer, address.c_str(), "f",
							 // Use C-style cast for lo_method_handler
							 (lo_method_handler)[](const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *data)->int {
				auto responseData = static_cast<std::pair<std::atomic<bool>*, float*>*>(data);
				if (argc >= 1 && types[0] == 'f')
				{
					*responseData->second = argv[0]->f;
					responseData->first->store(true);
				}
				return 0; }, responseData);

		// Send an empty message to request current value
		lo_message msg = lo_message_new();
		int result = lo_send_message(m_oscAddress, address.c_str(), msg);
		lo_message_free(msg);

		if (result == -1)
		{
			std::cerr << "RmeOscController: Failed to send query: "
					  << lo_address_errstr(m_oscAddress) << std::endl;
			lo_server_free(tempServer);
			delete responseData;
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
				delete responseData;
				return false;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		// Clean up
		lo_server_free(tempServer);
		outValue = receivedValue;
		delete responseData;
		return true;
	}

	std::string RmeOscController::buildChannelAddress(ChannelType type, int channel, ParamType param)
	{
		std::string prefix;
		std::string paramName;

		// Build prefix based on channel type
		switch (type)
		{
		case ChannelType::INPUT:
			prefix = "/input/";
			break;
		case ChannelType::PLAYBACK:
			prefix = "/playback/";
			break;
		case ChannelType::OUTPUT:
			prefix = "/output/";
			break;
		}

		// Build parameter name
		switch (param)
		{
		case ParamType::VOLUME:
			paramName = "volume";
			break;
		case ParamType::MUTE:
			paramName = "mute";
			break;
		case ParamType::STEREO:
			paramName = "stereo";
			break;
		case ParamType::PAN:
			paramName = "balance"; // For outputs, "balance" for inputs/playback, "pan"
			if (type != ChannelType::OUTPUT)
				paramName = "pan";
			break;
		case ParamType::PHASE:
			paramName = "phase";
			break;
		case ParamType::FX_SEND:
			paramName = "fx";
			break;
		case ParamType::FX_RETURN:
			paramName = "fx";
			break;
		case ParamType::GAIN:
			paramName = "gain";
			break;
		case ParamType::EQ_ENABLE:
			paramName = "eq";
			break;
		case ParamType::PHANTOM_POWER:
			paramName = "48v";
			break;
		case ParamType::HI_Z:
			paramName = "hi-z";
			break;
		case ParamType::DYN_ENABLE:
			paramName = "dynamics";
			break;
		// EQ bands have special handling
		case ParamType::EQ_BAND1_TYPE:
		case ParamType::EQ_BAND1_FREQ:
		case ParamType::EQ_BAND1_GAIN:
		case ParamType::EQ_BAND1_Q:
			// Will be handled separately below
			paramName = "eq";
			break;
		default:
			std::cerr << "RmeOscController: Unsupported parameter type" << std::endl;
			return "";
		}

		// Form the basic address
		std::string address = prefix + std::to_string(channel) + "/" + paramName;

		// Special handling for EQ parameters which need a sub-path
		if (param >= ParamType::EQ_BAND1_TYPE && param <= ParamType::EQ_BAND1_Q)
		{
			int band = 1; // Default to band 1

			std::string eqParam;
			switch (param)
			{
			case ParamType::EQ_BAND1_TYPE:
				eqParam = "band1type";
				break;
			case ParamType::EQ_BAND1_FREQ:
				eqParam = "band1freq";
				break;
			case ParamType::EQ_BAND1_GAIN:
				eqParam = "band1gain";
				break;
			case ParamType::EQ_BAND1_Q:
				eqParam = "band1q";
				break;
			default:
				break;
			}

			address += "/" + eqParam;
		}

		return address;
	}

	float RmeOscController::dbToNormalized(float db) const
	{
		// Convert dB to normalized range (0-1)
		// Based on RME's expected range in oscmix.c
		// volume value handling (typically -65dB to +6dB)

		if (db <= -65.0f)
		{
			return 0.0f; // Minimum value (-inf dB)
		}
		else
		{
			// Map from typical dB range to 0-1
			// This matches RME's volume scaling in the oscmix.c code
			float norm = (db + 65.0f) / 71.0f;			 // Map from -65..+6 to 0..1
			return std::max(0.0f, std::min(1.0f, norm)); // Clamp to 0-1
		}
	}

	float RmeOscController::normalizedToDb(float normalized) const
	{
		// Convert normalized volume back to dB
		// Inverse of dbToNormalized
		if (normalized <= 0.0f)
		{
			return -INFINITY; // Or use -96.0f as a practical "silence" level
		}
		else
		{
			return normalized * 71.0f - 65.0f; // Map from 0..1 to -65..+6
		}
	}

	bool RmeOscController::setMatrixCrosspointGain(int hw_input, int hw_output, float gain_db)
	{
		// Based on the oscmix.c implementation, we need to convert dB to a linear value
		// and use the appropriate address format

		std::string address = "/mix/" + std::to_string(hw_output) + "/input/" + std::to_string(hw_input);

		// In the oscmix.c code, setmix() function converts dB to linear using powf(10, vol / 20)
		// It also handles panning, but we'll keep it simple for this implementation
		return sendCommand(address, {std::any(gain_db)});
	}

	bool RmeOscController::setChannelMute(ChannelType type, int channel, bool mute)
	{
		std::string address = buildChannelAddress(type, channel, ParamType::MUTE);
		if (address.empty())
			return false;

		// Boolean values are sent as integers in OSC
		return sendCommand(address, {std::any(mute)});
	}

	bool RmeOscController::setChannelStereo(ChannelType type, int channel, bool stereo)
	{
		std::string address = buildChannelAddress(type, channel, ParamType::STEREO);
		if (address.empty())
			return false;

		// In oscmix.c, stereo links always affect two channels (odd + even)
		return sendCommand(address, {std::any(stereo)});
	}

	bool RmeOscController::setChannelVolume(ChannelType type, int channel, float volume_db)
	{
		std::string address = buildChannelAddress(type, channel, ParamType::VOLUME);
		if (address.empty())
			return false;

		// Convert dB to normalized range (0-1) for RME
		float volume_norm = dbToNormalized(volume_db);

		return sendCommand(address, {std::any(volume_norm)});
	}

	bool RmeOscController::setInputPhantomPower(int channel, bool enabled)
	{
		std::string address = buildChannelAddress(ChannelType::INPUT, channel, ParamType::PHANTOM_POWER);
		if (address.empty())
			return false;

		// In oscmix.c, this is only valid for certain inputs (check the INPUT_48V flag)
		return sendCommand(address, {std::any(enabled)});
	}

	bool RmeOscController::setInputHiZ(int channel, bool enabled)
	{
		std::string address = buildChannelAddress(ChannelType::INPUT, channel, ParamType::HI_Z);
		if (address.empty())
			return false;

		// In oscmix.c, this is only valid for certain inputs (check the INPUT_HIZ flag)
		return sendCommand(address, {std::any(enabled)});
	}

	bool RmeOscController::setChannelEQ(ChannelType type, int channel, bool enabled)
	{
		std::string address = buildChannelAddress(type, channel, ParamType::EQ_ENABLE);
		if (address.empty())
			return false;

		return sendCommand(address, {std::any(enabled)});
	}

	bool RmeOscController::setChannelEQBand(ChannelType type, int channel, int band, float freq, float gain, float q)
	{
		// Validate band number
		if (band < 1 || band > 3)
		{
			std::cerr << "RmeOscController: Invalid EQ band number (must be 1-3)" << std::endl;
			return false;
		}

		// Based on oscmix.c, EQ parameters are stored in separate registers
		// We'll need to send multiple commands

		// EQ must be enabled first
		if (!setChannelEQ(type, channel, true))
		{
			return false;
		}

		// Base address for this channel type
		std::string baseAddr = (type == ChannelType::INPUT) ? "/input/" : (type == ChannelType::OUTPUT) ? "/output/"
																										: "/playback/";
		baseAddr += std::to_string(channel);

		// Send frequency parameter
		std::string freqAddr = baseAddr + "/eq/band" + std::to_string(band) + "freq";
		if (!sendCommand(freqAddr, {std::any((int)freq)}))
		{
			return false;
		}

		// Send gain parameter (oscmix.c scales by 0.1)
		std::string gainAddr = baseAddr + "/eq/band" + std::to_string(band) + "gain";
		if (!sendCommand(gainAddr, {std::any(gain * 10.0f)}))
		{
			return false;
		}

		// Send Q parameter (oscmix.c scales by 0.1)
		std::string qAddr = baseAddr + "/eq/band" + std::to_string(band) + "q";
		if (!sendCommand(qAddr, {std::any(q * 10.0f)}))
		{
			return false;
		}

		return true;
	}

	bool RmeOscController::queryChannelVolume(ChannelType type, int channel, float &volume_db)
	{
		std::string address = buildChannelAddress(type, channel, ParamType::VOLUME);
		if (address.empty())
			return false;

		float normalized_volume = 0.0f;
		if (!querySingleValue(address, normalized_volume, 500))
		{
			return false;
		}

		// Convert normalized volume back to dB
		volume_db = normalizedToDb(normalized_volume);
		return true;
	}

	bool RmeOscController::requestRefresh()
	{
		// In oscmix.c, this sends a special command to request all current values
		return sendCommand("/refresh", {});
	}

	bool RmeOscController::refreshConnectionStatus()
	{
		// Send a ping to verify connection status
		// Based on TotalMix FX OSC documentation, we'll use a harmless query
		std::string address = "/hardware/dspload";
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

} // namespace AudioEngine
