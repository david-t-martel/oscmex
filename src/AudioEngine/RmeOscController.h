#pragma once

#include <string>
#include <vector>
#include <any>
#include <memory>

// Forward declaration of liblo types
struct _lo_address;
typedef struct _lo_address *lo_address;

namespace AudioEngine
{

	/**
	 * @brief Controller for RME TotalMix FX via OSC
	 *
	 * Uses liblo to send OSC messages to an RME interface
	 * for controlling routing, gain, and other parameters.
	 */
	class RmeOscController
	{
	public:
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
		 * @brief Send an OSC command to the RME device
		 *
		 * @param address OSC address (e.g., "/1/matrix/1/1/gain")
		 * @param args Command arguments (float, int, string)
		 * @return true if the command was sent successfully
		 * @return false if the command failed
		 */
		bool sendCommand(const std::string &address, const std::vector<std::any> &args);

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

	private:
		/**
		 * @brief Initialize liblo for OSC communication
		 */
		bool initOscConnection();

		/**
		 * @brief Format and send OSC message
		 *
		 * @param address OSC address
		 * @param args Command arguments
		 */
		void formatAndSend(const std::string &address, const std::vector<std::any> &args);

		// Member variables
		std::string m_rmeIp;	   // Target IP address
		int m_rmePort = 0;		   // Target port
		bool m_configured = false; // Configuration state
		lo_address m_oscAddress;   // liblo OSC address
	};

} // namespace AudioEngine
