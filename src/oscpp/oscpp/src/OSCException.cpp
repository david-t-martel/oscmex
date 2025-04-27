#include "Types.h"
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace osc
{
    // Static map for error code descriptions
    static const std::unordered_map<OSCException::ErrorCode, std::string> errorDescriptions = {
        {OSCException::ErrorCode::Unknown, "Unknown error"},
        {OSCException::ErrorCode::NetworkError, "Network error"},
        {OSCException::ErrorCode::InvalidAddress, "Invalid OSC address pattern"},
        {OSCException::ErrorCode::InvalidMessage, "Invalid OSC message format"},
        {OSCException::ErrorCode::InvalidBundle, "Invalid OSC bundle format"},
        {OSCException::ErrorCode::InvalidArgument, "Invalid OSC argument"},
        {OSCException::ErrorCode::PatternError, "OSC pattern matching error"}};

    // Constructor implementation
    OSCException::OSCException(ErrorCode code, const std::string &message)
        : std::runtime_error(message), code_(code)
    {
    }

    // Get the error code
    OSCException::ErrorCode OSCException::code() const
    {
        return code_;
    }

    // Get a description for an error code
    std::string OSCException::getErrorDescription(ErrorCode code)
    {
        auto it = errorDescriptions.find(code);
        if (it != errorDescriptions.end())
        {
            return it->second;
        }
        return "Unknown error code";
    }

    // Get the error message
    const char *OSCException::what() const noexcept
    {
        return std::runtime_error::what();
    }

    // Helper function to get an error message for a given error code
    std::string OSCException::getErrorMessage(ErrorCode code)
    {
        switch (code)
        {
        case ErrorCode::None:
            return "No error";
        case ErrorCode::NetworkError:
            return "Network error";
        case ErrorCode::InvalidArgument:
            return "Invalid argument";
        case ErrorCode::InvalidMessage:
            return "Invalid OSC message";
        case ErrorCode::InvalidBundle:
            return "Invalid OSC bundle";
        case ErrorCode::PatternError:
            return "Invalid pattern";
        case ErrorCode::ParseError:
            return "Parse error";
        case ErrorCode::ServerError:
            return "Server error";
        case ErrorCode::ClientError:
            return "Client error";
        case ErrorCode::OutOfMemory:
            return "Out of memory";
        case ErrorCode::NotImplemented:
            return "Feature not implemented";
        default:
            return "Unknown error";
        }
    }

} // namespace osc
