// filepath: c:\codedev\auricleinc\oscmex\src\oscpp\include\osc\OSC.h
/*
 *  OSCPP - Open Sound Control C++ (OSCPP) Library.
 *
 *  A modern C++ implementation of the Open Sound Control (OSC) protocol,
 *  supporting OSC 1.0 and 1.1 specifications, with UDP, TCP, and UNIX socket
 *  transports.
 */

#pragma once

/**
 * @file OSC.h
 * @brief Main include file for the OSC library
 *
 * This header includes all public components of the OSC library.
 * For most applications, including this file is all you need.
 *
 * Example usage:
 *
 * ```cpp
 * // Create an OSC message
 * osc::Message message("/test/address");
 * message.addInt32(42);
 * message.addFloat(3.14f);
 * message.addString("Hello OSC");
 *
 * // Send the message via UDP
 * osc::Address address("localhost", "8000", osc::Protocol::UDP);
 * address.send(message);
 *
 * // Create a server to receive OSC messages
 * osc::Server server("8000", osc::Protocol::UDP);
 * server.addMethod("/test/address", "", [](const osc::Message& msg) {
 *     std::cout << "Received message with " << msg.getArguments().size() << " arguments\n";
 * });
 *
 * // Start a thread to handle incoming messages
 * osc::ServerThread serverThread(server);
 * serverThread.start();
 * ```
 */

// Core component headers
#include "osc/Address.h"
#include "osc/Bundle.h"      // Updated to include the correct path
#include "osc/Exceptions.h"  // Added Exception header
#include "osc/Message.h"
#include "osc/Server.h"
#include "osc/ServerThread.h"
#include "osc/TimeTag.h"
#include "osc/Types.h"

// Version information
#define OSC_VERSION_MAJOR 1
#define OSC_VERSION_MINOR 0
#define OSC_VERSION_PATCH 0
#define OSC_VERSION_STRING "1.0.0"

/**
 * @namespace osc
 * @brief Namespace containing all OSC library components
 */
namespace osc {

    /**
     * @brief Get the library version as a string
     * @return Version string in format "major.minor.patch"
     */
    inline std::string getVersionString() { return OSC_VERSION_STRING; }

    /**
     * @brief Get the library version as components
     * @param major Receives the major version number
     * @param minor Receives the minor version number
     * @param patch Receives the patch version number
     */
    inline void getVersion(int &major, int &minor, int &patch) {
        major = OSC_VERSION_MAJOR;
        minor = OSC_VERSION_MINOR;
        patch = OSC_VERSION_PATCH;
    }

}  // namespace osc
