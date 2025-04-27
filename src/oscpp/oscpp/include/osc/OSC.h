// filepath: c:\codedev\auricleinc\oscmex\src\oscpp\include\osc\OSC.h
/*
 *  OSCPP - Open Sound Control C++ (OSCPP) Library.
 *  TO DO: Add Exceptions implementation from REQUIREMENTS.md and from SPECIFICATION.md
 */

#pragma once

/**
 * @file OSC.h
 * @brief Main include file for the OSC library
 *
 * This header includes all public components of the OSC library.
 * For most applications, including this file is all you need.
 */

// Core component headers
#include "Types.h"
#include "Message.h"
#include "Bundle.h"
#include "Address.h"
#include "Server.h"
#include "ServerThread.h"

// Version information
#define OSC_VERSION_MAJOR 1
#define OSC_VERSION_MINOR 0
#define OSC_VERSION_PATCH 0
#define OSC_VERSION_STRING "1.0.0"

/**
 * @namespace osc
 * @brief Namespace containing all OSC library components
 */
namespace osc
{

    /**
     * @brief Get the library version as a string
     * @return Version string in format "major.minor.patch"
     */
    inline std::string getVersionString()
    {
        return OSC_VERSION_STRING;
    }

    /**
     * @brief Get the library version as components
     * @param major Receives the major version number
     * @param minor Receives the minor version number
     * @param patch Receives the patch version number
     */
    inline void getVersion(int &major, int &minor, int &patch)
    {
        major = OSC_VERSION_MAJOR;
        minor = OSC_VERSION_MINOR;
        patch = OSC_VERSION_PATCH;
    }

} // namespace osc
