# OSC Framework (C++ Implementation)

A modern C++ implementation of the Open Sound Control (OSC) protocol based on the OSC 1.0 and 1.1 specifications. This library provides a robust, type-safe, and cross-platform interface for OSC communication.

## Overview

This project aims to provide a complete and efficient implementation of the OSC protocol in modern C++. Key features include:

- Support for all OSC data types including int32, int64, float, double, string, blob, and more
- Bundle support with immediate or timestamped message delivery
- Multiple transport protocols (UDP, TCP, Unix sockets)
- Cross-platform compatibility (Windows, macOS, Linux)
- Modern C++ design with RAII, strong typing, and exception safety
- Thread-safe operations where appropriate

## Requirements

- C++17 compatible compiler
- Platform-specific networking libraries (automatically included)
  - Windows: WinSock2
  - Unix/Linux/macOS: POSIX socket API

For detailed requirements, please refer to [REQUIREMENTS.md](REQUIREMENTS.md).

## Getting Started

### Basic Usage

```cpp
#include "osc/Server.h"

// Create a server listening on port 8000
osc::Server server("8000", osc::Protocol::UDP);

// Add a message handler
server.addHandler("/test/path", [](const osc::Message& message) {
    int32_t value = message.getArgument(0).asInt32();
    float floatVal = message.getArgument(1).asFloat();
    std::string str = message.getArgument(2).asString();

    std::cout << "Received: " << value << ", " << floatVal << ", " << str << std::endl;
});

// Start the server
server.start();

// In your main loop or event processing
server.processMessages();
```

```cpp
#include "osc/Address.h"
#include "osc/Message.h"

// Create an OSC message
osc::Message msg("/test/path");
msg.addInt32(42)
   .addFloat(3.14159f)
   .addString("Hello, OSC!");

// Send the message to a destination
osc::Address address("127.0.0.1", "8000", osc::Protocol::UDP);
address.send(msg.serialize());

// Create a bundle with multiple messages
osc::Bundle bundle;
bundle.addMessage(msg);

// Add another message with a different path
osc::Message msg2("/test/another");
msg2.addInt32(100);
bundle.addMessage(msg2);

// Send the bundle
address.send(bundle.serialize());
```

## Receiving OSC Messages

## Architecture

The library is designed with a layered architecture:

### Core Layer: Basic data types and serialization

- **Types.h**: Fundamental OSC data types
- **Value**: Type-safe container for OSC values
- **TimeTag**: OSC time tag representation

### Message Layer: OSC message and bundle handling

- **Message**: OSC message construction and parsing
- **Bundle**: Container for messages with common time tag

### Transport Layer: Network communication

- **Address**: Network address and send operations
- **Server**: Listening for and dispatching OSC messages

### Utility Layer: Helper functions and exception handling

- **Exceptions**: Error handling hierarchy

For more detailed architectural information, see [ARCHITECTURE.md](ARCHITECTURE.md).

## OSC Protocol Compliance

This implementation adheres to both OSC 1.0 and 1.1 specifications, supporting:

- Standard OSC address pattern syntax and matching
- All core OSC data types
- OSC bundles with time tags
- OSC 1.1 extensions (arrays, etc.)

For detailed information about the OSC protocol specifications, see [SPECIFICATION.md](SPECIFICATION.md).

## Implementation Details

### Type System

The library uses a modern C++ type system with std::variant and std::visit to provide type safety while maintaining flexibility. The Value class encapsulates all possible OSC data types.

### Network Layer

The network communication is abstracted through the Address class, which handles the details of different transport protocols (UDP, TCP, UNIX sockets) and platform-specific socket APIs.

### Thread Safety

Thread safety is maintained where appropriate, allowing for concurrent message handling in multi-threaded applications.

## Development Roadmap

For a list of upcoming features, improvements, and known issues, please refer to [TODO.md](TODO.md).

## License

[Your License Here]

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.
