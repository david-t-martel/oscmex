// In DeviceState.h (new file)
/**
 * @brief Represents the current state of a device
 */
class DeviceState
{
public:
    /**
     * @brief Get state value by parameter name
     * @param paramName Parameter name
     * @return std::any Parameter value
     */
    std::any getValue(const std::string &paramName) const;

    /**
     * @brief Set state value
     * @param paramName Parameter name
     * @param value Parameter value
     */
    void setValue(const std::string &paramName, const std::any &value);

    /**
     * @brief Check if state has a parameter
     * @param paramName Parameter name
     * @return bool True if parameter exists
     */
    bool hasParameter(const std::string &paramName) const;

    /**
     * @brief Get all state parameters
     * @return std::map<std::string, std::any> Map of all parameters
     */
    const std::map<std::string, std::any> &getAllParameters() const;

    /**
     * @brief Calculate differences from another state
     * @param other Other state to compare with
     * @return std::map<std::string, std::any> Map of differences
     */
    std::map<std::string, std::any> diffFrom(const DeviceState &other) const;

    /**
     * @brief Convert device state to configuration
     * @return Configuration Configuration representing this state
     */
    Configuration toConfiguration() const;

private:
    std::map<std::string, std::any> m_parameters;
};
