/*
 *  OSCPP - Open Sound Control C++ (OSCPP) Library.
 *  TO DO: Add Exceptions implementation from REQUIREMENTS.md and from SPECIFICATION.md
 */

#ifndef OSC_EXCEPTIONS_H
#define OSC_EXCEPTIONS_H

#include <stdexcept>
#include <string>

namespace osc
{

    class Exception : public std::runtime_error
    {
    public:
        Exception(const std::string &msg) : std::runtime_error(msg) {}
        virtual ~Exception() throw() {}
    };

    class InvalidMessageException : public Exception
    {
    public:
        InvalidMessageException(const std::string &msg) : Exception(msg) {}
    };

    class InvalidAddressException : public Exception
    {
    public:
        InvalidAddressException(const std::string &msg) : Exception(msg) {}
    };

} // namespace osc

#endif // OSC_EXCEPTIONS_H
