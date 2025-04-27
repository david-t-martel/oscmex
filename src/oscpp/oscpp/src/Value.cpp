// filepath: c:\codedev\auricleinc\oscmex\src\Value.cpp
/*
 * This file contains the implementation of the Value class,
 * which is responsible for handling various data types and
 * their associated operations.
 */

#include "Types.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <array>
#include <stdexcept>

namespace osc
{

    // Value Implementation

    // Default constructor (Nil)
    Value::Value() : value_(std::monostate{}) {}

    // Constructors for different types
    Value::Value(int32_t value) : value_(value) {}
    Value::Value(int64_t value) : value_(value) {}
    Value::Value(float value) : value_(value) {}
    Value::Value(double value) : value_(value) {}
    Value::Value(const std::string &value) : value_(value) {}
    Value::Value(const char *value) : value_(std::string(value)) {}
    Value::Value(const Blob &value) : value_(value) {}
    Value::Value(const TimeTag &value) : value_(value) {}
    Value::Value(char value) : value_(value) {}
    Value::Value(uint32_t rgba) : value_(rgba) {}
    Value::Value(const std::array<uint8_t, 4> &midi) : value_(midi) {}
    Value::Value(bool value) : value_(value) {}
    Value::Value(const Array &array) : value_(array) {}

    // Helper function to pad to 4-byte boundary
    inline size_t padSize(size_t size)
    {
        return (size + 3) & ~3;
    }

    // Helper function to add padding to a byte vector
    inline void addPadding(std::vector<std::byte> &data, size_t originalSize)
    {
        size_t paddedSize = padSize(originalSize);
        size_t paddingBytes = paddedSize - originalSize;

        if (paddingBytes > 0)
        {
            data.resize(data.size() + paddingBytes, std::byte{0});
        }
    }

    // Type checking methods
    bool Value::isInt32() const { return std::holds_alternative<int32_t>(value_); }
    bool Value::isInt64() const { return std::holds_alternative<int64_t>(value_); }
    bool Value::isFloat() const { return std::holds_alternative<float>(value_); }
    bool Value::isDouble() const { return std::holds_alternative<double>(value_); }
    bool Value::isString() const { return std::holds_alternative<std::string>(value_) && !isSymbol(); }
    bool Value::isSymbol() const
    {
        // TODO: Implement proper symbol detection
        // For now, assume no symbol type is present
        return false;
    }
    bool Value::isBlob() const { return std::holds_alternative<Blob>(value_); }
    bool Value::isTimeTag() const { return std::holds_alternative<TimeTag>(value_); }
    bool Value::isChar() const { return std::holds_alternative<char>(value_); }
    bool Value::isColor() const { return std::holds_alternative<uint32_t>(value_); }
    bool Value::isMidi() const { return std::holds_alternative<std::array<uint8_t, 4>>(value_); }
    bool Value::isBool() const { return std::holds_alternative<bool>(value_); }
    bool Value::isTrue() const { return isBool() && std::get<bool>(value_); }
    bool Value::isFalse() const { return isBool() && !std::get<bool>(value_); }
    bool Value::isNil() const { return std::holds_alternative<std::monostate>(value_); }
    bool Value::isInfinitum() const { return false; } // Placeholder implementation
    bool Value::isArray() const { return std::holds_alternative<Array>(value_); }

    // Value access methods (with type checking)
    int32_t Value::asInt32() const
    {
        if (!isInt32())
        {
            throw OSCException(OSCException::ErrorCode::InvalidArgument, "Value is not an Int32");
        }
        return std::get<int32_t>(value_);
    }

    int64_t Value::asInt64() const
    {
        if (!isInt64())
        {
            throw OSCException(OSCException::ErrorCode::InvalidArgument, "Value is not an Int64");
        }
        return std::get<int64_t>(value_);
    }

    float Value::asFloat() const
    {
        if (!isFloat())
        {
            throw OSCException(OSCException::ErrorCode::InvalidArgument, "Value is not a Float");
        }
        return std::get<float>(value_);
    }

    double Value::asDouble() const
    {
        if (!isDouble())
        {
            throw OSCException(OSCException::ErrorCode::InvalidArgument, "Value is not a Double");
        }
        return std::get<double>(value_);
    }

    std::string Value::asString() const
    {
        if (!isString())
        {
            throw OSCException(OSCException::ErrorCode::InvalidArgument, "Value is not a String");
        }
        return std::get<std::string>(value_);
    }

    std::string Value::asSymbol() const
    {
        if (!isSymbol())
        {
            throw OSCException(OSCException::ErrorCode::InvalidArgument, "Value is not a Symbol");
        }
        return std::get<std::string>(value_);
    }

    Blob Value::asBlob() const
    {
        if (!isBlob())
        {
            throw OSCException(OSCException::ErrorCode::InvalidArgument, "Value is not a Blob");
        }
        return std::get<Blob>(value_);
    }

    TimeTag Value::asTimeTag() const
    {
        if (!isTimeTag())
        {
            throw OSCException(OSCException::ErrorCode::InvalidArgument, "Value is not a TimeTag");
        }
        return std::get<TimeTag>(value_);
    }

    char Value::asChar() const
    {
        if (!isChar())
        {
            throw OSCException(OSCException::ErrorCode::InvalidArgument, "Value is not a Char");
        }
        return std::get<char>(value_);
    }

    uint32_t Value::asColor() const
    {
        if (!isColor())
        {
            throw OSCException(OSCException::ErrorCode::InvalidArgument, "Value is not a Color");
        }
        return std::get<uint32_t>(value_);
    }

    std::array<uint8_t, 4> Value::asMidi() const
    {
        if (!isMidi())
        {
            throw OSCException(OSCException::ErrorCode::InvalidArgument, "Value is not a MIDI message");
        }
        return std::get<std::array<uint8_t, 4>>(value_);
    }

    bool Value::asBool() const
    {
        if (!isBool())
        {
            throw OSCException(OSCException::ErrorCode::InvalidArgument, "Value is not a Boolean");
        }
        return std::get<bool>(value_);
    }

    Array Value::asArray() const
    {
        if (!isArray())
        {
            throw OSCException(OSCException::ErrorCode::InvalidArgument, "Value is not an Array");
        }
        return std::get<Array>(value_);
    }

    // Get OSC type tag character
    char Value::typeTag() const
    {
        if (isInt32())
            return 'i';
        if (isInt64())
            return 'h';
        if (isFloat())
            return 'f';
        if (isDouble())
            return 'd';
        if (isString())
            return 's';
        if (isSymbol())
            return 'S';
        if (isBlob())
            return 'b';
        if (isTimeTag())
            return 't';
        if (isChar())
            return 'c';
        if (isColor())
            return 'r';
        if (isMidi())
            return 'm';
        if (isTrue())
            return 'T';
        if (isFalse())
            return 'F';
        if (isNil())
            return 'N';
        if (isInfinitum())
            return 'I';
        if (isArray())
            return '['; // Opening bracket for array

        // Default case - shouldn't reach here if value_ is valid
        throw OSCException(OSCException::ErrorCode::InvalidArgument,
                           "Unknown value type");
    }

    // Serialize the value to OSC binary format
    std::vector<std::byte> Value::serialize() const
    {
        std::vector<std::byte> result;

        if (isInt32())
        {
            int32_t val = asInt32();
            int32_t bigEndian = htonl(val);
            const std::byte *bytes = reinterpret_cast<const std::byte *>(&bigEndian);
            result.insert(result.end(), bytes, bytes + sizeof(int32_t));
        }
        else if (isInt64())
        {
            int64_t val = asInt64();
            // Convert to network byte order (big-endian)
            int64_t bigEndian = ((int64_t)htonl((int32_t)(val >> 32)) |
                                 ((int64_t)htonl((int32_t)val) << 32));
            const std::byte *bytes = reinterpret_cast<const std::byte *>(&bigEndian);
            result.insert(result.end(), bytes, bytes + sizeof(int64_t));
        }
        else if (isFloat())
        {
            float val = asFloat();
            // Treat as 32-bit int for byte order conversion
            union
            {
                float f;
                int32_t i;
            } converter;
            converter.f = val;
            int32_t bigEndian = htonl(converter.i);
            const std::byte *bytes = reinterpret_cast<const std::byte *>(&bigEndian);
            result.insert(result.end(), bytes, bytes + sizeof(float));
        }
        else if (isDouble())
        {
            double val = asDouble();
            // Convert to network byte order (big-endian)
            union
            {
                double d;
                int64_t i;
            } converter;
            converter.d = val;
            int64_t bigEndian = ((int64_t)htonl((int32_t)(converter.i >> 32)) |
                                 ((int64_t)htonl((int32_t)converter.i) << 32));
            const std::byte *bytes = reinterpret_cast<const std::byte *>(&bigEndian);
            result.insert(result.end(), bytes, bytes + sizeof(double));
        }
        else if (isString() || isSymbol())
        {
            std::string str = isString() ? asString() : asSymbol();
            // Include null terminator
            size_t dataSize = str.size() + 1;
            const std::byte *bytes = reinterpret_cast<const std::byte *>(str.c_str());
            result.insert(result.end(), bytes, bytes + dataSize);
            // Add padding bytes to align to 4-byte boundary
            addPadding(result, dataSize);
        }
        else if (isBlob())
        {
            Blob blob = asBlob();
            // Size prefix (big-endian)
            int32_t size = static_cast<int32_t>(blob.size());
            int32_t sizeBigEndian = htonl(size);
            const std::byte *sizeBytes = reinterpret_cast<const std::byte *>(&sizeBigEndian);
            result.insert(result.end(), sizeBytes, sizeBytes + sizeof(int32_t));

            // Blob data
            const std::byte *blobBytes = static_cast<const std::byte *>(blob.ptr());
            result.insert(result.end(), blobBytes, blobBytes + blob.size());

            // Add padding bytes to align to 4-byte boundary
            addPadding(result, blob.size());
        }
        else if (isTimeTag())
        {
            TimeTag tag = asTimeTag();
            uint64_t ntp = tag.toNTP();
            // Convert to network byte order (big-endian)
            uint64_t bigEndian = ((uint64_t)htonl((uint32_t)(ntp >> 32)) |
                                  ((uint64_t)htonl((uint32_t)ntp) << 32));
            const std::byte *bytes = reinterpret_cast<const std::byte *>(&bigEndian);
            result.insert(result.end(), bytes, bytes + sizeof(uint64_t));
        }
        else if (isChar())
        {
            // Encode character as int32
            int32_t val = static_cast<int32_t>(asChar());
            int32_t bigEndian = htonl(val);
            const std::byte *bytes = reinterpret_cast<const std::byte *>(&bigEndian);
            result.insert(result.end(), bytes, bytes + sizeof(int32_t));
        }
        else if (isColor())
        {
            uint32_t val = asColor();
            uint32_t bigEndian = htonl(val);
            const std::byte *bytes = reinterpret_cast<const std::byte *>(&bigEndian);
            result.insert(result.end(), bytes, bytes + sizeof(uint32_t));
        }
        else if (isMidi())
        {
            std::array<uint8_t, 4> midi = asMidi();
            const std::byte *bytes = reinterpret_cast<const std::byte *>(midi.data());
            result.insert(result.end(), bytes, bytes + 4);
        }
        else if (isArray())
        {
            // Arrays are serialized as a sequence of values
            Array array = asArray();
            for (const auto &elem : array)
            {
                auto elemBytes = elem.serialize();
                result.insert(result.end(), elemBytes.begin(), elemBytes.end());
            }
        }
        // T, F, N, I have no data payload

        return result;
    }

} // namespace osc
