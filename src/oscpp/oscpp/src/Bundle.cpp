// filepath: c:\codedev\auricleinc\oscmex\src\Bundle.cpp
#include "Bundle.h"
#include "Message.h"
#include "Types.h"
#include <vector>
#include <chrono>

namespace osc
{

    // Bundle Implementation

    Bundle::Bundle(const TimeTag &timeTag)
        : timeTag_(timeTag)
    {
    }

    void Bundle::addMessage(const Message &message)
    {
        messages_.push_back(message);
    }

    const std::vector<Message> &Bundle::messages() const
    {
        return messages_;
    }

    const TimeTag &Bundle::time() const
    {
        return timeTag_;
    }

    std::vector<std::byte> Bundle::serialize() const
    {
        std::vector<std::byte> buffer;

        // Serialize the time tag
        auto timeTagBytes = timeTag_.toNTP();
        buffer.insert(buffer.end(), reinterpret_cast<std::byte *>(&timeTagBytes), reinterpret_cast<std::byte *>(&timeTagBytes) + sizeof(timeTagBytes));

        // Serialize each message in the bundle
        for (const auto &message : messages_)
        {
            auto messageBytes = message.serialize();
            buffer.insert(buffer.end(), messageBytes.begin(), messageBytes.end());
        }

        return buffer;
    }

} // namespace osc
