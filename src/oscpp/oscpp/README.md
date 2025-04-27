# OSC Library

The OSC (Open Sound Control) library is a C++ implementation designed for handling OSC messages and communication. This library provides a robust framework for sending and receiving OSC messages over various transport protocols, including UDP and TCP.

## Features

- **Address Management**: The library allows for easy management of OSC addresses, enabling the sending of messages to specific endpoints.
- **Message Handling**: Create and manage OSC messages with various argument types.
- **Bundle Support**: Group multiple OSC messages into a single bundle for efficient transmission.
- **Server Functionality**: Implement OSC servers that can receive and process incoming messages.
- **Threading Support**: Run OSC servers in separate threads for non-blocking operations.

## Installation

To build the OSC library, you will need CMake installed on your system. Follow these steps:

1. Clone the repository:

   ```
   git clone <repository-url>
   cd oscpp
   ```

2. Create a build directory:

   ```
   mkdir build
   cd build
   ```

3. Configure the project using CMake:

   ```
   cmake ..
   ```

4. Build the library:

   ```
   cmake --build .
   ```

## Usage

### Sending Messages

To send OSC messages, include the necessary headers and create an `Address` object. Use the `Message` class to construct your messages and send them through the address.

### Receiving Messages

Set up an `OSC Server` to listen for incoming messages. Implement handlers for specific message paths to process the received data.

### Example

Refer to the `examples` directory for sample client and server implementations demonstrating how to use the library.

## Testing

Unit tests are provided in the `tests` directory. To run the tests, use the following command after building the library:

```
ctest
```

## License

This library is licensed under the MIT License. See the `LICENSE` file for more details.

## Contributing

Contributions are welcome! Please submit a pull request or open an issue for any enhancements or bug fixes.

## Documentation

For more detailed documentation, refer to the `docs` folder (if available) or the comments within the source code files.
