#include "OscDeviceInterface.h"
#include "OscController.h"

#include <iostream>
#include <thread>
#include <atomic>
#include <map>

namespace AudioEngine
{
    OscDeviceInterface::OscDeviceInterface(OscController *oscController)
        : m_oscController(oscController)
    {
        if (!m_oscController)
        {
            throw std::invalid_argument("OscController cannot be null");
        }
    }

    bool OscDeviceInterface::queryState(const std::string &deviceName,
                                        std::function<void(bool, const DeviceState &)> callback)
    {
        if (!m_oscController)
        {
            DeviceState emptyState(deviceName);
            callback(false, emptyState);
            return false;
        }

        // Create a device state to populate
        DeviceState state(deviceName);
        state.setType(DeviceState::DeviceType::Unknown); // Will be refined later
        state.setStatus(DeviceState::Status::Disconnected);

        // Store connection parameters
        state.setProperty("target_ip", m_oscController->getTargetIp());
        state.setProperty("target_port", std::to_string(m_oscController->getTargetPort()));

        // Launch a thread to handle the asynchronous querying
        std::thread([this, deviceName, state, callback]() mutable
                    {
            bool success = true;

            try {
                // First check if the OSC endpoint is reachable with a ping
                bool pingSuccess = false;

                // Use a synchronization mechanism for the async ping
                std::promise<bool> pingPromise;
                std::future<bool> pingFuture = pingPromise.get_future();

                m_oscController->send("/ping", {},
                    [&pingPromise](bool success, const std::vector<std::any>& response) {
                        pingPromise.set_value(success);
                    });

                // Wait for ping response with timeout
                if (pingFuture.wait_for(std::chrono::seconds(2)) == std::future_status::ready) {
                    pingSuccess = pingFuture.get();
                }

                if (!pingSuccess) {
                    // If ping failed, try a different common OSC path
                    std::promise<bool> altPingPromise;
                    std::future<bool> altPingFuture = altPingPromise.get_future();

                    m_oscController->send("/status", {},
                        [&altPingPromise](bool success, const std::vector<std::any>& response) {
                            altPingPromise.set_value(success);
                        });

                    if (altPingFuture.wait_for(std::chrono::seconds(2)) == std::future_status::ready) {
                        pingSuccess = altPingFuture.get();
                    }
                }

                if (!pingSuccess) {
                    success = false;
                    callback(false, state);
                    return;
                }

                // Device is connected if we made it here
                state.setStatus(DeviceState::Status::Connected);

                // Now query the device parameters
                queryOscParameters(state, [&state, callback, &success](bool paramQuerySuccess) {
                    success &= paramQuerySuccess;
                    callback(success, state);
                });
            }
            catch (const std::exception& e) {
                std::cerr << "Error querying OSC device state: " << e.what() << std::endl;
                callback(false, state);
            } })
            .detach();

        return true;
    }

    void OscDeviceInterface::queryOscParameters(DeviceState &deviceState,
                                                std::function<void(bool)> callback)
    {
        // List of common OSC parameters to query
        std::vector<std::string> standardParameters = {
            "/device/name",
            "/device/version",
            "/device/type",
            "/device/channels/input/count",
            "/device/channels/output/count",
            "/device/samplerate",
            "/device/buffersize"};

        // Add channel volume, mute, etc. parameters
        // These are device specific but we'll try common patterns

        // Keep track of query progress
        auto paramCount = std::make_shared<int>(standardParameters.size());
        auto successCount = std::make_shared<int>(0);

        // Completion handler for individual parameter queries
        auto queryHandler = [paramCount, successCount, callback](bool success)
        {
            if (success)
            {
                (*successCount)++;
            }

            (*paramCount)--;
            if (*paramCount <= 0)
            {
                callback(*successCount > 0); // Success if at least one parameter was queried successfully
            }
        };

        // If no parameters to query, immediately call callback
        if (standardParameters.empty())
        {
            callback(true);
            return;
        }

        // Query each parameter
        for (const auto &parameter : standardParameters)
        {
            m_oscController->send(parameter, {},
                                  [this, parameter, &deviceState, queryHandler](bool success, const std::vector<std::any> &response)
                                  {
                                      if (success && !response.empty())
                                      {
                                          try
                                          {
                                              // Store the parameter in the device state
                                              // Convert the OSC parameter name to a property name
                                              std::string propName = parameter;
                                              // Remove leading slash
                                              if (!propName.empty() && propName[0] == '/')
                                              {
                                                  propName = propName.substr(1);
                                              }
                                              // Replace remaining slashes with underscores
                                              std::replace(propName.begin(), propName.end(), '/', '_');

                                              // Convert the value to string and store as property
                                              if (response[0].type() == typeid(int))
                                              {
                                                  int value = std::any_cast<int>(response[0]);
                                                  deviceState.setProperty(propName, std::to_string(value));

                                                  // Special handling for certain parameters
                                                  if (parameter == "/device/channels/input/count")
                                                  {
                                                      deviceState.setInputChannelCount(value);
                                                  }
                                                  else if (parameter == "/device/channels/output/count")
                                                  {
                                                      deviceState.setOutputChannelCount(value);
                                                  }
                                                  else if (parameter == "/device/buffersize")
                                                  {
                                                      deviceState.setBufferSize(value);
                                                  }
                                              }
                                              else if (response[0].type() == typeid(float))
                                              {
                                                  float value = std::any_cast<float>(response[0]);
                                                  deviceState.setProperty(propName, std::to_string(value));

                                                  // Special handling for sample rate
                                                  if (parameter == "/device/samplerate")
                                                  {
                                                      deviceState.setSampleRate(value);
                                                  }
                                              }
                                              else if (response[0].type() == typeid(double))
                                              {
                                                  double value = std::any_cast<double>(response[0]);
                                                  deviceState.setProperty(propName, std::to_string(value));

                                                  // Special handling for sample rate
                                                  if (parameter == "/device/samplerate")
                                                  {
                                                      deviceState.setSampleRate(value);
                                                  }
                                              }
                                              else if (response[0].type() == typeid(std::string))
                                              {
                                                  std::string value = std::any_cast<std::string>(response[0]);
                                                  deviceState.setProperty(propName, value);

                                                  // Special handling for device type
                                                  if (parameter == "/device/type")
                                                  {
                                                      if (value.find("RME") != std::string::npos ||
                                                          value.find("Fireface") != std::string::npos ||
                                                          value.find("Babyface") != std::string::npos ||
                                                          value.find("Digiface") != std::string::npos)
                                                      {
                                                          deviceState.setProperty("is_rme", "true");
                                                      }
                                                  }
                                              }

                                              queryHandler(true);
                                          }
                                          catch (const std::exception &e)
                                          {
                                              std::cerr << "Error processing OSC parameter " << parameter << ": " << e.what() << std::endl;
                                              queryHandler(false);
                                          }
                                      }
                                      else
                                      {
                                          queryHandler(false);
                                      }
                                  });
        }
    }

    bool OscDeviceInterface::applyConfiguration(const std::string &deviceName,
                                                const Configuration &config,
                                                std::function<void(bool, const std::string &)> callback)
    {
        if (!m_oscController)
        {
            callback(false, "OscController is null");
            return false;
        }

        // Lambda to handle the asynchronous configuration application
        std::thread([this, deviceName, config, callback]()
                    {
            // First, update the OSC connection parameters if necessary
            if (!config.getTargetIp().empty() &&
                (m_oscController->getTargetIp() != config.getTargetIp() ||
                 m_oscController->getTargetPort() != config.getTargetPort())) {

                if (!m_oscController->configure(config.getTargetIp(), config.getTargetPort())) {
                    callback(false, "Failed to configure OSC connection");
                    return;
                }
            }

            // Send all OSC commands from the configuration
            sendOscCommands(config, callback); })
            .detach();

        return true;
    }

    void OscDeviceInterface::sendOscCommands(const Configuration &config,
                                             std::function<void(bool, const std::string &)> callback)
    {
        const auto &commands = config.getCommands();

        // If no commands, success by default
        if (commands.empty())
        {
            callback(true, "No commands to send");
            return;
        }

        // Track progress of sending commands
        auto commandCount = std::make_shared<int>(commands.size());
        auto failedCommands = std::make_shared<int>(0);
        auto errorMessages = std::make_shared<std::string>();

        for (const auto &cmd : commands)
        {
            m_oscController->send(cmd.address, cmd.args,
                                  [commandCount, failedCommands, errorMessages, callback, cmd](bool success, const std::vector<std::any> &response)
                                  {
                                      if (!success)
                                      {
                                          (*failedCommands)++;
                                          if (!errorMessages->empty())
                                          {
                                              *errorMessages += "; ";
                                          }
                                          *errorMessages += "Failed to send command to " + cmd.address;
                                      }

                                      (*commandCount)--;
                                      if (*commandCount <= 0)
                                      {
                                          // All commands have been processed
                                          bool overallSuccess = (*failedCommands == 0);
                                          callback(overallSuccess, *errorMessages);
                                      }
                                  });
        }
    }

} // namespace AudioEngine
