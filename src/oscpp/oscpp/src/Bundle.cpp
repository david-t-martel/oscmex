// filepath: c:\codedev\auricleinc\oscmex\src\Bundle.cpp
#include "Bundle.h"
#include "Message.h"
#include "Types.h"
#include <vector>
#include <chrono>
#include <cstring>

namespace osc
{
    // Bundle Implementation

    // Constructor with time tag
    Bundle::Bundle(const TimeTag &timeTag)
        : timeTag_(timeTag)
    {
    }

    // Add a message to the bundle
    void Bundle::addMessage(const Message &message)
    {
        messages_.push_back(message);
    }

    // Get all messages in the bundle
    const std::vector<Message> &Bundle::messages() const
    {
        return messages_;
    }

    // Get the time tag
    const TimeTag &Bundle::time() const
    {
        return timeTag_;
    }

    // Check if the bundle is empty
    bool Bundle::isEmpty() const
    {
        return messages_.empty();
    }

    // Get the number of elements
    size_t Bundle::size() const
    {
        return messages_.size();
    }

    // Iterate through messages and nested bundles
    void Bundle::forEach(MessageCallbackFn messageCb, BundleCallbackFn bundleCb) const
    {
        // In this implementation, we only have direct messages, no nested bundles
        for (const auto &message : messages_)
        {
            if (messageCb)
            {
                messageCb(message);
            }
        }
    }

    // Helper function to append a 32-bit integer in big-endian format
    void appendInt32BE(std::vector<std::byte> &buffer, int32_t value)
    {
        buffer.push_back(std::byte((value >> 24) & 0xFF));
        buffer.push_back(std::byte((value >> 16) & 0xFF));
        buffer.push_back(std::byte((value >> 8) & 0xFF));
        buffer.push_back(std::byte(value & 0xFF));
    }

    // Helper function to append OSC-string with padding
    void appendOSCString(std::vector<std::byte> &buffer, const std::string &str)
    {
        // Add the string characters and null terminator
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

    // Serialize the bundle to OSC binary format
    std::vector<std::byte> Bundle::serialize() const
    {
        std::vector<std::byte> buffer;

        // OSC-string "#bundle"
        appendOSCString(buffer, "#bundle");

        // OSC-timetag (8 bytes)
        uint64_t ntp = timeTag_.toNTP();
        buffer.push_back(std::byte((ntp >> 56) & 0xFF));
        buffer.push_back(std::byte((ntp >> 48) & 0xFF));
        buffer.push_back(std::byte((ntp >> 40) & 0xFF));
        buffer.push_back(std::byte((ntp >> 32) & 0xFF));
        buffer.push_back(std::byte((ntp >> 24) & 0xFF));
        buffer.push_back(std::byte((ntp >> 16) & 0xFF));
        buffer.push_back(std::byte((ntp >> 8) & 0xFF));
        buffer.push_back(std::byte(ntp & 0xFF));

        // Bundle elements
        for (const auto &message : messages_)
        {
            // Serialize the message
            auto messageData = message.serialize();

            // Element size (32-bit big-endian integer)
            appendInt32BE(buffer, static_cast<int32_t>(messageData.size()));

            // Element data
            buffer.insert(buffer.end(), messageData.begin(), messageData.end());
        }

        return buffer;
    }

    // Deserialize a bundle from binary data
    Bundle Bundle::deserialize(const std::byte *data, size_t size)
    {
        if (size < 16)
        { // Minimum size: "#bundle\0" (8 bytes) + time tag (8 bytes)
            throw OSCException(OSCException::ErrorCode::InvalidBundle, "Bundle data too small");
        }

        // Check for "#bundle" identifier
        const char *cdata = reinterpret_cast<const char *>(data);
        if (std::strncmp(cdata, "#bundle", 7) != 0)
        {
            throw OSCException(OSCException::ErrorCode::InvalidBundle, "Not an OSC bundle (missing #bundle)");
        }

        // Skip past #bundle string (8 bytes with padding)
        size_t pos = 8;

        // Extract time tag (8 bytes)
        uint64_t ntpTime =
            (static_cast<uint64_t>(data[pos]) << 56) |
            (static_cast<uint64_t>(data[pos + 1]) << 48) |
            (static_cast<uint64_t>(data[pos + 2]) << 40) |
            (static_cast<uint64_t>(data[pos + 3]) << 32) |
            (static_cast<uint64_t>(data[pos + 4]) << 24) |
            (static_cast<uint64_t>(data[pos + 5]) << 16) |
            (static_cast<uint64_t>(data[pos + 6]) << 8) |
            static_cast<uint64_t>(data[pos + 7]);

        TimeTag timeTag(ntpTime);
        Bundle bundle(timeTag);

        // Move past time tag
        pos += 8;

        // Process bundle elements
        while (pos + 4 <= size)
        { // Need at least 4 bytes for size
            // Get element size
            int32_t elementSize =
                (static_cast<int32_t>(data[pos]) << 24) |
                (static_cast<int32_t>(data[pos + 1]) << 16) |
                (static_cast<int32_t>(data[pos + 2]) << 8) |
                static_cast<int32_t>(data[pos + 3]);

            pos += 4;

            // Validate element size
            if (elementSize < 0 || pos + elementSize > size)
            {
                throw OSCException(OSCException::ErrorCode::InvalidBundle, "Invalid element size in bundle");
            }

            // Check if this is a nested bundle or a message
            if (pos + 8 <= size && std::strncmp(cdata + pos, "#bundle", 7) == 0)
            {
                // Nested bundle - this is not implemented in the current version
                // Just skip past this element
                pos += elementSize;
            }
            else
            {
                // Regular message - parse and add to the bundle
                try
                {
                    Message message = Message::deserialize(data + pos, elementSize);
                    bundle.addMessage(message);
                }
                catch (const OSCException &e)
                {
                    // If we can't parse this message, skip it
                    if (e.code() == OSCException::ErrorCode::InvalidMessage)
                    {
                        // Just log and continue
                        // You might want to propagate this error in a real implementation
                    }
                    else
                    {
                        throw; // Re-throw other exceptions
                    }
                }

                // Move to the next element
                pos += elementSize;
            }
        }

        return bundle;
    }

} // namespace osc
