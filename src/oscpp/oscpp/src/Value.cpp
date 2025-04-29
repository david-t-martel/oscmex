// filepath: c:\codedev\auricleinc\oscmex\src\Value.cpp
/*
 * This file contains the implementation of the Value class,
 * which is responsible for handling various data types and
 * their associated operations.
 */

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "osc/Exceptions.h"  // For OSCException
#include "osc/Message.h"     // For Message class
#include "osc/Types.h"       // For Value class definition and constants

#ifdef _WIN32
#include <winsock2.h>  // For htonl, ntohl, htonll, ntohll
#else
#include <arpa/inet.h>  // For htonl, ntohl
#endif

namespace osc {
    // Maximum allowed size for OSC blobs to prevent excessive memory allocation
    // 32MB is a reasonable upper limit for most applications
    constexpr size_t MAX_BLOB_SIZE = 33554432;  // 32 MiB

    // Helper function to pad to 4-byte boundary
    inline size_t padSize(size_t size) { return (size + 3) & ~3; }

    // Helper functions for endianness conversion
    inline uint32_t bigEndian(uint32_t x) { return htonl(x); }

    inline int32_t bigEndian(int32_t x) {
        return static_cast<int32_t>(htonl(static_cast<uint32_t>(x)));
    }

    inline uint64_t bigEndian(uint64_t x) {
#ifdef _WIN32
        // Windows doesn't have htonll by default
        const uint32_t hi = htonl(static_cast<uint32_t>(x >> 32));
        const uint32_t lo = htonl(static_cast<uint32_t>(x & 0xFFFFFFFF));
        return (static_cast<uint64_t>(lo) << 32) | hi;
#else
        return htobe64(x);
#endif
    }

    inline int64_t bigEndian(int64_t x) {
        return static_cast<int64_t>(bigEndian(static_cast<uint64_t>(x)));
    }

    inline float bigEndian(float x) {
        uint32_t temp;
        std::memcpy(&temp, &x, sizeof(temp));
        temp = bigEndian(temp);
        std::memcpy(&x, &temp, sizeof(x));
        return x;
    }

    inline double bigEndian(double x) {
        uint64_t temp;
        std::memcpy(&temp, &x, sizeof(temp));
        temp = bigEndian(temp);
        std::memcpy(&x, &temp, sizeof(x));
        return x;
    }

    // Constructors for specific types
    Value::Value(Int32 value, bool isArrayElement)
        : value_(value), isArrayElement_(isArrayElement) {}

    Value::Value(Int64 value, bool isArrayElement)
        : value_(value), isArrayElement_(isArrayElement) {}

    Value::Value(Float value, bool isArrayElement)
        : value_(value), isArrayElement_(isArrayElement) {}

    Value::Value(Double value, bool isArrayElement)
        : value_(value), isArrayElement_(isArrayElement) {}

    Value::Value(const char* value, bool isArrayElement)
        : value_(String(value)), isArrayElement_(isArrayElement) {}

    Value::Value(String value, bool isArrayElement)
        : value_(std::move(value)), isArrayElement_(isArrayElement) {}

    Value::Value(Symbol value, bool isArrayElement)
        : value_(std::move(value)), isArrayElement_(isArrayElement) {}

    Value::Value(Blob value, bool isArrayElement)
        : value_(std::move(value)), isArrayElement_(isArrayElement) {}

    Value::Value(TimeTag value, bool isArrayElement)
        : value_(std::move(value)), isArrayElement_(isArrayElement) {}

    Value::Value(Char value, bool isArrayElement)
        : value_(value), isArrayElement_(isArrayElement) {}

    Value::Value(RGBAColor value, bool isArrayElement)
        : value_(value), isArrayElement_(isArrayElement) {}

    Value::Value(MIDIMessage value, bool isArrayElement)
        : value_(value), isArrayElement_(isArrayElement) {}

    Value::Value(Bool value, bool isArrayElement)
        : value_(value), isArrayElement_(isArrayElement) {}

    // Static methods for special values
    Value Value::nil(bool isArrayElement) {
        Value val;
        val.value_ = Nil{};
        val.isArrayElement_ = isArrayElement;
        return val;
    }

    Value Value::infinitum(bool isArrayElement) {
        Value val;
        val.value_ = Infinitum{};
        val.isArrayElement_ = isArrayElement;
        return val;
    }

    Value Value::trueBool(bool isArrayElement) { return Value(true, isArrayElement); }

    Value Value::falseBool(bool isArrayElement) { return Value(false, isArrayElement); }

    Value Value::arrayBegin() {
        Value val;
        val.isArrayElement_ = true;
        return val;
    }

    Value Value::arrayEnd() {
        Value val;
        val.isArrayElement_ = true;
        return val;
    }

    // Type checking methods
    bool Value::isInt32() const { return std::holds_alternative<Int32>(value_); }
    bool Value::isInt64() const { return std::holds_alternative<Int64>(value_); }
    bool Value::isFloat() const { return std::holds_alternative<Float>(value_); }
    bool Value::isDouble() const { return std::holds_alternative<Double>(value_); }
    bool Value::isString() const { return std::holds_alternative<String>(value_); }
    bool Value::isSymbol() const { return std::holds_alternative<Symbol>(value_); }
    bool Value::isBlob() const { return std::holds_alternative<Blob>(value_); }
    bool Value::isTimeTag() const { return std::holds_alternative<TimeTag>(value_); }
    bool Value::isChar() const { return std::holds_alternative<Char>(value_); }
    bool Value::isRGBA() const { return std::holds_alternative<RGBAColor>(value_); }
    bool Value::isMIDI() const { return std::holds_alternative<MIDIMessage>(value_); }
    bool Value::isBool() const { return std::holds_alternative<Bool>(value_); }
    bool Value::isTrue() const { return isBool() && std::get<Bool>(value_); }
    bool Value::isFalse() const { return isBool() && !std::get<Bool>(value_); }
    bool Value::isNil() const {
        return std::holds_alternative<Nil>(value_) && !std::holds_alternative<Infinitum>(value_);
    }
    bool Value::isInfinitum() const {
        return std::holds_alternative<Infinitum>(value_) && !std::holds_alternative<Nil>(value_);
    }
    bool Value::isArrayElement() const { return isArrayElement_; }

    // Value accessors
    Value::Int32 Value::asInt32() const {
        if (!isInt32()) throw TypeMismatchException("Value is not an Int32");
        return std::get<Int32>(value_);
    }

    Value::Int64 Value::asInt64() const {
        if (!isInt64()) throw TypeMismatchException("Value is not an Int64");
        return std::get<Int64>(value_);
    }

    Value::Float Value::asFloat() const {
        if (!isFloat()) throw TypeMismatchException("Value is not a Float");
        return std::get<Float>(value_);
    }

    Value::Double Value::asDouble() const {
        if (!isDouble()) throw TypeMismatchException("Value is not a Double");
        return std::get<Double>(value_);
    }

    Value::String Value::asString() const {
        if (!isString()) throw TypeMismatchException("Value is not a String");
        return std::get<String>(value_);
    }

    Value::Symbol Value::asSymbol() const {
        if (!isSymbol()) throw TypeMismatchException("Value is not a Symbol");
        return std::get<Symbol>(value_);
    }

    Blob Value::asBlob() const {
        if (!isBlob()) throw TypeMismatchException("Value is not a Blob");
        return std::get<Blob>(value_);
    }

    TimeTag Value::asTimeTag() const {
        if (!isTimeTag()) throw TypeMismatchException("Value is not a TimeTag");
        return std::get<TimeTag>(value_);
    }

    Value::Char Value::asChar() const {
        if (!isChar()) throw TypeMismatchException("Value is not a Char");
        return std::get<Char>(value_);
    }

    RGBAColor Value::asRGBA() const {
        if (!isRGBA()) throw TypeMismatchException("Value is not an RGBA color");
        return std::get<RGBAColor>(value_);
    }

    MIDIMessage Value::asMIDI() const {
        if (!isMIDI()) throw TypeMismatchException("Value is not a MIDI message");
        return std::get<MIDIMessage>(value_);
    }

    Value::Bool Value::asBool() const {
        if (!isBool()) throw TypeMismatchException("Value is not a Bool");
        return std::get<Bool>(value_);
    }

    char Value::typeTag() const {
        if (isInt32()) return INT32_TAG;
        if (isInt64()) return INT64_TAG;
        if (isFloat()) return FLOAT_TAG;
        if (isDouble()) return DOUBLE_TAG;
        if (isString()) return STRING_TAG;
        if (isSymbol()) return SYMBOL_TAG;
        if (isBlob()) return BLOB_TAG;
        if (isTimeTag()) return TIMETAG_TAG;
        if (isChar()) return CHAR_TAG;
        if (isRGBA()) return RGBA_TAG;
        if (isMIDI()) return MIDI_TAG;
        if (isTrue()) return TRUE_TAG;
        if (isFalse()) return FALSE_TAG;
        if (isNil()) return NIL_TAG;
        if (isInfinitum()) return INFINITUM_TAG;

        // Should never reach here if the variant is properly initialized
        throw UnknownTypeException("Unknown value type");
    }

    const Value::Variant& Value::variant() const { return value_; }

    // Serialization methods
    void Value::serialize(std::vector<std::byte>& buffer) const {
        try {
            switch (typeTag()) {
                case INT32_TAG: {
                    Int32 val = bigEndian(asInt32());
                    const std::byte* bytes = reinterpret_cast<const std::byte*>(&val);
                    buffer.insert(buffer.end(), bytes, bytes + sizeof(Int32));
                    break;
                }
                case INT64_TAG: {
                    Int64 val = bigEndian(asInt64());
                    const std::byte* bytes = reinterpret_cast<const std::byte*>(&val);
                    buffer.insert(buffer.end(), bytes, bytes + sizeof(Int64));
                    break;
                }
                case FLOAT_TAG: {
                    Float val = bigEndian(asFloat());
                    const std::byte* bytes = reinterpret_cast<const std::byte*>(&val);
                    buffer.insert(buffer.end(), bytes, bytes + sizeof(Float));
                    break;
                }
                case DOUBLE_TAG: {
                    Double val = bigEndian(asDouble());
                    const std::byte* bytes = reinterpret_cast<const std::byte*>(&val);
                    buffer.insert(buffer.end(), bytes, bytes + sizeof(Double));
                    break;
                }
                case STRING_TAG:
                case SYMBOL_TAG: {
                    const std::string& str = (typeTag() == STRING_TAG) ? asString() : asSymbol();
                    const size_t strSize = str.size() + 1;  // Include null terminator
                    const size_t paddedSize = padSize(strSize);

                    // Add the string with null terminator
                    const std::byte* bytes = reinterpret_cast<const std::byte*>(str.c_str());
                    buffer.insert(buffer.end(), bytes, bytes + strSize);

                    // Add padding
                    for (size_t i = strSize; i < paddedSize; ++i) {
                        buffer.push_back(std::byte{0});
                    }
                    break;
                }
                case BLOB_TAG: {
                    const Blob& blob = asBlob();
                    const size_t blobSize = blob.size();

                    // Add blob size (big endian)
                    Int32 size = bigEndian(static_cast<Int32>(blobSize));
                    const std::byte* sizeBytes = reinterpret_cast<const std::byte*>(&size);
                    buffer.insert(buffer.end(), sizeBytes, sizeBytes + sizeof(Int32));

                    // Add blob data
                    buffer.insert(buffer.end(), blob.bytes(), blob.bytes() + blobSize);

                    // Add padding if needed
                    const size_t paddedSize = padSize(blobSize);
                    for (size_t i = blobSize; i < paddedSize; ++i) {
                        buffer.push_back(std::byte{0});
                    }
                    break;
                }
                case TIMETAG_TAG: {
                    const TimeTag& tag = asTimeTag();
                    uint64_t ntp = bigEndian(tag.toNTP());
                    const std::byte* bytes = reinterpret_cast<const std::byte*>(&ntp);
                    buffer.insert(buffer.end(), bytes, bytes + sizeof(uint64_t));
                    break;
                }
                case CHAR_TAG: {
                    Int32 val = bigEndian(static_cast<Int32>(asChar()));
                    const std::byte* bytes = reinterpret_cast<const std::byte*>(&val);
                    buffer.insert(buffer.end(), bytes, bytes + sizeof(Int32));
                    break;
                }
                case RGBA_TAG: {
                    const RGBAColor& color = asRGBA();
                    uint32_t rgba = (static_cast<uint32_t>(color.r) << 24) |
                                    (static_cast<uint32_t>(color.g) << 16) |
                                    (static_cast<uint32_t>(color.b) << 8) |
                                    static_cast<uint32_t>(color.a);
                    rgba = bigEndian(rgba);
                    const std::byte* bytes = reinterpret_cast<const std::byte*>(&rgba);
                    buffer.insert(buffer.end(), bytes, bytes + sizeof(uint32_t));
                    break;
                }
                case MIDI_TAG: {
                    const MIDIMessage& midi = asMIDI();
                    buffer.push_back(std::byte{midi.bytes[0]});
                    buffer.push_back(std::byte{midi.bytes[1]});
                    buffer.push_back(std::byte{midi.bytes[2]});
                    buffer.push_back(std::byte{midi.bytes[3]});
                    break;
                }
                case TRUE_TAG:
                case FALSE_TAG:
                case NIL_TAG:
                case INFINITUM_TAG:
                    // These tags have no data
                    break;
                default:
                    throw UnknownTypeException("Cannot serialize unknown type '" +
                                               std::string(1, typeTag()) + "'");
            }
        } catch (const OSCException&) {
            // Re-throw OSC exceptions
            throw;
        } catch (const std::exception& e) {
            // Wrap other exceptions
            throw SerializationException("Error serializing value: " + std::string(e.what()));
        }
    }

    Value Value::deserialize(const std::byte*& data, size_t& remainingSize, char typeTag) {
        if (!data) {
            throw InvalidArgumentException("Null data pointer in Value::deserialize");
        }

        if (remainingSize == 0) {
            throw BufferOverflowException("No data available for deserializing type '" +
                                          std::string(1, typeTag) + "'");
        }

        try {
            switch (typeTag) {
                case INT32_TAG: {
                    if (remainingSize < sizeof(Int32)) {
                        throw BufferOverflowException("Not enough data for Int32");
                    }
                    Int32 val;
                    std::memcpy(&val, data, sizeof(Int32));
                    val = bigEndian(val);
                    data += sizeof(Int32);
                    remainingSize -= sizeof(Int32);
                    return Value(val);
                }
                case INT64_TAG: {
                    if (remainingSize < sizeof(Int64)) {
                        throw BufferOverflowException("Not enough data for Int64");
                    }
                    Int64 val;
                    std::memcpy(&val, data, sizeof(Int64));
                    val = bigEndian(val);
                    data += sizeof(Int64);
                    remainingSize -= sizeof(Int64);
                    return Value(val);
                }
                case FLOAT_TAG: {
                    if (remainingSize < sizeof(Float)) {
                        throw BufferOverflowException("Not enough data for Float");
                    }
                    Float val;
                    std::memcpy(&val, data, sizeof(Float));
                    val = bigEndian(val);
                    data += sizeof(Float);
                    remainingSize -= sizeof(Float);
                    return Value(val);
                }
                case DOUBLE_TAG: {
                    if (remainingSize < sizeof(Double)) {
                        throw BufferOverflowException("Not enough data for Double");
                    }
                    Double val;
                    std::memcpy(&val, data, sizeof(Double));
                    val = bigEndian(val);
                    data += sizeof(Double);
                    remainingSize -= sizeof(Double);
                    return Value(val);
                }
                case STRING_TAG:
                case SYMBOL_TAG: {
                    // Find the null terminator
                    const std::byte* nullByte = std::find(data, data + remainingSize, std::byte{0});
                    if (nullByte == data + remainingSize) {
                        throw MalformedPacketException("String has no null terminator");
                    }

                    // Calculate string length and padded size
                    size_t strLen = nullByte - data;
                    size_t paddedSize = padSize(strLen + 1);  // Include null terminator

                    if (remainingSize < paddedSize) {
                        throw BufferOverflowException("Not enough data for padded string");
                    }

                    // Create string from data
                    std::string str(reinterpret_cast<const char*>(data), strLen);

                    // Update data pointer and remaining size
                    data += paddedSize;
                    remainingSize -= paddedSize;

                    return (typeTag == STRING_TAG) ? Value(std::move(str))
                                                   : Value(Symbol(std::move(str)));
                }
                case BLOB_TAG: {
                    if (remainingSize < sizeof(Int32)) {
                        throw BufferOverflowException("Not enough data for blob size");
                    }

                    // Get blob size (big endian)
                    Int32 blobSize;
                    std::memcpy(&blobSize, data, sizeof(Int32));
                    blobSize = bigEndian(blobSize);

                    if (blobSize < 0) {
                        throw MalformedPacketException("Negative blob size");
                    }

                    if (static_cast<size_t>(blobSize) > MAX_BLOB_SIZE) {
                        throw MessageSizeException("Blob size too large");
                    }

                    data += sizeof(Int32);
                    remainingSize -= sizeof(Int32);

                    size_t paddedSize = padSize(static_cast<size_t>(blobSize));
                    if (remainingSize < paddedSize) {
                        throw BufferOverflowException("Not enough data for blob");
                    }

                    // Create blob from data
                    Blob blob(data, static_cast<size_t>(blobSize));

                    // Update data pointer and remaining size
                    data += paddedSize;
                    remainingSize -= paddedSize;

                    return Value(std::move(blob));
                }
                case TIMETAG_TAG: {
                    if (remainingSize < sizeof(uint64_t)) {
                        throw BufferOverflowException("Not enough data for timetag");
                    }

                    uint64_t ntp;
                    std::memcpy(&ntp, data, sizeof(uint64_t));
                    ntp = bigEndian(ntp);

                    data += sizeof(uint64_t);
                    remainingSize -= sizeof(uint64_t);

                    return Value(TimeTag(ntp));
                }
                case CHAR_TAG: {
                    if (remainingSize < sizeof(Int32)) {
                        throw BufferOverflowException("Not enough data for char");
                    }

                    Int32 val;
                    std::memcpy(&val, data, sizeof(Int32));
                    val = bigEndian(val);

                    data += sizeof(Int32);
                    remainingSize -= sizeof(Int32);

                    return Value(static_cast<Char>(val));
                }
                case RGBA_TAG: {
                    if (remainingSize < sizeof(uint32_t)) {
                        throw BufferOverflowException("Not enough data for RGBA");
                    }

                    uint32_t rgba;
                    std::memcpy(&rgba, data, sizeof(uint32_t));
                    rgba = bigEndian(rgba);

                    RGBAColor color(static_cast<uint8_t>((rgba >> 24) & 0xFF),
                                    static_cast<uint8_t>((rgba >> 16) & 0xFF),
                                    static_cast<uint8_t>((rgba >> 8) & 0xFF),
                                    static_cast<uint8_t>(rgba & 0xFF));

                    data += sizeof(uint32_t);
                    remainingSize -= sizeof(uint32_t);

                    return Value(color);
                }
                case MIDI_TAG: {
                    if (remainingSize < 4) {
                        throw BufferOverflowException("Not enough data for MIDI");
                    }

                    MIDIMessage midi(static_cast<uint8_t>(std::to_integer<uint8_t>(data[0])),
                                     static_cast<uint8_t>(std::to_integer<uint8_t>(data[1])),
                                     static_cast<uint8_t>(std::to_integer<uint8_t>(data[2])),
                                     static_cast<uint8_t>(std::to_integer<uint8_t>(data[3])));

                    data += 4;
                    remainingSize -= 4;

                    return Value(midi);
                }
                case TRUE_TAG:
                    return Value::trueBool();
                case FALSE_TAG:
                    return Value::falseBool();
                case NIL_TAG:
                    return Value::nil();
                case INFINITUM_TAG:
                    return Value::infinitum();
                default:
                    throw UnknownTypeException("Unknown type tag");
            }
        } catch (const OSCException&) {
            throw;
        } catch (const std::exception& e) {
            throw DeserializationException("Error deserializing type '" + std::string(1, typeTag) +
                                           "': " + std::string(e.what()));
        }
    }
}  // namespace osc
