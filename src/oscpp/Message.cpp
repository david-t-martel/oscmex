#include "Message.h"
#include "Exceptions.h"
#include <regex>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <array>

// Utility for aligning to 4-byte boundaries
#define OSC_ALIGN_SIZE 4
#define OSC_ALIGN(size) (((size) + (OSC_ALIGN_SIZE - 1)) & ~(OSC_ALIGN_SIZE - 1))

namespace osc
{

    // Implementation class (PIMPL idiom)
    class Message::MessageImpl
    {
    public:
        MessageImpl(std::string_view addressPattern)
            : addressPattern_(addressPattern)
        {
            if (!Message::isValidAddressPattern(addressPattern))
            {
                throw OSCException(OSCException::ErrorCode::InvalidAddress,
                                   "Invalid OSC address pattern: " + std::string(addressPattern));
            }
        }

        MessageImpl(const std::vector<std::byte> &data)
        {
            parse(data);
        }

        ~MessageImpl() = default;

        // Copy constructor
        MessageImpl(const MessageImpl &other)
            : addressPattern_(other.addressPattern_),
              typeTags_(other.typeTags_),
              arguments_(other.arguments_)
        {
        }

        // Move constructor
        MessageImpl(MessageImpl &&other) noexcept
            : addressPattern_(std::move(other.addressPattern_)),
              typeTags_(std::move(other.typeTags_)),
              arguments_(std::move(other.arguments_))
        {
        }

        // Add a value and its type tag
        void addValue(const Value &value)
        {
            typeTags_ += value.typeTag();
            arguments_.push_back(value);
        }

        // Add a type-specific value with its type tag
        void addInt32(int32_t value)
        {
            typeTags_ += 'i';
            arguments_.emplace_back(value);
        }

        void addInt64(int64_t value)
        {
            typeTags_ += 'h';
            arguments_.emplace_back(value);
        }

        void addFloat(float value)
        {
            typeTags_ += 'f';
            arguments_.emplace_back(value);
        }

        void addDouble(double value)
        {
            typeTags_ += 'd';
            arguments_.emplace_back(value);
        }

        void addString(std::string_view value)
        {
            typeTags_ += 's';
            arguments_.emplace_back(std::string(value));
        }

        void addSymbol(std::string_view value)
        {
            typeTags_ += 'S';
            arguments_.emplace_back(std::string(value));
        }

        void addBlob(const Blob &value)
        {
            typeTags_ += 'b';
            arguments_.emplace_back(value);
        }

        void addBlob(const void *data, size_t size)
        {
            typeTags_ += 'b';
            arguments_.emplace_back(Blob(data, size));
        }

        void addTimeTag(const TimeTag &value)
        {
            typeTags_ += 't';
            arguments_.emplace_back(value);
        }

        void addChar(char value)
        {
            typeTags_ += 'c';
            arguments_.emplace_back(value);
        }

        void addColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
        {
            typeTags_ += 'r';
            arguments_.emplace_back(static_cast<uint32_t>((r << 24) | (g << 16) | (b << 8) | a));
        }

        void addColor(uint32_t rgba)
        {
            typeTags_ += 'r';
            arguments_.emplace_back(rgba);
        }

        void addMidi(uint8_t port, uint8_t status, uint8_t data1, uint8_t data2)
        {
            typeTags_ += 'm';
            std::array<uint8_t, 4> midi = {port, status, data1, data2};
            arguments_.emplace_back(midi);
        }

        void addMidi(const std::array<uint8_t, 4> &midi)
        {
            typeTags_ += 'm';
            arguments_.emplace_back(midi);
        }

        void addTrue()
        {
            typeTags_ += 'T';
            arguments_.emplace_back(true);
        }

        void addFalse()
        {
            typeTags_ += 'F';
            arguments_.emplace_back(false);
        }

        void addBool(bool value)
        {
            if (value)
            {
                addTrue();
            }
            else
            {
                addFalse();
            }
        }

        void addNil()
        {
            typeTags_ += 'N';
            arguments_.emplace_back(); // Default constructs a nil value
        }

        void addInfinitum()
        {
            typeTags_ += 'I';
            arguments_.emplace_back(true); // Store infinitum as a bool
        }

        void addArray(const Array &array)
        {
            typeTags_ += '[';

            // Add array elements
            for (const auto &element : array)
            {
                typeTags_ += element.typeTag();
                arguments_.push_back(element);
            }

            typeTags_ += ']';
        }

        // Get information about the message
        std::string getAddressPattern() const
        {
            return addressPattern_;
        }

        std::string getTypeTagString() const
        {
            return typeTags_;
        }

        size_t getArgumentCount() const
        {
            return arguments_.size();
        }

        Value getArgument(size_t index) const
        {
            if (index >= arguments_.size())
            {
                throw std::out_of_range("Argument index out of range");
            }
            return arguments_[index];
        }

        std::vector<Value> getArguments() const
        {
            return arguments_;
        }

        // Serialize the message to OSC binary format
        std::vector<std::byte> serialize() const
        {
            std::vector<std::byte> result;

            // Reserve space to avoid frequent reallocations
            result.reserve(1024);

            // Add address pattern
            addString(result, addressPattern_);

            // Add type tag string (with leading comma)
            addString(result, "," + typeTags_);

            // Add arguments
            for (size_t i = 0; i < arguments_.size(); ++i)
            {
                addArgument(result, arguments_[i]);
            }

            return result;
        }

    private:
        std::string addressPattern_;
        std::string typeTags_;
        std::vector<Value> arguments_;

        // Helper methods for serialization

        // Add a null-terminated string with padding
        static void addString(std::vector<std::byte> &buffer, const std::string &str)
        {
            size_t padded_size = OSC_ALIGN(str.size() + 1); // Include null terminator
            size_t start_pos = buffer.size();

            // Resize buffer to fit the string and padding
            buffer.resize(start_pos + padded_size);

            // Copy string data
            std::memcpy(buffer.data() + start_pos, str.c_str(), str.size() + 1);

            // Add null padding (already filled with zeros due to resize)
        }

        // Add binary data with size prefix and padding
        static void addBlob(std::vector<std::byte> &buffer, const void *data, size_t size)
        {
            // Add size as int32
            int32_t size32 = static_cast<int32_t>(size);
            addInt32(buffer, size32);

            // Calculate padding size
            size_t padded_size = OSC_ALIGN(size);
            size_t start_pos = buffer.size();

            // Resize buffer to fit the data and padding
            buffer.resize(start_pos + padded_size);

            // Copy data
            if (size > 0)
            {
                std::memcpy(buffer.data() + start_pos, data, size);
            }

            // Padding is already filled with zeros due to resize
        }

        // Add an int32
        static void addInt32(std::vector<std::byte> &buffer, int32_t value)
        {
            // Convert to network byte order (big endian)
            uint32_t netValue = htonl(static_cast<uint32_t>(value));

            size_t start_pos = buffer.size();
            buffer.resize(start_pos + sizeof(int32_t));

            std::memcpy(buffer.data() + start_pos, &netValue, sizeof(int32_t));
        }

        // Add an int64
        static void addInt64(std::vector<std::byte> &buffer, int64_t value)
        {
            // Convert to network byte order (big endian)
            uint64_t netValue = htobe64(static_cast<uint64_t>(value));

            size_t start_pos = buffer.size();
            buffer.resize(start_pos + sizeof(int64_t));

            std::memcpy(buffer.data() + start_pos, &netValue, sizeof(int64_t));
        }

        // Add a float
        static void addFloat(std::vector<std::byte> &buffer, float value)
        {
            union
            {
                float f;
                uint32_t i;
            } u;

            u.f = value;

            // Convert to network byte order (big endian)
            uint32_t netValue = htonl(u.i);

            size_t start_pos = buffer.size();
            buffer.resize(start_pos + sizeof(float));

            std::memcpy(buffer.data() + start_pos, &netValue, sizeof(float));
        }

        // Add a double
        static void addDouble(std::vector<std::byte> &buffer, double value)
        {
            union
            {
                double d;
                uint64_t i;
            } u;

            u.d = value;

            // Convert to network byte order (big endian)
            uint64_t netValue = htobe64(u.i);

            size_t start_pos = buffer.size();
            buffer.resize(start_pos + sizeof(double));

            std::memcpy(buffer.data() + start_pos, &netValue, sizeof(double));
        }

        // Add an argument based on its type
        static void addArgument(std::vector<std::byte> &buffer, const Value &value)
        {
            if (value.isInt32())
            {
                addInt32(buffer, value.asInt32());
            }
            else if (value.isInt64())
            {
                addInt64(buffer, value.asInt64());
            }
            else if (value.isFloat())
            {
                addFloat(buffer, value.asFloat());
            }
            else if (value.isDouble())
            {
                addDouble(buffer, value.asDouble());
            }
            else if (value.isString())
            {
                addString(buffer, value.asString());
            }
            else if (value.isSymbol())
            {
                addString(buffer, value.asSymbol());
            }
            else if (value.isBlob())
            {
                const Blob &blob = value.asBlob();
                addBlob(buffer, blob.ptr(), blob.size());
            }
            else if (value.isTimeTag())
            {
                addInt64(buffer, value.asTimeTag().toNTP());
            }
            else if (value.isChar())
            {
                addInt32(buffer, value.asChar());
            }
            else if (value.isColor())
            {
                addInt32(buffer, value.asColor());
            }
            else if (value.isMidi())
            {
                const auto &midi = value.asMidi();
                size_t start_pos = buffer.size();
                buffer.resize(start_pos + 4);
                std::memcpy(buffer.data() + start_pos, midi.data(), 4);
            }
            // T, F, N, I have no argument data
        }

        // Parse a binary OSC message
        void parse(const std::vector<std::byte> &data)
        {
            if (data.empty())
            {
                throw OSCException(OSCException::ErrorCode::InvalidMessage,
                                   "Empty OSC message data");
            }

            size_t pos = 0;

            // Parse address pattern
            addressPattern_ = parseString(data, pos);

            if (addressPattern_.empty() || addressPattern_[0] != '/')
            {
                throw OSCException(OSCException::ErrorCode::InvalidAddress,
                                   "Invalid OSC address pattern: " + addressPattern_);
            }

            // Parse type tag string
            std::string typeTagString = parseString(data, pos);

            if (typeTagString.empty() || typeTagString[0] != ',')
            {
                throw OSCException(OSCException::ErrorCode::InvalidMessage,
                                   "Invalid OSC type tag string: " + typeTagString);
            }

            // Store type tags without the comma
            typeTags_ = typeTagString.substr(1);

            // Parse arguments
            for (char type : typeTags_)
            {
                arguments_.push_back(parseArgument(data, pos, type));
            }
        }

        // Parse a null-terminated string from binary data
        static std::string parseString(const std::vector<std::byte> &data, size_t &pos)
        {
            if (pos >= data.size())
            {
                throw OSCException(OSCException::ErrorCode::InvalidMessage,
                                   "End of data reached while parsing string");
            }

            const char *start = reinterpret_cast<const char *>(data.data() + pos);
            size_t maxLen = data.size() - pos;

            // Find null terminator
            size_t len = 0;
            while (len < maxLen && start[len] != '\0')
            {
                len++;
            }

            if (len == maxLen)
            {
                throw OSCException(OSCException::ErrorCode::InvalidMessage,
                                   "Missing null terminator in string");
            }

            // Create string from data
            std::string result(start, len);

            // Move position past the string and padding
            pos += OSC_ALIGN(len + 1);

            return result;
        }

        // Parse a binary blob from data
        static Blob parseBlob(const std::vector<std::byte> &data, size_t &pos)
        {
            if (pos + 4 > data.size())
            {
                throw OSCException(OSCException::ErrorCode::InvalidMessage,
                                   "End of data reached while parsing blob size");
            }

            // Parse blob size
            int32_t size = parseInt32(data, pos);

            if (size < 0)
            {
                throw OSCException(OSCException::ErrorCode::InvalidMessage,
                                   "Negative blob size: " + std::to_string(size));
            }

            if (pos + size > data.size())
            {
                throw OSCException(OSCException::ErrorCode::InvalidMessage,
                                   "Blob size exceeds available data");
            }

            // Create blob from data
            Blob result(data.data() + pos, size);

            // Move position past the blob and padding
            pos += OSC_ALIGN(size);

            return result;
        }

        // Parse an int32 from data
        static int32_t parseInt32(const std::vector<std::byte> &data, size_t &pos)
        {
            if (pos + 4 > data.size())
            {
                throw OSCException(OSCException::ErrorCode::InvalidMessage,
                                   "End of data reached while parsing int32");
            }

            uint32_t netValue;
            std::memcpy(&netValue, data.data() + pos, sizeof(uint32_t));

            // Move position
            pos += sizeof(uint32_t);

            // Convert from network byte order (big endian)
            return static_cast<int32_t>(ntohl(netValue));
        }

        // Parse an int64 from data
        static int64_t parseInt64(const std::vector<std::byte> &data, size_t &pos)
        {
            if (pos + 8 > data.size())
            {
                throw OSCException(OSCException::ErrorCode::InvalidMessage,
                                   "End of data reached while parsing int64");
            }

            uint64_t netValue;
            std::memcpy(&netValue, data.data() + pos, sizeof(uint64_t));

            // Move position
            pos += sizeof(uint64_t);

            // Convert from network byte order (big endian)
            return static_cast<int64_t>(be64toh(netValue));
        }

        // Parse a float from data
        static float parseFloat(const std::vector<std::byte> &data, size_t &pos)
        {
            if (pos + 4 > data.size())
            {
                throw OSCException(OSCException::ErrorCode::InvalidMessage,
                                   "End of data reached while parsing float");
            }

            union
            {
                uint32_t i;
                float f;
            } u;

            uint32_t netValue;
            std::memcpy(&netValue, data.data() + pos, sizeof(uint32_t));

            // Move position
            pos += sizeof(uint32_t);

            // Convert from network byte order (big endian)
            u.i = ntohl(netValue);

            return u.f;
        }

        // Parse a double from data
        static double parseDouble(const std::vector<std::byte> &data, size_t &pos)
        {
            if (pos + 8 > data.size())
            {
                throw OSCException(OSCException::ErrorCode::InvalidMessage,
                                   "End of data reached while parsing double");
            }

            union
            {
                uint64_t i;
                double d;
            } u;

            uint64_t netValue;
            std::memcpy(&netValue, data.data() + pos, sizeof(uint64_t));

            // Move position
            pos += sizeof(uint64_t);

            // Convert from network byte order (big endian)
            u.i = be64toh(netValue);

            return u.d;
        }

        // Parse MIDI message from data
        static std::array<uint8_t, 4> parseMidi(const std::vector<std::byte> &data, size_t &pos)
        {
            if (pos + 4 > data.size())
            {
                throw OSCException(OSCException::ErrorCode::InvalidMessage,
                                   "End of data reached while parsing MIDI message");
            }

            std::array<uint8_t, 4> midi;
            std::memcpy(midi.data(), data.data() + pos, 4);

            // Move position
            pos += 4;

            return midi;
        }

        // Parse an argument based on its type
        static Value parseArgument(const std::vector<std::byte> &data, size_t &pos, char type)
        {
            switch (type)
            {
            case 'i':
                return Value(parseInt32(data, pos));
            case 'h':
                return Value(parseInt64(data, pos));
            case 'f':
                return Value(parseFloat(data, pos));
            case 'd':
                return Value(parseDouble(data, pos));
            case 's':
                return Value(parseString(data, pos));
            case 'S':
                return Value(parseString(data, pos));
            case 'b':
                return Value(parseBlob(data, pos));
            case 't':
                return Value(TimeTag(parseInt64(data, pos)));
            case 'c':
                return Value(static_cast<char>(parseInt32(data, pos)));
            case 'r':
                return Value(static_cast<uint32_t>(parseInt32(data, pos)));
            case 'm':
                return Value(parseMidi(data, pos));
            case 'T':
                return Value(true);
            case 'F':
                return Value(false);
            case 'N':
                return Value();
            case 'I':
                return Value(true); // Infinitum
            default:
                throw OSCException(OSCException::ErrorCode::InvalidArgument,
                                   "Unsupported OSC type tag: " + std::string(1, type));
            }
        }

        // Helper for htobe64/be64toh which may not be available on all platforms
        static uint64_t htobe64(uint64_t value)
        {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
            return ((uint64_t)htonl(value & 0xFFFFFFFF) << 32) | htonl(value >> 32);
#else
            return value;
#endif
        }

        static uint64_t be64toh(uint64_t value)
        {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
            return ((uint64_t)ntohl(value & 0xFFFFFFFF) << 32) | ntohl(value >> 32);
#else
            return value;
#endif
        }
    };

    // Message class implementation using the PIMPL idiom

    Message::Message(std::string_view addressPattern)
        : impl_(std::make_unique<MessageImpl>(addressPattern))
    {
    }

    Message::Message(const std::vector<std::byte> &data)
        : impl_(std::make_unique<MessageImpl>(data))
    {
    }

    Message::~Message() = default;

    Message::Message(const Message &other)
        : impl_(std::make_unique<MessageImpl>(*other.impl_))
    {
    }

    Message::Message(Message &&other) noexcept = default;

    Message &Message::operator=(const Message &other)
    {
        if (this != &other)
        {
            impl_ = std::make_unique<MessageImpl>(*other.impl_);
        }
        return *this;
    }

    Message &Message::operator=(Message &&other) noexcept = default;

    std::string Message::getAddressPattern() const
    {
        return impl_->getAddressPattern();
    }

    size_t Message::getArgumentCount() const
    {
        return impl_->getArgumentCount();
    }

    std::string Message::getTypeTagString() const
    {
        return impl_->getTypeTagString();
    }

    Message &Message::addInt32(int32_t value)
    {
        impl_->addInt32(value);
        return *this;
    }

    Message &Message::addInt64(int64_t value)
    {
        impl_->addInt64(value);
        return *this;
    }

    Message &Message::addFloat(float value)
    {
        impl_->addFloat(value);
        return *this;
    }

    Message &Message::addDouble(double value)
    {
        impl_->addDouble(value);
        return *this;
    }

    Message &Message::addString(std::string_view value)
    {
        impl_->addString(value);
        return *this;
    }

    Message &Message::addSymbol(std::string_view value)
    {
        impl_->addSymbol(value);
        return *this;
    }

    Message &Message::addBlob(const Blob &value)
    {
        impl_->addBlob(value);
        return *this;
    }

    Message &Message::addBlob(const void *data, size_t size)
    {
        impl_->addBlob(data, size);
        return *this;
    }

    Message &Message::addTimeTag(const TimeTag &value)
    {
        impl_->addTimeTag(value);
        return *this;
    }

    Message &Message::addChar(char value)
    {
        impl_->addChar(value);
        return *this;
    }

    Message &Message::addColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        impl_->addColor(r, g, b, a);
        return *this;
    }

    Message &Message::addColor(uint32_t rgba)
    {
        impl_->addColor(rgba);
        return *this;
    }

    Message &Message::addMidi(uint8_t port, uint8_t status, uint8_t data1, uint8_t data2)
    {
        impl_->addMidi(port, status, data1, data2);
        return *this;
    }

    Message &Message::addMidi(const std::array<uint8_t, 4> &midi)
    {
        impl_->addMidi(midi);
        return *this;
    }

    Message &Message::addTrue()
    {
        impl_->addTrue();
        return *this;
    }

    Message &Message::addFalse()
    {
        impl_->addFalse();
        return *this;
    }

    Message &Message::addBool(bool value)
    {
        impl_->addBool(value);
        return *this;
    }

    Message &Message::addNil()
    {
        impl_->addNil();
        return *this;
    }

    Message &Message::addInfinitum()
    {
        impl_->addInfinitum();
        return *this;
    }

    Message &Message::addArray(const Array &array)
    {
        impl_->addArray(array);
        return *this;
    }

    Message &Message::addValue(const Value &value)
    {
        impl_->addValue(value);
        return *this;
    }

    Value Message::getArgument(size_t index) const
    {
        return impl_->getArgument(index);
    }

    std::vector<Value> Message::getArguments() const
    {
        return impl_->getArguments();
    }

    std::vector<std::byte> Message::serialize() const
    {
        return impl_->serialize();
    }

    Message Message::parse(const std::vector<std::byte> &data)
    {
        return Message(data);
    }

    bool Message::isValidAddressPattern(std::string_view pattern)
    {
        if (pattern.empty() || pattern[0] != '/')
        {
            return false;
        }

        // Check for characters not allowed in OSC address patterns
        // According to the spec, these are: space, #, *, ,, ?, [, ], {, }, (some patterns are actually special wildcards)
        static const std::regex validPattern("^/[^ #]*$");

        return std::regex_match(std::string(pattern), validPattern);
    }

} // namespace osc

#include "Message.h"
#include "Exceptions.h"
#include <cstring>

namespace lo
{

    Message::Message(const std::string &path)
    {
        // Validate the path
        if (path.empty() || path[0] != '/')
        {
            throw InvalidAddressException("OSC address must start with '/'");
        }

        path_ = path;
    }

    Message::~Message()
    {
        // Free any allocated data
    }

    const std::string &Message::getPath() const
    {
        return path_;
    }

    void Message::addInt32(int32_t value)
    {
        arguments_.push_back(Value(value));
    }

    void Message::addFloat(float value)
    {
        arguments_.push_back(Value(value));
    }

    void Message::addString(const std::string &value)
    {
        arguments_.push_back(Value(value));
    }

    void Message::addBlob(const void *data, size_t size)
    {
        Blob blob(data, size);
        arguments_.push_back(Value(blob));
    }

    const std::vector<Value> &Message::getArguments() const
    {
        return arguments_;
    }

    std::vector<std::byte> Message::serialize() const
    {
        // Implementation for serializing the message to binary format
        std::vector<std::byte> result;

        // Add path
        serializeString(path_, result);

        // Add type tag string
        std::string typeTags = ",";
        for (const auto &arg : arguments_)
        {
            typeTags += arg.getTypeTag();
        }
        serializeString(typeTags, result);

        // Add arguments
        for (const auto &arg : arguments_)
        {
            serializeArgument(arg, result);
        }

        return result;
    }

    void Message::serializeString(const std::string &str, std::vector<std::byte> &buffer)
    {
        // Add string characters including null terminator
        for (char c : str)
        {
            buffer.push_back(static_cast<std::byte>(c));
        }
        buffer.push_back(static_cast<std::byte>(0));

        // Pad to 4-byte boundary
        while (buffer.size() % 4 != 0)
        {
            buffer.push_back(static_cast<std::byte>(0));
        }
    }

    void Message::serializeArgument(const Value &value, std::vector<std::byte> &buffer)
    {
        // Implement serialization for different value types
        switch (value.getType())
        {
        case Value::Type::Int32:
        {
            int32_t val = value.asInt32();
            serializeInt32(val, buffer);
            break;
        }
        case Value::Type::Float:
        {
            float val = value.asFloat();
            serializeFloat(val, buffer);
            break;
        }
        case Value::Type::String:
        {
            serializeString(value.asString(), buffer);
            break;
        }
        case Value::Type::Blob:
        {
            const Blob &blob = value.asBlob();
            serializeBlob(blob, buffer);
            break;
        }
        default:
            throw MessageException("Unsupported argument type for serialization");
        }
    }

    void Message::serializeInt32(int32_t value, std::vector<std::byte> &buffer)
    {
        // Network byte order (big endian)
        buffer.push_back(static_cast<std::byte>((value >> 24) & 0xFF));
        buffer.push_back(static_cast<std::byte>((value >> 16) & 0xFF));
        buffer.push_back(static_cast<std::byte>((value >> 8) & 0xFF));
        buffer.push_back(static_cast<std::byte>(value & 0xFF));
    }

    void Message::serializeFloat(float value, std::vector<std::byte> &buffer)
    {
        // Convert float to int32 representation and serialize
        int32_t intVal;
        std::memcpy(&intVal, &value, sizeof(float));
        serializeInt32(intVal, buffer);
    }

    void Message::serializeBlob(const Blob &blob, std::vector<std::byte> &buffer)
    {
        // Size of blob
        serializeInt32(static_cast<int32_t>(blob.size()), buffer);

        // Blob data
        const std::byte *data = static_cast<const std::byte *>(blob.data());
        for (size_t i = 0; i < blob.size(); ++i)
        {
            buffer.push_back(data[i]);
        }

        // Pad to 4-byte boundary
        while (buffer.size() % 4 != 0)
        {
            buffer.push_back(static_cast<std::byte>(0));
        }
    }

} // namespace lo
