# OSC Implementation TODO List

This list tracks the remaining work, prioritizing the core features needed for a functional OSC library based on the current status as of April 27, 2025.

## Current Status & Progress

- ✅ Core architecture and class structure designed (see ARCHITECTURE.md)
- ✅ Basic header files in place for all major components
- ✅ Class declarations for Message, Server, ServerThread, Address, TimeTag, etc.
- ✅ Partial implementation of Message class (core methods and standard types)
- ✅ ServerThread implementation for running Server in background thread
- ✅ Bundle class implementation complete with serialization/deserialization and nesting
- ✅ Value system implementation with accessors and type conversions
- ✅ OSCException handling consolidated in Exceptions.h
- ❌ Pattern matching for address dispatching not fully implemented
- ❌ TCP framing for message boundaries not implemented

## Core Implementation Priorities (Ordered by Dependency)

1. **Implement OSC Address Pattern Matching (HIGH):**
    - Implement pattern matching logic in `ServerImpl` for `?`, `*`, `[]`, `{}` wildcards per OSC spec
    - Integrate pattern matching with server's message dispatch system
    - Optimize matching algorithm if needed for performance

2. **Implement TCP Message Framing (HIGH):**
    - Choose and implement framing strategy (SLIP encoding or length-prefixing)
    - Add framing to `AddressImpl::send` for TCP connections
    - Implement de-framing in `ServerImpl` for TCP reception
    - Test with various message sizes and connection scenarios

3. **Complete `osc::Value` Implementation (LOW):**
    - Verify edge cases for all type conversions
    - Ensure proper error handling for invalid type conversions
    - Optimize serialization/deserialization for performance
    - Add support for OSC Arrays (`[` and `]`) within `Value` implementation

4. **Complete `osc::Message` Implementation (LOW):**
    - Fix serialization/deserialization issues
    - Add any missing `Message::add...()` methods for OSC 1.1 types
    - Enhance type tag handling for better performance

5. **Create Comprehensive Unit Tests (MEDIUM):**
    - Expand tests for `Bundle` serialization/deserialization with complex nested structures
    - Add tests for pattern matching logic with complex patterns
    - Test networking with various protocols (UDP, TCP, UNIX sockets)
    - Add performance benchmarks for high-frequency messaging

6. **Expand Examples & Documentation (MEDIUM):**
    - Create fully-functional examples demonstrating real-world use cases
    - Ensure Doxygen comments are comprehensive for all public APIs
    - Update README.md with current implementation status and usage examples
    - Verify and update SPECIFICATION.md and ARCHITECTURE.md for accuracy

## Secondary Improvements & Features (Post-Core Implementation)

- **API Refinements:**
  - Add a generic `Message::addValue(Value)` method for type flexibility
  - Consider template-based accessors like `message.get<float>(0)`
  - Review method naming for consistency across classes

- **Performance Optimization:**
  - Profile and optimize serialization/deserialization
  - Consider buffer pooling for high-frequency messaging scenarios
  - Optimize pattern matching for common cases

- **Extended Features:**
  - Implement optional type coercion in `Server`
  - Add support for OSC query namespace extension
  - Consider WebSocket transport support

- **Quality Assurance:**
  - Add static analysis to build process
  - Implement stress testing and benchmarks
  - Ensure cross-platform testing (Windows, Linux, macOS)

## Long-term Goals

- WebSocket and MQTT transport support
- Visual debugging and message inspection tools
- Integration with audio frameworks (JUCE, RtAudio, etc.)
- Higher-level abstraction for common OSC-based applications
