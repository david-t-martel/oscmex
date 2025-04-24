# OSCMix: Open Sound Control Interface for RME Audio Devices

OSCMix provides a bridge between OSC (Open Sound Control) and RME audio interfaces, allowing remote control of device parameters using standard OSC messages.

## Overview

`oscmix` is a command-line application that acts as a bridge between OSC (Open Sound Control) messages and RME audio interfaces. It enables remote control of RME mixer functionality via network communications, translating OSC messages into MIDI SysEx commands that RME devices understand.

The application allows for:

- Controlling mixer settings (volume, mute, pan, etc.)
- Adjusting device parameters (sample rate, clock source)
- Applying effects (EQ, dynamics, reverb, etc.)
- Receiving meter/level information from the device
- Controlling recording functions on supported devices

## Architecture

OSCMix follows a modular architecture with these key components:

- **oscmix**: Core coordination module that handles message routing and main application flow
- **oscnode_tree**: Parameter tree definition and lookup functionality
- **oscmix_commands**: Command handlers for setting device parameters
- **oscmix_midi**: MIDI SysEx communication with the device
- **device_state**: Internal state tracking for all device parameters
- **device**: Device-specific definitions and capabilities

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

For a detailed description of the code structure, component interactions, and implementation details, see the [ARCHITECTURE.md](ARCHITECTURE.md) document.

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

### Using CMake (Cross-Platform)

```bash
# Create build directory
mkdir build
cd build

# Configure and build
cmake ..
cmake --build .

# Install (optional)
cmake --install .
```

## Command Syntax

```
Usage: oscmix [-dlmh?] [-r addr] [-s addr] [-R port] [-S port] [-p device]

Options:
  -d           Enable debug output
  -l           Disable level meter monitoring
  -m           Use multicast for sending (224.0.0.1)
  -r addr      Set UDP receive address (default: 127.0.0.1)
  -s addr      Set UDP send address (default: 127.0.0.1)
  -R port      Set UDP receive port (default: 7222)
  -S port      Set UDP send port (default: 8222)
  -p device    Specify RME device name/ID
  -h, -?       Display this help message
```

### Parameter Details

- **-d** (Debug): Enables debug output to standard error. Useful for troubleshooting.
- **-l** (Level meters): Disables level meter monitoring. By default, oscmix will monitor and transmit level meters.
- **-m** (Multicast): Uses multicast address 224.0.0.1 for sending OSC messages, allowing multiple clients to receive updates.
- **-r addr** (Receive address): Specifies the IP address to bind for receiving OSC messages. Use "0.0.0.0" to listen on all interfaces.
- **-s addr** (Send address): Specifies the destination IP address for OSC messages sent from oscmix.
- **-R port** (Receive port): Specifies the UDP port to listen on for incoming OSC messages (default: 7222).
- **-S port** (Send port): Specifies the UDP port to send OSC messages to (default: 8222).
- **-p device** (MIDI device): Specifies the RME device name or ID to connect to. This is required unless the MIDIPORT environment variable is set.
- **-h, -?** (Help): Displays the help message with command usage information.

### Environment Variables

- **MIDIPORT**: Alternative way to specify the MIDI device name/ID. Used if the `-p` option is not provided.

### Examples

```bash
# Start oscmix listening on all interfaces, port 7222, sending to 127.0.0.1:8222
oscmix -r 0.0.0.0 -p "Fireface UCX II"

# Start oscmix with debug output enabled
oscmix -d -p "Fireface UCX II"

# Start oscmix using non-standard ports
oscmix -R 9000 -S 9001 -p "Fireface UCX II"

# Start oscmix with multicast output for multiple clients
oscmix -m -p "Fireface UCX II"

# Start oscmix specifying device through the environment variable
export MIDIPORT="Fireface UCX II"
oscmix
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

## OSC Commands

### Device State Management

- `/dump` - Prints the current device state to the console (for debugging)
- `/dump/save` - Exports the current device configuration to a JSON file in the app's home directory
  - File is saved to `~/device_config/audio-device_DEVICENAME_date-time_DATETIME.json` (Windows: `%APPDATA%\OSCMix\device_config\...`)
  - JSON format is machine-readable and can be used to restore device state or analyze configuration

For example:

- `/system/samplerate` - Get/set the device sample rate
- `/input/1/gain` - Get/set the gain of input channel 1
- `/output/2/volume` - Get/set the volume of output channel 2

### Device State Management

- `/refresh` - Request a full device state refresh
- `/dump` - Prints the current device state to the console (for debugging)
- `/dump/save` - Exports the current device configuration to a JSON file in the app's home directory
  - File is saved to `~/device_config/audio-device_DEVICENAME_date-time_DATETIME.json` (Windows: `%APPDATA%\OSCMix\device_config\...`)
  - JSON format is machine-readable and can be used to restore device state or analyze configuration

### System Parameters

- `/system/samplerate` - Current sample rate (44100, 48000, 88200, 96000, etc.)
- `/system/clocksource` - Clock source selection ("Internal", "AES", "ADAT", etc.)
- `/system/buffersize` - Device buffer size (32, 64, 128, 256, 512, 1024)
- `/system/phantompower` - Global phantom power (1 = on, 0 = off)
- `/system/mastervol` - Master volume level (0.0 to 1.0)
- `/system/mastermute` - Master mute state (1 = muted, 0 = unmuted)

### Input Channel Parameters

- `/input/N/gain` - Input gain in dB (0.0 to 75.0 for mic inputs, 0.0 to 24.0 for line)
- `/input/N/phantom` - 48V phantom power for channel N (1 = on, 0 = off)
- `/input/N/pad` - Pad for channel N (1 = enabled, 0 = disabled)
- `/input/N/reflevel` - Reference level ("-10 dBV", "+4 dBu", "HiZ")
- `/input/N/mute` - Mute state (1 = muted, 0 = unmuted)
- `/input/N/stereo` - Stereo link with adjacent channel (1 = linked, 0 = mono)

### Output Channel Parameters

- `/output/N/volume` - Output volume level (0.0 to 1.0)
- `/output/N/mute` - Mute state (1 = muted, 0 = unmuted)
- `/output/N/reflevel` - Reference level ("-10 dBV", "+4 dBu", "HiZ")
- `/output/N/dither` - Dither setting ("Off", "16 bit", "20 bit")
- `/output/N/phase` - Phase inversion (1 = inverted, 0 = normal)
- `/output/N/mono` - Mono mode (1 = mono, 0 = stereo)

### Mixer Parameters

- `/mixer/out/in/volume` - Mixer volume from input to output (-65.0 to 6.0 dB)
- `/mixer/out/in/pan` - Panning from input to output (-100 to 100)

## Building

See the BUILDING.md file for detailed build instructions for various platforms.

## License

OSCMix is licensed under the MIT License. See LICENSE file for details.
