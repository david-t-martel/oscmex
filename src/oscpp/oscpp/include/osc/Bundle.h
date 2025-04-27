// filepath: c:\codedev\auricleinc\oscmex\include\osc\Bundle.h
#pragma once

#include "Types.h"
#include <vector>
#include <string>

namespace osc
{

    /**
     * @brief Represents a collection of OSC messages that can be sent together.
     */
    class Bundle
    {
    public:
        /**
         * @brief Default constructor for Bundle.
         */
        Bundle();

        /**
         * @brief Add a message to the bundle.
         * @param message The OSC message to add.
         */
        void addMessage(const Message &message);

        /**
         * @brief Get the messages in the bundle.
         * @return A vector of OSC messages.
         */
        const std::vector<Message>& messages() const;

        /**
         * @brief Serialize the bundle to a binary format for transmission.
         * @return A vector of bytes representing the serialized bundle.
         */
        std::vector<std::byte> serialize() const;

    private:
        std::vector<Message> messages_; ///< Collection of OSC messages in the bundle.
    };

} // namespace osc