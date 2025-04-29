#include <unordered_map>

#include "osc/Exceptions.h"

namespace osc {
    // Static method to get a description for an error code
    std::string OSCException::getErrorDescription(ErrorCode code) {
        static const std::unordered_map<ErrorCode, std::string> descriptions = {
            {ErrorCode::None, "No error"},
            {ErrorCode::NetworkError, "Network error"},
            {ErrorCode::MalformedPacket, "Malformed OSC packet"},
            {ErrorCode::TypeMismatch, "OSC type mismatch"},
            {ErrorCode::BufferOverflow, "Buffer overflow"},
            {ErrorCode::UnknownType, "Unknown OSC type"},
            {ErrorCode::AddressError, "Invalid OSC address pattern"},
            {ErrorCode::InvalidArgument, "Invalid argument"},
            {ErrorCode::InvalidMessage, "Invalid OSC message"},
            {ErrorCode::InvalidBundle, "Invalid OSC bundle"},
            {ErrorCode::PatternError, "Pattern matching error"},
            {ErrorCode::ServerError, "Server error"},
            {ErrorCode::ClientError, "Client error"},
            {ErrorCode::OutOfMemory, "Out of memory"},
            {ErrorCode::NotImplemented, "Feature not implemented"},
            {ErrorCode::SocketError, "Socket error"},
            {ErrorCode::NetworkingError, "Networking subsystem error"},
            {ErrorCode::MessageTooLarge, "Message exceeds maximum allowed size"},
            {ErrorCode::DeserializationError, "Error during OSC deserialization"},
            {ErrorCode::SerializationError, "Error during OSC serialization"},
            {ErrorCode::MatchError, "Error in pattern matching"}};

        auto it = descriptions.find(code);
        if (it != descriptions.end()) {
            return it->second;
        }

        return "Unknown error";
    }
}  // namespace osc
