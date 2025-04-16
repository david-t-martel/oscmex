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
	// Utility function to convert log levels to dB
	static inline float logToDB(float value)
	{
		// Prevent log(0)
		if (value <= 0)
			return -144.0f; // approx. silence level

		return 20.0f * log10f(value);
	}

	RmeOscController::RmeOscController()
		: m_oscAddress(nullptr), m_oscServer(nullptr), m_oscThread(nullptr),
		  m_messageCallback(nullptr), m_levelCallback(nullptr)
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

		// Configure timeout to prevent blocking for too long
		lo_address_set_timeout_ms(m_oscAddress, 500); // 500ms timeout

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
					return controller->handleOscMessage(path, types, argv, argc, msg, user_data); }, this);

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

		// Convert arguments to a vector
		std::vector<std::any> args;
		for (int i = 0; i < argc; i++)
		{
			switch (types[i])
			{
			case 'i':
				args.push_back(std::any(argv[i]->i));
				break;
			case 'f':
				args.push_back(std::any(argv[i]->f));
				break;
			case 's':
				args.push_back(std::any(std::string(&(argv[i]->s))));
				break;
			// Handle other OSC types as needed
			default:
				break;
			}
		}

		// Check if this is a level meter message
		std::string pathStr(path);
		if (m_levelMetersEnabled &&
			(pathStr.find("/input/") == 0 ||
			 pathStr.find("/playback/") == 0 ||
			 pathStr.find("/output/") == 0) &&
			pathStr.find("/level") != std::string::npos)
		{
			// Parse channel type and number
			ChannelType type;
			if (pathStr.find("/input/") == 0)
			{
				type = ChannelType::INPUT;
			}
			else if (pathStr.find("/playback/") == 0)
			{
				type = ChannelType::PLAYBACK;
			}
			else
			{
				type = ChannelType::OUTPUT;
			}

			// Extract channel number
			size_t startPos = pathStr.find("/") + 1;
			size_t endPos = pathStr.find("/", startPos);
			std::string typeStr = pathStr.substr(startPos, endPos - startPos);

			startPos = pathStr.find("/", startPos) + 1;
			endPos = pathStr.find("/", startPos);
			int channel = std::stoi(pathStr.substr(startPos, endPos - startPos));

			// Process level meter data
			if (argc >= 2)
			{
				float peakdB = 0.0f;
				float rmsdB = 0.0f;
				float peakFxdB = 0.0f;
				float rmsFxdB = 0.0f;
				bool clipping = false;

				// Basic format: peak, RMS
				if (types[0] == 'f')
					peakdB = argv[0]->f;
				if (types[1] == 'f')
					rmsdB = argv[1]->f;

				// Extended format: peak, RMS, peakFX, rmsFX, clipping
				if (argc >= 4)
				{
					if (types[2] == 'f')
						peakFxdB = argv[2]->f;
					if (types[3] == 'f')
						rmsFxdB = argv[3]->f;
				}

				// Clipping indicator
				if (argc >= 5 && types[4] == 'i')
				{
					clipping = (argv[4]->i != 0);
				}

				// Process the level meter data
				processLevelMeter(type, channel, peakdB, rmsdB, peakFxdB, rmsFxdB, clipping);
			}

			// Still pass the message to the general message handler if registered
		}

		// Check for DSP status updates
		if (pathStr == "/hardware/dspload" && argc >= 1 && types[0] == 'i')
		{
			m_dspStatus.loadPercent = argv[0]->i;
		}
		else if (pathStr == "/hardware/dspvers" && argc >= 1 && types[0] == 'i')
		{
			m_dspStatus.version = argv[0]->i;
		}

		// Call the registered callback if available
		if (m_messageCallback)
		{
			// Invoke the callback with the parsed message
			m_messageCallback(path, args);
		}

		// Check if this is a response to a pending query
		auto it = m_pendingResponses.find(pathStr);
		if (it != m_pendingResponses.end())
		{
			it->second.store(true);
		}

		return 0; // Return 0 to indicate we handled the message
	}

	void RmeOscController::processLevelMeter(ChannelType type, int channel,
											 float peakdB, float rmsdB,
											 float peakFxdB, float rmsFxdB,
											 bool clipping)
	{
		// Skip processing if no callback is registered
		if (!m_levelCallback)
		{
			return;
		}

		// Create level meter data and invoke callback
		LevelMeterData data;
		data.peakdB = peakdB;
		data.rmsdB = rmsdB;
		data.peakFxdB = peakFxdB;
		data.rmsFxdB = rmsFxdB;
		data.clipping = clipping;

		m_levelCallback(type, channel, data);
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

	bool RmeOscController::setLevelMeterCallback(LevelMeterCallback callback)
	{
		m_levelCallback = callback;
		return true;
	}

	bool RmeOscController::enableLevelMeters(bool enable)
	{
		if (!m_configured || !m_oscAddress)
		{
			std::cerr << "RmeOscController: Not configured or no OSC address" << std::endl;
			return false;
		}

		m_levelMetersEnabled = enable;

		// If enabling, send a message to request level data
		if (enable)
		{
			// In oscmix.c, level data is requested by sending a special SysEx message
			// Here we'll request it via OSC instead
			return sendCommand("/hardware/meters", {std::any(enable ? 1 : 0)});
		}

		return true;
	}

	RmeOscController::DspStatus RmeOscController::getDspStatus() const
	{
		return m_dspStatus;
	}

	bool RmeOscController::sendCommand(const std::string &address, const std::vector<std::any> &args)
	{
		if (!m_configured || !m_oscAddress)
		{
			std::cerr << "RmeOscController: Not configured or no OSC address" << std::endl;
			return false;
		}

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
			}
			else if (arg.type() == typeid(int))
			{
				lo_message_add_int32(msg, std::any_cast<int>(arg));
			}
			else if (arg.type() == typeid(bool))
			{
				// Booleans are sent as integers (0/1) in OSC
				lo_message_add_int32(msg, std::any_cast<bool>(arg) ? 1 : 0);
			}
			else if (arg.type() == typeid(std::string))
			{
				lo_message_add_string(msg, std::any_cast<std::string>(arg).c_str());
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

		return true;
	}

	lo_bundle RmeOscController::createBundle()
	{
		// Create bundle with immediate timestamp (LO_TT_IMMEDIATE)
		return lo_bundle_new(LO_TT_IMMEDIATE);
	}

	bool RmeOscController::sendBatch(const std::vector<RmeOscCommandConfig> &commands)
	{
		if (!m_configured || !m_oscAddress)
		{
			std::cerr << "RmeOscController: Not configured or no OSC address" << std::endl;
			return false;
		}

		// Create bundle
		lo_bundle bundle = createBundle();
		if (!bundle)
		{
			std::cerr << "RmeOscController: Failed to create OSC bundle" << std::endl;
			return false;
		}

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

		return true;
	}

	bool RmeOscController::querySingleValue(const std::string &address, float &outValue, int timeoutMs)
	{
		if (!m_oscAddress)
		{
			std::cerr << "RmeOscController: OSC address not configured" << std::endl;
			return false;
		}

		if (!m_oscServer)
		{
			std::cerr << "RmeOscController: OSC server not started, cannot receive responses" << std::endl;
			return false;
		}

		// Setup a flag for response tracking
		m_pendingResponses[address].store(false);

		// Create a temporary variable to hold the result
		// We will grab it from the response handler
		float result = 0.0f;

		// Send an empty message to request the value
		lo_message msg = lo_message_new();
		int sendResult = lo_send_message(m_oscAddress, address.c_str(), msg);
		lo_message_free(msg);

		if (sendResult == -1)
		{
			std::cerr << "RmeOscController: Failed to send query: "
					  << lo_address_errstr(m_oscAddress) << std::endl;
			m_pendingResponses.erase(address);
			return false;
		}

		// Wait for response with timeout
		auto startTime = std::chrono::steady_clock::now();
		while (!m_pendingResponses[address].load())
		{
			// Check if timeout has elapsed
			auto elapsed = std::chrono::steady_clock::now() - startTime;
			if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > timeoutMs)
			{
				std::cerr << "RmeOscController: Timeout waiting for response to " << address << std::endl;
				m_pendingResponses.erase(address);
				return false;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		// Clean up
		m_pendingResponses.erase(address);

		// The value will have been updated by handleOscMessage
		outValue = result;
		return true;
	}

	std::string RmeOscController::buildChannelAddress(ChannelType type, int channel, ParamType param)
	{
		// Channel numbers in OSC are 1-based
		if (channel < 1)
		{
			std::cerr << "RmeOscController: Invalid channel number (must be >= 1)" << std::endl;
			return "";
		}

		std::string prefix;
		std::string paramName;

		// Build prefix based on channel type (these match the paths in oscmix.c)
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

		// Build parameter name based on parameter type
		// These match the paramName usage in the oscnode trees in oscmix.c
		switch (param)
		{
		case ParamType::VOLUME:
			paramName = type == ChannelType::OUTPUT ? "volume" : "volume";
			break;
		case ParamType::MUTE:
			paramName = "mute";
			break;
		case ParamType::STEREO:
			paramName = "stereo";
			break;
		case ParamType::PAN:
			// In oscmix.c this is called "balance" for outputs and "pan" for inputs/playbacks
			paramName = (type == ChannelType::OUTPUT) ? "balance" : "pan";
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
			// Only valid for inputs and refers to the preamp gain in oscmix.c
			if (type != ChannelType::INPUT)
			{
				std::cerr << "RmeOscController: Gain parameter only valid for inputs" << std::endl;
				return "";
			}
			paramName = "gain";
			break;
		case ParamType::EQ_ENABLE:
			paramName = "eq";
			break;
		case ParamType::PHANTOM_POWER:
			// 48V is only valid for inputs in oscmix.c
			if (type != ChannelType::INPUT)
			{
				std::cerr << "RmeOscController: Phantom power only valid for inputs" << std::endl;
				return "";
			}
			paramName = "48v";
			break;
		case ParamType::HI_Z:
			// Hi-Z is only valid for inputs in oscmix.c
			if (type != ChannelType::INPUT)
			{
				std::cerr << "RmeOscController: Hi-Z only valid for inputs" << std::endl;
				return "";
			}
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
		if (db <= -65.0f)
		{
			return 0.0f; // Minimum value (-inf dB)
		}
		else
		{
			// This matches the conversion in oscmix.c which represents volume
			// as a float between 0 and 1
			float range = 71.0f; // From -65 dB to +6 dB = 71 dB range
			float norm = (db + 65.0f) / range;
			return std::max(0.0f, std::min(1.0f, norm)); // Clamp to 0-1
		}
	}

	float RmeOscController::normalizedToDb(float normalized) const
	{
		// Convert normalized volume back to dB
		// Inverse of dbToNormalized
		if (normalized <= 0.0f)
		{
			return -INFINITY; // In oscmix.c, this is represented as -650 in tenths of dB
		}
		else
		{
			// Map from 0..1 to -65..+6 dB
			return normalized * 71.0f - 65.0f;
		}
	}

	bool RmeOscController::setMatrixCrosspointGain(int hw_input, int hw_output, float gain_db)
	{
		// Clamp gain to valid range (-65.0 to +6.0 dB)
		float clamped_db = clampVolumeDb(gain_db);
		if (clamped_db != gain_db)
		{
			std::cerr << "[RmeOscController] Matrix gain dB clamped from " << gain_db << " to " << clamped_db << std::endl;
		}
		std::string address = "/mix/" + std::to_string(hw_output) + "/input/" + std::to_string(hw_input);
		if (clamped_db <= -65.0f)
		{
			clamped_db = -INFINITY;
		}
		return sendCommand(address, {std::any(clamped_db)});
	}

	bool RmeOscController::setChannelMute(ChannelType type, int channel, bool mute)
	{
		// No bounds needed for bool
		std::string address = buildChannelAddress(type, channel, ParamType::MUTE);
		if (address.empty())
			return false;
		return sendCommand(address, {std::any(mute)});
	}

	bool RmeOscController::setChannelStereo(ChannelType type, int channel, bool stereo)
	{
		// No bounds needed for bool
		std::string address = buildChannelAddress(type, channel, ParamType::STEREO);
		if (address.empty())
			return false;
		return sendCommand(address, {std::any(stereo)});
	}

	bool RmeOscController::setChannelVolume(ChannelType type, int channel, float volume_db)
	{
		// Clamp volume to valid range (-65.0 to +6.0 dB)
		float clamped_db = clampVolumeDb(volume_db);
		if (clamped_db != volume_db)
		{
			std::cerr << "[RmeOscController] Volume dB clamped from " << volume_db << " to " << clamped_db << std::endl;
		}
		std::string address = buildChannelAddress(type, channel, ParamType::VOLUME);
		if (address.empty())
			return false;
		float volume_norm = dbToNormalized(clamped_db);
		return sendCommand(address, {std::any(volume_norm)});
	}

	bool RmeOscController::setInputPhantomPower(int channel, bool enabled)
	{
		// No bounds needed for bool
		std::string address = buildChannelAddress(ChannelType::INPUT, channel, ParamType::PHANTOM_POWER);
		if (address.empty())
			return false;
		return sendCommand(address, {std::any(enabled)});
	}

	bool RmeOscController::setInputHiZ(int channel, bool enabled)
	{
		// No bounds needed for bool
		std::string address = buildChannelAddress(ChannelType::INPUT, channel, ParamType::HI_Z);
		if (address.empty())
			return false;
		return sendCommand(address, {std::any(enabled)});
	}

	bool RmeOscController::setChannelEQ(ChannelType type, int channel, bool enabled)
	{
		// No bounds needed for bool
		std::string address = buildChannelAddress(type, channel, ParamType::EQ_ENABLE);
		if (address.empty())
			return false;
		return sendCommand(address, {std::any(enabled)});
	}

	bool RmeOscController::setChannelEQBand(ChannelType type, int channel, int band, float freq, float gain, float q)
	{
		// Clamp EQ parameters to valid ranges
		float clamped_freq = clampEQFreq(freq);
		float clamped_gain = clampEQGain(gain);
		float clamped_q = clampEQQ(q);
		if (clamped_freq != freq)
		{
			std::cerr << "[RmeOscController] EQ freq clamped from " << freq << " to " << clamped_freq << std::endl;
		}
		if (clamped_gain != gain)
		{
			std::cerr << "[RmeOscController] EQ gain clamped from " << gain << " to " << clamped_gain << std::endl;
		}
		if (clamped_q != q)
		{
			std::cerr << "[RmeOscController] EQ Q clamped from " << q << " to " << clamped_q << std::endl;
		}
		// ...existing code for address and batch...
		std::string baseAddr = (type == ChannelType::INPUT) ? "/input/" : (type == ChannelType::OUTPUT) ? "/output/"
																										: "/playback/";
		baseAddr += std::to_string(channel);
		std::vector<RmeOscCommandConfig> commands;
		std::string freqAddr = baseAddr + "/eq/band" + std::to_string(band) + "freq";
		commands.push_back({freqAddr, {std::any((int)clamped_freq)}});
		std::string gainAddr = baseAddr + "/eq/band" + std::to_string(band) + "gain";
		commands.push_back({gainAddr, {std::any(clamped_gain * 10.0f)}});
		std::string qAddr = baseAddr + "/eq/band" + std::to_string(band) + "q";
		commands.push_back({qAddr, {std::any(clamped_q * 10.0f)}});
		return sendBatch(commands);
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
		// In oscmix.c, the setrefresh function does a few important things:
		// 1. It sends a specific register value (setreg(0x3e04, 0x67cd))
		// 2. It sets refreshing = true
		// 3. It sends updates for certain parameters directly

		std::cout << "Requesting complete refresh from RME device..." << std::endl;

		// First send the refresh command
		bool result = sendCommand("/refresh", {});

		if (result)
		{
			// Wait a short time for the device to start sending updates
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

			// Query some essential parameters
			// This helps ensure we have current values for critical parameters
			float dummyValue;
			for (int i = 1; i <= 2; i++)
			{ // Just check first couple outputs
				queryChannelVolume(ChannelType::OUTPUT, i, dummyValue);
			}
		}

		return result;
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
