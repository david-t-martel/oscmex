#include "osc/Bundle.h"

#include <chrono>
#include <cstring>
#include <vector>

#include "osc/Address.h"
#include "osc/Exceptions.h"
#include "osc/Message.h"
#include "osc/Types.h"

namespace osc {
    // Constructor with time tag
    Bundle::Bundle(const TimeTag& timeTag) : timeTag_(timeTag) {}

    // Add a message to the bundle
    Bundle& Bundle::addMessage(const Message& message) {
        messages_.push_back(message);
        return *this;
    }

    // Add a child bundle to this bundle
    Bundle& Bundle::addBundle(const Bundle& bundle) {
        childBundles_.push_back(bundle);
        return *this;
    }

    // Get the time tag for this bundle
    TimeTag Bundle::getTimeTag() const { return timeTag_; }

    // Get all messages in the bundle
    const std::vector<Message>& Bundle::messages() const { return messages_; }

    // Get all child bundles
    const std::vector<Bundle>& Bundle::bundles() const { return childBundles_; }

    // Helper function to pad to 4-byte boundary
    inline size_t padSize(size_t size) { return (size + 3) & ~3; }

    // Serialize the bundle to OSC binary format
    std::vector<std::byte> Bundle::serialize() const {
        try {
            std::vector<std::byte> result;

            // Reserve a reasonable initial capacity to avoid frequent reallocations
            result.reserve(512);  // Adjust based on expected bundle size

            // 1. Add "#bundle" OSC-string (with null terminator and padding)
            const char* bundleStr = "#bundle";
            size_t bundleLen = std::strlen(bundleStr) + 1;  // Include null terminator

            const std::byte* bundleBytes = reinterpret_cast<const std::byte*>(bundleStr);
            result.insert(result.end(), bundleBytes, bundleBytes + bundleLen);

            // Add padding to align to 4-byte boundary
            size_t paddedBundleSize = padSize(bundleLen);
            result.resize(paddedBundleSize, std::byte{0});

            // 2. Add Time Tag (8 bytes, big-endian)
            uint64_t ntp = timeTag_.toNTP();

            // Split into high and low 32-bit words
            uint32_t high = static_cast<uint32_t>(ntp >> 32);
            uint32_t low = static_cast<uint32_t>(ntp & 0xFFFFFFFF);

            // Convert to big-endian
            high = htonl(high);
            low = htonl(low);

            // Add high word (first 4 bytes)
            const std::byte* highBytes = reinterpret_cast<const std::byte*>(&high);
            result.insert(result.end(), highBytes, highBytes + sizeof(uint32_t));

            // Add low word (next 4 bytes)
            const std::byte* lowBytes = reinterpret_cast<const std::byte*>(&low);
            result.insert(result.end(), lowBytes, lowBytes + sizeof(uint32_t));

            // 3. Add Bundle Elements (each prefixed with its size)

            // Process messages
            for (const auto& message : messages_) {
                try {
                    // Serialize the message
                    auto messageData = message.serialize();

                    // Check for empty message (shouldn't happen but let's be safe)
                    if (messageData.empty()) {
                        continue;  // Skip empty messages
                    }

                    // Add element size (int32, big-endian)
                    uint32_t size = static_cast<uint32_t>(messageData.size());
                    uint32_t sizeBE = htonl(size);

                    const std::byte* sizeBytes = reinterpret_cast<const std::byte*>(&sizeBE);
                    result.insert(result.end(), sizeBytes, sizeBytes + sizeof(uint32_t));

                    // Add element data
                    result.insert(result.end(), messageData.begin(), messageData.end());
                } catch (const OSCException& e) {
                    // Add context to the exception and rethrow
                    throw OSCException(
                        "Error serializing message in bundle: " + std::string(e.what()), e.code());
                }
            }

            // Process child bundles
            for (const auto& childBundle : childBundles_) {
                try {
                    // Serialize the child bundle
                    auto bundleData = childBundle.serialize();

                    // Check for empty bundle (shouldn't happen but let's be safe)
                    if (bundleData.empty()) {
                        continue;  // Skip empty bundles
                    }

                    // Add element size (int32, big-endian)
                    uint32_t size = static_cast<uint32_t>(bundleData.size());
                    uint32_t sizeBE = htonl(size);

                    const std::byte* sizeBytes = reinterpret_cast<const std::byte*>(&sizeBE);
                    result.insert(result.end(), sizeBytes, sizeBytes + sizeof(uint32_t));

                    // Add element data
                    result.insert(result.end(), bundleData.begin(), bundleData.end());
                } catch (const OSCException& e) {
                    // Add context to the exception and rethrow
                    throw OSCException("Error serializing nested bundle: " + std::string(e.what()),
                                       e.code());
                }
            }

            return result;

        } catch (const OSCException&) {
            // Re-throw OSC exceptions (already properly formatted)
            throw;
        } catch (const std::exception& e) {
            // Wrap other exceptions with additional context
            throw OSCException("Error serializing OSC bundle: " + std::string(e.what()),
                               OSCException::ErrorCode::SerializationError);
        } catch (...) {
            // Catch any other unexpected errors
            throw OSCException("Unknown error during OSC bundle serialization",
                               OSCException::ErrorCode::SerializationError);
        }
    }

    // Deserialize a bundle from binary data
    Bundle Bundle::deserialize(const std::byte* data, size_t size) {
        if (!data) {
            throw OSCException("Null data pointer provided to deserialize",
                               OSCException::ErrorCode::InvalidArgument);
        }

        if (size < 16) {  // Minimum size: "#bundle\0" (8 bytes with padding) + time tag (8 bytes)
            throw OSCException("Bundle data too small: " + std::to_string(size) +
                                   " bytes (minimum required: 16 bytes)",
                               OSCException::ErrorCode::MalformedPacket);
        }

        try {
            // 1. Check for "#bundle" identifier
            const char* cdata = reinterpret_cast<const char*>(data);
            if (std::strncmp(cdata, "#bundle", 7) != 0) {
                throw OSCException("Not an OSC bundle (missing #bundle identifier)",
                                   OSCException::ErrorCode::MalformedPacket);
            }

            // Skip past "#bundle" string (8 bytes with padding)
            size_t pos = 8;

            // 2. Extract time tag (8 bytes)
            if (pos + 8 > size) {
                throw OSCException("Bundle data truncated: missing time tag",
                                   OSCException::ErrorCode::MalformedPacket);
            }

            uint32_t seconds, fraction;
            std::memcpy(&seconds, data + pos, sizeof(uint32_t));
            std::memcpy(&fraction, data + pos + sizeof(uint32_t), sizeof(uint32_t));

            // Convert from big-endian
            seconds = ntohl(seconds);
            fraction = ntohl(fraction);

            // Create TimeTag from seconds and fraction
            TimeTag timeTag(seconds, fraction);

            // Create the bundle with extracted time tag
            Bundle bundle(timeTag);

            // Move past time tag
            pos += 8;

            // 3. Process bundle elements
            while (pos + 4 <= size) {  // Need at least 4 bytes for element size
                // Get element size (int32, big-endian)
                uint32_t elementSizeBE;
                std::memcpy(&elementSizeBE, data + pos, sizeof(uint32_t));
                uint32_t elementSize = ntohl(elementSizeBE);

                // Move past size field
                pos += 4;

                // Validate element size
                if (elementSize == 0) {
                    throw OSCException("Invalid element size: zero-length element",
                                       OSCException::ErrorCode::MalformedPacket);
                }

                if (pos + elementSize > size) {
                    throw OSCException("Invalid element size or truncated bundle: claimed size " +
                                           std::to_string(elementSize) +
                                           " bytes exceeds remaining data",
                                       OSCException::ErrorCode::MalformedPacket);
                }

                // Check if this element is a nested bundle or a message
                if (elementSize >= 8 && std::strncmp(cdata + pos, "#bundle", 7) == 0) {
                    // Nested bundle - recursively deserialize
                    try {
                        Bundle childBundle = Bundle::deserialize(data + pos, elementSize);
                        bundle.addBundle(childBundle);
                    } catch (const OSCException& e) {
                        // Add context to the exception and rethrow
                        throw OSCException("Error in nested bundle: " + std::string(e.what()),
                                           e.code());
                    }
                } else {
                    // Regular message - parse and add to the bundle
                    try {
                        Message message = Message::deserialize(data + pos, elementSize);
                        bundle.addMessage(message);
                    } catch (const OSCException& e) {
                        // Add context to the exception and rethrow
                        throw OSCException("Error in bundled message: " + std::string(e.what()),
                                           e.code());
                    }
                }

                // Move to the next element
                pos += elementSize;
            }

            // Ensure we consumed all the data - this is more of a sanity check since
            // we allow extra bytes at the end for forward compatibility
            if (pos < size) {
                // This is just a warning - we don't throw an exception for this case
                // Some implementations might add padding or extra data
            }

            return bundle;

        } catch (const OSCException&) {
            // Re-throw OSC exceptions (already properly formatted)
            throw;
        } catch (const std::exception& e) {
            // Wrap other exceptions with additional context
            throw OSCException("Error deserializing OSC bundle: " + std::string(e.what()),
                               OSCException::ErrorCode::DeserializationError);
        } catch (...) {
            // Catch any other unexpected errors
            throw OSCException("Unknown error during OSC bundle deserialization",
                               OSCException::ErrorCode::DeserializationError);
        }
    }

    // Execute a function for each message in the bundle and its sub-bundles
    void Bundle::forEach(std::function<void(const Message&)> callback) const {
        // Process messages in this bundle
        for (const auto& message : messages_) {
            callback(message);
        }

        // Recursively process messages in child bundles
        for (const auto& childBundle : childBundles_) {
            childBundle.forEach(callback);
        }
    }

}  // namespace osc
