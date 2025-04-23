#pragma once

#include "Types.h"
#include "Message.h"
#include <vector>
#include <string>
#include <string_view>
#include <functional>
#include <variant>

namespace osc
{

    /**
     * @brief Represents an OSC bundle containing messages and/or other bundles
     *
     * The Bundle class allows grouping multiple OSC messages, with an optional
     * time tag for scheduling when they should be executed.
     */
    class Bundle
    {
    public:
        /**
         * @brief Create a bundle with immediate execution
         */
        Bundle();

        /**
         * @brief Create a bundle with a specific timetag
         * @param time The time at which this bundle should be executed
         */
        explicit Bundle(const TimeTag &time);

        /**
         * @brief Add a message to the bundle
         * @param message The message to add
         * @return Reference to this bundle for chaining
         */
        Bundle &addMessage(Message message);

        /**
         * @brief Add a message with a modified path to the bundle
         * @param message The message to add
         * @param path The new path to use
         * @return Reference to this bundle for chaining
         */
        Bundle &addMessage(Message message, std::string_view path);

        /**
         * @brief Add a nested bundle to this bundle
         * @param bundle The bundle to add
         * @return Reference to this bundle for chaining
         */
        Bundle &addBundle(Bundle bundle);

        /**
         * @brief Get the time tag of this bundle
         * @return Time tag
         */
        const TimeTag &time() const { return time_; }

        /**
         * @brief Serialize the bundle to binary format
         * @return Binary data as vector of bytes
         */
        std::vector<std::byte> serialize() const;

        /**
         * @brief Deserialize a bundle from binary data
         * @param data Pointer to binary data
         * @param size Size of the data in bytes
         * @return Deserialized bundle
         * @throws OSCException if data is invalid
         */
        static Bundle deserialize(const std::byte *data, size_t size);

        /**
         * @brief Process all messages and nested bundles in this bundle
         * @param messageCallback Function to call for each message
         * @param bundleCallback Function to call for each nested bundle (before its contents)
         */
        void forEach(
            std::function<void(const Message &)> messageCallback,
            std::function<void(const Bundle &)> bundleCallback = nullptr) const;

        /**
         * @brief Check if the bundle is empty
         * @return true if the bundle has no elements
         */
        bool isEmpty() const { return elements_.empty(); }

        /**
         * @brief Get the number of elements in the bundle
         * @return Element count
         */
        size_t size() const { return elements_.size(); }

        /**
         * @brief Factory method to create a bundle with messages in one call
         * @tparam Args Types of the messages or bundles to add
         * @param time Time tag for the bundle
         * @param args Messages or bundles to include
         * @return New bundle with the given messages/bundles
         */
        template <typename... Args>
        static Bundle create(const TimeTag &time, Args &&...args)
        {
            Bundle bundle(time);
            (addElement(bundle, std::forward<Args>(args)), ...);
            return bundle;
        }

    private:
        TimeTag time_;                                        // Time tag for execution
        std::vector<std::variant<Message, Bundle>> elements_; // Bundle elements

        // Helper for the factory method
        template <typename T>
        static void addElement(Bundle &bundle, T &&element)
        {
            if constexpr (std::is_same_v<std::decay_t<T>, Message>)
            {
                bundle.addMessage(std::forward<T>(element));
            }
            else if constexpr (std::is_same_v<std::decay_t<T>, Bundle>)
            {
                bundle.addBundle(std::forward<T>(element));
            }
            else
            {
                static_assert(std::is_same_v<std::decay_t<T>, Message> ||
                                  std::is_same_v<std::decay_t<T>, Bundle>,
                              "Bundle can only contain Message or Bundle objects");
            }
        }
    };

} // namespace osc
