#include "osc/Message.h"

#include <cstring>
#include <sstream>

#include "osc/Types.h"

namespace osc {
    // Constructor for Message class with address path validation
    Message::Message(const std::string &path) : path_(path) {
        // Validate OSC path (must start with '/')
        if (path.empty() || path[0] != '/') {
            throw OSCException("Invalid OSC address pattern (must start with '/')",
                               OSCException::ErrorCode::AddressError);
        }
    }

    // Get the message path
    std::string Message::getPath() const { return path_; }

    // Get all arguments
    const std::vector<Value> &Message::getArguments() const { return arguments_; }

    // Add an Int32 argument (type tag 'i')
    Message &Message::addInt32(int32_t value) {
        arguments_.emplace_back(Value(value));
        return *this;
    }

    // Add an Int64 argument (type tag 'h')
    Message &Message::addInt64(int64_t value) {
        arguments_.emplace_back(Value(value));
        return *this;
    }

    // Add a Float argument (type tag 'f')
    Message &Message::addFloat(float value) {
        arguments_.emplace_back(Value(value));
        return *this;
    }

    // Add a Double argument (type tag 'd')
    Message &Message::addDouble(double value) {
        arguments_.emplace_back(Value(value));
        return *this;
    }

    // Add a String argument (type tag 's')
    Message &Message::addString(const std::string &value) {
        arguments_.emplace_back(Value(value));
        return *this;
    }

    // Add a Symbol argument (type tag 'S')
    Message &Message::addSymbol(const std::string &value) {
        // Symbols are stored as strings but will have different type tag
        arguments_.emplace_back(Value(std::string(value)));
        return *this;
    }

    // Add a Blob argument from raw data (type tag 'b')
    Message &Message::addBlob(const void *data, size_t size) {
        arguments_.emplace_back(Value(Blob(data, size)));
        return *this;
    }

    // Add a TimeTag argument (type tag 't')
    Message &Message::addTimeTag(const TimeTag &timeTag) {
        arguments_.emplace_back(Value(timeTag));
        return *this;
    }

    // Add a Character argument (type tag 'c')
    Message &Message::addChar(char value) {
        arguments_.emplace_back(Value(value));
        return *this;
    }

    // Add a RGBA Color argument (type tag 'r')
    Message &Message::addColor(uint32_t value) {
        RGBAColor color;
        color.r = (value >> 24) & 0xFF;
        color.g = (value >> 16) & 0xFF;
        color.b = (value >> 8) & 0xFF;
        color.a = value & 0xFF;
        arguments_.emplace_back(Value(color));
        return *this;
    }

    // Add a MIDI message argument (type tag 'm')
    Message &Message::addMidi(uint8_t port, uint8_t status, uint8_t data1, uint8_t data2) {
        MIDIMessage midi;
        midi.bytes = {port, status, data1, data2};
        arguments_.emplace_back(Value(midi));
        return *this;
    }

    // Add a True argument (type tag 'T')
    Message &Message::addTrue() {
        arguments_.emplace_back(Value::trueBool());
        return *this;
    }

    // Add a False argument (type tag 'F')
    Message &Message::addFalse() {
        arguments_.emplace_back(Value::falseBool());
        return *this;
    }

    // Add a boolean argument (type tag 'T' or 'F')
    Message &Message::addBool(bool value) {
        if (value)
            return addTrue();
        else
            return addFalse();
    }

    // Add a Nil argument (type tag 'N')
    Message &Message::addNil() {
        arguments_.emplace_back(Value::nil());
        return *this;
    }

    // Add an Infinitum argument (type tag 'I')
    Message &Message::addInfinitum() {
        arguments_.emplace_back(Value::infinitum());
        return *this;
    }

    // Add an Array of arguments (type tags '[ ... ]')
    Message &Message::addArray(const std::vector<Value> &array) {
        // Mark all values in the array as array elements
        std::vector<Value> markedArray;
        markedArray.reserve(array.size());

        for (const auto &val : array) {
            // Create a copy of the value and mark it as an array element
            Value arrVal(val.variant(), true);
            markedArray.push_back(arrVal);
        }

        // Add opening array bracket
        arguments_.push_back(Value::arrayBegin());

        // Add array elements
        for (const auto &val : markedArray) {
            arguments_.push_back(val);
        }

        // Add closing array bracket
        arguments_.push_back(Value::arrayEnd());

        return *this;
    }

    // Add a generic Value argument
    Message &Message::addValue(const Value &value) {
        arguments_.push_back(value);
        return *this;
    }

    // Helper function to pad to 4-byte boundary
    inline size_t padSize(size_t size) { return (size + 3) & ~3; }

    // Serialize the message to OSC format
    std::vector<std::byte> Message::serialize() const {
        // Pre-calculate buffer size to avoid reallocations
        size_t totalSize = 0;

        // Calculate address size with padding
        size_t pathLen = path_.length() + 1;  // Include null terminator
        size_t paddedPathSize = padSize(pathLen);
        totalSize += paddedPathSize;

        // Calculate type tag size with padding
        std::string typeTag = ",";
        for (const auto &arg : arguments_) {
            typeTag += arg.typeTag();
        }
        size_t typeTagLen = typeTag.length() + 1;
        size_t paddedTypeTagSize = padSize(typeTagLen);
        totalSize += paddedTypeTagSize;

        // Estimate argument sizes (conservatively)
        for (const auto &arg : arguments_) {
            char tag = arg.typeTag();

            // Skip array markers as they don't have binary representation
            if (tag == ARRAY_BEGIN_TAG || tag == ARRAY_END_TAG) {
                continue;
            }

            // T, F, N, I types have no binary data
            if (tag == TRUE_TAG || tag == FALSE_TAG || tag == NIL_TAG || tag == INFINITUM_TAG) {
                continue;
            }

            // Add estimated size based on type
            switch (tag) {
                case INT32_TAG:
                case FLOAT_TAG:
                case CHAR_TAG:
                case RGBA_TAG:
                case MIDI_TAG:
                    totalSize += 4;
                    break;
                case INT64_TAG:
                case DOUBLE_TAG:
                case TIMETAG_TAG:
                    totalSize += 8;
                    break;
                case STRING_TAG:
                case SYMBOL_TAG:
                    // String length + null terminator + padding
                    if (tag == STRING_TAG) {
                        const auto &str = arg.asString();
                        totalSize += padSize(str.length() + 1);
                    } else {
                        const auto &sym = arg.asSymbol();
                        totalSize += padSize(sym.length() + 1);
                    }
                    break;
                case BLOB_TAG:
                    // Size field + blob data + padding
                    const auto &blob = arg.asBlob();
                    totalSize += 4 + padSize(blob.size());
                    break;
            }
        }

        // Create result vector with pre-allocated size
        std::vector<std::byte> result;
        result.reserve(totalSize);

        // 1. Add Address Pattern (null-terminated, padded to 4-byte boundary)
        const char *pathStr = path_.c_str();

        // Add path string with null terminator
        const std::byte *pathBytes = reinterpret_cast<const std::byte *>(pathStr);
        result.insert(result.end(), pathBytes, pathBytes + pathLen);

        // Add padding to align to 4-byte boundary
        result.resize(paddedPathSize, std::byte{0});

        // 2. Add Type Tag String (starts with ',', null-terminated, padded)
        // Add type tag string with null terminator
        const char *typeTagStr = typeTag.c_str();
        const std::byte *typeTagBytes = reinterpret_cast<const std::byte *>(typeTagStr);
        result.insert(result.end(), typeTagBytes, typeTagBytes + typeTagLen);

        // Add padding to align to 4-byte boundary
        result.resize(paddedPathSize + paddedTypeTagSize, std::byte{0});

        // 3. Add Argument Data
        for (const auto &arg : arguments_) {
            // Skip array markers as they don't have a binary representation
            if (arg.typeTag() == ARRAY_BEGIN_TAG || arg.typeTag() == ARRAY_END_TAG) {
                continue;
            }

            // T, F, N, I types have no binary data
            if (arg.typeTag() == TRUE_TAG || arg.typeTag() == FALSE_TAG ||
                arg.typeTag() == NIL_TAG || arg.typeTag() == INFINITUM_TAG) {
                continue;
            }

            // Serialize the argument value
            std::vector<std::byte> argData;
            argData.reserve(16);  // Small initial reservation for most argument types
            arg.serialize(argData);
            result.insert(result.end(), argData.begin(), argData.end());
        }

        return result;
    }

    // Deserialize a message from binary data
    Message Message::deserialize(const std::byte *data, size_t size) {
        if (size < 4) {
            throw OSCException("Message data too small", OSCException::ErrorCode::MalformedPacket);
        }

        // 1. Extract OSC Address Pattern
        const char *cdata = reinterpret_cast<const char *>(data);
        if (cdata[0] != '/') {
            throw OSCException("Invalid OSC address pattern (must start with '/')",
                               OSCException::ErrorCode::AddressError);
        }

        // Find the null terminator for the address pattern
        size_t pathEnd = 0;
        while (pathEnd < size && cdata[pathEnd] != '\0') {
            pathEnd++;
        }

        if (pathEnd >= size) {
            throw OSCException("Missing null terminator in OSC address pattern",
                               OSCException::ErrorCode::MalformedPacket);
        }

        // Extract the address pattern string
        std::string path(cdata, pathEnd);

        // Create message object with validated path
        Message message(path);

        // Move position past the address pattern including padding
        size_t pos = padSize(pathEnd + 1);  // +1 for null terminator

        if (pos >= size) {
            // No type tag string or arguments
            return message;
        }

        // 2. Extract Type Tag String
        if (cdata[pos] != ',') {
            throw OSCException("Type tag string must start with ','",
                               OSCException::ErrorCode::MalformedPacket);
        }

        // Find the null terminator for the type tag string
        size_t typeTagStart = pos;
        size_t typeTagEnd = pos;

        while (typeTagEnd < size && cdata[typeTagEnd] != '\0') {
            typeTagEnd++;
        }

        if (typeTagEnd >= size) {
            throw OSCException("Missing null terminator in type tag string",
                               OSCException::ErrorCode::MalformedPacket);
        }

        // Extract the type tag string
        std::string typeTag(cdata + typeTagStart, typeTagEnd - typeTagStart);

        // Move position past the type tag string including padding
        pos = padSize(typeTagEnd + 1);  // +1 for null terminator

        // 3. Extract Arguments based on Type Tags
        bool inArray = false;

        // Start from index 1 to skip the initial ',' character
        for (size_t i = 1; i < typeTag.length(); i++) {
            char tag = typeTag[i];

            // Handle array markers
            if (tag == Value::ARRAY_BEGIN_TAG) {
                inArray = true;
                message.arguments_.push_back(Value::arrayBegin());
                continue;
            }

            if (tag == Value::ARRAY_END_TAG) {
                inArray = false;
                message.arguments_.push_back(Value::arrayEnd());
                continue;
            }

            // Handle other value types using Value::deserialize
            if (pos <= size) {
                size_t remainingSize = size - pos;
                const std::byte *dataPtr = data + pos;
                // Create the Value directly in the vector to avoid copy construction
                message.arguments_.push_back(Value::deserialize(dataPtr, remainingSize, tag));
                pos = size - remainingSize;  // Update position based on bytes consumed
            } else {
                throw OSCException("Message data truncated while parsing arguments",
                                   OSCException::ErrorCode::MalformedPacket);
            }
        }

        return message;
    }

}  // namespace osc
