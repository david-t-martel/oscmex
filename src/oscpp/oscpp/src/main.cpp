/*
 *  OSCPP - Open Sound Control C++ (OSCPP) Library.
 *
 * This file implements the core entry points and utility functions for the OSCPP library.
 * It provides initialization and cleanup procedures that handle platform-specific requirements
 * and offers utility functions that simplify common OSC operations.
 */

#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include "osc/AddressImpl.h"
#include "osc/Bundle.h"
#include "osc/Exceptions.h"
#include "osc/Message.h"
#include "osc/OSC.h"
#include "osc/Server.h"  // For networking initialization
#include "osc/Types.h"

namespace osc {

    /**
     * @brief Initialize the OSC library
     *
     * This function initializes any required platform-specific resources
     * (like Windows sockets) needed by the OSC library. Call this before
     * any other OSC function.
     *
     * @return true if initialization succeeded, false otherwise
     */
    bool initialize() {
        try {
            // Initialize networking (platform-specific)
            if (!osc::AddressImpl::initializeNetworking()) {
                std::cerr << "Failed to initialize OSC networking" << std::endl;
                return false;
            }
            return true;
        } catch (const osc::OSCException& ex) {
            std::cerr << "OSC initialization error: " << ex.what()
                      << " (code: " << static_cast<int>(ex.code()) << ")" << std::endl;
            return false;
        } catch (const std::exception& ex) {
            std::cerr << "Error during OSC initialization: " << ex.what() << std::endl;
            return false;
        } catch (...) {
            std::cerr << "Unknown error during OSC initialization" << std::endl;
            return false;
        }
    }

    /**
     * @brief Clean up the OSC library
     *
     * This function cleans up any resources allocated by the OSC library.
     * Call this when you're done using OSC, typically at application shutdown.
     */
    void cleanup() {
        try {
            // Clean up networking (platform-specific)
            osc::AddressImpl::cleanupNetworking();
        } catch (...) {
            // Silently ignore cleanup errors
        }
    }

    /**
     * @brief Create a new OSC message
     *
     * Convenience function to create a new OSC message with the given address pattern.
     *
     * @param path OSC address pattern
     * @return A new OSC message
     */
    Message createMessage(const std::string& path) { return Message(path); }

    /**
     * @brief Create a new OSC bundle
     *
     * Convenience function to create a new OSC bundle with the given time tag.
     *
     * @param timeTag OSC time tag for the bundle
     * @return A new OSC bundle
     */
    Bundle createBundle(const TimeTag& timeTag = TimeTag::immediate()) { return Bundle(timeTag); }

    /**
     * @brief Create an OSC server
     *
     * Convenience function to create a new OSC server listening on the specified port.
     *
     * @param port Port number or service name to listen on
     * @param protocol Transport protocol (UDP, TCP, or UNIX)
     * @return A new OSC server
     */
    Server createServer(const std::string& port, Protocol protocol = Protocol::UDP) {
        return Server(port, protocol);
    }

    /**
     * @brief Create an OSC address for sending messages
     *
     * Convenience function to create a new OSC address for the specified host and port.
     *
     * @param host Hostname or IP address
     * @param port Port number or service name
     * @param protocol Transport protocol (UDP, TCP, or UNIX)
     * @return A new OSC address
     */
    Address createAddress(const std::string& host, const std::string& port,
                          Protocol protocol = Protocol::UDP) {
        return Address(host, port, protocol);
    }

    /**
     * @brief Get the version of the OSC library
     *
     * @param major Reference to receive the major version number
     * @param minor Reference to receive the minor version number
     * @param patch Reference to receive the patch version number
     */
    void getLibraryVersion(int& major, int& minor, int& patch) { getVersion(major, minor, patch); }

    /**
     * @brief Get the version string of the OSC library
     *
     * @return Version string in format "major.minor.patch"
     */
    std::string getLibraryVersionString() { return getVersionString(); }

    // Automatic initialization and cleanup using static objects
    namespace {
        class OSCLibraryInitializer {
           public:
            OSCLibraryInitializer() { initialized = initialize(); }

            ~OSCLibraryInitializer() {
                if (initialized) {
                    cleanup();
                }
            }

            bool isInitialized() const { return initialized; }

           private:
            bool initialized;
        };

        // Static instance to handle automatic initialization/cleanup
        static OSCLibraryInitializer libraryInitializer;
    }  // namespace

    /**
     * @brief Check if the OSC library is initialized
     *
     * @return true if the library is initialized, false otherwise
     */
    bool isInitialized() { return libraryInitializer.isInitialized(); }

}  // namespace osc

// Optional entry point that can be used for testing
// This is not necessary for library functionality but helps with testing
#ifdef OSC_STANDALONE_TEST
int main() {
    std::cout << "OSCPP Library version " << osc::getLibraryVersionString() << std::endl;
    if (osc::isInitialized()) {
        std::cout << "OSC Library successfully initialized." << std::endl;
    } else {
        std::cerr << "OSC Library initialization failed." << std::endl;
        return 1;
    }

    // Basic self-test
    try {
        osc::Message msg("/test");
        msg.addInt32(42);
        msg.addFloat(3.14f);
        msg.addString("Hello OSC");

        auto data = msg.serialize();
        std::cout << "Serialized message with " << data.size() << " bytes." << std::endl;

        // Deserialize the message
        auto receivedMsg = osc::Message::deserialize(data.data(), data.size());
        std::cout << "Successfully deserialized message to: " << receivedMsg.getPath() << std::endl;

    } catch (const osc::OSCException& ex) {
        std::cerr << "OSC Error: " << ex.what() << " (Code: " << static_cast<int>(ex.code()) << ")"
                  << std::endl;
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
#endif  // OSC_STANDALONE_TEST
