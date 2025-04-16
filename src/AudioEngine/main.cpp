#include "AudioEngine.h"
#include "Configuration.h"
#include <iostream>
#include <csignal>
#include <thread>
#include <atomic>
#include <chrono>

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

		// Main thread monitoring loop
		while (!g_shutdown_requested.load())
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			// Here we could add:
			// - Console command processing
			// - Status updates
			// - UI event handling
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
