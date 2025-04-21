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
#include <sstream>
#include <fstream>

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

		// Check if this is a response to a parameter request
		{
			std::lock_guard<std::mutex> lock(m_callbackMutex);
			auto it = m_parameterCallbacks.find(pathStr);
			if (it != m_parameterCallbacks.end())
			{
				// Call the parameter callback with the received value
				it->second(true, args);

				// Remove the callback after it's called
				m_parameterCallbacks.erase(it);

				// Continue processing (don't return yet)
			}
		}

		// Call all registered event callbacks
		{
			std::lock_guard<std::mutex> lock(m_callbackMutex);
			for (const auto &[id, callback] : m_eventCallbacks)
			{
				callback(pathStr, args);
			}
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

		// Create a promise/future for the response value
		std::promise<std::optional<float>> responsePromise;
		std::future<std::optional<float>> responseFuture = responsePromise.get_future();

		// Set up a callback to handle the response
		{
			std::lock_guard<std::mutex> lock(m_callbackMutex);
			m_parameterCallbacks[address] = [&responsePromise](bool success, const std::vector<std::any> &args)
			{
				if (success && !args.empty())
				{
					try
					{
						// Try to extract a float value from the first argument
						if (args[0].type() == typeid(float))
						{
							responsePromise.set_value(std::any_cast<float>(args[0]));
						}
						else if (args[0].type() == typeid(int))
						{
							responsePromise.set_value(static_cast<float>(std::any_cast<int>(args[0])));
						}
						else if (args[0].type() == typeid(double))
						{
							responsePromise.set_value(static_cast<float>(std::any_cast<double>(args[0])));
						}
						else
						{
							// Unsupported type
							responsePromise.set_value(std::nullopt);
						}
					}
					catch (const std::exception &e)
					{
						std::cerr << "OscController: Error parsing response value: " << e.what() << std::endl;
						responsePromise.set_value(std::nullopt);
					}
				}
				else
				{
					// No data or failure
					responsePromise.set_value(std::nullopt);
				}
			};
		}

		// Mark the address as pending a response
		m_pendingResponses[address].store(false);

		// Send query command (empty message to request current value)
		bool sendResult = sendCommand(address, {});
		if (!sendResult)
		{
			// If sending fails, remove the callback and return error
			std::lock_guard<std::mutex> lock(m_callbackMutex);
			m_parameterCallbacks.erase(address);
			m_pendingResponses.erase(address);
			return false;
		}

		// Wait for the response with timeout
		std::future_status status = responseFuture.wait_for(std::chrono::milliseconds(timeoutMs));

		// Clean up
		m_pendingResponses.erase(address);

		{
			std::lock_guard<std::mutex> lock(m_callbackMutex);
			m_parameterCallbacks.erase(address);
		}

		// Check result
		if (status == std::future_status::ready)
		{
			std::optional<float> result = responseFuture.get();
			if (result.has_value())
			{
				outValue = result.value();
				return true;
			}
		}
		else if (status == std::future_status::timeout)
		{
			std::cerr << "OscController: Timeout waiting for response to " << address << std::endl;
		}
		else
		{
			std::cerr << "OscController: Error waiting for response to " << address << std::endl;
		}

		return false;
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
		if (!m_configured || !m_oscAddress)
		{
			std::cerr << "OscController: Not configured or no OSC address" << std::endl;
			return false;
		}

		// Create a timeout flag
		std::atomic<bool> receivedResponse(false);
		std::atomic<bool> timedOut(false);

		// Register a one-time callback to handle the response
		int callbackId = addEventCallback([&receivedResponse](const std::string &path, const std::vector<std::any> &args)
										  {
			if (path == "/1/busLoad") {
				receivedResponse.store(true);
			} });

		// Send a request for DSP load (harmless query)
		bool sendResult = sendCommand("/1/busLoad", {});
		if (!sendResult)
		{
			removeEventCallback(callbackId);
			return false;
		}

		// Start a timeout thread
		std::thread timeoutThread([&timedOut, &receivedResponse]()
								  {
									  // Wait for response or timeout after 500ms
									  for (int i = 0; i < 50; i++)
									  {
										  if (receivedResponse.load())
										  {
											  return; // Response received, exit thread
										  }
										  std::this_thread::sleep_for(std::chrono::milliseconds(10));
									  }
									  timedOut.store(true); // Set timeout flag
								  });

		// Wait for timeout thread to finish
		timeoutThread.join();

		// Clean up event callback
		removeEventCallback(callbackId);

		// If we received a response before timeout, connection is active
		if (receivedResponse.load())
		{
			std::cout << "OSC connection active to " << m_targetIp << ":" << m_targetPort << std::endl;
			return true;
		}

		// If we timed out, connection may be down
		if (timedOut.load())
		{
			std::cerr << "OSC connection timed out to " << m_targetIp << ":" << m_targetPort << std::endl;
		}

		return false;
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

	// =================== IExternalControl Interface Implementation ===================

	bool OscController::configure(const std::string &configFile)
	{
		if (m_configured)
		{
			std::cerr << "OscController: Already configured. Call cleanup() first." << std::endl;
			return false;
		}

		// Parse JSON configuration file
		std::ifstream file(configFile);
		if (!file.is_open())
		{
			std::cerr << "OscController: Failed to open configuration file: " << configFile << std::endl;
			return false;
		}

		try
		{
			// Parse JSON using nlohmann::json
			nlohmann::json config = nlohmann::json::parse(file);
			file.close();

			// Extract configuration values with proper error handling
			std::string ip = "127.0.0.1"; // Default values
			int port = 7001;
			int receivePort = 9001;

			// Get IP address
			if (config.contains("rmeOscIp"))
			{
				ip = config["rmeOscIp"].get<std::string>();
			}
			else if (config.contains("oscIp"))
			{
				ip = config["oscIp"].get<std::string>();
			}
			else if (config.contains("ip"))
			{
				ip = config["ip"].get<std::string>();
			}

			// Get target port
			if (config.contains("rmeOscPort"))
			{
				port = config["rmeOscPort"].get<int>();
			}
			else if (config.contains("oscPort"))
			{
				port = config["oscPort"].get<int>();
			}
			else if (config.contains("port"))
			{
				port = config["port"].get<int>();
			}

			// Get receive port
			if (config.contains("oscReceivePort"))
			{
				receivePort = config["oscReceivePort"].get<int>();
			}
			else if (config.contains("receivePort"))
			{
				receivePort = config["receivePort"].get<int>();
			}

			// Configure with extracted parameters
			if (!configure(ip, port, receivePort))
			{
				return false;
			}

			std::cout << "OscController: Configured from file " << configFile
					  << " (IP: " << ip << ", Port: " << port
					  << ", Receive Port: " << receivePort << ")" << std::endl;

			return true;
		}
		catch (const nlohmann::json::exception &e)
		{
			std::cerr << "OscController: JSON parsing error: " << e.what() << std::endl;
			return false;
		}
		catch (const std::exception &e)
		{
			std::cerr << "OscController: Error configuring from file: " << e.what() << std::endl;
			return false;
		}
	}

	bool OscController::configure(const std::string &ip, int port, int receivePort)
	{
		if (!configure(ip, port))
		{
			return false;
		}

		// Start receiver if requested
		if (receivePort > 0)
		{
			if (!startReceiver(receivePort))
			{
				std::cerr << "OscController: Failed to start receiver on port " << receivePort << std::endl;
				// Don't fail configuration if just the receiver fails
				// Some use cases may not need receiving
			}
		}

		return true;
	}

	bool OscController::setParameter(const std::string &address, const std::vector<std::any> &args)
	{
		// Direct implementation using sendCommand
		return sendCommand(address, args);
	}

	bool OscController::getParameter(const std::string &address,
									 std::function<void(bool, const std::vector<std::any> &)> callback)
	{
		if (!m_configured || !m_oscAddress)
		{
			std::cerr << "OscController: Not configured or no OSC address" << std::endl;
			return false;
		}

		if (!m_oscServer)
		{
			std::cerr << "OscController: OSC server not started, cannot receive responses" << std::endl;
			return false;
		}

		// Store the callback for this parameter request
		{
			std::lock_guard<std::mutex> lock(m_callbackMutex);
			m_parameterCallbacks[address] = callback;
		}

		// Send query command (empty message to request current value)
		bool result = sendCommand(address, {});

		if (!result)
		{
			// If sending fails, remove the callback and return error
			std::lock_guard<std::mutex> lock(m_callbackMutex);
			m_parameterCallbacks.erase(address);
			return false;
		}

		return true;
	}

	bool OscController::queryDeviceState(std::function<void(bool, const Configuration &)> callback)
	{
		if (!m_configured || !m_oscAddress)
		{
			std::cerr << "OscController: Not configured or no OSC address" << std::endl;
			callback(false, Configuration());
			return false;
		}

		// Request a refresh from the device to ensure state is current
		bool refreshResult = requestRefresh();
		if (!refreshResult)
		{
			std::cerr << "OscController: Failed to request device state refresh" << std::endl;
		}

		// Create a state collection thread that will query device parameters
		// and construct a Configuration object
		std::thread([this, callback]()
					{
			// Create configuration object with basic device info
			Configuration config;
			config.targetIp = m_targetIp;
			config.targetPort = m_targetPort;

			// Populate device information
			bool success = true;

			// Query DSP status
			auto dspStatus = getDspStatus();
			config.dspLoadPercent = dspStatus.loadPercent;

			// Example: Query key channel parameters (extend as needed)
			std::vector<RmeOscCommandConfig> commands;

			// Query channel parameters for a subset of channels (extend based on needs)
			const int maxChannelsToQuery = 8; // Limit queries to avoid too much traffic

			// Input channels
			for (int i = 1; i <= maxChannelsToQuery; i++)
			{
				float volumeDb = 0.0f;
				if (queryChannelVolume(ChannelType::INPUT, i, volumeDb))
				{
					RmeOscCommandConfig cmd;
					cmd.address = buildChannelAddress(ChannelType::INPUT, i, ParamType::VOLUME);
					cmd.args.push_back(std::any(dbToNormalized(volumeDb)));
					commands.push_back(cmd);
				}
				else
				{
					success = false;
				}

				// Add small delay between queries to avoid overwhelming the device
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			}

			// Playback channels
			for (int i = 1; i <= maxChannelsToQuery; i++)
			{
				float volumeDb = 0.0f;
				if (queryChannelVolume(ChannelType::PLAYBACK, i, volumeDb))
				{
					RmeOscCommandConfig cmd;
					cmd.address = buildChannelAddress(ChannelType::PLAYBACK, i, ParamType::VOLUME);
					cmd.args.push_back(std::any(dbToNormalized(volumeDb)));
					commands.push_back(cmd);
				}
				else
				{
					success = false;
				}

				// Add small delay between queries
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			}

			// Output channels
			for (int i = 1; i <= maxChannelsToQuery; i++)
			{
				float volumeDb = 0.0f;
				if (queryChannelVolume(ChannelType::OUTPUT, i, volumeDb))
				{
					RmeOscCommandConfig cmd;
					cmd.address = buildChannelAddress(ChannelType::OUTPUT, i, ParamType::VOLUME);
					cmd.args.push_back(std::any(dbToNormalized(volumeDb)));
					commands.push_back(cmd);
				}
				else
				{
					success = false;
				}

				// Add small delay between queries
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			}

			// Store the commands in the configuration
			config.rmeCommands = commands;

			// Get device name if available (may require specific query)
			config.deviceName = "RME Audio Device"; // Placeholder - ideally this would be queried
			config.deviceType = "TotalMix FX";     // Placeholder

			// Call the callback with the result
			callback(success, config); })
			.detach();

		return true;
	}

	int OscController::addEventCallback(
		std::function<void(const std::string &, const std::vector<std::any> &)> callback)
	{
		std::lock_guard<std::mutex> lock(m_callbackMutex);
		int callbackId = m_nextCallbackId++;
		m_eventCallbacks[callbackId] = callback;
		return callbackId;
	}

	void OscController::removeEventCallback(int callbackId)
	{
		std::lock_guard<std::mutex> lock(m_callbackMutex);
		m_eventCallbacks.erase(callbackId);
	}

	// =================== Standalone Operation ===================

	int OscController::main(int argc, char *argv[])
	{
		std::cout << "OSC Controller Standalone Mode" << std::endl;
		std::cout << "================================" << std::endl;

		// Parse command line arguments
		std::string configFile;
		std::string targetIp = "127.0.0.1";
		int targetPort = 7001;
		int receivePort = 9001;
		bool interactive = true;

		// Simple command line parsing
		for (int i = 1; i < argc; i++)
		{
			std::string arg = argv[i];
			if (arg == "--config" && i + 1 < argc)
			{
				configFile = argv[++i];
			}
			else if (arg == "--ip" && i + 1 < argc)
			{
				targetIp = argv[++i];
			}
			else if (arg == "--port" && i + 1 < argc)
			{
				try
				{
					targetPort = std::stoi(argv[++i]);
				}
				catch (const std::exception &)
				{
					std::cerr << "Invalid port number: " << argv[i] << std::endl;
					return 1;
				}
			}
			else if (arg == "--receive-port" && i + 1 < argc)
			{
				try
				{
					receivePort = std::stoi(argv[++i]);
				}
				catch (const std::exception &)
				{
					std::cerr << "Invalid receive port number: " << argv[i] << std::endl;
					return 1;
				}
			}
			else if (arg == "--daemon")
			{
				interactive = false;
			}
			else if (arg == "--help")
			{
				std::cout << "Usage: osc_controller [options]" << std::endl;
				std::cout << "Options:" << std::endl;
				std::cout << "  --config <file>        Configuration file (JSON)" << std::endl;
				std::cout << "  --ip <address>         Target IP address (default: 127.0.0.1)" << std::endl;
				std::cout << "  --port <port>          Target port number (default: 7001)" << std::endl;
				std::cout << "  --receive-port <port>  Receiving port number (default: 9001)" << std::endl;
				std::cout << "  --daemon               Run in daemon mode (non-interactive)" << std::endl;
				std::cout << "  --help                 Show this help" << std::endl;
				return 0;
			}
		}

		// Create and configure controller
		OscController controller;

		if (!configFile.empty())
		{
			if (!controller.configure(configFile))
			{
				std::cerr << "Failed to load configuration from " << configFile << std::endl;
				return 1;
			}
		}
		else
		{
			if (!controller.configure(targetIp, targetPort, receivePort))
			{
				std::cerr << "Failed to configure OSC controller" << std::endl;
				return 1;
			}
		}

		// Set message callback for logging received messages
		controller.setMessageCallback([](const std::string &path, const std::vector<std::any> &args)
									  {
			std::cout << "Received OSC message: " << path;
			if (!args.empty())
			{
				std::cout << " with " << args.size() << " argument(s)";
			}
			std::cout << std::endl; });

		// Interactive mode with command loop
		if (interactive)
		{
			std::cout << "Enter commands (type 'help' for available commands, 'quit' to exit):" << std::endl;
			std::string line;

			while (true)
			{
				std::cout << "> ";
				std::getline(std::cin, line);

				if (line == "quit" || line == "exit")
				{
					break;
				}
				else if (line == "help")
				{
					std::cout << "Available commands:" << std::endl;
					std::cout << "  send <address> <arg1> <arg2> ...   Send OSC message" << std::endl;
					std::cout << "  vol <channel> <value>              Set output channel volume (dB)" << std::endl;
					std::cout << "  mute <channel> <0|1>               Set output channel mute" << std::endl;
					std::cout << "  refresh                            Request device state refresh" << std::endl;
					std::cout << "  meters <0|1>                       Enable/disable level meters" << std::endl;
					std::cout << "  status                             Show connection status" << std::endl;
					std::cout << "  quit                               Exit the program" << std::endl;
					std::cout << "  help                               Show this help" << std::endl;
				}
				else if (line.find("send ") == 0)
				{
					// Parse address and arguments
					std::stringstream ss(line.substr(5));
					std::string address;
					ss >> address;

					std::vector<std::any> args;
					std::string arg;
					while (ss >> arg)
					{
						// Try to parse as different types
						try
						{
							// Check if it's a float (contains decimal point)
							if (arg.find('.') != std::string::npos)
							{
								args.push_back(std::any(std::stof(arg)));
							}
							// Check if it's a boolean
							else if (arg == "true" || arg == "1")
							{
								args.push_back(std::any(true));
							}
							else if (arg == "false" || arg == "0")
							{
								args.push_back(std::any(false));
							}
							// Otherwise try as integer
							else
							{
								args.push_back(std::any(std::stoi(arg)));
							}
						}
						catch (const std::exception &)
						{
							// If not a number, treat as string
							args.push_back(std::any(arg));
						}
					}

					// Send the command
					bool result = controller.sendCommand(address, args);
					std::cout << "Send result: " << (result ? "success" : "failed") << std::endl;
				}
				else if (line.find("vol ") == 0)
				{
					// Parse channel and volume
					std::stringstream ss(line.substr(4));
					int channel;
					float volume;
					ss >> channel >> volume;

					bool result = controller.setChannelVolume(OscController::ChannelType::OUTPUT, channel, volume);
					std::cout << "Set volume result: " << (result ? "success" : "failed") << std::endl;
				}
				else if (line.find("mute ") == 0)
				{
					// Parse channel and mute state
					std::stringstream ss(line.substr(5));
					int channel;
					int mute;
					ss >> channel >> mute;

					bool result = controller.setChannelMute(OscController::ChannelType::OUTPUT, channel, mute != 0);
					std::cout << "Set mute result: " << (result ? "success" : "failed") << std::endl;
				}
				else if (line == "refresh")
				{
					bool result = controller.requestRefresh();
					std::cout << "Refresh result: " << (result ? "success" : "failed") << std::endl;
				}
				else if (line.find("meters ") == 0)
				{
					// Parse enable state
					std::stringstream ss(line.substr(7));
					int enable;
					ss >> enable;

					bool result = controller.enableLevelMeters(enable != 0);
					std::cout << "Set meters result: " << (result ? "success" : "failed") << std::endl;
				}
				else if (line == "status")
				{
					bool result = controller.refreshConnectionStatus();
					std::cout << "Connection status: " << (result ? "OK" : "Failed") << std::endl;
					std::cout << "Target: " << controller.getTargetIp() << ":" << controller.getTargetPort() << std::endl;

					auto status = controller.getDspStatus();
					std::cout << "DSP Load: " << status.loadPercent << "%" << std::endl;
				}
				else if (!line.empty())
				{
					std::cout << "Unknown command. Type 'help' for available commands." << std::endl;
				}
			}
		}
		else // Daemon mode
		{
			std::cout << "Running in daemon mode. Press Ctrl+C to exit." << std::endl;

			// Set up level meter callback
			controller.setLevelMeterCallback([](OscController::ChannelType type, int channel, const OscController::LevelMeterData &data)
											 {
												 // Optional: log level data in daemon mode
											 });

			// Enable level meters
			controller.enableLevelMeters(true);

			// Just wait until terminated
			while (true)
			{
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
		}

		// Clean up
		controller.cleanup();
		std::cout << "OSC Controller terminated." << std::endl;
		return 0;
	}

} // namespace AudioEngine
