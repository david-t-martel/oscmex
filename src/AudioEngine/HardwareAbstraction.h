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
