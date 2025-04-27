/*
 *  OSCPP - Open Sound Control C++ (OSCPP) Library.
 *  TO DO: Add Exceptions implementation from REQUIREMENTS.md and from SPECIFICATION.md
 */

#pragma once

#include "Types.h"
#include <string>
#include <vector>
#include <cstddef>

namespace osc
{

    class Message
    {
    public:
        explicit Message(const std::string &path);
        ~Message();

        // Accessors
        const std::string &getPath() const;
        const std::vector<Value> &getArguments() const;

        // Add arguments of different types
        void addInt32(int32_t value);
        void addFloat(float value);
        void addString(const std::string &value);
        void addBlob(const void *data, size_t size);

        // Serialize the message to binary format according to OSC spec
        std::vector<std::byte> serialize() const;

    private:
        std::string path_;
        std::vector<Value> arguments_;

        // Helper methods for serialization
        static void serializeString(const std::string &str, std::vector<std::byte> &buffer);
        static void serializeInt32(int32_t value, std::vector<std::byte> &buffer);
        static void serializeFloat(float value, std::vector<std::byte> &buffer);
        static void serializeBlob(const Blob &blob, std::vector<std::byte> &buffer);
        static void serializeArgument(const Value &value, std::vector<std::byte> &buffer);
    };

} // namespace osc
