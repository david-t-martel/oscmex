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
namespace osc
{
	class OutboundPacketStream;
}
namespace Ip
{
	class UdpSocket;
}

namespace AudioEngine
{
	/**
	 * @brief Open Sound Control (OSC) implementation of the external control interface
	 *
	 * This class handles communication with external devices using the OSC protocol.
	 * It implements the IExternalControl interface, allowing it to be used by the
	 * AudioEngine or as a standalone component.
	 */
	class OscController : public IExternalControl
	{
	public:
		/**
		 * @brief Constructor for OSC controller
		 */
		OscController();

		/**
		 * @brief Destructor
		 */
		~OscController();

		/**
		 * @brief Configure the OSC controller
		 *
		 * @param targetIp IP address of the target device
		 * @param targetPort Port number of the target device
		 * @param receivePort Local port to listen for responses (0 for no listener)
		 * @return true if configuration was successful
		 */
		bool configure(const std::string &targetIp, int targetPort, int receivePort = 0);

		/**
		 * @brief Configure from a configuration file
		 *
		 * @param configFile Path to the configuration file
		 * @return true if configuration was successful
		 */
		bool configure(const std::string &configFile);

		// IExternalControl implementation

		/**
		 * @brief Set a parameter value on the device
		 *
		 * @param address OSC address path
		 * @param args Parameter value(s)
		 * @return true if parameter was set successfully
		 */
		bool setParameter(const std::string &address, const std::vector<std::any> &args) override;

		/**
		 * @brief Get a parameter value from the device
		 *
		 * @param address OSC address path
		 * @param callback Function to receive the result
		 * @return true if the request was sent successfully
		 */
		bool getParameter(const std::string &address,
						  std::function<void(bool, const std::vector<std::any> &)> callback) override;

		/**
		 * @brief Apply a complete configuration to the device
		 *
		 * @param config Configuration to apply
		 * @return true if the configuration was applied successfully
		 */
		bool applyConfiguration(const Configuration &config) override;

		/**
		 * @brief Query the current device state
		 *
		 * @param callback Function to receive the result
		 * @return true if the query was started successfully
		 */
		bool queryDeviceState(std::function<void(bool, const Configuration &)> callback) override;

		/**
		 * @brief Add an event callback for receiving notifications
		 *
		 * @param callback Function to call when events occur
		 * @return int Callback ID for later removal
		 */
		int addEventCallback(
			std::function<void(const std::string &, const std::vector<std::any> &)> callback) override;

		/**
		 * @brief Remove an event callback
		 *
		 * @param callbackId ID of the callback to remove
		 */
		void removeEventCallback(int callbackId) override;

		/**
		 * @brief Standalone application entry point
		 *
		 * @param argc Argument count
		 * @param argv Argument values
		 * @return int Exit code
		 */
		static int main(int argc, char *argv[]);

	private:
		std::string m_targetIp;
		int m_targetPort;
		int m_receivePort;

		// OSC socket and communication
		std::unique_ptr<Ip::UdpSocket> m_socket;
		std::atomic<bool> m_running;
		std::mutex m_socketMutex;

		// Callback management
		std::map<int, std::function<void(const std::string &, const std::vector<std::any> &)>> m_callbacks;
		int m_nextCallbackId;
		std::mutex m_callbackMutex;

		// Pending queries
		std::map<std::string, std::function<void(bool, const std::vector<std::any> &)>> m_pendingQueries;
		std::mutex m_queryMutex;

		// OSC message processing
		bool sendOscMessage(const std::string &address, const std::vector<std::any> &args);
		bool startListener();
		void stopListener();
		void listenerThread();
		void processOscMessage(const char *data, size_t size);

		// Helper methods
		std::any convertOscValue(const void *value, char type);
		bool appendOscArg(osc::OutboundPacketStream &packet, const std::any &arg);

		// Thread for listener
		std::unique_ptr<std::thread> m_listenerThread;
	};
}
