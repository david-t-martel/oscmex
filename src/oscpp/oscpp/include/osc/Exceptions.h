/*
 *  OSCPP - Open Sound Control C++ (OSCPP) Library.
 *  This file defines exceptions used throughout the OSC library.
 */

#pragma once

#include <stdexcept>
#include <string>

namespace osc {
    /**
     * @brief Base exception class for all OSC-related errors
     */
    class OSCException : public std::runtime_error {
       public:
        /**
         * @brief Error codes for OSC exceptions
         */
        enum class ErrorCode {
            None = 0,
            NetworkError,          ///< Network-related error
            MalformedPacket,       ///< Packet does not conform to OSC spec
            TypeMismatch,          ///< Type tag doesn't match expected types
            BufferOverflow,        ///< Buffer size exceeded
            UnknownType,           ///< Unknown OSC type tag
            AddressError,          ///< Error with OSC address pattern
            InvalidArgument,       ///< Invalid function argument
            InvalidMessage,        ///< Invalid OSC message format
            InvalidBundle,         ///< Invalid OSC bundle format
            PatternError,          ///< OSC pattern matching error
            ServerError,           ///< Server error
            ClientError,           ///< Client error
            OutOfMemory,           ///< Out of memory error
            NotImplemented,        ///< Feature not implemented
            SocketError,           ///< Socket error
            NetworkingError,       ///< Networking subsystem error
            MessageTooLarge,       ///< Message exceeds maximum allowed size
            DeserializationError,  ///< Error during deserialization
            SerializationError,    ///< Error during serialization
            MatchError             ///< Error during pattern matching
        };

        /**
         * @brief Construct a new OSC Exception
         * @param message Error message
         * @param code Error code
         */
        OSCException(const std::string &message, ErrorCode code = ErrorCode::None)
            : std::runtime_error(message), code_(code) {}

        /**
         * @brief Get the error code
         * @return ErrorCode
         */
        ErrorCode code() const { return code_; }

        /**
         * @brief Get a description for an error code
         * @param code The error code
         * @return std::string The description
         */
        static std::string getErrorDescription(ErrorCode code);

       private:
        ErrorCode code_;
    };

    /**
     * @brief Exception for network-related errors
     */
    class NetworkException : public OSCException {
       public:
        NetworkException(const std::string &message)
            : OSCException(message, ErrorCode::NetworkError) {}

        NetworkException(const std::string &message, ErrorCode code)
            : OSCException(message, code) {}
    };

    /**
     * @brief Exception for malformed packets
     */
    class MalformedPacketException : public OSCException {
       public:
        MalformedPacketException(const std::string &message)
            : OSCException(message, ErrorCode::MalformedPacket) {}
    };

    /**
     * @brief Exception for type mismatches
     */
    class TypeMismatchException : public OSCException {
       public:
        TypeMismatchException(const std::string &message)
            : OSCException(message, ErrorCode::TypeMismatch) {}
    };

    /**
     * @brief Exception for buffer overflows
     */
    class BufferOverflowException : public OSCException {
       public:
        BufferOverflowException(const std::string &message)
            : OSCException(message, ErrorCode::BufferOverflow) {}
    };

    /**
     * @brief Exception for unknown types
     */
    class UnknownTypeException : public OSCException {
       public:
        UnknownTypeException(const std::string &message)
            : OSCException(message, ErrorCode::UnknownType) {}
    };

    /**
     * @brief Exception for address errors
     */
    class AddressException : public OSCException {
       public:
        AddressException(const std::string &message)
            : OSCException(message, ErrorCode::AddressError) {}

        AddressException(const std::string &message, ErrorCode code)
            : OSCException(message, code) {}
    };

    /**
     * @brief Exception for invalid arguments
     */
    class InvalidArgumentException : public OSCException {
       public:
        InvalidArgumentException(const std::string &message)
            : OSCException(message, ErrorCode::InvalidArgument) {}
    };

    /**
     * @brief Exception for invalid messages
     */
    class InvalidMessageException : public OSCException {
       public:
        InvalidMessageException(const std::string &message)
            : OSCException(message, ErrorCode::InvalidMessage) {}
    };

    /**
     * @brief Exception for invalid bundles
     */
    class InvalidBundleException : public OSCException {
       public:
        InvalidBundleException(const std::string &message)
            : OSCException(message, ErrorCode::InvalidBundle) {}
    };

    /**
     * @brief Exception for pattern errors
     */
    class PatternException : public OSCException {
       public:
        PatternException(const std::string &message)
            : OSCException(message, ErrorCode::PatternError) {}
    };

    /**
     * @brief Exception for server errors
     */
    class ServerException : public OSCException {
       public:
        ServerException(const std::string &message)
            : OSCException(message, ErrorCode::ServerError) {}
    };

    /**
     * @brief Exception for client errors
     */
    class ClientException : public OSCException {
       public:
        ClientException(const std::string &message)
            : OSCException(message, ErrorCode::ClientError) {}
    };

    /**
     * @brief Exception for out of memory errors
     */
    class OutOfMemoryException : public OSCException {
       public:
        OutOfMemoryException(const std::string &message)
            : OSCException(message, ErrorCode::OutOfMemory) {}
    };

    /**
     * @brief Exception for not implemented features
     */
    class NotImplementedException : public OSCException {
       public:
        NotImplementedException(const std::string &message)
            : OSCException(message, ErrorCode::NotImplemented) {}
    };

    /**
     * @brief Exception for socket errors
     */
    class SocketException : public OSCException {
       public:
        SocketException(const std::string &message)
            : OSCException(message, ErrorCode::SocketError) {}

        SocketException(const std::string &message, ErrorCode code) : OSCException(message, code) {}
    };

    /**
     * @brief Exception for networking subsystem errors
     */
    class NetworkingException : public OSCException {
       public:
        NetworkingException(const std::string &message)
            : OSCException(message, ErrorCode::NetworkingError) {}
    };

    /**
     * @brief Exception for message size errors
     */
    class MessageSizeException : public OSCException {
       public:
        MessageSizeException(const std::string &message)
            : OSCException(message, ErrorCode::MessageTooLarge) {}
    };

    /**
     * @brief Exception for deserialization errors
     */
    class DeserializationException : public OSCException {
       public:
        DeserializationException(const std::string &message)
            : OSCException(message, ErrorCode::DeserializationError) {}
    };

    /**
     * @brief Exception for serialization errors
     */
    class SerializationException : public OSCException {
       public:
        SerializationException(const std::string &message)
            : OSCException(message, ErrorCode::SerializationError) {}
    };

    /**
     * @brief Exception for pattern matching errors
     */
    class MatchException : public OSCException {
       public:
        MatchException(const std::string &message) : OSCException(message, ErrorCode::MatchError) {}
    };

    /**
     * @brief Exception for platform-specific errors
     */
    class PlatformException : public OSCException {
       public:
        PlatformException(const std::string &message, ErrorCode code = ErrorCode::NotImplemented)
            : OSCException(message, code) {}
    };

}  // namespace osc
