#pragma once

#include <string>
#include <vector>
#include <any>
#include <memory>
#include <functional>
#include <atomic>
#include <map>

// Include Configuration.h for RmeOscCommandConfig structure
#include "Configuration.h"

// Forward declaration of liblo types
struct _lo_address;
typedef struct _lo_address *lo_address;
struct _lo_server_thread;
typedef struct _lo_server_thread *lo_server_thread;
struct _lo_message;
typedef struct _lo_message *lo_message;
struct _lo_bundle;
typedef struct _lo_bundle *lo_bundle;

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
		 * @brief RME Device Channel Types
		 */
		enum class ChannelType
		{
			INPUT,	  // Hardware inputs
			PLAYBACK, // Software playback channels
			OUTPUT	  // Hardware outputs
		};

		/**
		 * @brief Common parameter types for RME devices
		 */
		enum class ParamType
		{
			GAIN,		   // Input gain (microphone preamp)
			VOLUME,		   // Channel volume
			MUTE,		   // Channel mute state
			STEREO,		   // Channel stereo link
			PAN,		   // Channel pan position (-100 to +100)
			PHASE,		   // Phase invert
			FX_SEND,	   // Effect send level
			FX_RETURN,	   // Effect return level
			EQ_ENABLE,	   // EQ on/off
			EQ_BAND1_TYPE, // First EQ band type
			EQ_BAND1_FREQ, // First EQ band frequency
			EQ_BAND1_GAIN, // First EQ band gain
			EQ_BAND1_Q,	   // First EQ band Q
			DYN_ENABLE,	   // Dynamics on/off
			PHANTOM_POWER, // 48V phantom power
			HI_Z		   // Hi-Z input impedance
		};

		/**
		 * @brief Level meter data structure
		 */
		struct LevelMeterData
		{
			float peakdB;	// Peak level in dB
			float rmsdB;	// RMS level in dB
			float peakFxdB; // FX send peak level in dB (if supported)
			float rmsFxdB;	// FX send RMS level in dB (if supported)
			bool clipping;	// Clipping indicator

			LevelMeterData() : peakdB(-144.0f), rmsdB(-144.0f),
							   peakFxdB(-144.0f), rmsFxdB(-144.0f),
							   clipping(false) {}
		};

		/**
		 * @brief DSP status data
		 */
		struct DspStatus
		{
			int version;	 // DSP firmware version
			int loadPercent; // DSP load percentage (0-100)

			DspStatus() : version(0), loadPercent(0) {}
		};

		/**
		 * @brief Callback type for OSC message handling
		 */
		using OscMessageCallback = std::function<void(const std::string &, const std::vector<std::any> &)>;

		/**
		 * @brief Callback type for level meter updates
		 */
		using LevelMeterCallback = std::function<void(ChannelType, int, const LevelMeterData &)>;

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
		 * @param port Target port number (e.g., 7001)
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
		 * @brief Set a callback function for level meter updates
		 *
		 * @param callback Function to call when level meter data is received
		 * @return true if callback was set successfully
		 */
		bool setLevelMeterCallback(LevelMeterCallback callback);

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
		 * @param type Channel type (input, playback, output)
		 * @param channel Channel number (1-based)
		 * @param mute Mute state (true = muted)
		 * @return true if the command was sent successfully
		 * @return false if the command failed
		 */
		bool setChannelMute(ChannelType type, int channel, bool mute);

		/**
		 * @brief Set channel strip stereo link state
		 *
		 * @param type Channel type (input, playback, output)
		 * @param channel Channel number (1-based)
		 * @param stereo Stereo link state (true = linked)
		 * @return true if the command was sent successfully
		 * @return false if the command failed
		 */
		bool setChannelStereo(ChannelType type, int channel, bool stereo);

		/**
		 * @brief Set channel strip volume
		 *
		 * @param type Channel type (input, playback, output)
		 * @param channel Channel number (1-based)
		 * @param volume_db Volume in decibels
		 * @return true if the command was sent successfully
		 * @return false if the command failed
		 */
		bool setChannelVolume(ChannelType type, int channel, float volume_db);

		/**
		 * @brief Legacy method for volume control (output only)
		 * @deprecated Use setChannelVolume(ChannelType::OUTPUT, ...) instead
		 */
		bool setChannelVolume(int channel, float volume_db)
		{
			return setChannelVolume(ChannelType::OUTPUT, channel, volume_db);
		}

		/**
		 * @brief Set channel phantom power (48V)
		 *
		 * @param channel Input channel number (1-based)
		 * @param enabled Phantom power state
		 * @return true if the command was sent successfully
		 * @return false if the command failed or not supported on this input
		 */
		bool setInputPhantomPower(int channel, bool enabled);

		/**
		 * @brief Set input channel Hi-Z mode
		 *
		 * @param channel Input channel number (1-based)
		 * @param enabled Hi-Z state
		 * @return true if the command was sent successfully
		 * @return false if the command failed or not supported on this input
		 */
		bool setInputHiZ(int channel, bool enabled);

		/**
		 * @brief Set channel EQ state
		 *
		 * @param type Channel type (input, playback, output)
		 * @param channel Channel number (1-based)
		 * @param enabled EQ state
		 * @return true if the command was sent successfully
		 * @return false if the command failed
		 */
		bool setChannelEQ(ChannelType type, int channel, bool enabled);

		/**
		 * @brief Set channel EQ band parameters
		 *
		 * @param type Channel type (input, playback, output)
		 * @param channel Channel number (1-based)
		 * @param band Band number (1-3)
		 * @param freq Frequency in Hz
		 * @param gain Gain in dB (-20 to +20)
		 * @param q Q factor (0.4 to 9.9)
		 * @return true if the command was sent successfully
		 * @return false if the command failed
		 */
		bool setChannelEQBand(ChannelType type, int channel, int band, float freq, float gain, float q);

		/**
		 * @brief Query channel volume from the device
		 *
		 * @param type Channel type (input, playback, output)
		 * @param channel Channel number (1-based)
		 * @param volume_db Output parameter for volume in decibels
		 * @return true if the volume was retrieved successfully
		 * @return false if the query failed
		 */
		bool queryChannelVolume(ChannelType type, int channel, float &volume_db);

		/**
		 * @brief Legacy method to query channel volume (output only)
		 * @deprecated Use queryChannelVolume(ChannelType::OUTPUT, ...) instead
		 */
		bool queryChannelVolume(int channel, float &volume_db)
		{
			return queryChannelVolume(ChannelType::OUTPUT, channel, volume_db);
		}

		/**
		 * @brief Enable or disable level meter updates
		 *
		 * @param enable Whether to enable level meter updates
		 * @return true if the setting was changed successfully
		 */
		bool enableLevelMeters(bool enable);

		/**
		 * @brief Get current DSP status
		 *
		 * @return DspStatus Current DSP status
		 */
		DspStatus getDspStatus() const;

		/**
		 * @brief Send a TotalMix refresh command
		 * This requests the device to send all current parameter values
		 *
		 * @return true if the command was sent successfully
		 */
		bool requestRefresh();

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
							 lo_arg **argv, int argc, _lo_message *msg);

		/**
		 * @brief Process a level meter message from the device
		 *
		 * @param type Channel type
		 * @param channel Channel number (1-based)
		 * @param peakdB Peak level in dB
		 * @param rmsdB RMS level in dB
		 * @param peakFxdB FX peak level in dB (may be unused)
		 * @param rmsFxdB FX RMS level in dB (may be unused)
		 * @param clipping Clipping indicator
		 */
		void processLevelMeter(ChannelType type, int channel,
							   float peakdB, float rmsdB,
							   float peakFxdB, float rmsFxdB,
							   bool clipping);

		/**
		 * @brief Build an OSC address string for a channel parameter
		 *
		 * @param type Channel type
		 * @param channel Channel number (1-based)
		 * @param param Parameter type
		 * @return OSC address string
		 */
		std::string buildChannelAddress(ChannelType type, int channel, ParamType param);

		/**
		 * @brief Convert dB value to normalized (0-1) value for RME
		 *
		 * @param db Value in decibels
		 * @return float Normalized value (0-1)
		 */
		float dbToNormalized(float db) const;

		/**
		 * @brief Convert normalized (0-1) value to dB value
		 *
		 * @param normalized Normalized value (0-1)
		 * @return float Value in decibels
		 */
		float normalizedToDb(float normalized) const;

		/**
		 * @brief Create a new lo_bundle object
		 *
		 * @return lo_bundle The new bundle or nullptr if failed
		 */
		lo_bundle createBundle();

		// Member variables
		std::string m_rmeIp;										 // Target IP address
		int m_rmePort = 0;											 // Target port
		bool m_configured = false;									 // Configuration state
		lo_address m_oscAddress;									 // liblo OSC address for sending
		lo_server_thread m_oscServer = nullptr;						 // liblo OSC server for receiving
		std::thread *m_oscThread = nullptr;							 // OSC thread for receiving
		OscMessageCallback m_messageCallback;						 // Callback for OSC messages
		LevelMeterCallback m_levelCallback;							 // Callback for level meter updates
		bool m_levelMetersEnabled = false;							 // Whether level meters are enabled
		DspStatus m_dspStatus;										 // Current DSP status
		std::map<std::string, std::atomic<bool>> m_pendingResponses; // For handling query responses

		// Clamp helpers for parameter bounds (from rme_osc_commands.md)
		float clampVolumeDb(float db) const { return std::max(-65.0f, std::min(6.0f, db)); }
		int clampPan(int pan) const { return std::max(-100, std::min(100, pan)); }
		float clampGain(float gain, bool mic) const { return std::max(0.0f, std::min(mic ? 75.0f : 24.0f, gain)); }
		float clampEQGain(float gain) const { return std::max(-20.0f, std::min(20.0f, gain)); }
		float clampEQFreq(float freq) const { return std::max(20.0f, std::min(20000.0f, freq)); }
		float clampEQQ(float q) const { return std::max(0.4f, std::min(9.9f, q)); }
	};

}
