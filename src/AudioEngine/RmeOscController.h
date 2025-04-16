#pragma once

#include <string>
#include <vector>
#include <any>
#include <memory>
#include <functional>
#include <atomic>

// Include Configuration.h for RmeOscCommandConfig structure
#include "Configuration.h"

// Forward declaration of liblo types
struct _lo_address;
typedef struct _lo_address *lo_address;
struct _lo_server_thread;
typedef struct _lo_server_thread *lo_server_thread;
struct _lo_message;
typedef struct _lo_message *lo_message;

namespace AudioEngine
{
	/**
	 * @brief Controller for RME TotalMix FX via OSC
	 *
	 * Uses liblo to send OSC messages to an RME interface
	 * for controlling routing, gain, and other parameters.
	 * Also provides capability to receive OSC messages from RME.
	 */
	class RmeOscController
	{
	public:
		/**
		 * @brief Callback type for OSC message handling
		 */
		using OscMessageCallback = std::function<void(const std::string &, const std::vector<std::any> &)>;

		/**
		 * @brief Construct a new RmeOscController
		 */
		RmeOscController();

		/**
		 * @brief Destroy the RmeOscController and cleanup resources
		 */
		~RmeOscController();

		/**
		 * @brief Configure the controller with target IP and port
		 *
		 * @param ip Target IP address (e.g., "127.0.0.1")
		 * @param port Target port number (e.g., 9000)
		 * @return true if configuration succeeded
		 * @return false if configuration failed
		 */
		bool configure(const std::string &ip, int port);

		/**
		 * @brief Start the OSC receiver on the specified port
		 *
		 * @param receivePort Port to listen on for OSC messages
		 * @return true if the receiver started successfully
		 * @return false if the receiver failed to start
		 */
		bool startReceiver(int receivePort);

		/**
		 * @brief Stop the OSC receiver
		 */
		void stopReceiver();

		/**
		 * @brief Set a callback function to handle incoming OSC messages
		 *
		 * @param callback Function to call when messages are received
		 * @return true if callback was set successfully
		 * @return false if configuration failed
		 */
		bool setMessageCallback(OscMessageCallback callback);

		/**
		 * @brief Send an OSC command to the RME device
		 *
		 * @param address OSC address (e.g., "/1/matrix/1/1/gain")
		 * @param args Command arguments (float, int, string)
		 * @return true if the command was sent successfully
		 * @return false if the command failed
		 */
		bool sendCommand(const std::string &address, const std::vector<std::any> &args);

		/**
		 * @brief Send a batch of OSC commands as a bundle
		 *
		 * @param commands Vector of command configurations
		 * @return true if the bundle was sent successfully
		 * @return false if the bundle failed to send
		 */
		bool sendBatch(const std::vector<RmeOscCommandConfig> &commands);

		/**
		 * @brief Query a single float value from the device
		 *
		 * @param address OSC address to query
		 * @param outValue Output parameter for the returned value
		 * @param timeoutMs Timeout in milliseconds
		 * @return true if value was retrieved successfully
		 * @return false if query timed out or failed
		 */
		bool querySingleValue(const std::string &address, float &outValue, int timeoutMs = 500);

		/**
		 * @brief Helper to set matrix crosspoint gain
		 *
		 * @param hw_input Hardware input channel (1-based)
		 * @param hw_output Hardware output channel (1-based)
		 * @param gain_db Gain in decibels
		 * @return true if the command was sent successfully
		 * @return false if the command failed
		 */
		bool setMatrixCrosspointGain(int hw_input, int hw_output, float gain_db);

		/**
		 * @brief Set channel strip mute state
		 *
		 * @param channel Channel number (1-based)
		 * @param mute Mute state (true = muted)
		 * @return true if the command was sent successfully
		 * @return false if the command failed
		 */
		bool setChannelMute(int channel, bool mute);

		/**
		 * @brief Set channel strip volume
		 *
		 * @param channel Channel number (1-based)
		 * @param volume_db Volume in decibels
		 * @return true if the command was sent successfully
		 * @return false if the command failed
		 */
		bool setChannelVolume(int channel, float volume_db);

		/**
		 * @brief Query channel volume from the device
		 *
		 * @param channel Channel number (1-based)
		 * @param volume_db Output parameter for volume in decibels
		 * @return true if the volume was retrieved successfully
		 * @return false if the query failed
		 */
		bool queryChannelVolume(int channel, float &volume_db);

		/**
		 * @brief Send a ping to verify connection status
		 *
		 * @return true if the connection is active
		 * @return false if the connection is not responding
		 */
		bool refreshConnectionStatus();

		/**
		 * @brief Clean up all resources
		 */
		void cleanup();

	private:
		/**
		 * @brief Handle an incoming OSC message
		 *
		 * @param path OSC address path
		 * @param types Type tag string
		 * @param argv Argument values
		 * @param argc Argument count
		 * @param msg Original message
		 * @return int Return value to indicate message handling status
		 */
		int handleOscMessage(const char *path, const char *types,
							 void *argv, int argc, _lo_message *msg);

		// Member variables
		std::string m_rmeIp;					// Target IP address
		int m_rmePort = 0;						// Target port
		bool m_configured = false;				// Configuration state
		lo_address m_oscAddress;				// liblo OSC address for sending
		lo_server_thread m_oscServer = nullptr; // liblo OSC server for receiving
		std::thread *m_oscThread = nullptr;		// Thread for receiving OSC messages
		OscMessageCallback m_messageCallback;	// Callback for received messages
	};

} // namespace AudioEngine
