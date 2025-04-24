# OSCMix Architecture

This document describes the architectural design and code-level implementation of the OSCMix application, detailing how the various components interact to provide a bridge between OSC (Open Sound Control) messages and RME audio interfaces.

## Overview

OSCMix is designed to translate network OSC messages into MIDI SysEx commands for controlling RME audio devices, and to report device state changes back as OSC messages. The application is structured to work across multiple platforms (Windows, macOS, and Linux) while maintaining consistent behavior.

## Main Components

### 1. Main Entry Point (main.c)

The entry point (unified main.c) provides platform-specific implementations for:

- Command line argument parsing
- Network socket initialization
- MIDI device initialization and I/O
- Threading and event handling
- Signal handling for cleanup

The main.c file creates two primary threads:

- `midiread`: Reads MIDI SysEx responses from the RME device
- `oscread`: Reads OSC messages from the network

### 2. Core Logic (oscmix.c)

The core logic in oscmix.c handles:

- Device parameter mapping
- OSC message parsing and dispatching
- MIDI SysEx message generation
- State tracking and synchronization
- Error handling and reporting

The processing flow is:

1. Initialize device parameters and state
2. Wait for OSC messages
3. Parse OSC messages to determine parameter to adjust
4. Convert parameter changes to MIDI SysEx format
5. Send SysEx commands to the device
6. Read responses and update internal state
7. Broadcast state changes via OSC

### 3. MIDI SysEx Handling (sysex.c)

The SysEx module:

- Formats parameter changes as MIDI SysEx commands
- Parses SysEx responses from the device
- Maps between parameter IDs and register addresses
- Handles special SysEx message types

### 4. OSC Protocol (osc.c)

The OSC module:

- Parses OSC messages from the network
- Formats OSC messages to send over the network
- Handles OSC type tags and message arguments
- Manages OSC address patterns and matching

### 5. Socket Communications (socket.c)

The socket module provides:

- Cross-platform socket initialization
- UDP message sending and receiving
- Multicast support
- Socket error handling

### 6. Device Handling (device.c)

The device module:

- Defines device capabilities and parameter ranges
- Maps between user-friendly parameter names and device registers
- Manages device-specific quirks and behaviors
- Initializes device state on connection

### 7. Utilities (util.c, intpack.c)

Utility modules provide:

- Error reporting functions
- Memory management helpers
- Integer packing/unpacking for network and MIDI messages
- Debugging aids

## Data Flow

```
[Network] <--OSC--> [Socket Module] <---> [OSC Module]
                                            ^
                                            |
[RME Device] <--MIDI SysEx--> [MIDI I/O] <---> [Core Logic] <---> [Device Module]
                                            ^
                                            |
                                     [SysEx Module]
```

## Function Call Flow

### Initialization

1. `main()` → Parses arguments and sets up environment
2. `init_networking()` → Platform-specific network initialization
3. `init_midi()` → Platform-specific MIDI device initialization
4. `init()` → Initialize internal state and parameters
   - `device_init()` → Configure device capabilities
   - `inputs_init()`, `outputs_init()`, etc. → Initialize channel state
   - `refresh()` → Request initial state from device

### Message Processing Loop

1. `oscread()` → Thread that processes incoming OSC messages
   - `handleosc()` → Process the OSC message
     - `oscmsg_parse()` → Parse the OSC address and arguments
     - `dispatch_set()` or `dispatch_get()` → Handle parameter changes
     - `setreg()`, `setint()`, etc. → Set device parameters
     - `writesysex()` → Send MIDI SysEx to device
     - `oscsend()` → Send OSC response/notification

2. `midiread()` → Thread that processes incoming MIDI messages
   - `handlesysex()` → Process SysEx response from device
     - Update internal state based on response
     - `oscsend()` → Notify of parameter changes via OSC

3. `handletimer()` → Periodic processing for level meters and other time-based tasks

## Platform-Specific Implementations

### Windows-specific

- Uses Windows Multimedia API (winmm.lib) for MIDI I/O
- Uses WinSock2 for network connectivity
- Uses Windows threads via _beginthreadex

### macOS/Linux-specific

- Uses file descriptors for MIDI I/O
- Uses POSIX sockets for network connectivity
- Uses pthread for threading
- Uses signals and timers for periodic processing

## Error Handling

- Network errors are reported but non-fatal (to allow reconnection)
- MIDI device errors are typically fatal
- Parameter validation happens before commands are sent
- Logged errors include context for debugging

## Key Data Structures

1. **Device Parameters**: Maps between parameter names, register addresses, and value ranges

   ```c
   typedef struct {
       const char *name;
       uint16_t reg;
       enum param_type type;
       union {
           struct { int min, max; } intrange;
           struct { fixed min, max; } fixedrange;
           struct { const char **values; size_t n; } enumrange;
       };
   } param;
   ```

2. **OSC Message**: Represents an OSC message with address and arguments

   ```c
   typedef struct {
       char *address;
       char *tags;
       char **argv;
       size_t argc;
   } oscmsg;
   ```

3. **Device State**: Tracks the current state of all device parameters

   ```c
   typedef struct {
       uint16_t addr;
       int32_t val;
   } regval;
   ```

## Code Organization

- **Platform Independence**: Core logic is platform-agnostic, with platform-specific code isolated in main.c
- **Modularity**: Clear separation between network, MIDI, and core logic
- **Extensibility**: Device parameters can be extended for new device models
- **Reusability**: OSC and SysEx modules can be reused in other projects

## Design Patterns

1. **Observer Pattern**: OSC clients observe device state changes
2. **Command Pattern**: OSC messages encapsulate commands to execute
3. **Adapter Pattern**: Translates between OSC and MIDI SysEx protocols
4. **Factory Pattern**: Creates appropriate handlers based on parameter types
