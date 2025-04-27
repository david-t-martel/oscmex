// filepath: c:\codedev\auricleinc\oscmex\src\oscpp\include\osc\Types.h
/*
 *  OSCPP - Open Sound Control C++ (OSCPP) Library.
 *  This header file defines various types used in the OSC library,
 *  including the TimeTag, Blob, and Value classes.
 */

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
        uint32_t seconds() const;
        uint32_t fraction() const;
        bool isImmediate() const;
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
        const std::vector<std::byte> &data() const;
        size_t size() const;
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
        char typeTag() const;
        std::vector<std::byte> serialize() const;

    private:
        std::variant<
            std::monostate,            int32_t,            int64_t,            float,            double,            std::string,            std::string,            Blob,            TimeTag,            char,            uint32_t,            std::array<uint8_t, 4>,            bool,            bool,            Array            >
            value_;
    };

    // Forward declarations for main classes
    class Message;
    class Bundle;
    class Address;
    class Server;
    class ServerThread;
    class Method;

    using MethodId = int;

} // namespace osc