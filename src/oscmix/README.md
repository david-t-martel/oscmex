# OSCMix: Open Sound Control Interface for RME Audio Devices

## Overview

`oscmix` is a command-line application that acts as a bridge between OSC (Open Sound Control) messages and RME audio interfaces. It enables remote control of RME mixer functionality via network communications, translating OSC messages into MIDI SysEx commands that RME devices understand.

The application allows for:
- Controlling mixer settings (volume, mute, pan, etc.)
- Adjusting device parameters (sample rate, clock source)
- Applying effects (EQ, dynamics, reverb, etc.)
- Receiving meter/level information from the device
- Controlling recording functions on supported devices

## Architecture

The application follows a message-based architecture:

1. **Message Reception**: Receives OSC messages over UDP
2. **Command Parsing**: Matches OSC addresses to internal parameter tree
3. **Command Execution**: Translates commands to MIDI SysEx messages and sends to device
4. **State Tracking**: Maintains internal representation of device state
5. **Response Handling**: Receives MIDI SysEx responses from device
6. **OSC Broadcasting**: Forwards device state changes as OSC messages

```
+--------+    OSC    +--------+    MIDI SysEx    +-------------+
| Client | <-------> | oscmix | <------------->  | RME Device  |
+--------+           +--------+                  +-------------+
```

## Dependencies

- **POSIX-compatible OS** or **Windows**
- **Socket libraries** for network communication
- **MIDI I/O capabilities** for communicating with RME device

## Building

### Linux/macOS

```bash
# Create build directory
mkdir -p build
cd build

# Compile
gcc -o oscmix ../src/oscmix/main_unix.c ../src/oscmix/oscmix.c ../src/oscmix/osc.c ../src/oscmix/socket.c ../src/oscmix/sysex.c ../src/oscmix/intpack.c ../src/oscmix/device.c ../src/oscmix/util.c -lm -lpthread

# Install (optional)
sudo cp oscmix /usr/local/bin/
```

### Windows

```batch
REM Create build directory
mkdir build
cd build

REM Compile
cl.exe /Fe:oscmix.exe ..\src\oscmix\main_old.c ..\src\oscmix\oscmix.c ..\src\oscmix\osc.c ..\src\oscmix\socket.c ..\src\oscmix\sysex.c ..\src\oscmix\intpack.c ..\src\oscmix\device.c ..\src\oscmix\util.c /link ws2_32.lib winmm.lib
```

## Usage

```
Usage: oscmix [-dlm] [-r addr] [-s addr] [-R port] [-S port] [-p device]

Options:
  -d           Enable debug output
  -l           Disable level meter monitoring
  -m           Use multicast for sending (224.0.0.1)
  -r addr      Set UDP receive address (default: 127.0.0.1)
  -s addr      Set UDP send address (default: 127.0.0.1)
  -R port      Set UDP receive port (default: 7222)
  -S port      Set UDP send port (default: 8222)
  -p device    Specify RME device name/ID
```

### Example

```bash
# Start oscmix listening on all interfaces, port 7222, sending to 127.0.0.1:8222
oscmix -r 0.0.0.0 -p "Fireface UCX II"
```

## OSC Message Format

### Setting Parameters

```
/<path>/<to>/<parameter> <value>
```

Example:
```
/output/1/volume -10.5
/input/3/mute 1
/mix/1/input/2 -20.0
```

### Getting Parameters

Simply sending an OSC message to a parameter address without arguments will request its current value.

### Special Commands

- `/refresh`: Request a full refresh of all parameters
- `/dump`: Debug output of internal state
- `/enum`: List available enum values for a parameter

## Supported Devices

Currently, the application supports:
- RME Fireface UCX II

Support for additional RME devices can be added by implementing their device descriptors in the `device.c` file.

## OSC Command Reference

For a full list of available OSC commands and their parameters, see the detailed parameter tree in the source code.
