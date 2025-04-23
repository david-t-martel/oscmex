#include "Bundle.h"
#include <cstring>
#include <algorithm>
#include <array>

namespace osc
{

    // OSC bundle marker
    static const char *OSC_BUNDLE_TAG = "#bundle";

    // Helper functions for OSC bundle serialization
    namespace
    {
        // Align to 4-byte boundary
        size_t alignSize(size_t size)
        {
            return (size + 3) & ~3;
        }

        // Add padding bytes to align to 4-byte boundary
        void addPadding(std::vector<std::byte> &buffer, size_t originalSize)
        {
            size_t paddedSize = alignSize(originalSize);
            size_t paddingBytes = paddedSize - originalSize;

            if (paddingBytes > 0)
            {
                buffer.resize(buffer.size() + paddingBytes, std::byte{0});
            }
        }

        // Helper for big-endian conversion (network byte order)
        template <typename T>
        std::array<std::byte, sizeof(T)> toBigEndian(T value)
        {
            std::array<std::byte, sizeof(T)> result;

            // Check if host is little-endian
            const uint32_t test = 0x01020304;
            const bool isLittleEndian = (*reinterpret_cast<const uint8_t *>(&test) == 0x04);

            if (isLittleEndian)
            {
                // Convert to big-endian (network byte order)
                for (size_t i = 0; i < sizeof(T); ++i)
                {
                    result[i] = std::byte((value >> (8 * (sizeof(T) - 1 - i))) & 0xFF);
                }
            }
            else
            {
                // Already big-endian, just copy
                std::memcpy(result.data(), &value, sizeof(T));
            }

            return result;
        }
    }

    // Constructor with immediate timetag
    Bundle::Bundle() : time_(TimeTag::immediate())
    {
    }

    // Constructor with specific timetag
    Bundle::Bundle(const TimeTag &time) : time_(time)
    {
    }

    // Add a message to the bundle
    Bundle &Bundle::addMessage(Message message)
    {
        elements_.push_back(std::move(message));
        return *this;
    }

    // Add a message with a modified path
    Bundle &Bundle::addMessage(Message message, std::string_view path)
    {
        // Create a new message with the specified path
        Message newMsg(path);

        // Copy all arguments from the original message
        for (const auto &arg : message.arguments())
        {
            newMsg.add(arg);
        }

        return addMessage(std::move(newMsg));
    }

    // Add a nested bundle
    Bundle &Bundle::addBundle(Bundle bundle)
    {
        elements_.push_back(std::move(bundle));
        return *this;
    }

    // Serialize the bundle to binary data
    std::vector<std::byte> Bundle::serialize() const
    {
        std::vector<std::byte> buffer;

        // Allocate estimated size to avoid frequent reallocation
        buffer.reserve(
            alignSize(strlen(OSC_BUNDLE_TAG) + 1) +
            sizeof(TimeTag) +
            elements_.size() * 64 // Conservative estimate per element
        );

        // Add #bundle tag
        buffer.insert(buffer.end(),
                      reinterpret_cast<const std::byte *>(OSC_BUNDLE_TAG),
                      reinterpret_cast<const std::byte *>(OSC_BUNDLE_TAG) + strlen(OSC_BUNDLE_TAG) + 1);
        addPadding(buffer, strlen(OSC_BUNDLE_TAG) + 1);

        // Add timetag
        auto secBytes = toBigEndian(time_.seconds);
        auto fracBytes = toBigEndian(time_.fraction);
        buffer.insert(buffer.end(), secBytes.begin(), secBytes.end());
        buffer.insert(buffer.end(), fracBytes.begin(), fracBytes.end());

        // Add elements
        for (const auto &element : elements_)
        {
            std::vector<std::byte> elementData;

            // Get serialized data based on element type
            if (const Message *msg = std::get_if<Message>(&element))
            {
                elementData = msg->serialize();
            }
            else if (const Bundle *bundle = std::get_if<Bundle>(&element))
            {
                elementData = bundle->serialize();
            }

            // Add size of element
            auto sizeBytes = toBigEndian<int32_t>(static_cast<int32_t>(elementData.size()));
            buffer.insert(buffer.end(), sizeBytes.begin(), sizeBytes.end());

            // Add element data
            buffer.insert(buffer.end(), elementData.begin(), elementData.end());
        }

        return buffer;
    }

    // Deserialize from binary data
    Bundle Bundle::deserialize(const std::byte *data, size_t size)
    {
        if (size < 16)
        { // #bundle\0 + 8-byte timetag
            throw OSCException(OSCException::ErrorCode::InvalidMessage,
                               "Bundle data too small to be valid");
        }

        // Check for #bundle tag
        const char *tag = reinterpret_cast<const char *>(data);
        if (std::strcmp(tag, OSC_BUNDLE_TAG) != 0)
        {
            throw OSCException(OSCException::ErrorCode::InvalidMessage,
                               "Invalid bundle: missing #bundle tag");
        }

        // Skip to timetag (align to 4-byte boundary)
        size_t offset = alignSize(strlen(OSC_BUNDLE_TAG) + 1);

        // Read timetag
        uint32_t seconds, fraction;
        std::memcpy(&seconds, data + offset, 4);
        std::memcpy(&fraction, data + offset + 4, 4);

        // Convert from network byte order if needed
        const uint32_t test = 0x01020304;
        const bool isLittleEndian = (*reinterpret_cast<const uint8_t *>(&test) == 0x04);
        if (isLittleEndian)
        {
            seconds = ((seconds & 0xFF) << 24) |
                      ((seconds & 0xFF00) << 8) |
                      ((seconds & 0xFF0000) >> 8) |
                      ((seconds & 0xFF000000) >> 24);
            fraction = ((fraction & 0xFF) << 24) |
                       ((fraction & 0xFF00) << 8) |
                       ((fraction & 0xFF0000) >> 8) |
                       ((fraction & 0xFF000000) >> 24);
        }

        TimeTag time(seconds, fraction);
        Bundle bundle(time);

        // Move to first element
        offset += 8;

        // Parse elements
        while (offset + 4 <= size)
        {
            // Read element size
            int32_t elementSize;
            std::memcpy(&elementSize, data + offset, 4);

            // Convert from network byte order if needed
            if (isLittleEndian)
            {
                elementSize = ((elementSize & 0xFF) << 24) |
                              ((elementSize & 0xFF00) << 8) |
                              ((elementSize & 0xFF0000) >> 8) |
                              ((elementSize & 0xFF000000) >> 24);
            }

            offset += 4;

            if (offset + elementSize > size)
            {
                throw OSCException(OSCException::ErrorCode::InvalidMessage,
                                   "Bundle element size exceeds remaining data");
            }

            // Check if this is a nested bundle or a message
            if (elementSize >= 8 && std::strcmp(reinterpret_cast<const char *>(data + offset), OSC_BUNDLE_TAG) == 0)
            {
                // Nested bundle
                Bundle nestedBundle = Bundle::deserialize(data + offset, elementSize);
                bundle.addBundle(std::move(nestedBundle));
            }
            else
            {
                // Message
                Message message = Message::deserialize(data + offset, elementSize);
                bundle.addMessage(std::move(message));
            }

            offset += elementSize;
        }

        return bundle;
    }

    void Bundle::forEach(
        std::function<void(const Message &)> messageCallback,
        std::function<void(const Bundle &)> bundleCallback) const
    {
        for (const auto &element : elements_)
        {
            std::visit([&](const auto &e)
                       {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, Message>) {
                if (messageCallback) {
                    messageCallback(e);
                }
            }
            else if constexpr (std::is_same_v<T, Bundle>) {
                if (bundleCallback) {
                    bundleCallback(e);
                }
                // Recursively process nested bundles
                e.forEach(messageCallback, bundleCallback);
            } }, element);
        }
    }

} // namespace osc
