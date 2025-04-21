#pragma once

#include <string>
#include <vector>
#include <any>
#include <memory>
#include <functional>
#include <atomic>
#include <map>
#include <thread>
#include <mutex>

// Include Configuration.h for RmeOscCommandConfig structure
#include "Configuration.h"
#include "IExternalControl.h"

// Include liblo headers directly
extern "C"
{
#include "lo/lo_cpp.h"
}

namespace AudioEngine
{
	/**
	 * @brief Generic OSC Controller
	 *
	 * Uses liblo to send and receive OSC messages.
	 * Can be configured for specific devices like RME TotalMix FX.
	 * Implements IExternalControl for integration with AudioEngine.
	 * Can also operate in standalone mode.
	 */
	class OscController : public IExternalControl
	{
	public:
		/**
		 * @brief Device-specific Channel Types (Example: RME)
		 */
		enum class ChannelType
		{
			INPUT,	  // Hardware inputs
			PLAYBACK, // Software playback channels
			OUTPUT	  // Hardware outputs
		};

		/**
		 * @brief Common parameter types (Example: RME)
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
		 * @brief Construct a new OscController
		 */
		OscController();

		/**
		 * @brief Destroy the OscController and cleanup resources
		 */
		~OscController();

		/**
		 * @brief Configure the controller with a config file
		 *
		 * @param configFile Path to a configuration file
		 * @return true if configuration succeeded
		 * @return false if configuration failed
		 */
		bool configure(const std::string &configFile);

		/**
		 * @brief Configure the controller with target IP and port
		 *
		 * @param ip Target IP address (e.g., "127.0.0.1")
		 * @param port Target port number (e.g., 7001)
		 * @param receivePort Optional port to listen on (0 to disable receiving)
		 * @return true if configuration succeeded
		 * @return false if configuration failed
		 */
		bool configure(const std::string &ip, int port, int receivePort = 0);

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
		 * @brief Send an OSC command to the target device
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
		 * @brief Helper to set matrix crosspoint gain (Example: RME)
		 *
		 * @param hw_input Hardware input channel (1-based)
		 * @param hw_output Hardware output channel (1-based)
		 * @param gain_db Gain in decibels
		 * @return true if the command was sent successfully
		 * @return false if the command failed
		 */
		bool setMatrixCrosspointGain(int hw_input, int hw_output, float gain_db);

		/**
		 * @brief Set channel strip mute state (Example: RME)
		 *
		 * @param type Channel type (input, playback, output)
		 * @param channel Channel number (1-based)
		 * @param mute Mute state (true = muted)
		 * @return true if the command was sent successfully
		 * @return false if the command failed
		 */
		bool setChannelMute(ChannelType type, int channel, bool mute);

		/**
		 * @brief Set channel strip stereo link state (Example: RME)
		 *
		 * @param type Channel type (input, playback, output)
		 * @param channel Channel number (1-based)
		 * @param stereo Stereo link state (true = linked)
		 * @return true if the command was sent successfully
		 * @return false if the command failed
		 */
		bool setChannelStereo(ChannelType type, int channel, bool stereo);

		/**
		 * @brief Set channel strip volume (Example: RME)
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
		 * @brief Set channel phantom power (48V) (Example: RME)
		 *
		 * @param channel Input channel number (1-based)
		 * @param enabled Phantom power state
		 * @return true if the command was sent successfully
		 * @return false if the command failed or not supported on this input
		 */
		bool setInputPhantomPower(int channel, bool enabled);

		/**
		 * @brief Set input channel Hi-Z mode (Example: RME)
		 *
		 * @param channel Input channel number (1-based)
		 * @param enabled Hi-Z state
		 * @return true if the command was sent successfully
		 * @return false if the command failed or not supported on this input
		 */
		bool setInputHiZ(int channel, bool enabled);

		/**
		 * @brief Set channel EQ state (Example: RME)
		 *
		 * @param type Channel type (input, playback, output)
		 * @param channel Channel number (1-based)
		 * @param enabled EQ state
		 * @return true if the command was sent successfully
		 * @return false if the command failed
		 */
		bool setChannelEQ(ChannelType type, int channel, bool enabled);

		/**
		 * @brief Set channel EQ band parameters (Example: RME)
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
		 * @brief Query channel volume from the device (Example: RME)
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
		 * @brief Enable or disable level meter updates (Example: RME)
		 *
		 * @param enable Whether to enable level meter updates
		 * @return true if the setting was changed successfully
		 */
		bool enableLevelMeters(bool enable);

		/**
		 * @brief Get current DSP status (Example: RME)
		 *
		 * @return DspStatus Current DSP status
		 */
		DspStatus getDspStatus() const;

		/**
		 * @brief Send a TotalMix refresh command (Example: RME)
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

		/**
		 * @brief Get the target IP address
		 *
		 * @return The IP address that this controller is sending to
		 */
		std::string getTargetIp() const { return m_targetIp; }

		/**
		 * @brief Get the target port
		 *
		 * @return The port that this controller is sending to
		 */
		int getTargetPort() const { return m_targetPort; }

		/**
		 * @brief Apply a configuration to the RME device
		 *
		 * This method sends all the OSC commands in the configuration to the device
		 *
		 * @param config The configuration to apply
		 * @return true if all commands were sent successfully
		 */
		bool applyConfiguration(const Configuration &config) override;

		// =================== IExternalControl Interface Implementation ===================

		/**
		 * @brief Set a parameter value
		 * Implementation of IExternalControl::setParameter
		 *
		 * @param address Parameter address (e.g., "/1/channel/1/volume")
		 * @param args Parameter value(s)
		 * @return true if successful
		 */
		bool setParameter(const std::string &address, const std::vector<std::any> &args) override;

		/**
		 * @brief Get a parameter value
		 * Implementation of IExternalControl::getParameter
		 *
		 * @param address Parameter address (e.g., "/1/channel/1/volume")
		 * @param callback Function to receive the result (success, value)
		 * @return true if request was sent successfully
		 */
		bool getParameter(const std::string &address,
						  std::function<void(bool, const std::vector<std::any> &)> callback) override;

		/**
		 * @brief Query the current device state
		 * Implementation of IExternalControl::queryDeviceState
		 *
		 * @param callback Function to receive the result (success, configuration)
		 * @return true if query was started successfully
		 */
		bool queryDeviceState(std::function<void(bool, const Configuration &)> callback) override;

		/**
		 * @brief Add an event callback for receiving notifications
		 * Implementation of IExternalControl::addEventCallback
		 *
		 * @param callback Function to call when events occur (address, args)
		 * @return int Callback ID for later removal
		 */
		int addEventCallback(
			std::function<void(const std::string &, const std::vector<std::any> &)> callback) override;

		/**
		 * @brief Remove an event callback
		 * Implementation of IExternalControl::removeEventCallback
		 *
		 * @param callbackId ID of the callback to remove
		 */
		void removeEventCallback(int callbackId) override;

		// =================== Standalone Operation ===================

		/**
		 * @brief Main entry point for standalone operation
		 *
		 * @param argc Argument count
		 * @param argv Argument values
		 * @return int Return code
		 */
		static int main(int argc, char *argv[]);

	private:
		/**
		 * @brief Handle an incoming OSC message (liblo callback)
		 *
		 * @param path OSC address path
		 * @param types Type tag string
		 * @param argv Argument values
		 * @param argc Argument count
		 * @param msg Original message
		 * @param user_data Pointer to the OscController instance
		 * @return int Return value to indicate message handling status
		 */
		static int handleOscMessageStatic(const char *path, const char *types,
										  lo_arg **argv, int argc, lo_message msg, void *user_data);

		/**
		 * @brief Instance method to handle OSC message processing
		 */
		int handleOscMessage(const char *path, const char *types,
							 lo_arg **argv, int argc, lo_message msg);

		/**
		 * @brief Process a level meter message from the device (Example: RME)
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
		 * @brief Build an OSC address string for a channel parameter (Example: RME)
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
		std::string m_targetIp;					// Target IP address
		int m_targetPort = 0;					// Target port
		bool m_configured = false;				// Configuration state
		lo_address m_oscAddress = nullptr;		// liblo OSC address for sending
		lo_server_thread m_oscServer = nullptr; // liblo OSC server for receiving
		// std::thread *m_oscThread = nullptr;						 // No longer needed, liblo manages its thread
		OscMessageCallback m_messageCallback;						 // Callback for OSC messages
		LevelMeterCallback m_levelCallback;							 // Callback for level meter updates
		bool m_levelMetersEnabled = false;							 // Whether level meters are enabled
		DspStatus m_dspStatus;										 // Current DSP status
		std::map<std::string, std::atomic<bool>> m_pendingResponses; // For handling query responses

		// Event callback management
		std::map<int, std::function<void(const std::string &, const std::vector<std::any> &)>> m_eventCallbacks;
		int m_nextCallbackId = 0;
		std::mutex m_callbackMutex;

		// For async parameter queries
		using ParameterCallback = std::function<void(bool, const std::vector<std::any> &)>;
		std::map<std::string, ParameterCallback> m_parameterCallbacks;

		// For device state queries
		using DeviceStateCallback = std::function<void(bool, const Configuration &)>;

		// Clamp helpers for parameter bounds (Example: RME)
		float clampVolumeDb(float db) const { return std::max(-65.0f, std::min(6.0f, db)); }
		int clampPan(int pan) const { return std::max(-100, std::min(100, pan)); }
		float clampGain(float gain, bool mic) const { return std::max(0.0f, std::min(mic ? 75.0f : 24.0f, gain)); }
		float clampEQGain(float gain) const { return std::max(-20.0f, std::min(20.0f, gain)); }
		float clampEQFreq(float freq) const { return std::max(20.0f, std::min(20000.0f, freq)); }
		float clampEQQ(float q) const { return std::max(0.4f, std::min(9.9f, q)); }
	};

	// Periodic monitoring/ping system for RME connection status
	class ConnectionMonitor
	{
	public:
		ConnectionMonitor(OscController *controller, int pingIntervalMs = 5000)
			: m_controller(controller), m_pingIntervalMs(pingIntervalMs), m_running(false) {}

		~ConnectionMonitor()
		{
			stop();
		}

		// Start periodic ping monitoring
		void start()
		{
			if (m_running)
				return;

			m_running = true;
			m_monitorThread = std::thread([this]()
										  {
				while (m_running) {
					// Check connection status
					bool connected = m_controller->refreshConnectionStatus();

					// Get DSP status if connected
					if (connected) {
						auto status = m_controller->getDspStatus();

						// Notify any registered callbacks
						std::lock_guard<std::mutex> lock(m_callbackMutex);
						for (const auto& callback : m_statusCallbacks) {
							callback(true, status);
						}
					} else {
						// Notify about disconnection
						std::lock_guard<std::mutex> lock(m_callbackMutex);
						for (const auto& callback : m_statusCallbacks) {
							callback(false, OscController::DspStatus());
						}
					}

					// Wait for next ping interval
					std::this_thread::sleep_for(std::chrono::milliseconds(m_pingIntervalMs));
				} });
		}

		// Stop monitoring
		void stop()
		{
			m_running = false;
			if (m_monitorThread.joinable())
			{
				m_monitorThread.join();
			}
		}

		// Register callback for connection status changes
		using StatusCallback = std::function<void(bool, const OscController::DspStatus &)>;
		int addStatusCallback(StatusCallback callback)
		{
			std::lock_guard<std::mutex> lock(m_callbackMutex);
			int id = m_nextCallbackId++;
			m_statusCallbacks[id] = callback;
			return id;
		}

		// Remove a callback
		void removeStatusCallback(int id)
		{
			std::lock_guard<std::mutex> lock(m_callbackMutex);
			m_statusCallbacks.erase(id);
		}

	private:
		OscController *m_controller;
		int m_pingIntervalMs;
		std::atomic<bool> m_running;
		std::thread m_monitorThread;

		std::mutex m_callbackMutex;
		std::map<int, StatusCallback> m_statusCallbacks;
		int m_nextCallbackId = 0;
	};

} // namespace AudioEngine
