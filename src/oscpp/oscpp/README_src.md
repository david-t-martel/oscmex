# OSC Framework (C++ Implementation)

A modern C++ implementation of the Open Sound Control (OSC) protocol based on the OSC 1.0 and 1.1 specifications. This library provides a robust, type-safe, and cross-platform interface for OSC communication.

## Overview

This project aims to provide a complete and efficient implementation of the OSC protocol in modern C++. Key features include:

- Support for all OSC data types including int32, int64, float, double, string, blob, and more *(Partially Implemented)*
- Bundle support with immediate or timestamped message delivery *(Partially Implemented)*
- Multiple transport protocols (UDP, TCP, Unix sockets) *(Foundation Laid, TCP Framing Missing)*
- Cross-platform compatibility (Windows, macOS, Linux) *(Foundation Laid)*
- Modern C++ design with RAII, strong typing, and exception safety *(Partially Implemented)*
- Thread-safe server implementation (`ServerThread`)

## Requirements

- C++17 compatible compiler
- Platform-specific networking libraries (automatically included)
  - Windows: WinSock2
  - Unix/Linux/macOS: POSIX socket API

For detailed requirements, please refer to [REQUIREMENTS.md](REQUIREMENTS.md).

## Current Status & Roadmap

The library currently provides the core class structure and networking foundation for OSC communication. Key components like `Address`, `Server`, and `ServerThread` are defined, along with basic type representations (`TimeTag`, `Blob`).

**Work Remaining:**

1.  **Complete Data Type Implementation (`osc::Value`):** Implement serialization, deserialization, and accessors for *all* OSC 1.0/1.1 data types (including int64, double, symbol, char, color, MIDI, bool, nil, infinitum, arrays).
2.  **Complete Message & Bundle Handling:** Implement full serialization and deserialization logic for `osc::Message` and `osc::Bundle`, including all argument types and nested structures. Add missing `add...` methods to `Message`.
3.  **Implement OSC Pattern Matching:** Implement the core logic for matching incoming message address patterns against registered server methods (supporting `?`, `*`, `[]`, `{}`).
4.  **Implement TCP Message Framing:** Add SLIP encoding or length-prefixing to ensure reliable message boundaries over TCP connections.
5.  **Consolidate & Implement Error Handling:** Standardize on `osc::OSCException` and throw exceptions appropriately for network, parsing, and type errors.
6.  **Add Unit Tests:** Create comprehensive tests for all components.
7.  **Expand Examples & Documentation:** Provide more detailed usage examples and verify existing ones.

## Getting Started (Illustrative - API may evolve)

### Basic Usage (Server)

```cpp
// filepath: examples/server_example.cpp
#include "osc/ServerThread.h"
#include "osc/Message.h" // Include necessary headers
#include "osc/Value.h"   // Include necessary headers
#include <iostream>
#include <chrono>
#include <thread>

int main() {
    try {
        // Create a server thread listening on UDP port 8000
        osc::ServerThread server("8000", osc::Protocol::UDP);

        // Add a message handler for "/test/path" expecting int, float, string
        server.addMethod("/test/path", "ifs", [](const osc::Message& message) {
            try {
                // Access arguments (assuming Value accessors are implemented)
                int32_t intVal = message.getArguments()[0].asInt32();
                float floatVal = message.getArguments()[1].asFloat();
                std::string strVal = message.getArguments()[2].asString();

                std::cout << "Received /test/path: "
                          << intVal << ", " << floatVal << ", \"" << strVal << "\"" << std::endl;
            } catch (const osc::OSCException& e) {
                 std::cerr << "Error processing message args: " << e.what() << std::endl;
            } catch (const std::exception& e) {
                 std::cerr << "Standard exception in handler: " << e.what() << std::endl;
            }
        });

        // Add a default handler for other messages
        server.addDefaultMethod([](const osc::Message& message){
            std::cout << "Received unhandled message: " << message.getPath() << std::endl;
        });

        // Set an error handler
        server.setErrorHandler([](int code, const std::string& msg, const std::string& where){
            std::cerr << "Server Error " << code << " in " << where << ": " << msg << std::endl;
        });

        // Start the server thread
        if (server.start()) {
            std::cout << "Server started on UDP port " << server.port() << std::endl;
            // Keep the main thread alive (e.g., wait for user input)
            std::cout << "Press Enter to exit." << std::endl;
            std::cin.get();
            server.stop();
            std::cout << "Server stopped." << std::endl;
        } else {
            std::cerr << "Failed to start server." << std::endl;
            return 1;
        }

    } catch (const osc::OSCException& e) {
        std::cerr << "OSC Initialization Error: " << e.what() << " (Code: " << static_cast<int>(e.code()) << ")" << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
```

### Basic Usage (Client)

```cpp
// filepath: examples/client_example.cpp
#include "osc/Address.h"
#include "osc/Message.h"
#include "osc/Bundle.h"
#include "osc/TimeTag.h" // Include necessary headers
#include <iostream>
#include <vector>
#include <cstddef> // For std::byte

int main() {
    try {
        // Target address: UDP to localhost port 8000
        osc::Address address("127.0.0.1", "8000", osc::Protocol::UDP);
        if (!address.isValid()) {
             std::cerr << "Failed to initialize address: " << address.getErrorMessage() << std::endl;
             return 1;
        }

        // Create an OSC message (assuming Message methods are implemented)
        osc::Message msg("/test/path");
        // msg.addInt32(42); // Assuming these methods exist and work
        // msg.addFloat(3.14159f);
        // msg.addString("Hello, OSC!");

        // Send the message
        // if (!address.send(msg)) {
        //     std::cerr << "Failed to send message: " << address.getErrorMessage() << std::endl;
        // } else {
        //     std::cout << "Message sent." << std::endl;
        // }

        // Create a bundle with multiple messages (illustrative)
        // osc::Bundle bundle(osc::TimeTag::immediate()); // Immediate execution
        // bundle.addMessage(msg);

        // osc::Message msg2("/test/another");
        // msg2.addInt64(100LL); // Assuming Int64 support
        // bundle.addMessage(msg2);

        // Send the bundle
        // if (!address.send(bundle)) {
        //     std::cerr << "Failed to send bundle: " << address.getErrorMessage() << std::endl;
        // } else {
        //     std::cout << "Bundle sent." << std::endl;
        // }

        std::cout << "Client example finished (functionality depends on implementation progress)." << std::endl;

    } catch (const osc::OSCException& e) {
        std::cerr << "OSC Error: " << e.what() << " (Code: " << static_cast<int>(e.code()) << ")" << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
```

## Architecture

The library is designed with a layered architecture:

### Core Layer: Basic data types and serialization

- **Types.h**: Fundamental OSC data types (`TimeTag`, `Blob`, `Value`, `Protocol`, `OSCException`)
- **Value**: Type-safe container for OSC values using `std::variant` *(Implementation Pending)*
- **TimeTag**: OSC time tag representation

### Message Layer: OSC message and bundle handling

- **Message**: OSC message construction and parsing *(Serialization/Deserialization Pending)*
- **Bundle**: Container for messages with common time tag *(Serialization/Deserialization Pending)*

### Transport Layer: Network communication

- **Address**: Network address and send operations *(TCP Framing Pending)*
- **Server**: Listening for and dispatching OSC messages *(Pattern Matching Pending, TCP Framing Pending)*

### High-Level API

- **ServerThread**: Convenience class for running a server in a background thread.

### Utility Layer: Helper functions and exception handling

- **OSCException**: Error handling hierarchy *(Integration Pending)*

For more detailed architectural information, see [ARCHITECTURE.md](ARCHITECTURE.md).

## OSC Protocol Compliance

This implementation aims to adhere to both OSC 1.0 and 1.1 specifications, supporting:

- Standard OSC address pattern syntax and matching *(Implementation Pending)*
- All core OSC data types *(Implementation Pending)*
- OSC bundles with time tags *(Implementation Pending)*
- OSC 1.1 extensions (arrays, etc.) *(Implementation Pending)*

For detailed information about the OSC protocol specifications, see [SPECIFICATION.md](SPECIFICATION.md).

## Implementation Details

### Type System

The library uses `std::variant` in the `Value` class to encapsulate all possible OSC data types. *(Implementation Pending)*

### Network Layer

The network communication is abstracted through the `Address` and `Server` classes, using PIMPL to hide platform-specific socket APIs. *(TCP Framing Pending)*

### Thread Safety

`ServerThread` provides a thread-safe way to run the server. `Message` and `Bundle` objects are intended to be immutable or used in a thread-safe manner once created.

## License

[Your License Here]

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request, focusing on the items listed in the Roadmap.
