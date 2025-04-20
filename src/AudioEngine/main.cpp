#include "AudioEngine.h"
#include "Configuration.h"
#include "OscController.h"
#include <iostream>
#include <csignal>
#include <thread>
#include <atomic>
#include <chrono>
#include <string>
#include <vector>
#include <map>

// Global shutdown flag for signal handling
std::atomic<bool> g_shutdown_requested(false);

/**
 * @brief Signal handler for graceful shutdown
 *
 * @param signum Signal number
 */
void signal_handler(int signum)
{
	std::cout << "\nShutdown requested (Signal " << signum << ")...\n";
	g_shutdown_requested.store(true);
}

/**
 * @brief OSC message handler callback
 *
 * @param path OSC address path
 * @param args Vector of arguments of various types
 */
void oscMessageHandler(const std::string &path, const std::vector<std::any> &args)
{
	std::cout << "OSC Message Handler: " << path << " with " << args.size() << " arguments" << std::endl;

	// Example of processing specific OSC messages
	if (path == "/1/vumeters")
	{
		// Handle VU meter updates
		std::cout << "  VU Meters update received" << std::endl;
	}
	else if (path.find("/1/channel") != std::string::npos && path.find("/volume") != std::string::npos)
	{
		// Handle volume updates
		if (!args.empty() && args[0].type() == typeid(float))
		{
			float vol = std::any_cast<float>(args[0]);
			std::cout << "  Volume update: " << vol << std::endl;
		}
	}
}

/**
 * @brief Separate function to showcase RmeOscController independent of AudioEngine
 */
void demonstrateRmeOscControl()
{
	std::cout << "Demonstrating RME OSC Control Module...\n";

	// Create and configure RME OSC controller
	AudioEngine::RmeOscController controller;

	if (!controller.configure("127.0.0.1", 7001))
	{
		std::cerr << "Failed to configure RME OSC controller\n";
		return;
	}

	// Start OSC receiver on port 9000
	if (!controller.startReceiver(9000))
	{
		std::cerr << "Failed to start OSC receiver\n";
		// We can continue without receiving if needed
	}
	else
	{
		// Set callback for handling incoming messages
		controller.setMessageCallback(oscMessageHandler);
		std::cout << "OSC receiver started successfully\n";
	}

	// Example commands to send
	std::cout << "Sending test RME OSC commands...\n";

	// Set channel 1 volume to -10dB
	controller.setChannelVolume(1, -10.0f);

	// Send a batch of commands
	std::vector<AudioEngine::RmeOscCommandConfig> commands = {
		{"/1/channel/2/mute", {std::any(true)}},
		{"/1/channel/3/volume", {std::any(0.8f)}},
		{"/1/matrix/1/2/gain", {std::any(0.5f)}}};

	controller.sendBatch(commands);

	// Query a value
	float volume;
	if (controller.queryChannelVolume(1, volume))
	{
		std::cout << "Channel 1 volume: " << volume << " dB\n";
	}
	else
	{
		std::cout << "Failed to query channel 1 volume\n";
	}

	// Process incoming messages for a while
	std::cout << "Monitoring for incoming OSC messages (5 seconds)...\n";
	auto startTime = std::chrono::steady_clock::now();
	while (std::chrono::duration_cast<std::chrono::seconds>(
			   std::chrono::steady_clock::now() - startTime)
			   .count() < 5)
	{
		// Messages will be processed via callback
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	// Clean up
	controller.cleanup();
	std::cout << "RME OSC demonstration complete\n";
}

/**
 * @brief Main entry point for the Audio Engine application
 *
 * @param argc Argument count
 * @param argv Argument values
 * @return int Exit code
 */
int main(int argc, char *argv[])
{
	std::cout << "Starting Modular Audio Engine v1.0...\n";

	// Set up signal handlers for graceful shutdown
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	// Check if we should run in standalone RME OSC demonstration mode
	bool demonstrationMode = false;
	for (int i = 1; i < argc; i++)
	{
		if (std::string(argv[i]) == "--demo-osc")
		{
			demonstrationMode = true;
			break;
		}
	}

	if (demonstrationMode)
	{
		demonstrateRmeOscControl();
		return 0;
	}

	// Parse configuration from command line or config file
	AudioEngine::Configuration config;
	AudioEngine::ConfigurationParser parser;
	AudioEngine::AudioEngine engine;

	try
	{
		// Parse configuration
		if (!parser.parse(argc, argv, config))
		{
			std::cerr << "Failed to parse configuration.\n";
			return 1;
		}

		// Initialize the engine
		if (!engine.initialize(std::move(config)))
		{
			std::cerr << "Failed to initialize audio engine.\n";
			engine.cleanup();
			return 1;
		}

		// Start the engine
		engine.run();

		std::cout << "Engine running. Press Ctrl+C to stop.\n";

		// Set up command processing map for console commands
		std::map<std::string, std::function<void(const std::vector<std::string> &)>> commands;

		// Add command to set matrix crosspoint gain
		commands["matrix"] = [&engine](const std::vector<std::string> &args)
		{
			if (args.size() < 3)
			{
				std::cout << "Usage: matrix <input> <output> <gain_db>\n";
				return;
			}
			try
			{
				int input = std::stoi(args[0]);
				int output = std::stoi(args[1]);
				float gain = std::stof(args[2]);
				if (engine.getRmeController()->setMatrixCrosspointGain(input, output, gain))
				{
					std::cout << "Matrix " << input << "," << output << " gain set to " << gain << " dB\n";
				}
			}
			catch (const std::exception &e)
			{
				std::cerr << "Error: " << e.what() << std::endl;
			}
		};

		// Add command to set channel volume
		commands["volume"] = [&engine](const std::vector<std::string> &args)
		{
			if (args.size() < 2)
			{
				std::cout << "Usage: volume <channel> <volume_db>\n";
				return;
			}
			try
			{
				int channel = std::stoi(args[0]);
				float volume = std::stof(args[1]);
				if (engine.getRmeController()->setChannelVolume(channel, volume))
				{
					std::cout << "Channel " << channel << " volume set to " << volume << " dB\n";
				}
			}
			catch (const std::exception &e)
			{
				std::cerr << "Error: " << e.what() << std::endl;
			}
		};

		// Add command to mute/unmute a channel
		commands["mute"] = [&engine](const std::vector<std::string> &args)
		{
			if (args.size() < 2)
			{
				std::cout << "Usage: mute <channel> <0|1>\n";
				return;
			}
			try
			{
				int channel = std::stoi(args[0]);
				bool mute = std::stoi(args[1]) != 0;
				if (engine.getRmeController()->setChannelMute(channel, mute))
				{
					std::cout << "Channel " << channel << (mute ? " muted" : " unmuted") << std::endl;
				}
			}
			catch (const std::exception &e)
			{
				std::cerr << "Error: " << e.what() << std::endl;
			}
		};

		// Add command to query channel volume
		commands["query"] = [&engine](const std::vector<std::string> &args)
		{
			if (args.size() < 1)
			{
				std::cout << "Usage: query <channel>\n";
				return;
			}
			try
			{
				int channel = std::stoi(args[0]);
				float volume;
				if (engine.getRmeController()->queryChannelVolume(channel, volume))
				{
					std::cout << "Channel " << channel << " volume: " << volume << " dB\n";
				}
				else
				{
					std::cout << "Failed to query channel " << channel << " volume\n";
				}
			}
			catch (const std::exception &e)
			{
				std::cerr << "Error: " << e.what() << std::endl;
			}
		};

		// Main thread monitoring loop with interactive console
		std::cout << "Interactive console ready. Type 'help' for commands.\n";
		std::string input;
		while (!g_shutdown_requested.load())
		{
			// Check for console input
			std::cout << "> ";
			std::getline(std::cin, input);

			if (input.empty())
				continue;

			// Parse input into command and arguments
			std::vector<std::string> tokens;
			std::stringstream ss(input);
			std::string token;
			while (std::getline(ss, token, ' '))
			{
				if (!token.empty())
				{
					tokens.push_back(token);
				}
			}

			if (tokens.empty())
				continue;

			std::string cmd = tokens[0];
			std::vector<std::string> args(tokens.begin() + 1, tokens.end());

			if (cmd == "quit" || cmd == "exit")
			{
				break;
			}
			else if (cmd == "help")
			{
				std::cout << "Available commands:\n"
						  << "  help       - Show this help\n"
						  << "  exit       - Exit the application\n"
						  << "  matrix i o g - Set matrix crosspoint gain (input, output, gain_db)\n"
						  << "  volume c v - Set channel volume (channel, volume_db)\n"
						  << "  mute c m   - Set channel mute state (channel, mute_state)\n"
						  << "  query c    - Query channel volume (channel)\n";
			}
			else if (commands.find(cmd) != commands.end())
			{
				commands[cmd](args);
			}
			else
			{
				std::cout << "Unknown command: " << cmd << ". Type 'help' for available commands.\n";
			}
		}
	}
	catch (const std::exception &e)
	{
		std::cerr << "Unhandled exception: " << e.what() << std::endl;
		engine.cleanup();
		return 1;
	}
	catch (...)
	{
		std::cerr << "Unknown exception occurred." << std::endl;
		engine.cleanup();
		return 1;
	}

	std::cout << "Shutting down...\n";

	// Stop and clean up
	engine.stop();
	engine.cleanup();

	std::cout << "Application finished.\n";
	return 0;
}
