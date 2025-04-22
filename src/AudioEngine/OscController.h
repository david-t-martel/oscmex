#pragma once

#include "IExternalControl.h"

#include "Configuration.h"
#include <string>
#include <vector>
#include <map>
#include <any>
#include <functional>
#include <memory>
#include <mutex>
#include <atomic>

// Forward declarations for OSC libraries
struct lo_address_t;
typedef struct lo_address_t *lo_address;

struct lo_server_thread_t;
typedef struct lo_server_thread_t *lo_server_thread;

struct lo_message_t;
typedef struct lo_message_t *lo_message;

typedef void *lo_arg;

namespace AudioEngine
{
	/**
	 * @brief OSC Controller for external device communication
	 *
	 * This class implements the IExternalControl interface and provides
	 * bidirectional OSC communication with audio devices.
	 */
	class OscController : public IExternalControl
	{
	public:
		/**
		 * @brief Construct a new OscController object
		 */
		OscController();

		/**
		 * @brief Destroy the OscController object and clean up resources
		 */
		~OscController();

		// IExternalControl interface implementation

		/**
		 * @brief Configure the OSC controller with connection parameters
		 *
		 * @param targetIp IP address of the target device
		 * @param targetPort Port number of the target device
		 * @param receivePort Local port for receiving OSC messages
		 * @return true if configuration was successful
		 */
		bool configure(const std::string &targetIp, int targetPort, int receivePort) override;

		/**
		 * @brief Configure from a Configuration object
		 *
		 * @param config Configuration object containing connection parameters
		 * @return true if configuration was successful
		 */
		bool configure(const Configuration &config) override;

		/**
		 * @brief Configure from a JSON configuration file
		 *
		 * @param configFile Path to JSON configuration file
		 * @return true if configuration was successful
		 */
		bool configure(const std::string &configFile) override;

		/**
		 * @brief Set a parameter value on the device
		 *
		 * @param address OSC address path
		 * @param args Parameter values
		 * @return true if command was sent successfully
		 */
		bool setParameter(const std::string &address, const std::vector<std::any> &args) override;

		/**
		 * @brief Get a parameter value from the device
		 *
		 * @param address OSC address path
		 * @param callback Callback function to receive the result
		 * @return true if query was sent successfully
		 */
		bool getParameter(const std::string &address,
						  std::function<void(bool, const std::vector<std::any> &)> callback) override;

		/**
		 * @brief Apply a complete configuration to the device
		 *
		 * @param config Configuration object containing commands to apply
		 * @return true if all commands were sent successfully
		 */
		bool applyConfiguration(const Configuration &config) override;

		/**
		 * @brief Query the current device state
		 *
		 * @param callback Callback function to receive the device state
		 * @return true if query was initiated successfully
		 */
		bool queryDeviceState(std::function<void(bool, const Configuration &)> callback) override;

		// Additional OSC-specific methods

		/**
		 * @brief Start the OSC message receiver
		 *
		 * @param receivePort Local port to listen on
		 * @return true if receiver started successfully
		 */
		bool startReceiver(int receivePort);

		/**
		 * @brief Stop the OSC message receiver
		 */
		void stopReceiver();

		/**
		 * @brief Register a callback for all OSC events
		 *
		 * @param callback Callback function to receive OSC messages
		 * @return int Callback ID for later removal
		 */
		int addEventCallback(std::function<void(const std::string &, const std::vector<std::any> &)> callback);

		/**
		 * @brief Remove a previously registered event callback
		 *
		 * @param callbackId ID of the callback to remove
		 */
		void removeEventCallback(int callbackId);

		/**
		 * @brief Query a single value with timeout
		 *
		 * @param address OSC address to query
		 * @param value Reference to receive the value
		 * @param timeoutMs Timeout in milliseconds
		 * @return true if value was successfully received
		 */
		bool querySingleValue(const std::string &address, float &value, int timeoutMs = 500);

		/**
		 * @brief Start the OSC listener
		 *
		 * @return true if listener started successfully
		 */
		bool startListener();

		/**
		 * @brief Stop the OSC listener
		 */
		void stopListener();

		/**
		 * @brief Request a device state refresh
		 *
		 * @return true if refresh request was sent successfully
		 */
		bool requestRefresh();

		/**
		 * @brief Check if the OSC connection is active
		 *
		 * @return true if connection is active
		 */
		bool refreshConnectionStatus();

		/**
		 * @brief Get the target IP address
		 *
		 * @return std::string IP address
		 */
		std::string getTargetIp() const;

		/**
		 * @brief Get the target port number
		 *
		 * @return int Port number
		 */
		int getTargetPort() const;

		/**
		 * @brief Get the local receive port number
		 *
		 * @return int Receive port number
		 */
		int getReceivePort() const;

		/**
		 * @brief Clean up OSC resources
		 */
		void cleanup();

		/**
		 * @brief Static main method for standalone operation
		 *
		 * @param argc Command line argument count
		 * @param argv Command line argument values
		 * @return int Exit code
		 */
		static int main(int argc, char *argv[]);

	private:
		// OSC resources
		lo_address m_oscAddress;
		lo_server_thread m_oscServer;

		// Callback for incoming messages
		std::function<void(const std::string &, const std::vector<std::any> &)> m_messageCallback;

		// Callback for level meters
		std::function<void(const std::string &, float)> m_levelCallback;

		// Connection parameters
		std::string m_targetIp;
		int m_targetPort;
		int m_receivePort;

		// State tracking
		std::atomic<bool> m_running;
		bool m_configured;

		// Callback management
		std::map<int, std::function<void(const std::string &, const std::vector<std::any> &)>> m_eventCallbacks;
		std::mutex m_callbackMutex;
		int m_nextCallbackId;

		// Query tracking
		std::map<std::string, std::function<void(bool, const std::vector<std::any> &)>> m_pendingQueries;
		std::mutex m_queryMutex;

		// Handler for OSC messages
		static int handleOscMessageStatic(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
		int handleOscMessage(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg);
	};

} // namespace AudioEngine
