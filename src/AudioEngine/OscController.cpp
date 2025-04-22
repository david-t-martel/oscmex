#include "OscController.h"
#include "vendor/lo/lo_cpp.h"
#include "Configuration.h"

#include <iostream>
#include <cmath>
#include <thread>
#include <chrono>
#include <functional>
#include <algorithm>
#include <map>
#include <vector>
#include <any>
#include <sstream>
#include <fstream>
#include <atomic>
#include <signal.h>

using json = nlohmann::json;

namespace AudioEngine
{
	// Signal handler for standalone mode
	static std::atomic<bool> g_running(true);

	static void signal_handler(int sig)
	{
		g_running = false;
	}

	// Utility function to convert log levels to dB
	static inline float logToDB(float value)
	{
		// Prevent log(0)
		if (value <= 0)
			return -144.0f; // approx. silence level

		return 20.0f * log10f(value);
	}

	OscController::OscController()
		: m_oscAddress(nullptr), m_oscServer(nullptr),
		  m_messageCallback(nullptr), m_levelCallback(nullptr),
		  m_targetIp("127.0.0.1"), m_targetPort(0), m_receivePort(0),
		  m_running(false), m_nextCallbackId(1), m_configured(false)
	{
		// Constructor is minimal, initialization happens in configure()
	}

	OscController::~OscController()
	{
		// Clean up liblo resources
		cleanup();
		if (m_running)
		{
			stopListener();
		}
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

	// IExternalControl interface implementation
	bool OscController::configure(const std::string &targetIp, int targetPort, int receivePort)
	{
		if (m_configured)
		{
			cleanup();
		}

		m_targetIp = targetIp;
		m_targetPort = targetPort;
		m_receivePort = receivePort;

		// Create OSC address for sending
		m_oscAddress = lo_address_new(targetIp.c_str(), std::to_string(targetPort).c_str());
		if (!m_oscAddress)
		{
			std::cerr << "OscController: Failed to create OSC address for " << targetIp << ":" << targetPort << std::endl;
			return false;
		}

		// Set UDP transmission timeout (default is -1 which can hang indefinitely)
		lo_address_set_ttl(m_oscAddress, 4); // Reasonable TTL for local network

		// Configure timeout to prevent blocking for too long
		lo_address_set_timeout_ms(m_oscAddress, 500); // 500ms timeout

		m_configured = true;
		std::cout << "OSC Controller configured for " << targetIp << ":" << targetPort << std::endl;

		// Start receiver if required
		if (receivePort > 0)
		{
			if (!startReceiver(receivePort))
			{
				std::cerr << "Warning: Failed to start OSC receiver on port " << receivePort << std::endl;
				// Don't fail configuration if just the receiver fails
			}
		}

		return true;
	}

	bool OscController::configure(const Configuration &config)
	{
		// Extract network parameters from configuration
		std::string targetIp = config.getTargetIp();
		int targetPort = config.getTargetPort();
		int receivePort = config.getReceivePort();

		return configure(targetIp, targetPort, receivePort);
	}

	bool OscController::configure(const std::string &configFile)
	{
		if (m_configured)
		{
			cleanup();
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
			// Parse JSON
			json config = json::parse(file);
			file.close();

			// Extract configuration values with proper error handling
			std::string ip = "127.0.0.1"; // Default values
			int port = 7001;
			int receivePort = 9001;

			// Get IP address
			if (config.contains("targetIp"))
			{
				ip = config["targetIp"].get<std::string>();
			}
			else if (config.contains("ip"))
			{
				ip = config["ip"].get<std::string>();
			}

			// Get target port
			if (config.contains("targetPort"))
			{
				port = config["targetPort"].get<int>();
			}
			else if (config.contains("port"))
			{
				port = config["port"].get<int>();
			}

			// Get receive port
			if (config.contains("receivePort"))
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
		catch (const json::exception &e)
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

	bool OscController::startReceiver(int receivePort)
	{
		if (m_oscServer)
		{
			std::cerr << "OscController: OSC receiver already started" << std::endl;
			return false;
		}

		m_receivePort = receivePort;

		// Create OSC server for receiving
		m_oscServer = lo_server_thread_new(std::to_string(receivePort).c_str(),
										   [](int num, const char *msg, const char *path)
										   {
											   std::cerr << "OSC error " << num << ": " << msg << " (" << (path ? path : "null") << ")" << std::endl;
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
				args.push_back(std::any(std::string(&(argv[i]->s))));
				break;
			case LO_TRUE:
				args.push_back(std::any(true));
				break;
			case LO_FALSE:
				args.push_back(std::any(false));
				break;
			// Handle other OSC types as needed
			default:
				break;
			}
		}

		std::string pathStr = path ? path : ""; // Ensure path is not null

		// Call the registered general message callback if available
		if (m_messageCallback)
		{
			// Invoke the callback with the parsed message path and arguments
			m_messageCallback(pathStr, args);
		}

		// Check if this is a response to a pending query
		{
			std::lock_guard<std::mutex> lock(m_queryMutex);
			auto it = m_pendingQueries.find(pathStr);
			if (it != m_pendingQueries.end())
			{
				// Call the parameter callback with the received value
				it->second(true, args);

				// Remove the callback after it's called
				m_pendingQueries.erase(it);
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

	bool OscController::setParameter(const std::string &address, const std::vector<std::any> &args)
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

	bool OscController::getParameter(const std::string &address,
									 std::function<void(bool, const std::vector<std::any> &)> callback)
	{
		// Register the callback for this address
		{
			std::lock_guard<std::mutex> lock(m_queryMutex);
			m_pendingQueries[address] = callback;
		}

		// Send query message (typically address with no args or a specific query flag)
		std::vector<std::any> args;
		return setParameter(address, args);
	}

	bool OscController::applyConfiguration(const Configuration &config)
	{
		if (!m_configured || !m_oscAddress)
		{
			std::cerr << "OscController: Not configured or no OSC address" << std::endl;
			return false;
		}

		// Apply all commands in the configuration
		bool allSuccess = true;
		for (const auto &cmd : config.getCommands())
		{
			if (!setParameter(cmd.address, cmd.args))
			{
				allSuccess = false;
			}

			// Add small delay between commands to avoid overwhelming the device
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}

		return allSuccess;
	}

	bool OscController::queryDeviceState(std::function<void(bool, const Configuration &)> callback)
	{
		if (!m_configured || !m_oscAddress)
		{
			std::cerr << "OscController: Not configured or no OSC address" << std::endl;
			callback(false, Configuration());
			return false;
		}

		// Launch query in separate thread to avoid blocking
		std::thread([this, callback]()
					{
			// Request a refresh to ensure we get current state
			setParameter("/refresh", {});

			// Allow time for the device to process the refresh
			std::this_thread::sleep_for(std::chrono::milliseconds(300));

			// Create a configuration to represent device state
			Configuration config;
			config.setConnectionParams(m_targetIp, m_targetPort, m_receivePort);

			// In an actual implementation, we would query various parameters
			// For now, we'll provide a minimal example
			bool success = true;

			// Query sample rate if supported
			float sampleRate = 0.0f;
			if (querySingleValue("/sample_rate", sampleRate))
			{
				config.setSampleRate(static_cast<double>(sampleRate));
			}

			// Query buffer size if supported
			float bufferSize = 0.0f;
			if (querySingleValue("/buffer_size", bufferSize))
			{
				config.setBufferSize(static_cast<long>(bufferSize));
			}

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

	bool OscController::querySingleValue(const std::string &address, float &value, int timeoutMs)
	{
		std::atomic<bool> gotResponse(false);
		std::atomic<float> resultValue(0.0f);

		// Create a callback for the query
		auto callback = [&gotResponse, &resultValue](bool success, const std::vector<std::any> &args)
		{
			if (success && !args.empty())
			{
				try
				{
					if (args[0].type() == typeid(float))
					{
						resultValue = std::any_cast<float>(args[0]);
						gotResponse = true;
					}
					else if (args[0].type() == typeid(int))
					{
						resultValue = static_cast<float>(std::any_cast<int>(args[0]));
						gotResponse = true;
					}
					else if (args[0].type() == typeid(bool))
					{
						resultValue = std::any_cast<bool>(args[0]) ? 1.0f : 0.0f;
						gotResponse = true;
					}
				}
				catch (const std::bad_any_cast &)
				{
					// Type conversion error
				}
			}
		};

		// Send the query
		if (!getParameter(address, callback))
		{
			return false;
		}

		// Wait for response with timeout
		int waited = 0;
		int sleepMs = 10;

		while (!gotResponse && waited < timeoutMs)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
			waited += sleepMs;
		}

		if (gotResponse)
		{
			value = resultValue;
			return true;
		}

		return false;
	}

	bool OscController::startListener()
	{
		if (m_running)
		{
			return true; // Already running
		}

		if (m_receivePort <= 0)
		{
			std::cerr << "OscController: No receive port specified for listener" << std::endl;
			return false;
		}

		// Start the receiver if not already started
		if (!m_oscServer && !startReceiver(m_receivePort))
		{
			return false;
		}

		m_running = true;
		return true;
	}

	void OscController::stopListener()
	{
		if (!m_running)
		{
			return;
		}

		m_running = false;
		stopReceiver();
	}

	bool OscController::requestRefresh()
	{
		// Generic refresh command - device-specific implementations might override this
		return setParameter("/refresh", {});
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
			// Check for any response
			receivedResponse.store(true); });

		// Send a simple query
		bool sendResult = setParameter("/status", {});
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

	// Add getters for target IP and port
	std::string OscController::getTargetIp() const
	{
		return m_targetIp;
	}

	int OscController::getTargetPort() const
	{
		return m_targetPort;
	}

	int OscController::getReceivePort() const
	{
		return m_receivePort;
	}

	// Static main method for standalone operation
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
			else if ((arg == "--ip" || arg == "-i") && i + 1 < argc)
			{
				targetIp = argv[++i];
			}
			else if ((arg == "--port" || arg == "-p") && i + 1 < argc)
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
			else if ((arg == "--receive-port" || arg == "-r") && i + 1 < argc)
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
			else if (arg == "--daemon" || arg == "-d")
			{
				interactive = false;
			}
			else if (arg == "--help" || arg == "-h")
			{
				std::cout << "Usage: osc_controller [options]" << std::endl;
				std::cout << "Options:" << std::endl;
				std::cout << "  --config, -c <file>      Configuration file (JSON)" << std::endl;
				std::cout << "  --ip, -i <address>       Target IP address (default: 127.0.0.1)" << std::endl;
				std::cout << "  --port, -p <port>        Target port number (default: 7001)" << std::endl;
				std::cout << "  --receive-port, -r <port> Receiving port number (default: 9001)" << std::endl;
				std::cout << "  --daemon, -d             Run in daemon mode (non-interactive)" << std::endl;
				std::cout << "  --help, -h               Show this help" << std::endl;
				return 0;
			}
		}

		// Set up signal handler for clean termination
		signal(SIGINT, signal_handler);

		// Create and configure controller
		OscController controller;

		if (!configFile.empty())
		{
			if (!controller.configure(configFile))
			{
				std::cerr << "Failed to load configuration from " << configFile << std::endl;
				return 1;
			}
			std::cout << "Loaded configuration from: " << configFile << std::endl;
		}
		else
		{
			if (!controller.configure(targetIp, targetPort, receivePort))
			{
				std::cerr << "Failed to configure OSC controller" << std::endl;
				return 1;
			}
			std::cout << "Configured for target: " << targetIp << ":" << targetPort;
			if (receivePort > 0)
			{
				std::cout << " (receiving on port " << receivePort << ")";
			}
			std::cout << std::endl;
		}

		// Add an event callback to display received messages
		int callbackId = controller.addEventCallback([](const std::string &path, const std::vector<std::any> &args)
													 {
			std::cout << "Received OSC message: " << path;

			for (const auto &arg : args) {
				std::cout << " ";
				if (arg.type() == typeid(int)) {
					std::cout << std::any_cast<int>(arg);
				}
				else if (arg.type() == typeid(float)) {
					std::cout << std::any_cast<float>(arg);
				}
				else if (arg.type() == typeid(std::string)) {
					std::cout << "\"" << std::any_cast<std::string>(arg) << "\"";
				}
				else if (arg.type() == typeid(bool)) {
					std::cout << (std::any_cast<bool>(arg) ? "true" : "false");
				}
				else {
					std::cout << "[unknown type]";
				}
			}
			std::cout << std::endl; });

		// Interactive mode with command loop
		if (interactive)
		{
			std::cout << "Enter commands (type 'help' for available commands, 'quit' to exit):" << std::endl;
			std::string line;

			while (g_running)
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
					std::cout << "  query <address>                    Query a parameter" << std::endl;
					std::cout << "  refresh                            Request device state refresh" << std::endl;
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
					bool result = controller.setParameter(address, args);
					std::cout << "Send result: " << (result ? "success" : "failed") << std::endl;
				}
				else if (line.find("query ") == 0)
				{
					// Parse address to query
					std::string address = line.substr(6);

					std::cout << "Querying " << address << "..." << std::endl;

					controller.getParameter(address, [](bool success, const std::vector<std::any> &args)
											{
						if (success) {
							std::cout << "Response received: ";
							for (const auto &arg : args) {
								if (arg.type() == typeid(int)) {
									std::cout << std::any_cast<int>(arg) << " ";
								}
								else if (arg.type() == typeid(float)) {
									std::cout << std::any_cast<float>(arg) << " ";
								}
								else if (arg.type() == typeid(std::string)) {
									std::cout << "\"" << std::any_cast<std::string>(arg) << "\" ";
								}
								else if (arg.type() == typeid(bool)) {
									std::cout << (std::any_cast<bool>(arg) ? "true" : "false") << " ";
								}
							}
							std::cout << std::endl;
						}
						else {
							std::cout << "Query failed or timed out" << std::endl;
						} });
				}
				else if (line == "refresh")
				{
					bool result = controller.requestRefresh();
					std::cout << "Refresh result: " << (result ? "success" : "failed") << std::endl;
				}
				else if (line == "status")
				{
					bool result = controller.refreshConnectionStatus();
					std::cout << "Connection status: " << (result ? "OK" : "Failed") << std::endl;
					std::cout << "Target: " << controller.getTargetIp() << ":" << controller.getTargetPort() << std::endl;
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

			// Just wait until terminated
			while (g_running)
			{
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
		}

		// Clean up
		controller.removeEventCallback(callbackId);
		controller.cleanup();
		std::cout << "OSC Controller terminated." << std::endl;
		return 0;
	}

} // namespace AudioEngine
