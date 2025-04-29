/*
 *  OSCPP - Open Sound Control C++ (OSCPP) Library.
 *  This header file defines core types used throughout the OSCPP library.
 */

#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "osc/Exceptions.h"

namespace osc {

// Socket type definitions for cross-platform compatibility
#ifdef _WIN32
// Windows socket definitions
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <WS2tcpip.h>
#include <WinSock2.h>
    using SOCKET_TYPE = SOCKET;
#define INVALID_SOCKET_VALUE INVALID_SOCKET
#define SOCKET_ERROR_VALUE SOCKET_ERROR
#define CLOSE_SOCKET closesocket
#else
// POSIX socket definitions
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
    using SOCKET_TYPE = int;
#define INVALID_SOCKET_VALUE -1
#define SOCKET_ERROR_VALUE -1
#define CLOSE_SOCKET close
#endif

    // Forward declarations
    class Message;
    class Bundle;

    /**
     * @brief Enumeration of supported transport protocols
     */
    enum class Protocol {
        UDP,  ///< User Datagram Protocol
        TCP,  ///< Transmission Control Protocol
        UNIX  ///< Unix Domain Socket
    };

    /**
     * @brief Class representing an OSC Time Tag
     *
     * OSC Time Tags are 64-bit fixed-point numbers representing
     * time in NTP format (seconds since Jan 1, 1900).
     */
    class TimeTag {
       public:
        /**
         * @brief Default constructor (creates an immediate time tag)
         */
        TimeTag();

        /**
         * @brief Construct from NTP format (64-bit)
         * @param ntp NTP timestamp
         */
        explicit TimeTag(uint64_t ntp);

        /**
         * @brief Construct from seconds and fraction
         * @param seconds Seconds since Jan 1, 1900
         * @param fraction Fractional seconds (0-0xFFFFFFFF)
         */
        TimeTag(uint32_t seconds, uint32_t fraction);

        /**
         * @brief Construct from std::chrono::system_clock::time_point
         * @param tp Time point
         */
        explicit TimeTag(std::chrono::system_clock::time_point tp);

        /**
         * @brief Get current time as TimeTag
         * @return TimeTag for current time
         */
        static TimeTag now();

        /**
         * @brief Get immediate execution time tag (special value)
         * @return TimeTag for immediate execution
         */
        static TimeTag immediate();

        /**
         * @brief Convert to NTP format
         * @return 64-bit NTP timestamp
         */
        uint64_t toNTP() const;

        /**
         * @brief Convert to std::chrono::system_clock::time_point
         * @return Time point
         */
        std::chrono::system_clock::time_point toTimePoint() const;

        /**
         * @brief Get seconds part of time tag
         * @return Seconds since Jan 1, 1900
         */
        uint32_t seconds() const;

        /**
         * @brief Get fraction part of time tag
         * @return Fraction in NTP format
         */
        uint32_t fraction() const;

        /**
         * @brief Check if this is an immediate time tag
         * @return true if immediate, false otherwise
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
        uint32_t seconds_;   ///< Seconds since Jan 1, 1900
        uint32_t fraction_;  ///< Fractional seconds (0-0xFFFFFFFF)
    };

    /**
     * @brief Class representing an OSC Blob
     *
     * OSC Blobs are binary data with a specified size.
     */
    class Blob {
       public:
        /**
         * @brief Default constructor (empty blob)
         */
        Blob() = default;

        /**
         * @brief Construct from data
         * @param data Binary data
         */
        explicit Blob(std::vector<std::byte> data);

        /**
         * @brief Construct from raw data
         * @param data Pointer to data
         * @param size Size of data in bytes
         */
        Blob(const void *data, size_t size);

        /**
         * @brief Get the data
         * @return Reference to internal data vector
         */
        const std::vector<std::byte> &data() const;

        /**
         * @brief Get the data size
         * @return Size in bytes
         */
        size_t size() const;

        /**
         * @brief Get pointer to raw data
         * @return Const pointer to data
         */
        const std::byte *bytes() const;

       private:
        std::vector<std::byte> data_;
    };

    /**
     * @brief Structure for MIDI message (OSC type 'm')
     */
    struct MIDIMessage {
        std::array<uint8_t, 4> bytes;

        MIDIMessage() : bytes{0, 0, 0, 0} {}
        MIDIMessage(uint8_t port, uint8_t status, uint8_t data1, uint8_t data2)
            : bytes{port, status, data1, data2} {}
    };

    /**
     * @brief Structure for RGBA color (OSC type 'r')
     */
    struct RGBAColor {
        uint8_t r, g, b, a;

        RGBAColor() : r(0), g(0), b(0), a(0) {}
        RGBAColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
            : r(red), g(green), b(blue), a(alpha) {}
    };

    /**
     * @brief Class representing an OSC value
     *
     * OSC values can be of various types, represented here as a variant.
     */
    class Value {
       public:
        // Type tag constants
        static constexpr char INT32_TAG = 'i';
        static constexpr char INT64_TAG = 'h';
        static constexpr char FLOAT_TAG = 'f';
        static constexpr char DOUBLE_TAG = 'd';
        static constexpr char STRING_TAG = 's';
        static constexpr char SYMBOL_TAG = 'S';
        static constexpr char BLOB_TAG = 'b';
        static constexpr char TRUE_TAG = 'T';
        static constexpr char FALSE_TAG = 'F';
        static constexpr char NIL_TAG = 'N';
        static constexpr char INFINITUM_TAG = 'I';
        static constexpr char TIMETAG_TAG = 't';
        static constexpr char CHAR_TAG = 'c';
        static constexpr char RGBA_TAG = 'r';
        static constexpr char MIDI_TAG = 'm';
        static constexpr char ARRAY_BEGIN_TAG = '[';
        static constexpr char ARRAY_END_TAG = ']';

        // Type definitions
        using Int32 = int32_t;
        using Int64 = int64_t;
        using Float = float;
        using Double = double;
        using String = std::string;
        using Symbol = std::string;
        using Bool = bool;
        using Char = char;
        using Nil = std::monostate;
        using Infinitum = std::monostate;

        // Variant definition for all possible OSC types
        using Variant = std::variant<Nil,          // N
                                     Bool,         // T, F
                                     Int32,        // i
                                     Int64,        // h
                                     Float,        // f
                                     Double,       // d
                                     String,       // s
                                     Symbol,       // S
                                     Blob,         // b
                                     TimeTag,      // t
                                     Char,         // c
                                     RGBAColor,    // r
                                     MIDIMessage,  // m
                                     Infinitum     // I
                                     >;

        // Default constructor (creates Nil value)
        Value() : value_(Nil{}), isArrayElement_(false) {}

        // Constructor from variant
        explicit Value(Variant value, bool isArrayElement = false)
            : value_(std::move(value)), isArrayElement_(isArrayElement) {}

        // Constructors for specific types
        explicit Value(Int32 value, bool isArrayElement = false);
        explicit Value(Int64 value, bool isArrayElement = false);
        explicit Value(Float value, bool isArrayElement = false);
        explicit Value(Double value, bool isArrayElement = false);
        explicit Value(const char *value, bool isArrayElement = false);
        explicit Value(String value, bool isArrayElement = false);
        explicit Value(Symbol value, bool isArrayElement = false);
        explicit Value(Blob value, bool isArrayElement = false);
        explicit Value(TimeTag value, bool isArrayElement = false);
        explicit Value(Char value, bool isArrayElement = false);
        explicit Value(RGBAColor value, bool isArrayElement = false);
        explicit Value(MIDIMessage value, bool isArrayElement = false);
        explicit Value(Bool value, bool isArrayElement = false);

        // Static methods for special values
        static Value nil(bool isArrayElement = false);
        static Value infinitum(bool isArrayElement = false);
        static Value trueBool(bool isArrayElement = false);
        static Value falseBool(bool isArrayElement = false);
        static Value arrayBegin();
        static Value arrayEnd();

        // Type checking
        bool isInt32() const;
        bool isInt64() const;
        bool isFloat() const;
        bool isDouble() const;
        bool isString() const;
        bool isSymbol() const;
        bool isBlob() const;
        bool isTimeTag() const;
        bool isChar() const;
        bool isRGBA() const;
        bool isMIDI() const;
        bool isBool() const;
        bool isTrue() const;
        bool isFalse() const;
        bool isNil() const;
        bool isInfinitum() const;
        bool isArrayElement() const;

        // Value accessors (with type checking)
        Int32 asInt32() const;
        Int64 asInt64() const;
        Float asFloat() const;
        Double asDouble() const;
        String asString() const;
        Symbol asSymbol() const;
        Blob asBlob() const;
        TimeTag asTimeTag() const;
        Char asChar() const;
        RGBAColor asRGBA() const;
        MIDIMessage asMIDI() const;
        Bool asBool() const;

        // Get the type tag for this value
        char typeTag() const;

        // Get the raw variant
        const Variant &variant() const;

        // Serialization methods
        void serialize(std::vector<std::byte> &buffer) const;
        static Value deserialize(const std::byte *&data, size_t &remainingSize, char typeTag);

       private:
        Variant value_;
        bool isArrayElement_;
    };

    // Type alias for a method ID
    using MethodId = int;

    // Method structure
    struct Method {
        std::string pathPattern;
        std::string typeSpec;
        std::function<void(const Message &)> handler;
        MethodId id;
    };

}  // namespace osc
