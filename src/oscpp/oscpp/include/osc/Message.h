// filepath: c:\codedev\auricleinc\oscmex\include\osc\Message.h
/*
 *  OSCPP - Open Sound Control C++ (OSCPP) Library.
 *  This header file declares the Message class, which represents an OSC message,
 *  including its path and arguments.
 */

#pragma once

#include "Types.h"
#include <string>
#include <vector>

namespace osc
{

    class Message
    {
    public:
        // Constructor with path
        explicit Message(const std::string &path);

        // Add argument methods
        void addInt32(int32_t value);
        void addFloat(float value);
        void addString(const std::string &value);
        void addTimeTag(const TimeTag &value);
        void addBlob(const Blob &value);
        void addMidi(const std::array<uint8_t, 4> &midi);
        void addBool(bool value);
        void addChar(char value);
        void addColor(uint32_t rgba);

        // Getters
        const std::string &getPath() const;
        size_t getArgumentCount() const;
        const Value &getArgument(size_t index) const;

        // Serialization
        std::vector<std::byte> serialize() const;

    private:
        std::string path_;                     // OSC message path
        std::vector<Value> arguments_;         // Arguments of the message
    };

} // namespace osc