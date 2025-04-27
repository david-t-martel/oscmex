// filepath: c:\codedev\auricleinc\oscmex\include\osc\Bundle.h
#pragma once

#include "Types.h"
#include "Message.h"
#include <vector>
#include <string>

namespace osc
{

    /**
     * @brief The Bundle class represents an OSC bundle, which is a collection of OSC messages or other bundles
     * that should be executed atomically and potentially at a specific time.
     */
    class Bundle
    {
    public:
        /**
         * @brief Construct a new Bundle object with the specified time tag
         * @param timeTag The time at which this bundle should be executed (default: immediate)
         */
        explicit Bundle(const TimeTag &timeTag = TimeTag::immediate());

        /**
         * @brief Add a message to this bundle
         * @param message The OSC message to add
         * @return Reference to this bundle for method chaining
         */
        Bundle &addMessage(const Message &message);

        /**
         * @brief Add another bundle as a child of this bundle
         * @param bundle The child bundle to add
         * @return Reference to this bundle for method chaining
         */
        Bundle &addBundle(const Bundle &bundle);

        /**
         * @brief Get the time tag for this bundle
         * @return The time tag
         */
        TimeTag getTimeTag() const;

        /**
         * @brief Get the messages in this bundle
         * @return Const reference to the vector of messages
         */
        const std::vector<Message> &messages() const;

        /**
         * @brief Get the child bundles in this bundle
         * @return Const reference to the vector of child bundles
         */
        const std::vector<Bundle> &bundles() const;

        /**
         * @brief Serialize the bundle to the OSC binary format
         * @return Vector of bytes representing the serialized bundle
         */
        std::vector<std::byte> serialize() const;

        /**
         * @brief Deserialize a bundle from OSC binary format
         * @param data Pointer to the binary data
         * @param size Size of the binary data in bytes
         * @return The deserialized Bundle object
         * @throws OSCException if the data is invalid
         */
        static Bundle deserialize(const std::byte *data, size_t size);

        /**
         * @brief Execute a function for each message in the bundle and its sub-bundles
         * @param callback The function to call for each message
         */
        void forEach(std::function<void(const Message &)> callback) const;

    private:
        TimeTag timeTag_;
        std::vector<Message> messages_;
        std::vector<Bundle> childBundles_;
    };

} // namespace osc
