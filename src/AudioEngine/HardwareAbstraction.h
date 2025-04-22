#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace AudioEngine
{

    /**
     * @brief Defines a hardware device type
     */
    enum class DeviceType
    {
        AudioInput,
        AudioOutput,
        MidiInput,
        MidiOutput
    };

    /**
     * @brief Represents device capabilities
     */
    struct DeviceCapabilities
    {
        bool supportsBufferSizeChange;
        bool supportsSampleRateChange;
        bool supportsAsio;
        bool supportsDirectSound;
        std::vector<int> supportedBufferSizes;
        std::vector<double> supportedSampleRates;
        int channelCount;
    };

    /**
     * @brief Abstract base class for hardware device access
     */
    class HardwareAbstraction
    {
    public:
        /**
         * @brief Virtual destructor
         */
        virtual ~HardwareAbstraction() = default;

        /**
         * @brief Initialize the hardware abstraction layer
         *
         * @return true if initialization was successful
         */
        virtual bool initialize() = 0;

        /**
         * @brief Scan for available audio devices
         *
         * @return true if scan was successful
         */
        virtual bool scanDevices() = 0;

        /**
         * @brief Get list of available device names by type
         *
         * @param type Type of device to list
         * @return std::vector<std::string> List of device names
         */
        virtual std::vector<std::string> getDeviceList(DeviceType type) = 0;

        /**
         * @brief Get device capabilities
         *
         * @param deviceName Name of the device
         * @param type Type of the device
         * @return DeviceCapabilities Capabilities of the device
         */
        virtual DeviceCapabilities getDeviceCapabilities(const std::string &deviceName, DeviceType type) = 0;

        /**
         * @brief Register a callback for device hotplug events
         *
         * @param callback Function to call when devices are added or removed
         * @return int Callback ID used to remove the callback later
         */
        virtual int registerDeviceChangeCallback(std::function<void()> callback) = 0;

        /**
         * @brief Unregister a device change callback
         *
         * @param callbackId ID of the callback to remove
         */
        virtual void unregisterDeviceChangeCallback(int callbackId) = 0;
    };

    /**
     * @brief Factory function to create appropriate hardware abstraction implementation
     *
     * @return std::unique_ptr<HardwareAbstraction> A new hardware abstraction instance
     */
    std::unique_ptr<HardwareAbstraction> createHardwareAbstraction();

} // namespace AudioEngine

// New class in HardwareAbstraction.h
class DeviceStateInterface
{
public:
    /**
     * @brief Query current device state
     * @param deviceName Name of the device
     * @param callback Callback to be called with device state
     * @return bool True if query was initiated
     */
    virtual bool queryState(const std::string &deviceName, std::function<void(bool, const DeviceState &)> callback) = 0;

    /**
     * @brief Apply configuration to device
     * @param deviceName Name of the device
     * @param config Configuration to apply
     * @return bool True if configuration was applied
     */
    virtual bool applyConfiguration(const std::string &deviceName, const Configuration &config) = 0;

    /**
     * @brief Convert device state to configuration
     * @param state Current device state
     * @return Configuration Configuration representing current state
     */
    static Configuration stateToConfiguration(const DeviceState &state);
};

// New class in HardwareAbstraction.h
class HardwareCapabilities
{
public:
    /**
     * @brief Get available ASIO devices
     * @return std::vector<std::string> List of device names
     */
    static std::vector<std::string> getAsioDevices();

    /**
     * @brief Get device capabilities
     * @param deviceName Name of the device
     * @return std::map<std::string, std::any> Map of capability name to value
     */
    static std::map<std::string, std::any> getDeviceCapabilities(const std::string &deviceName);

    /**
     * @brief Query if a specific capability is supported
     * @param deviceName Name of the device
     * @param capability Capability to check
     * @return bool True if supported
     */
    static bool hasCapability(const std::string &deviceName, const std::string &capability);

    /**
     * @brief Get optimal device configuration
     * @param deviceName Name of the device
     * @return Configuration Optimal configuration for device
     */
    static Configuration getOptimalConfiguration(const std::string &deviceName);
};
