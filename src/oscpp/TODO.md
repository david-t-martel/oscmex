# OSC Implementation TODO List

## Immediate Priorities

### Complete Address Pattern Matching Implementation

- Implement full OSC address pattern matching with wildcards
- Add pattern tree optimization for faster dispatch
- Support OSC address pattern queries

### Server Implementation

- Complete the Server class for receiving OSC messages
- Add support for method dispatching based on OSC paths
- Implement thread-safe message queue

### Testing Framework

- Add comprehensive unit tests
- Implement integration tests with other OSC libraries
- Set up CI/CD pipeline

## Improvements

### API Consistency

- Standardize method naming across all classes
- Ensure Message class has both general add() method and specific methods like addInt32()
- Review and refine error handling strategy

### Performance Optimization

- Add message/buffer pooling for high-frequency applications
- Optimize serialization/deserialization
- Investigate zero-copy options for large blobs

### Documentation

- Add comprehensive API documentation
- Create usage examples
- Add benchmarks

### Features

- Add TCP server implementation with connection management
- Implement automatic type conversion for convenience methods
- Add support for OSC query namespace extension
- Create high-level wrappers for common OSC workflows

## Bug Fixes

### Message Class Issues

- Fix inconsistency between arguments() and getArguments() methods
- Implement missing add() method in Message
- Ensure Message::deserialize() is properly implemented

### Bundle Handling

- Ensure proper timestamping for nested bundles
- Fix potential memory issues with deep bundle hierarchies

### Cross-Platform Compatibility

- Address endianness issues on various platforms
- Fix socket handling differences between platforms
- Ensure proper Unicode support for string types

## Code Quality

### Refactoring

- Review PIMPL implementation for consistency
- Consolidate redundant serialization code
- Improve error messages for better debugging

### Standard Compliance

- Ensure full compliance with C++17 best practices
- Validate against OSC 1.1 specification
- Add static analysis to CI pipeline

## Long-term Goals

### Extended Protocol Support

- Add support for OSC over WebSockets
- Implement OSC over MQTT for IoT applications
- Consider OSC discovery protocol

### Tooling

- Create command-line tools for OSC debugging
- Develop visualization tools for OSC message traffic
- Add OSC bridge capabilities to other protocols

### Performance Testing

- Benchmark against other OSC implementations
- Optimize for real-time applications
- Profile memory usage for embedded applications
