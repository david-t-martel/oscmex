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

// From HardwareAbstraction.h
class HardwareCapabilities
{
public:
    static std::vector<std::string> getAsioDevices();

    static std::map<std::string, std::any> getDeviceCapabilities(const std::string &deviceName);

    static bool hasCapability(const std::string &deviceName, const std::string &capability);

    static Configuration getOptimalConfiguration(const std::string &deviceName);
};
