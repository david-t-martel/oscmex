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

    // Type checking methods
    bool Value::isInt32() const { return std::holds_alternative<int32_t>(value_); }
    bool Value::isInt64() const { return std::holds_alternative<int64_t>(value_); }
    bool Value::isFloat() const { return std::holds_alternative<float>(value_); }
    bool Value::isDouble() const { return std::holds_alternative<double>(value_); }
    bool Value::isString() const { return std::holds_alternative<std::string>(value_) && !isSymbol(); }
    bool Value::isSymbol() const { return std::holds_alternative<std::string>(value_); }
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
    int32_t Value::asInt32() const { return std::get<int32_t>(value_); }
    int64_t Value::asInt64() const { return std::get<int64_t>(value_); }
    float Value::asFloat() const { return std::get<float>(value_); }
    double Value::asDouble() const { return std::get<double>(value_); }
    std::string Value::asString() const { return std::get<std::string>(value_); }
    std::string Value::asSymbol() const { return std::get<std::string>(value_); }
    Blob Value::asBlob() const { return std::get<Blob>(value_); }
    TimeTag Value::asTimeTag() const { return std::get<TimeTag>(value_); }
    char Value::asChar() const { return std::get<char>(value_); }
    uint32_t Value::asColor() const { return std::get<uint32_t>(value_); }
    std::array<uint8_t, 4> Value::asMidi() const { return std::get<std::array<uint8_t, 4>>(value_); }
    bool Value::asBool() const { return std::get<bool>(value_); }
    Array Value::asArray() const { return std::get<Array>(value_); }

    // Get OSC type tag character
    char Value::typeTag() const
    {
        if (isInt32()) return 'i';
        if (isInt64()) return 'h';
        if (isFloat()) return 'f';
        if (isDouble()) return 'd';
        if (isString()) return 's';
        if (isSymbol()) return 'S';
        if (isBlob()) return 'b';
        if (isTimeTag()) return 't';
        if (isChar()) return 'c';
        if (isColor()) return 'r';
        if (isMidi()) return 'm';
        if (isBool()) return 'T'; // True
        if (isNil()) return 'N'; // Nil
        return 'N'; // Default to Nil
    }

    // Serialize the value to OSC binary format
    std::vector<std::byte> Value::serialize() const
    {
        std::vector<std::byte> buffer;
        // Serialization logic goes here
        return buffer;
    }

} // namespace osc