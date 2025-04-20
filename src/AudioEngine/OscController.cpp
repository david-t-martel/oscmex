#include "OscController.h" // Updated include

#include <iostream>
#include <cmath>
#include <thread>
#include <chrono>
#include <functional>
#include <algorithm>
#include <map>
#include <vector> // Added for std::vector
#include <any>	  // Added for std::any

// liblo headers are now included in OscController.h

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

	OscController::OscController() // Renamed from RmeOscController
		: m_oscAddress(nullptr), m_oscServer(nullptr),
		  m_messageCallback(nullptr), m_levelCallback(nullptr)
	{
		// Constructor is minimal, initialization happens in configure()
	}

	OscController::~OscController() // Renamed from RmeOscController
	{
		// Clean up liblo resources
		cleanup();
	}

	void OscController::cleanup()
	{
		stopReceiver();

		if (m_oscAddress)
		{
			lo_address_free(m_oscAddress);
			m_oscAddress = nullptr;
		}
		m_configured = false; // Mark as not configured after cleanup
	}

	bool OscController::configure(const std::string &ip, int port)
	{
		if (m_configured)
		{
			std::cerr << "OscController: Already configured. Call cleanup() first." << std::endl;
			return false;
		}

		m_targetIp = ip;
		m_targetPort = port;

		// Create OSC address for sending
		m_oscAddress = lo_address_new(ip.c_str(), std::to_string(port).c_str());
		if (!m_oscAddress)
		{
			std::cerr << "OscController: Failed to create OSC address for " << ip << ":" << port << std::endl;
			return false;
		}

		// Set UDP transmission timeout (default is -1 which can hang indefinitely)
		lo_address_set_ttl(m_oscAddress, 4); // Reasonable TTL for local network

		// Configure timeout to prevent blocking for too long
		lo_address_set_timeout_ms(m_oscAddress, 500); // 500ms timeout

		m_configured = true;
		std::cout << "OSC Controller configured for " << ip << ":" << port << std::endl;
		return true;
	}

	bool OscController::startReceiver(int receivePort)
	{
		if (m_oscServer)
		{
			std::cerr << "OscController: OSC receiver already started" << std::endl;
			return false;
		}

		// Create OSC server for receiving
		m_oscServer = lo_server_thread_new(std::to_string(receivePort).c_str(),
										   [](int num, const char *msg, const char *path)
										   {
											   std::cerr << "OSC error " << num << ": " << msg << " (" << (path ? path : "null") << ")" << std::endl;
											   // Consider adding more robust error handling or reporting
										   });

		if (!m_oscServer)
		{
			std::cerr << "OscController: Failed to create OSC server on port " << receivePort << std::endl;
			return false;
		}

		// Set up generic message handler using the static wrapper
		lo_server_thread_add_method(m_oscServer, nullptr, nullptr,
									OscController::handleOscMessageStatic, this);

		// Start the server thread
		if (lo_server_thread_start(m_oscServer) < 0)
		{
			std::cerr << "OscController: Failed to start OSC server thread" << std::endl;
			lo_server_thread_free(m_oscServer);
			m_oscServer = nullptr;
			return false;
		}

		std::cout << "OSC Receiver started on port " << receivePort << std::endl;
		return true;
	}

	void OscController::stopReceiver()
	{
		if (m_oscServer)
		{
			lo_server_thread_stop(m_oscServer);
			lo_server_thread_free(m_oscServer);
			m_oscServer = nullptr;
			std::cout << "OSC Receiver stopped." << std::endl;
		}
	}

	// Static wrapper for the liblo callback
	int OscController::handleOscMessageStatic(const char *path, const char *types,
											  lo_arg **argv, int argc, lo_message msg, void *user_data)
	{
		OscController *controller = static_cast<OscController *>(user_data);
		if (controller)
		{
			return controller->handleOscMessage(path, types, argv, argc, msg);
		}
		return 1; // Indicate error if controller is null
	}

	// Instance method that does the actual processing
	int OscController::handleOscMessage(const char *path, const char *types,
										lo_arg **argv, int argc, lo_message msg)
	{
		// Get the source address
		lo_address source = lo_message_get_source(msg);
		char *sourceUrl = source ? lo_address_get_url(source) : nullptr;
		std::string sourceStr = sourceUrl ? sourceUrl : "unknown";
		if (sourceUrl)
			free(sourceUrl); // Free the URL string allocated by liblo

		// Convert arguments to a vector of std::any
		std::vector<std::any> args;
		for (int i = 0; i < argc; i++)
		{
			if (!types || !argv || !argv[i])
				continue; // Basic safety check

			switch (types[i])
			{
			case LO_INT32:
				args.push_back(std::any(argv[i]->i));
				break;
			case LO_FLOAT:
				args.push_back(std::any(argv[i]->f));
				break;
			case LO_STRING:
				// Ensure null termination or handle length if necessary
				args.push_back(std::any(std::string(&(argv[i]->s))));
				break;
			case LO_TRUE:
				args.push_back(std::any(true));
				break;
			case LO_FALSE:
				args.push_back(std::any(false));
				break;
			// Handle other OSC types as needed (blob, int64, double, char, midi, etc.)
			default:
				// Optionally log unsupported type
				// std::cout << "OSC Received unsupported type: " << types[i] << std::endl;
				break;
			}
		}

		std::string pathStr = path ? path : ""; // Ensure path is not null

		// --- RME-Specific Processing (Example) ---
		// Check if this is a level meter message (RME specific path structure)
		if (m_levelMetersEnabled && !pathStr.empty() &&
			(pathStr.find("/1/level") == 0 || // RME uses /1/level/input/x, /1/level/playback/x, /1/level/output/x
			 pathStr.find("/2/level") == 0 || // For second device if applicable
			 pathStr.find("/3/level") == 0))  // For third device if applicable
		{
			// Example parsing for RME level meter: /1/level/<type>/<channel>
			size_t typeStart = pathStr.find('/', 10); // Find '/' after "/1/level/"
			if (typeStart != std::string::npos)
			{
				size_t channelStart = pathStr.find('/', typeStart + 1);
				if (channelStart != std::string::npos)
				{
					std::string typeStr = pathStr.substr(typeStart + 1, channelStart - typeStart - 1);
					int channel = 0;
					try
					{
						channel = std::stoi(pathStr.substr(channelStart + 1));
					}
					catch (const std::invalid_argument &)
					{ /* ignore invalid channel */
					}
					catch (const std::out_of_range &)
					{ /* ignore out of range channel */
					}

					if (channel > 0)
					{ // Channel numbers are 1-based
						ChannelType type;
						if (typeStr == "input")
							type = ChannelType::INPUT;
						else if (typeStr == "playback")
							type = ChannelType::PLAYBACK;
						else if (typeStr == "output")
							type = ChannelType::OUTPUT;
						else
							goto skip_level_meter; // Not a recognized type

						// Process level meter data (RME sends peak, RMS, ...)
						if (argc >= 2 && types[0] == LO_FLOAT && types[1] == LO_FLOAT)
						{
							float peakdB = argv[0]->f;
							float rmsdB = argv[1]->f;
							float peakFxdB = (argc >= 3 && types[2] == LO_FLOAT) ? argv[2]->f : -144.0f;
							float rmsFxdB = (argc >= 4 && types[3] == LO_FLOAT) ? argv[3]->f : -144.0f;
							bool clipping = (argc >= 5 && types[4] == LO_TRUE); // RME sends 'T' for clipping

							processLevelMeter(type, channel, peakdB, rmsdB, peakFxdB, rmsFxdB, clipping);
						}
					}
				}
			}
		skip_level_meter:; // Label to jump to if parsing fails
		}

		// Check for DSP status updates (RME specific paths)
		if (pathStr == "/1/busLoad" && argc >= 1 && types[0] == LO_FLOAT) // RME uses float 0-1
		{
			m_dspStatus.loadPercent = static_cast<int>(argv[0]->f * 100.0f);
		}
		// RME doesn't seem to expose DSP version via standard OSC, might need specific query

		// --- End RME-Specific Processing ---

		// Call the registered general message callback if available
		if (m_messageCallback)
		{
			// Invoke the callback with the parsed message path and arguments
			m_messageCallback(pathStr, args);
		}

		// Check if this is a response to a pending query
		auto it = m_pendingResponses.find(pathStr);
		if (it != m_pendingResponses.end())
		{
			// TODO: Store the actual received value associated with the response
			// For now, just mark as received. Need a way to pass the value back.
			it->second.store(true);
		}

		return 0; // Return 0 to indicate we handled the message
	}

	void OscController::processLevelMeter(ChannelType type, int channel,
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

	bool OscController::setMessageCallback(OscMessageCallback callback)
	{
		// No configuration check needed to set a callback
		m_messageCallback = callback;
		return true;
	}

	bool OscController::setLevelMeterCallback(LevelMeterCallback callback)
	{
		m_levelCallback = callback;
		return true;
	}

	bool OscController::enableLevelMeters(bool enable)
	{
		if (!m_configured || !m_oscAddress)
		{
			std::cerr << "OscController: Not configured or no OSC address" << std::endl;
			return false;
		}

		m_levelMetersEnabled = enable;

		// Send the command to enable/disable level meter streaming (RME specific)
		// RME uses /1/meterconfig/input/enable, /1/meterconfig/playback/enable, /1/meterconfig/output/enable
		// We can send them all or make it more granular
		bool success = true;
		success &= sendCommand("/1/meterconfig/input/enable", {std::any(enable)});
		success &= sendCommand("/1/meterconfig/playback/enable", {std::any(enable)});
		success &= sendCommand("/1/meterconfig/output/enable", {std::any(enable)});

		// Optionally set update rate (e.g., /1/meterconfig/rate <int_ms>)
		// sendCommand("/1/meterconfig/rate", {std::any(100)}); // e.g., 100ms update rate

		return success;
	}

	OscController::DspStatus OscController::getDspStatus() const
	{
		// Note: DSP load is updated asynchronously via OSC message handling
		return m_dspStatus;
	}

	bool OscController::sendCommand(const std::string &address, const std::vector<std::any> &args)
	{
		if (!m_configured || !m_oscAddress)
		{
			std::cerr << "OscController: Not configured or no OSC address" << std::endl;
			return false;
		}

		// Create OSC message
		lo_message msg = lo_message_new();
		if (!msg)
		{
			std::cerr << "OscController: Failed to create OSC message" << std::endl;
			return false;
		}

		// Add arguments to message
		for (const auto &arg : args)
		{
			try
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
					// Booleans are often sent as T/F in OSC, but liblo uses int 0/1 or specific types
					// Check target device documentation. For RME, T/F or 1/0 might work.
					// Using int 1/0 is generally safer if T/F isn't explicitly supported by liblo add.
					// lo_message_add_true(msg) / lo_message_add_false(msg) could also be used.
					// RME seems to accept float 1.0/0.0 for many boolean parameters.
					lo_message_add_float(msg, std::any_cast<bool>(arg) ? 1.0f : 0.0f);
					// lo_message_add_int32(msg, std::any_cast<bool>(arg) ? 1 : 0); // Alternative
				}
				else if (arg.type() == typeid(std::string))
				{
					lo_message_add_string(msg, std::any_cast<const std::string &>(arg).c_str());
				}
				else if (arg.type() == typeid(const char *))
				{
					lo_message_add_string(msg, std::any_cast<const char *>(arg));
				}
				// Add other types as needed (double -> float, etc.)
				else
				{
					std::cerr << "OscController: Unsupported argument type: " << arg.type().name() << std::endl;
					lo_message_free(msg);
					return false;
				}
			}
			catch (const std::bad_any_cast &e)
			{
				std::cerr << "OscController: Bad any_cast sending command: " << e.what() << std::endl;
				lo_message_free(msg);
				return false;
			}
		}

		// Send the message
		int result = lo_send_message(m_oscAddress, address.c_str(), msg);
		lo_message_free(msg);

		if (result == -1)
		{
			std::cerr << "OscController: Failed to send OSC message to " << address << ": "
					  << lo_address_errstr(m_oscAddress) << std::endl;
			return false;
		}

		return true;
	}

	lo_bundle OscController::createBundle()
	{
		// Create bundle with immediate timestamp (LO_TT_IMMEDIATE)
		return lo_bundle_new(LO_TT_IMMEDIATE);
	}

	bool OscController::sendBatch(const std::vector<RmeOscCommandConfig> &commands)
	{
		if (!m_configured || !m_oscAddress)
		{
			std::cerr << "OscController: Not configured or no OSC address" << std::endl;
			return false;
		}

		// Create bundle
		lo_bundle bundle = createBundle();
		if (!bundle)
		{
			std::cerr << "OscController: Failed to create OSC bundle" << std::endl;
			return false;
		}

		// Add all commands to bundle
		for (const auto &cmd : commands)
		{
			lo_message msg = lo_message_new();
			if (!msg)
			{
				std::cerr << "OscController: Failed to create OSC message for batch" << std::endl;
				lo_bundle_free_recursive(bundle); // Use recursive free for bundles
				return false;
			}

			// Add arguments to message
			for (const auto &arg : cmd.args)
			{
				try
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
						lo_message_add_float(msg, std::any_cast<bool>(arg) ? 1.0f : 0.0f);
					}
					else if (arg.type() == typeid(std::string))
					{
						lo_message_add_string(msg, std::any_cast<const std::string &>(arg).c_str());
					}
					else if (arg.type() == typeid(const char *))
					{
						lo_message_add_string(msg, std::any_cast<const char *>(arg));
					}
					else
					{
						std::cerr << "OscController: Unsupported argument type in batch: " << arg.type().name() << std::endl;
						lo_message_free(msg);
						lo_bundle_free_recursive(bundle);
						return false;
					}
				}
				catch (const std::bad_any_cast &e)
				{
					std::cerr << "OscController: Bad any_cast sending batch: " << e.what() << std::endl;
					lo_message_free(msg);
					lo_bundle_free_recursive(bundle);
					return false;
				}
			}

			// Add message to bundle
			lo_bundle_add_message(bundle, cmd.address.c_str(), msg);
			// Note: lo_bundle_add_message copies the message, so we don't need to free msg here
			// However, the original code frees it, which is also acceptable if less efficient.
			// Let's stick to the original pattern for consistency unless issues arise.
			lo_message_free(msg);
		}

		// Send the bundle
		int result = lo_send_bundle(m_oscAddress, bundle);
		lo_bundle_free_recursive(bundle); // Use recursive free

		if (result == -1)
		{
			std::cerr << "OscController: Failed to send OSC bundle: "
					  << lo_address_errstr(m_oscAddress) << std::endl;
			return false;
		}

		return true;
	}

	bool OscController::querySingleValue(const std::string &address, float &outValue, int timeoutMs)
	{
		if (!m_configured || !m_oscAddress)
		{
			std::cerr << "OscController: OSC address not configured" << std::endl;
			return false;
		}

		if (!m_oscServer)
		{
			std::cerr << "OscController: OSC server not started, cannot receive responses" << std::endl;
			return false;
		}

		// TODO: Implement robust query mechanism
		// This requires associating the response message with the query.
		// Options:
		// 1. Use a map with unique IDs or the address itself.
		// 2. Have the callback store the result in a shared structure protected by a mutex.
		// 3. Use liblo's request/reply features if available/suitable.

		// Placeholder implementation - sends request but doesn't wait/parse response correctly.
		m_pendingResponses[address].store(false); // Mark as pending

		// Send an empty message to request the value (standard OSC query pattern)
		int sendResult = sendCommand(address, {});

		if (!sendResult)
		{
			std::cerr << "OscController: Failed to send query to " << address << std::endl;
			m_pendingResponses.erase(address);
			return false;
		}

		// Wait for response with timeout (Basic polling)
		auto startTime = std::chrono::steady_clock::now();
		while (!m_pendingResponses[address].load())
		{
			// Check if timeout has elapsed
			auto elapsed = std::chrono::steady_clock::now() - startTime;
			if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > timeoutMs)
			{
				std::cerr << "OscController: Timeout waiting for response to " << address << std::endl;
				m_pendingResponses.erase(address);
				return false; // Timeout
			}

			// Yield or sleep briefly to avoid busy-waiting
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}

		// --- This part is missing the crucial step of retrieving the value ---
		// The value would have been received by handleOscMessage and needs to be
		// stored somewhere accessible here (e.g., another map or a shared variable
		// associated with the pending response).
		// For now, we just return true if a response flag was set.
		// outValue = ???; // Retrieve the actual value received by the callback

		m_pendingResponses.erase(address); // Clean up pending flag
		std::cerr << "OscController: Received response flag for " << address << ", but value retrieval not implemented." << std::endl;
		// return true; // Return true indicating a response *flag* was received
		return false; // Return false as value retrieval is not implemented
	}

	// --- RME Specific Helper Methods ---
	// These methods construct RME-specific OSC addresses and values.
	// They rely on the enums and clamp helpers defined in the header.

	std::string OscController::buildChannelAddress(ChannelType type, int channel, ParamType param)
	{
		// Channel numbers in OSC are 1-based for RME
		if (channel < 1)
		{
			std::cerr << "OscController: Invalid channel number (must be >= 1)" << std::endl;
			return "";
		}

		// RME OSC addresses typically start with /<device_index>/... (e.g., /1/...)
		std::string prefix = "/1/"; // Assuming device 1

		std::string typeStr;
		switch (type)
		{
		case ChannelType::INPUT:
			typeStr = "input";
			break;
		case ChannelType::PLAYBACK:
			typeStr = "playback";
			break;
		case ChannelType::OUTPUT:
			typeStr = "output";
			break;
		default:
			return ""; // Invalid type
		}

		std::string paramStr;
		// Map ParamType enum to RME OSC parameter names
		switch (param)
		{
		case ParamType::VOLUME:
			paramStr = "volume";
			break; // Applies to all types
		case ParamType::MUTE:
			paramStr = "mute";
			break;
		case ParamType::STEREO:
			paramStr = "stereo";
			break;
		case ParamType::PAN:
			paramStr = "pan";
			break; // RME uses 'pan' for all types now
		case ParamType::PHASE:
			paramStr = "phaseL";
			break; // RME uses phaseL/phaseR for stereo
		case ParamType::FX_SEND:
			paramStr = "fxSend";
			break; // Check RME docs for exact FX send path
		case ParamType::GAIN:
			if (type != ChannelType::INPUT)
				return "";
			paramStr = "gain";
			break; // Mic Pre Gain
		case ParamType::EQ_ENABLE:
			paramStr = "eq/enable";
			break; // EQ is often grouped
		case ParamType::PHANTOM_POWER:
			if (type != ChannelType::INPUT)
				return "";
			paramStr = "phantomPower";
			break; // RME uses 'phantomPower'
		case ParamType::HI_Z:
			if (type != ChannelType::INPUT)
				return "";
			paramStr = "instrument";
			break; // RME uses 'instrument' for Hi-Z
		case ParamType::DYN_ENABLE:
			paramStr = "dynamics/enable";
			break; // Dynamics often grouped
		// EQ bands need special handling within the function or dedicated methods
		case ParamType::EQ_BAND1_FREQ:
			paramStr = "eq/band1/freq";
			break;
		case ParamType::EQ_BAND1_GAIN:
			paramStr = "eq/band1/gain";
			break;
		case ParamType::EQ_BAND1_Q:
			paramStr = "eq/band1/q";
			break;
		case ParamType::EQ_BAND1_TYPE:
			paramStr = "eq/band1/type";
			break;
		// Add cases for other EQ bands (2, 3, ...) and other params
		default:
			std::cerr << "OscController: Unsupported parameter type for address building: " << static_cast<int>(param) << std::endl;
			return "";
		}

		return prefix + typeStr + "/" + std::to_string(channel) + "/" + paramStr;
	}

	float OscController::dbToNormalized(float db) const
	{
		// RME Volume/Gain often uses a 0.0 to 1.0 float scale internally in OSC messages.
		// The mapping depends on the specific parameter.
		// For Volume (-65dB to +6dB):
		float range = 71.0f; // 6 - (-65)
		float norm = (clampVolumeDb(db) + 65.0f) / range;
		return std::max(0.0f, std::min(1.0f, norm)); // Clamp to 0-1
	}

	float OscController::normalizedToDb(float normalized) const
	{
		// Inverse of dbToNormalized for Volume
		if (normalized <= 0.0f)
		{
			return -INFINITY; // Or -65.0f depending on representation
		}
		// Map from 0..1 to -65..+6 dB
		return (normalized * 71.0f) - 65.0f;
	}

	bool OscController::setMatrixCrosspointGain(int hw_input, int hw_output, float gain_db)
	{
		// RME Matrix Crosspoint Address: /<device>/matrix/<output_type>/<output_channel>/<input_type>/<input_channel>
		// Example: /1/matrix/output/1/input/1 for HW Out 1 <- HW In 1
		// Gain is typically float 0.0 (off) to 1.0 (+6dB), mapping from dB needed.
		std::string address = "/1/matrix/output/" + std::to_string(hw_output) + "/input/" + std::to_string(hw_input);
		float gain_norm = dbToNormalized(gain_db); // Use the volume mapping for now
		return sendCommand(address, {std::any(gain_norm)});
	}

	bool OscController::setChannelMute(ChannelType type, int channel, bool mute)
	{
		std::string address = buildChannelAddress(type, channel, ParamType::MUTE);
		if (address.empty())
			return false;
		// RME expects float 1.0f (muted) or 0.0f (unmuted)
		return sendCommand(address, {std::any(mute ? 1.0f : 0.0f)});
	}

	bool OscController::setChannelStereo(ChannelType type, int channel, bool stereo)
	{
		std::string address = buildChannelAddress(type, channel, ParamType::STEREO);
		if (address.empty())
			return false;
		// RME expects float 1.0f (linked) or 0.0f (unlinked)
		return sendCommand(address, {std::any(stereo ? 1.0f : 0.0f)});
	}

	bool OscController::setChannelVolume(ChannelType type, int channel, float volume_db)
	{
		std::string address = buildChannelAddress(type, channel, ParamType::VOLUME);
		if (address.empty())
			return false;
		float volume_norm = dbToNormalized(volume_db);
		return sendCommand(address, {std::any(volume_norm)});
	}

	bool OscController::setInputPhantomPower(int channel, bool enabled)
	{
		std::string address = buildChannelAddress(ChannelType::INPUT, channel, ParamType::PHANTOM_POWER);
		if (address.empty())
			return false;
		// RME expects float 1.0f (on) or 0.0f (off)
		return sendCommand(address, {std::any(enabled ? 1.0f : 0.0f)});
	}

	bool OscController::setInputHiZ(int channel, bool enabled)
	{
		// RME uses "instrument" parameter
		std::string address = buildChannelAddress(ChannelType::INPUT, channel, ParamType::HI_Z);
		if (address.empty())
			return false;
		// RME expects float 1.0f (on) or 0.0f (off)
		return sendCommand(address, {std::any(enabled ? 1.0f : 0.0f)});
	}

	bool OscController::setChannelEQ(ChannelType type, int channel, bool enabled)
	{
		std::string address = buildChannelAddress(type, channel, ParamType::EQ_ENABLE);
		if (address.empty())
			return false;
		// RME expects float 1.0f (on) or 0.0f (off)
		return sendCommand(address, {std::any(enabled ? 1.0f : 0.0f)});
	}

	bool OscController::setChannelEQBand(ChannelType type, int channel, int band, float freq, float gain, float q)
	{
		// Clamp EQ parameters to valid ranges (using RME specific clamps)
		float clamped_freq = clampEQFreq(freq);
		float clamped_gain = clampEQGain(gain);
		float clamped_q = clampEQQ(q);

		// Build base address
		std::string baseAddr;
		switch (type)
		{
		case ChannelType::INPUT:
			baseAddr = "/1/input/" + std::to_string(channel) + "/eq/band" + std::to_string(band);
			break;
		case ChannelType::PLAYBACK:
			baseAddr = "/1/playback/" + std::to_string(channel) + "/eq/band" + std::to_string(band);
			break;
		case ChannelType::OUTPUT:
			baseAddr = "/1/output/" + std::to_string(channel) + "/eq/band" + std::to_string(band);
			break;
		default:
			return false;
		}

		// RME expects specific value ranges/types for EQ params
		// Freq: float Hz
		// Gain: float dB
		// Q: float
		// Type: int (e.g., 0=low shelf, 1=peak, 2=high shelf, ...) - Not set here

		std::vector<RmeOscCommandConfig> commands;
		commands.push_back({baseAddr + "/freq", {std::any(clamped_freq)}});
		commands.push_back({baseAddr + "/gain", {std::any(clamped_gain)}});
		commands.push_back({baseAddr + "/q", {std::any(clamped_q)}});

		return sendBatch(commands);
	}

	bool OscController::queryChannelVolume(ChannelType type, int channel, float &volume_db)
	{
		std::string address = buildChannelAddress(type, channel, ParamType::VOLUME);
		if (address.empty())
			return false;

		float normalized_volume = 0.0f;
		// Use querySingleValue (which needs full implementation)
		if (!querySingleValue(address, normalized_volume, 500))
		{
			return false;
		}

		// Convert normalized volume back to dB
		volume_db = normalizedToDb(normalized_volume);
		return true; // Will be false until querySingleValue is fully implemented
	}

	bool OscController::requestRefresh()
	{
		// RME uses /1/refresh command
		std::cout << "Requesting complete refresh from RME device via OSC..." << std::endl;
		bool result = sendCommand("/1/refresh", {});

		// Optionally wait briefly for device to respond
		if (result)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
		return result;
	}

	bool OscController::refreshConnectionStatus()
	{
		// Send a harmless query to check if the device responds
		// Querying DSP load is a good option
		std::string address = "/1/busLoad"; // RME path for DSP load
		// Use querySingleValue or just send and check for timeout error
		// For now, just send and assume success if no immediate error
		bool result = sendCommand(address, {});

		if (result)
		{
			// std::cout << "OSC connection check sent successfully." << std::endl;
			// A more robust check would involve waiting for a response or error
		}
		else
		{
			std::cerr << "OSC connection check failed to send." << std::endl;
		}
		return result; // This only indicates successful send, not actual connection status
	}

	bool OscController::applyConfiguration(const Configuration &config)
	{
		if (!m_configured || !m_oscAddress)
		{
			std::cerr << "OscController: Not configured or no OSC address" << std::endl;
			return false;
		}

		// We'll send the commands as a bundle for better performance
		if (config.rmeCommands.empty())
		{
			std::cerr << "OscController: No commands in configuration to apply" << std::endl;
			return true; // Not an error, just nothing to do
		}

		// Send the RME commands in batches
		const size_t MAX_BATCH_SIZE = 50; // Avoid overflowing message buffers

		// Calculate number of batches needed
		size_t totalCommands = config.rmeCommands.size();
		size_t batchCount = (totalCommands + MAX_BATCH_SIZE - 1) / MAX_BATCH_SIZE;

		bool success = true;
		for (size_t batchIndex = 0; batchIndex < batchCount; batchIndex++)
		{
			// Calculate start and end indices for this batch
			size_t startIdx = batchIndex * MAX_BATCH_SIZE;
			size_t endIdx = std::min(startIdx + MAX_BATCH_SIZE, totalCommands);

			// Create a vector containing just this batch of commands
			std::vector<RmeOscCommandConfig> batch(
				config.rmeCommands.begin() + startIdx,
				config.rmeCommands.begin() + endIdx);

			// Send the batch
			if (!sendBatch(batch))
			{
				std::cerr << "OscController: Failed to send command batch "
						  << (batchIndex + 1) << " of " << batchCount << std::endl;
				success = false;
			}

			// Small delay between batches to avoid overwhelming the device
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}

		return success;
	}

	// Add methods to get target IP and port for DeviceStateManager
	std::string OscController::getTargetIp() const
	{
		return m_targetIp;
	}

	int OscController::getTargetPort() const
	{
		return m_targetPort;
	}

} // namespace AudioEngine
