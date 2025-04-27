/*
* OSCPP - Open Sound Control C++ (OSCPP) Library.
 */

# Modern C++ OSC Library Architecture

This document outlines the architecture of our modern C++ implementation of the Open Sound Control (OSC) protocol. The library is designed with SOLID principles, modern C++ features, and cross-platform compatibility in mind.

## Core Architecture

```mermaid
graph TD
    subgraph "OSC Core Modules"
        Types[Types]
        Value[Value (Variant)]
        Message[Message]
        Bundle[Bundle]
        Address[Address]
        Server[Server]
    end

    subgraph "Transport Layer (Abstracted by Address/Server)"
        AddressImpl[AddressImpl (PIMPL)]
        ServerImpl[ServerImpl (PIMPL)]
        UDP[UDP Support]
        TCP[TCP Support (Framing Pending)]
        UNIX[UNIX Socket Support]
        Address --> AddressImpl
        Server --> ServerImpl
        AddressImpl --> UDP
        AddressImpl --> TCP
        AddressImpl --> UNIX
        ServerImpl --> UDP
        ServerImpl --> TCP
        ServerImpl --> UNIX
    end

    subgraph "High-Level API & Dispatch"
        ServerThread[ServerThread]
        Method[Method Handling]
        Pattern[Pattern Matching (Pending)]
        ServerThread --> Server
        Server --> Method
        Method -- Needs --> Pattern
    end

    subgraph "Utilities & Base Types"
        Error[Error Handling (OSCException)]
        TimeTag[TimeTag]
        Blob[Blob]
    end

    Value --> Types
    Message --> Types
    Message --> Value
    Bundle --> Message
    Bundle --> TimeTag
    Server --> Error
    Address --> Error
    Types --> Blob
    Types --> TimeTag
    Types --> Error
```

## Module Dependencies and Responsibilities

### Core Data Types (`Types.h`)

* Defines fundamental data types: `TimeTag`, `Blob`, `Protocol`, `MethodId`.
* Defines the core `osc::Value` class structure (using `std::variant`). *(Implementation Pending)*
* Defines the standard `osc::OSCException` class for error handling. *(Integration Pending)*

### Value System (`Value`)

* Uses `std::variant` to represent all OSC 1.0/1.1 argument types. *(Implementation Pending)*
* Provides type-safe accessors (`asInt32`, `isFloat`, etc.). *(Implementation Pending)*
* Handles serialization/deserialization for each type, including alignment/padding. *(Implementation Pending)*

### Message Class (`Message`)

* Represents an OSC message (address pattern, type tag string, arguments).
* Provides methods to add arguments (`addInt32`, etc.). *(Needs methods for all types)*
* Provides access to arguments (`getArguments`).
* Handles serialization to/deserialization from binary format. *(Implementation Pending)*

### Bundle Class (`Bundle`)

* Represents an OSC bundle (`#bundle`, time tag, elements).
* Contains `osc::Message` or nested `osc::Bundle` elements.
* Handles serialization/deserialization of the bundle structure. *(Implementation Pending)*

### Address Class (`Address`)

* Manages network destination information (host, port, protocol).
* Provides interface for sending serialized `Message` or `Bundle` data.
* Uses PIMPL (`AddressImpl`) to hide platform-specific socket details (Winsock/POSIX).
* Handles address resolution and URL parsing.
* Abstracts UDP, TCP, UNIX socket sending logic. *(TCP Framing Pending)*

### Server Class (`Server`)

* Listens for incoming OSC packets on a specified port/protocol.
* Uses PIMPL (`ServerImpl`) to hide platform-specific socket details.
* Manages method registration (`addMethod`, `addDefaultMethod`).
* Dispatches incoming messages to registered handlers based on address patterns. *(Pattern Matching Logic Pending)*
* Handles bundle processing (start/end callbacks).
* Abstracts UDP, TCP, UNIX socket receiving logic. *(TCP Framing Pending)*

### Method Handling (`Server`/`ServerImpl`)

* Stores registered callbacks (`MethodHandler`) associated with address patterns and type specs.
* Requires the Pattern Matching system to select the correct handler.

### High-Level Server (`ServerThread`)

* Wraps a `Server` instance to run it in a dedicated background thread.
* Provides thread management (`start`, `stop`) and convenience methods mirroring `Server`.

### Pattern Matching System (`Pattern` - within `ServerImpl`)

* Implements OSC pattern matching rules (`?`, `*`, `[]`, `{}`). *(Implementation Pending)*
* Used by the `Server` to dispatch messages to the correct `MethodHandler`.

### Error Handling Strategy (`OSCException`)

* Standardize on `osc::OSCException` defined in `Types.h`. *(Integration Pending)*
* Use exceptions for critical errors (e.g., setup, parsing failures).
* Provide error callbacks (`Server::ErrorHandler`) for non-fatal runtime issues.

## Implementation Details

### Modern C++ Features

* Uses C++17 features (`std::variant`, `std::optional`, `std::string_view`, `std::byte`).
* Employs RAII for resource management (sockets, threads).
* Uses smart pointers (`std::unique_ptr`) for PIMPL and resource ownership.

### Cross-Platform Considerations

* Abstracts platform-specific socket APIs via PIMPL (`AddressImpl`, `ServerImpl`).
* Handles endianness conversion during serialization/deserialization. *(Implementation Pending)*

### Design Patterns

* **PIMPL** (Pointer to Implementation): Used in `Address` and `Server` for ABI stability and platform abstraction.
* **Strategy** (Implicit): Different protocols handled within `AddressImpl`/`ServerImpl`.
* **Callback/Observer**: Used for message handling (`MethodHandler`) and error reporting (`ErrorHandler`).

### Thread Safety Considerations

* `ServerThread` provides a thread-safe server interface using mutexes.
* `Server` itself is not inherently thread-safe for modification (e.g., adding methods) while running; modifications should ideally happen before starting or be synchronized externally if modified while polled manually. `ServerThread` handles this synchronization internally.
* `Message` and `Bundle` objects are generally designed to be value types or used immutably after creation/deserialization.
