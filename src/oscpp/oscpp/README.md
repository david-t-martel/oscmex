# OSC++ Library

A modern C++ implementation of the Open Sound Control (OSC) protocol based on the OSC 1.0 and 1.1 specifications. This library provides a robust, type-safe, and cross-platform interface for OSC communication.

## Features

- Support for all OSC data types including int32, int64, float, double, string, blob, and more *(Implemented)*
- Bundle support with immediate or timestamped message delivery *(Implemented)*
- Multiple transport protocols (UDP, TCP, Unix sockets) *(Foundation Laid, TCP Framing Missing)*
- Cross-platform compatibility (Windows, macOS, Linux) *(Foundation Laid)*
- Modern C++ design with RAII, strong typing, and exception safety *(Implemented)*
- Thread-safe server implementation (`ServerThread`)

## Requirements

- C++17 compatible compiler
- CMake 3.14 or higher for building
- Platform-specific requirements:
  - **Windows**:
    - Visual Studio 2017 or newer
    - Windows SDK 10.0 or newer
    - WinSock2 libraries (automatically included)
  - **macOS**:
    - Xcode command-line tools
    - macOS 10.13 or newer
  - **Linux/Unix**:
    - GCC 7+ or Clang 6+
    - POSIX socket API
    - pthread library

For detailed requirements, please refer to [REQUIREMENTS.md](REQUIREMENTS.md).

## Building the Library

### Using CMake (Recommended)

#### Windows

```batch
# Navigate to the library directory
cd oscpp

# Generate build files
build.bat

# For Debug build
build.bat debug

# For standalone test
build.bat standalone-test

# To clean previous builds
build.bat clean

# To build with examples
build.bat examples

# To create an installation package
build.bat install
```

#### Linux/macOS

```bash
# Navigate to the library directory
cd oscpp

# Make the build script executable if needed
chmod +x build.sh

# Generate build files and build
./build.sh

# For Debug build
./build.sh debug

# For standalone test
./build.sh standalone-test

# To clean previous builds
./build.sh clean

# To build with examples
./build.sh examples

# To create an installation package
./build.sh install
```

### Manual CMake Build

```bash
# Create and navigate to a build directory
mkdir build && cd build

# Configure the project
cmake ..

# Build the library
cmake --build . --config Release

# Run tests
ctest -C Release

# Install (optional)
cmake --install . --config Release
```

### CMake Options

| Option                  | Description                         | Default |
| ----------------------- | ----------------------------------- | ------- |
| `OSCPP_BUILD_EXAMPLES`  | Build example applications          | ON      |
| `OSCPP_BUILD_TESTS`     | Build test applications             | ON      |
| `OSCPP_INSTALL`         | Generate installation target        | ON      |
| `OSC_STANDALONE_TEST`   | Build a standalone test application | OFF     |
| `OSCPP_EXTRA_C_FLAGS`   | Additional C compiler flags         | ""      |
| `OSCPP_EXTRA_CXX_FLAGS` | Additional C++ compiler flags       | ""      |

### Using CMake Presets

If you have CMake 3.20 or newer, you can use presets:

```bash
# Configure using a preset
cmake --preset windows-msvc-release

# Build using the preset
cmake --build --preset windows-msvc-release

# Test using the preset
ctest --preset windows-msvc-release
```

## Current Status & Roadmap

The library provides a comprehensive implementation of the OSC protocol with complete Bundle and Message handling, type system, and server architecture. We're now focusing on optimizing the pattern matching and completing TCP transport handling.

**Work Remaining:**

1. **Consolidate Error Handling:** Standardize exception handling throughout the codebase to improve error reporting.
2. **Implement OSC Pattern Matching:** Implement the core logic for matching incoming message address patterns against registered server methods (supporting `?`, `*`, `[]`, `{}`).
3. **Implement TCP Message Framing:** Add SLIP encoding or length-prefixing to ensure reliable message boundaries over TCP connections.
4. **Expand Test Coverage:** Create more comprehensive tests for nested bundle structures, pattern matching, and network operations.
5. **Expand Examples & Documentation:** Provide more detailed usage examples and ensure API documentation is complete.

## Usage Examples

### Basic Usage (Server)

```cpp
#include "osc/ServerThread.h"
#include "osc/Message.h"
#include "osc/Value.h"
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
#include "osc/Address.h"
#include "osc/Message.h"
#include "osc/Bundle.h"
#include "osc/TimeTag.h"
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
        msg.addInt32(42);
        msg.addFloat(3.14159f);
        msg.addString("Hello, OSC!");

        // Send the message
        if (!address.send(msg)) {
            std::cerr << "Failed to send message: " << address.getErrorMessage() << std::endl;
        } else {
            std::cout << "Message sent." << std::endl;
        }

        // Create a bundle with multiple messages
        osc::Bundle bundle(osc::TimeTag::immediate()); // Immediate execution
        bundle.addMessage(msg);

        osc::Message msg2("/test/another");
        msg2.addInt64(100LL);
        bundle.addMessage(msg2);

        // Send the bundle
        if (!address.send(bundle)) {
            std::cerr << "Failed to send bundle: " << address.getErrorMessage() << std::endl;
        } else {
            std::cout << "Bundle sent." << std::endl;
        }

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
- **Value**: Type-safe container for OSC values using `std::variant`
- **TimeTag**: OSC time tag representation

### Message Layer: OSC message and bundle handling

- **Message**: OSC message construction and parsing
- **Bundle**: Container for messages with common time tag

### Transport Layer: Network communication

- **Address**: Network address and send operations
- **Server**: Listening for and dispatching OSC messages

### High-Level API

- **ServerThread**: Convenience class for running a server in a background thread.

### Utility Layer: Helper functions and exception handling

- **OSCException**: Error handling hierarchy

For more detailed architectural information, see [ARCHITECTURE.md](ARCHITECTURE.md).

## Documentation

- [Architecture Documentation](ARCHITECTURE.md) - Detailed design of the library components
- [Requirements](REQUIREMENTS.md) - System and compilation requirements
- [OSC Specification](SPECIFICATION.md) - Details about the OSC protocol implementation
- [TODO List](TODO.md) - Upcoming features and known issues

## Testing

Unit tests are provided in the `tests` directory. To run the tests, use the following command after building the library:

```bash
# From build directory
ctest --output-on-failure

# Or using build script
build.bat test    # Windows
./build.sh test   # Linux/macOS
```

## OSC Protocol Compliance

This implementation aims to adhere to both OSC 1.0 and 1.1 specifications, supporting:

- Standard OSC address pattern syntax and matching
- All core OSC data types
- OSC bundles with time tags
- OSC 1.1 extensions (arrays, etc.)

For detailed information about the OSC protocol specifications, see [SPECIFICATION.md](SPECIFICATION.md).

## Troubleshooting

### Common Build Issues

1. **CMake Not Found**: Ensure CMake is installed and in your PATH
2. **Compilation Errors**:
   - Check your compiler supports C++17
   - On Windows, ensure Visual Studio and Windows SDK are properly installed
   - On Linux, make sure you have the pthread development libraries installed
3. **Socket Initialization Failures**:
   - On Windows, check that WinSock libraries are available
   - On Linux/macOS, check your firewall settings if socket binding fails

### Runtime Issues

1. **Network Permission Errors**: Some platforms require elevated permissions to bind to ports below 1024
2. **Firewall Blocking**: Ensure your firewall allows the application to send/receive UDP or TCP traffic

## License

[Your License Here]

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request, focusing on the items listed in the Roadmap.
