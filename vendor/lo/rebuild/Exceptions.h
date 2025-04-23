/*
 *  Copyright (C) 2014 Steve Harris et al. (see AUTHORS)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
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
