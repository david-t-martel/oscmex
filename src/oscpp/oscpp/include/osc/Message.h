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
#include <stdexcept>

namespace osc
{
    /**
     * @brief The Message class represents an OSC message.
     */
    class Message
    {
    public:
        /**
         * @brief Construct a new OSC Message object
         * @param path The OSC address path (must start with '/')
         */
        explicit Message(const std::string &path);

        /**
         * @brief Get the OSC address path
         * @return The path as a string
         */
        std::string getPath() const;

        /**
         * @brief Get the arguments in this message
         * @return A const reference to the argument vector
         */
        const std::vector<Value> &getArguments() const;

        /**
         * @brief Add an Int32 argument (type tag 'i')
         * @param value The integer value to add
         * @return Reference to this message for method chaining
         */
        Message &addInt32(int32_t value);

        /**
         * @brief Add an Int64 argument (type tag 'h')
         * @param value The int64 value to add
         * @return Reference to this message for method chaining
         */
        Message &addInt64(int64_t value);

        /**
         * @brief Add a Float argument (type tag 'f')
         * @param value The float value to add
         * @return Reference to this message for method chaining
         */
        Message &addFloat(float value);

        /**
         * @brief Add a Double argument (type tag 'd')
         * @param value The double value to add
         * @return Reference to this message for method chaining
         */
        Message &addDouble(double value);

        /**
         * @brief Add a String argument (type tag 's')
         * @param value The string value to add
         * @return Reference to this message for method chaining
         */
        Message &addString(const std::string &value);

        /**
         * @brief Add a Symbol argument (type tag 'S')
         * @param value The symbol string to add
         * @return Reference to this message for method chaining
         */
        Message &addSymbol(const std::string &value);

        /**
         * @brief Add a Blob argument (type tag 'b')
         * @param data Pointer to the binary data
         * @param size Size of the binary data in bytes
         * @return Reference to this message for method chaining
         */
        Message &addBlob(const void *data, size_t size);

        /**
         * @brief Add a TimeTag argument (type tag 't')
         * @param timeTag The time tag value to add
         * @return Reference to this message for method chaining
         */
        Message &addTimeTag(const TimeTag &timeTag);

        /**
         * @brief Add a Character argument (type tag 'c')
         * @param value The character value to add
         * @return Reference to this message for method chaining
         */
        Message &addChar(char value);

        /**
         * @brief Add a RGBA Color argument (type tag 'r')
         * @param value The RGBA color value to add (packed as a uint32_t)
         * @return Reference to this message for method chaining
         */
        Message &addColor(uint32_t value);

        /**
         * @brief Add a MIDI message argument (type tag 'm')
         * @param port Port ID (byte 0)
         * @param status Status byte (byte 1)
         * @param data1 Data byte 1 (byte 2)
         * @param data2 Data byte 2 (byte 3)
         * @return Reference to this message for method chaining
         */
        Message &addMidi(uint8_t port, uint8_t status, uint8_t data1, uint8_t data2);

        /**
         * @brief Add a True argument (type tag 'T')
         * @return Reference to this message for method chaining
         */
        Message &addTrue();

        /**
         * @brief Add a False argument (type tag 'F')
         * @return Reference to this message for method chaining
         */
        Message &addFalse();

        /**
         * @brief Add a boolean argument (type tag 'T' or 'F')
         * @param value The boolean value to add
         * @return Reference to this message for method chaining
         */
        Message &addBool(bool value);

        /**
         * @brief Add a Nil argument (type tag 'N')
         * @return Reference to this message for method chaining
         */
        Message &addNil();

        /**
         * @brief Add an Infinitum argument (type tag 'I')
         * @return Reference to this message for method chaining
         */
        Message &addInfinitum();

        /**
         * @brief Add an Array of arguments (type tags '[ ... ]')
         * @param array The array of values to add
         * @return Reference to this message for method chaining
         */
        Message &addArray(const std::vector<Value> &array);

        /**
         * @brief Add a generic Value argument
         * @param value The OSC value to add
         * @return Reference to this message for method chaining
         */
        Message &addValue(const Value &value);

        /**
         * @brief Serialize the message to the OSC binary format
         * @return Vector of bytes representing the serialized message
         */
        std::vector<std::byte> serialize() const;

        /**
         * @brief Deserialize a message from OSC binary format
         * @param data Pointer to the binary data
         * @param size Size of the binary data in bytes
         * @return The deserialized Message object
         * @throws OSCException if the data is invalid
         */
        static Message deserialize(const std::byte *data, size_t size);

    private:
        std::string path_;
        std::vector<Value> arguments_;
    };

} // namespace osc
