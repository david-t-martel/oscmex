// filepath: c:\codedev\auricleinc\oscmex\src\Message.cpp
#include "Message.h"
#include "Types.h"
#include <cstring>
#include <sstream>

namespace osc
{

    // Constructor for Message class
    Message::Message(const std::string &path) : path_(path) {}

    // Copy constructor
    Message::Message(const Message &other) : path_(other.path_), arguments_(other.arguments_) {}

    // Move constructor
    Message::Message(Message &&other) noexcept
        : path_(std::move(other.path_)), arguments_(std::move(other.arguments_)) {}

    // Add an argument directly
    void Message::addValue(const Value &value)
    {
        arguments_.push_back(value);
    }

    // Add an int32 argument
    void Message::addInt32(int32_t value)
    {
        arguments_.emplace_back(value);
    }

    // Add an int64 argument
    void Message::addInt64(int64_t value)
    {
        arguments_.emplace_back(value);
    }

    // Add a float argument
    void Message::addFloat(float value)
    {
        arguments_.emplace_back(value);
    }

    // Add a double argument
    void Message::addDouble(double value)
    {
        arguments_.emplace_back(value);
    }

    // Add a string argument
    void Message::addString(const std::string &value)
    {
        arguments_.emplace_back(value);
    }

    // Add a symbol argument
    void Message::addSymbol(const std::string &value)
    {
        // Note: In the current implementation, symbols are stored as strings
        // We'd need a proper way to distinguish them when querying type
        arguments_.emplace_back(value);
    }

    // Add a blob argument
    void Message::addBlob(const Blob &value)
    {
        arguments_.emplace_back(value);
    }

    // Add blob from raw data
    void Message::addBlob(const void *data, size_t size)
    {
        arguments_.emplace_back(Blob(data, size));
    }

    // Add a time tag argument
    void Message::addTimeTag(const TimeTag &value)
    {
        arguments_.emplace_back(value);
    }

    // Add a char argument
    void Message::addChar(char value)
    {
        arguments_.emplace_back(value);
    }

    // Add a color argument
    void Message::addColor(uint32_t rgba)
    {
        arguments_.emplace_back(rgba);
    }

    // Add a MIDI message
    void Message::addMidi(const std::array<uint8_t, 4> &midi)
    {
        arguments_.emplace_back(midi);
    }

    // Add a boolean argument
    void Message::addBool(bool value)
    {
        arguments_.emplace_back(value);
    }

    // Add a nil argument
    void Message::addNil()
    {
        arguments_.emplace_back();
    }

    // Add an infinitum argument
    void Message::addInfinitum()
    {
        // TODO: Implement proper infinitum type
        arguments_.emplace_back(true);
    }

    // Add an array argument
    void Message::addArray(const std::vector<Value> &array)
    {
        arguments_.emplace_back(array);
    }

    // Get the message path
    const std::string &Message::getPath() const
    {
        return path_;
    }

    // Get the number of arguments
    size_t Message::getArgumentCount() const
    {
        return arguments_.size();
    }

    // Get all arguments
    const std::vector<Value> &Message::getArguments() const
    {
        return arguments_;
    }

    // Get an argument by index
    const Value &Message::getArgument(size_t index) const
    {
        if (index < arguments_.size())
        {
            return arguments_[index];
        }
        throw OSCException(OSCException::ErrorCode::InvalidArgument, "Argument index out of range");
    }

    // Helper function to append string with OSC padding
    void Message::appendString(std::vector<std::byte> &buffer, const std::string &str)
    {
        // Add the string characters including null terminator
        buffer.insert(buffer.end(),
                      reinterpret_cast<const std::byte *>(str.c_str()),
                      reinterpret_cast<const std::byte *>(str.c_str() + str.size() + 1));

        // Add padding to align to 4-byte boundary
        size_t padding = (4 - ((str.size() + 1) % 4)) % 4;
        for (size_t i = 0; i < padding; ++i)
        {
            buffer.push_back(std::byte(0));
        }
    }

    // Serialize the message to OSC format
    std::vector<std::byte> Message::serialize() const
    {
        std::vector<std::byte> buffer;

        // First add the OSC address pattern (with padding)
        appendString(buffer, path_);

        // Build the type tag string
        std::string typeTagString = ","; // Always starts with comma
        for (const auto &arg : arguments_)
        {
            typeTagString += arg.typeTag();
        }

        // Add the type tag string (with padding)
        appendString(buffer, typeTagString);

        // Add the argument data
        for (const auto &arg : arguments_)
        {
            auto argData = arg.serialize();
            buffer.insert(buffer.end(), argData.begin(), argData.end());
        }

        return buffer;
    }

    // Deserialize a message from binary data
    Message Message::deserialize(const std::byte *data, size_t size)
    {
        if (size < 4)
        {
            throw OSCException(OSCException::ErrorCode::InvalidMessage, "Message data too small");
        }

        // Extract path (which should be the first part of the message)
        const char *cdata = reinterpret_cast<const char *>(data);
        if (cdata[0] != '/')
        {
            throw OSCException(OSCException::ErrorCode::InvalidMessage, "Invalid OSC address pattern");
        }

        // Find the null terminator for the path
        size_t pathLen = 0;
        while (pathLen < size && cdata[pathLen] != '\0')
        {
            pathLen++;
        }

        if (pathLen >= size)
        {
            throw OSCException(OSCException::ErrorCode::InvalidMessage, "Missing null terminator in OSC address pattern");
        }

        // Extract the path string
        std::string path(cdata, pathLen);

        // Create message with the path
        Message message(path);

        // Skip past the path and its padding
        size_t pos = pathLen + 1; // +1 for null terminator
        pos = (pos + 3) & ~3;     // Round up to next multiple of 4

        if (pos >= size)
        {
            // No type tag or arguments, return message with just the path
            return message;
        }

        // Check if we have a type tag (should start with ',')
        if (cdata[pos] != ',')
        {
            throw OSCException(OSCException::ErrorCode::InvalidMessage, "Missing type tag string");
        }

        // Extract type tag string
        size_t typeTagPos = pos;
        size_t typeTagLen = 0;
        while (pos + typeTagLen < size && cdata[pos + typeTagLen] != '\0')
        {
            typeTagLen++;
        }

        if (pos + typeTagLen >= size)
        {
            throw OSCException(OSCException::ErrorCode::InvalidMessage, "Missing null terminator in type tag string");
        }

        std::string typeTagString(cdata + pos, typeTagLen);

        // Skip past the type tag string and its padding
        pos += typeTagLen + 1; // +1 for null terminator
        pos = (pos + 3) & ~3;  // Round up to next multiple of 4

        // Parse arguments based on the type tag characters (skipping the initial ',')
        for (size_t i = 1; i < typeTagString.size(); ++i)
        {
            char tag = typeTagString[i];

            // Process based on type tag
            switch (tag)
            {
            case 'i':
            { // int32
                if (pos + 4 > size)
                {
                    throw OSCException(OSCException::ErrorCode::InvalidMessage, "Message too short for int32 argument");
                }
                int32_t value = (static_cast<int32_t>(data[pos]) << 24) |
                                (static_cast<int32_t>(data[pos + 1]) << 16) |
                                (static_cast<int32_t>(data[pos + 2]) << 8) |
                                static_cast<int32_t>(data[pos + 3]);
                message.addInt32(value);
                pos += 4;
                break;
            }
            case 'f':
            { // float
                if (pos + 4 > size)
                {
                    throw OSCException(OSCException::ErrorCode::InvalidMessage, "Message too short for float argument");
                }
                union
                {
                    int32_t i;
                    float f;
                } value;
                value.i = (static_cast<int32_t>(data[pos]) << 24) |
                          (static_cast<int32_t>(data[pos + 1]) << 16) |
                          (static_cast<int32_t>(data[pos + 2]) << 8) |
                          static_cast<int32_t>(data[pos + 3]);
                message.addFloat(value.f);
                pos += 4;
                break;
            }
            case 's':
            { // string
                const char *str = cdata + pos;
                size_t strLen = 0;
                while (pos + strLen < size && str[strLen] != '\0')
                {
                    strLen++;
                }

                if (pos + strLen >= size)
                {
                    throw OSCException(OSCException::ErrorCode::InvalidMessage, "Missing null terminator in string argument");
                }

                message.addString(std::string(str, strLen));

                // Skip past the string and its padding
                pos += strLen + 1;    // +1 for null terminator
                pos = (pos + 3) & ~3; // Round up to next multiple of 4
                break;
            }
            case 'b':
            { // blob
                if (pos + 4 > size)
                {
                    throw OSCException(OSCException::ErrorCode::InvalidMessage, "Message too short for blob size");
                }

                // Get blob size
                int32_t blobSize = (static_cast<int32_t>(data[pos]) << 24) |
                                   (static_cast<int32_t>(data[pos + 1]) << 16) |
                                   (static_cast<int32_t>(data[pos + 2]) << 8) |
                                   static_cast<int32_t>(data[pos + 3]);
                pos += 4;

                if (blobSize < 0 || pos + blobSize > size)
                {
                    throw OSCException(OSCException::ErrorCode::InvalidMessage, "Invalid blob size or message too short");
                }

                // Create and add blob
                Blob blob(data + pos, blobSize);
                message.addBlob(blob);

                // Skip past the blob data and its padding
                pos += blobSize;
                pos = (pos + 3) & ~3; // Round up to next multiple of 4
                break;
            }
            // Additional types would be handled here
            case 'T': // True
                message.addBool(true);
                break;
            case 'F': // False
                message.addBool(false);
                break;
            case 'N': // Nil
                message.addNil();
                break;
            case 'I': // Infinitum
                message.addInfinitum();
                break;
            // Placeholders for other types
            default:
                throw OSCException(OSCException::ErrorCode::InvalidMessage,
                                   "Unsupported type tag: " + std::string(1, tag));
            }
        }

        return message;
    }

} // namespace osc
