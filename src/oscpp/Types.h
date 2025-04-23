#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <variant>
#include <optional>
#include <chrono>

namespace osc
{

    /**
     * @brief Exception class for OSC errors
     */
    class OSCException : public std::runtime_error
    {
    public:
        /**
         * @brief Error codes for OSC operations
         */
        enum class ErrorCode
        {
            Unknown,
            NetworkError,
            InvalidAddress,
            InvalidMessage,
            InvalidBundle,
            InvalidArgument,
            PatternError
        };

        /**
         * @brief Construct a new OSCException
         * @param code The error code
         * @param message The error message
         */
        OSCException(ErrorCode code, const std::string &message)
            : std::runtime_error(message), code_(code) {}

        /**
         * @brief Get the error code
         * @return The error code
         */
        ErrorCode code() const { return code_; }

    private:
        ErrorCode code_;
    };

    /**
     * @brief Protocol types for OSC communication
     */
    enum class Protocol
    {
        UDP, ///< UDP protocol (connectionless)
        TCP, ///< TCP protocol (connection-oriented)
        UNIX ///< UNIX domain socket (local IPC)
    };

    /**
     * @brief OSC Time Tag representation
     *
     * Uses NTP format: first 32 bits are seconds since Jan 1, 1900
     * and the second 32 bits are fractions of a second
     */
    class TimeTag
    {
    public:
        /**
         * @brief Construct a TimeTag with the immediate value (1)
         */
        TimeTag();

        /**
         * @brief Construct a TimeTag from NTP format value
         *
         * @param ntp 64-bit NTP timestamp
         */
        explicit TimeTag(uint64_t ntp);

        /**
         * @brief Construct a TimeTag from seconds and fractions
         *
         * @param seconds Seconds since Jan 1, 1900
         * @param fraction Fraction of a second (0 to 2^32-1)
         */
        TimeTag(uint32_t seconds, uint32_t fraction);

        /**
         * @brief Construct a TimeTag from a std::chrono time point
         *
         * @param tp Time point
         */
        explicit TimeTag(std::chrono::system_clock::time_point tp);

        /**
         * @brief Get the current time as a TimeTag
         *
         * @return TimeTag Current time
         */
        static TimeTag now();

        /**
         * @brief Get immediate time tag (1)
         *
         * @return TimeTag Immediate time tag
         */
        static TimeTag immediate();

        /**
         * @brief Convert to NTP format (64-bit unsigned integer)
         *
         * @return uint64_t NTP timestamp
         */
        uint64_t toNTP() const;

        /**
         * @brief Convert to std::chrono time point
         *
         * @return std::chrono::system_clock::time_point
         */
        std::chrono::system_clock::time_point toTimePoint() const;

        /**
         * @brief Get seconds part of the time tag
         *
         * @return uint32_t Seconds since Jan 1, 1900
         */
        uint32_t seconds() const;

        /**
         * @brief Get fraction part of the time tag
         *
         * @return uint32_t Fraction of a second
         */
        uint32_t fraction() const;

        /**
         * @brief Check if this is an immediate time tag
         *
         * @return true if immediate (1)
         */
        bool isImmediate() const;

        // Comparison operators
        bool operator==(const TimeTag &other) const;
        bool operator!=(const TimeTag &other) const;
        bool operator<(const TimeTag &other) const;
        bool operator>(const TimeTag &other) const;
        bool operator<=(const TimeTag &other) const;
        bool operator>=(const TimeTag &other) const;

    private:
        uint32_t seconds_;
        uint32_t fraction_;
    };

    /**
     * @brief OSC Blob type for binary data
     */
    class Blob
    {
    public:
        /**
         * @brief Construct an empty Blob
         */
        Blob();

        /**
         * @brief Construct a Blob with data
         *
         * @param data Binary data
         */
        explicit Blob(const std::vector<std::byte> &data);

        /**
         * @brief Construct a Blob with data
         *
         * @param data Pointer to binary data
         * @param size Size of data in bytes
         */
        Blob(const void *data, size_t size);

        /**
         * @brief Get the raw data
         *
         * @return const std::vector<std::byte>& Reference to data
         */
        const std::vector<std::byte> &data() const;

        /**
         * @brief Get the size of the blob in bytes
         *
         * @return size_t Size in bytes
         */
        size_t size() const;

        /**
         * @brief Get pointer to raw data
         *
         * @return const void* Pointer to data
         */
        const void *ptr() const;

    private:
        std::vector<std::byte> data_;
    };

    /**
     * @brief Forward declaration for variant
     */
    class Value;

    /**
     * @brief Type for OSC array values
     */
    using Array = std::vector<Value>;

    /**
     * @brief OSC Value type using std::variant
     *
     * Represents any valid OSC value type as a type-safe union
     */
    class Value
    {
    public:
        // Default constructor creates a nil value
        Value();

        // Construct from various types
        Value(int32_t value);
        Value(int64_t value);
        Value(float value);
        Value(double value);
        Value(const std::string &value);
        Value(const char *value);
        Value(const Blob &value);
        Value(const TimeTag &value);
        Value(char value);
        Value(uint32_t rgba);                      // RGBA color
        Value(const std::array<uint8_t, 4> &midi); // MIDI message
        Value(bool value);
        Value(const Array &array);

        // Type identification
        bool isInt32() const;
        bool isInt64() const;
        bool isFloat() const;
        bool isDouble() const;
        bool isString() const;
        bool isSymbol() const;
        bool isBlob() const;
        bool isTimeTag() const;
        bool isChar() const;
        bool isColor() const;
        bool isMidi() const;
        bool isBool() const;
        bool isTrue() const;
        bool isFalse() const;
        bool isNil() const;
        bool isInfinitum() const;
        bool isArray() const;

        // Value access (throws if wrong type)
        int32_t asInt32() const;
        int64_t asInt64() const;
        float asFloat() const;
        double asDouble() const;
        std::string asString() const;
        std::string asSymbol() const;
        Blob asBlob() const;
        TimeTag asTimeTag() const;
        char asChar() const;
        uint32_t asColor() const;
        std::array<uint8_t, 4> asMidi() const;
        bool asBool() const;
        Array asArray() const;

        // Type tag character for this value
        char typeTag() const;

        // Serialize to bytes
        std::vector<std::byte> serialize() const;

    private:
        // Variant to hold the actual value
        std::variant<
            std::monostate,         // Nil
            int32_t,                // i
            int64_t,                // h
            float,                  // f
            double,                 // d
            std::string,            // s
            std::string,            // S (symbol)
            Blob,                   // b
            TimeTag,                // t
            char,                   // c
            uint32_t,               // r (RGBA color)
            std::array<uint8_t, 4>, // m (MIDI)
            bool,                   // T/F
            bool,                   // I (Infinitum - stored as bool)
            Array                   // Array
            >
            value_;
    };

    // Forward declarations for main classes
    class Message;
    class Bundle;
    class Address;
    class Server;
    class ServerThread;
    class Method;

} // namespace osc
