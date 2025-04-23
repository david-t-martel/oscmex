# OSCMix Architecture

This document describes the architectural design and code-level implementation of the OSCMix application, detailing how the various components interact to provide a bridge between OSC (Open Sound Control) messages and RME audio interfaces.

## Overview

OSCMix is designed to translate network OSC messages into MIDI SysEx commands for controlling RME audio devices, and to report device state changes back as OSC messages. The application is structured to work across multiple platforms (Windows, macOS, and Linux) while maintaining consistent behavior.

## Core Components and Data Flow

```
[Network] <--OSC--> [Socket Module] <---> [OSC Module]
                                            ^
                                            |
[RME Device] <--MIDI SysEx--> [MIDI I/O] <---> [Core Logic] <---> [Device Module]
                                            ^
                                            |
                                     [SysEx Module]
```

### 1. Main Entry Point (`main_unix.c` and `main_old.c`)

The entry points provide platform-specific implementations for:

- Command line argument parsing
- Network socket initialization
- MIDI device initialization and I/O
- Threading and event handling
- Signal handling for cleanup

The main.c files create two primary threads:

- `midiread`: Reads MIDI SysEx responses from the RME device
- `oscread`: Reads OSC messages from the network

### 2. Core Logic (`oscmix.c`)

The core logic in oscmix.c is responsible for:

- Device parameter mapping
- OSC message parsing and dispatching
- MIDI SysEx message generation
- State tracking and synchronization
- Error handling and reporting

### 3. Device Abstraction (`device.h` and `device_ffucxii.c`)

These files define the device-specific capabilities, parameter mappings, and behavior.

## Device Interaction Layer

### Device Description Model

The foundation of device interaction is the device description model defined in `device.h`:

```c
struct device {
    const char *id;           // Unique device identifier
    const char *name;         // Human-readable device name
    int version;              // Firmware/protocol version
    int flags;                // Device capability flags

    const struct inputinfo *inputs;    // Array of input channel definitions
    int inputslen;                     // Number of input channels
    const struct outputinfo *outputs;  // Array of output channel definitions
    int outputslen;                    // Number of output channels
};
```

Each device has specific input and output configurations:

```c
struct inputinfo {
    char name[12];            // Channel name
    int flags;                // Channel capabilities (48V, gain, etc.)
};

struct outputinfo {
    const char *name;         // Channel name
    int flags;                // Channel capabilities (reference level, etc.)
};
```

### Device Implementation

Each RME device has its own implementation file, like `device_ffucxii.c` for the Fireface UCX II. This file defines:

1. The input channels and their capabilities
2. The output channels and their capabilities
3. The device-specific features and flags

For example, the Fireface UCX II defines inputs like:

```c
static const struct inputinfo inputs[] = {
    {"Mic/Line 1",  INPUT_GAIN | INPUT_48V},        // Supports gain control and phantom power
    {"Mic/Line 2",  INPUT_GAIN | INPUT_48V},        // Supports gain control and phantom power
    {"Inst/Line 3", INPUT_GAIN | INPUT_REFLEVEL},   // Supports gain and reference level selection
    // ... more inputs
};
```

And outputs like:

```c
static const struct outputinfo outputs[] = {
    {"Analog 1", OUTPUT_REFLEVEL},   // Supports reference level selection
    {"Analog 2", OUTPUT_REFLEVEL},   // Supports reference level selection
    // ... more outputs
};
```

These definitions inform OSCMix about which parameters are available for each channel, what their value ranges are, and how they should be translated to MIDI SysEx commands.

## Parameter Processing Pipeline

### 1. Device Registration

During initialization (`init()` in `oscmix.c`), the appropriate device is selected based on the user-provided device name or ID. The device's capabilities are then registered with the core logic:

```c
int init(const char *port) {
    // Find matching device
    for (i = 0; i < LEN(devices); ++i) {
        device = devices[i];
        if (strcmp(port, device->id) == 0)
            break;
        // ...name matching logic...
    }

    // Allocate state tracking structures based on device information
    inputs = calloc(device->inputslen, sizeof *inputs);
    playbacks = calloc(device->outputslen, sizeof *playbacks);
    outputs = calloc(device->outputslen, sizeof *outputs);

    // Initialize parameter state
    // ...
}
```

### 2. Parameter Tree Construction

OSCMix builds a parameter tree structure that maps between OSC address paths and device parameters. This is defined as a static tree in `oscmix.c`:

```c
static const struct oscnode tree[] = {
    {"input", 0, .child = (const struct oscnode[]){
        {"1", 0x000, .child = inputtree},
        {"2", 0x040, .child = inputtree},
        // ... more inputs
    }},
    {"output", 0x0500, .child = (const struct oscnode[]){
        {"1", 0x000, .child = outputtree},
        {"2", 0x040, .child = outputtree},
        // ... more outputs
    }},
    // ... more parameter groups
};
```

Each node represents a segment of an OSC address path and maps to a specific register address in the device.

### 3. OSC Message Handling

When an OSC message is received by the `oscread` thread, it is passed to `handleosc()` in `oscmix.c`:

```c
int handleosc(const unsigned char *buf, size_t len) {
    // Parse OSC message
    addr = oscgetstr(&msg);
    msg.type = oscgetstr(&msg);

    // Find matching parameter in the tree
    for (node = tree; node->name;) {
        next = match(addr + 1, node->name);
        if (next) {
            path[pathlen++] = node;
            reg += node->reg;
            if (*next) {
                node = node->child;
                addr = next;
            } else {
                // Found a matching parameter
                if (node->set) {
                    node->set(path + pathlen - 1, reg, &msg);
                }
                break;
            }
        } else {
            ++node;
        }
    }
}
```

### 4. Parameter Setting

When a parameter is found, its setter function is called. Different parameter types have different setter functions:

- `setint()`: For integer parameters
- `setfixed()`: For fixed-point parameters
- `setenum()`: For enumerated parameters
- `setbool()`: For boolean parameters
- `setmix()`: For mixer parameters

For example, when setting a gain parameter on input 1, the OSC path `/input/1/gain 45.0` is processed:

1. The path is matched to the parameter tree
2. The parameter is identified as a gain control
3. `setinputgain()` is called with the register address and value
4. The value is validated against the allowed range
5. `setreg()` is called to send the MIDI SysEx command to the device

```c
static int
setinputgain(const struct oscnode *path[], int reg, struct oscmsg *msg) {
    float val;
    bool mic;

    val = oscgetfloat(msg);
    if (oscend(msg) != 0)
        return -1;
    mic = (path[-1] - path[-2]->child) <= 1;
    if (val < 0 || val > 75 || (!mic && val > 24))
        return -1;
    setreg(reg, val * 10);
    return 0;
}
```

### 5. SysEx Command Generation

The `setreg()` function converts parameter changes to MIDI SysEx commands:

```c
static int
setreg(unsigned reg, unsigned val) {
    unsigned long regval;
    unsigned char buf[4], sysexbuf[7 + 5];
    unsigned par;

    regval = (reg & 0x7fff) << 16 | (val & 0xffff);
    // Calculate parity bit
    par = regval >> 16 ^ regval;
    par ^= par >> 8;
    par ^= par >> 4;
    par ^= par >> 2;
    par ^= par >> 1;
    regval |= (~par & 1) << 31;
    putle32(buf, regval);

    writesysex(0, buf, sizeof buf, sysexbuf);
    return 0;
}
```

### 6. SysEx Response Handling

When the device responds with a SysEx message, the `midiread` thread receives it and passes it to `handlesysex()`:

```c
void handlesysex(const unsigned char *buf, size_t len, uint_least32_t *payload) {
    // Decode SysEx message
    ret = sysexdec(&sysex, buf, len, SYSEX_MFRID | SYSEX_DEVID | SYSEX_SUBID);

    // Extract register values
    pos = payload;
    for (i = 0; i < sysex.datalen; i += 5)
        *pos++ = getle32_7bit(sysex.data + i);

    // Process according to subid
    switch (sysex.subid) {
    case 0:
        handleregs(payload, pos - payload);
        break;
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
        handlelevels(sysex.subid, payload, pos - payload);
        break;
    }
}
```

### 7. State Update and Notification

When register values are received, they update the internal state and trigger OSC notifications:

```c
static void
handleregs(uint_least32_t *payload, size_t len) {
    for (i = 0; i < len; ++i) {
        reg = payload[i] >> 16 & 0x7fff;
        val = (long)((payload[i] & 0xffff) ^ 0x8000) - 0x8000;

        // Find parameter in tree and update
        // ...

        // Call notification handler
        if (reg == off + node->reg && node->new) {
            node->new(path + pathlen - 1, addr, reg, val);
        }
    }
}
```

The notification handler like `newint()`, `newfixed()`, etc. send OSC messages to notify clients of the changed value:

```c
static int
newint(const struct oscnode *path[], const char *addr, int reg, int val) {
    oscsend(addr, ",i", val);
    return 0;
}
```

## Special Parameter Handlers

### 1. Input/Output Channel States

Both input and output channels have state structures tracking their current settings:

```c
struct input {
    bool stereo;
    bool mute;
    float width;
};

struct output {
    bool stereo;
    struct mix *mix;
};
```

### 2. Mixer Matrix

The mixer matrix maps between inputs and outputs with pan and volume settings:

```c
struct mix {
    signed char pan;
    short vol;
};
```

The matrix is maintained in memory and updated based on OSC commands and device responses.

### 3. Level Meters

Level metering is handled via periodic polling in `handletimer()`, which requests level updates from the device and processes them in `handlelevels()`:

```c
static void
handlelevels(int subid, uint_least32_t *payload, size_t len) {
    // Process level data
    for (i = 0; i < len; ++i) {
        rms = *payload++;
        rms |= (uint_least64_t)*payload++ << 32;
        peak = *payload++;

        // Calculate dB values
        peakdb = 20 * log10((peak >> 4) / 0x1p23);
        rmsdb = 10 * log10(rms / 0x1p54);

        // Send OSC notification
        oscsend(addr, ",ffffi", peakdb, rmsdb, peakfxdb, rmsfxdb, peak & peakfx[i] & 1);
    }
}
```

## Platform Abstraction

OSCMix uses platform abstraction for socket operations and MIDI I/O:

### 1. Socket Operations

In `main_unix.c` and `main_old.c`, platform-specific socket code is used to:

- Open UDP sockets
- Receive OSC messages
- Send OSC notifications

### 2. MIDI I/O

Each platform has specific MIDI I/O implementations:

- Windows: Windows Multimedia API (winmm.lib)
- macOS/Linux: ALSA or Core MIDI access via file descriptors

The `writemidi()` and `midiread()` functions handle platform-specific MIDI I/O details.

## Conclusion

OSCMix creates a sophisticated bridge between the OSC network protocol and the MIDI SysEx commands used by RME devices. The architecture is modular, allowing for:

1. **Cross-platform operation** through abstracted platform-specific code
2. **Device extensibility** via the device description model
3. **Comprehensive parameter control** through a hierarchical parameter tree
4. **Real-time monitoring** of device state and audio levels
5. **Efficient bidirectional communication** between network clients and audio hardware

This architecture allows for easy integration with various control surfaces, custom GUIs, automation systems, and other OSC-capable software, making RME devices more versatile in professional audio workflows.
