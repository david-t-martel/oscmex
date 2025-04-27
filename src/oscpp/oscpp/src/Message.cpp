// filepath: c:\codedev\auricleinc\oscmex\src\Message.cpp
#include "Message.h"
#include "Types.h"
#include <cstring>
#include <sstream>

namespace osc
{

    // Constructor for Message class
    Message::Message(const std::string &path) : path_(path) {}

    // Add an integer argument
    void Message::addInt32(int32_t value)
    {
        arguments_.emplace_back(Value(value));
    }

    // Add a float argument
    void Message::addFloat(float value)
    {
        arguments_.emplace_back(Value(value));
    }

    // Add a string argument
    void Message::addString(const std::string &value)
    {
        arguments_.emplace_back(Value(value));
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

    // Get an argument by index
    const Value &Message::getArgument(size_t index) const
    {
        if (index < arguments_.size())
        {
            return arguments_[index];
        }
        throw OSCException(OSCException::ErrorCode::InvalidArgument, "Argument index out of range");
    }

    // Serialize the message to OSC format
    std::vector<std::byte> Message::serialize() const
    {
        std::vector<std::byte> buffer;

        // Add the path
        appendString(buffer, path_);

        // Add each argument
        for (const auto &arg : arguments_)
        {
            auto argData = arg.serialize();
            buffer.insert(buffer.end(), argData.begin(), argData.end());
        }

        return buffer;
    }

    // Helper function to append a string with padding
    void Message::appendString(std::vector<std::byte> &buffer, const std::string &str)
    {
        buffer.insert(buffer.end(), reinterpret_cast<const std::byte *>(str.data()), reinterpret_cast<const std::byte *>(str.data() + str.size()));
        // Add null terminator and padding
        buffer.push_back(std::byte(0));
        while (buffer.size() % 4 != 0)
        {
            buffer.push_back(std::byte(0));
        }
    }

} // namespace osc