# AudioEngine / OSCMex Project

## Overview

This project combines a C++ audio processing library (`AudioEngine`) with the `liblo` C library for Open Sound Control (OSC) communication and integrates a local FFmpeg source for audio processing. The primary goal is to create a flexible audio engine controllable via OSC messages, specifically targeting RME audio interfaces through their TotalMix FX OSC API.

The project offers high-performance audio processing capabilities with Intel oneAPI optimizations, comprehensive device configuration via JSON, and a modular architecture for future extensibility.

## Architecture

The project follows a modular architecture detailed in [`AudioEngine_ARCHITECTURE.md`](AudioEngine_ARCHITECTURE.md) with the following key components:

1. **`AudioEngine` (C++ Library - `src/AudioEngine`):** The core component responsible for audio processing tasks. It uses a node-based processing architecture with these key elements:
   - `AudioNode` base class and specialized node types (ASIO I/O, file I/O, FFmpeg processing)
   - Connection-based signal routing
   - Enhanced buffer management with reference counting
   - Integrated FFmpeg libraries for comprehensive audio processing
   - Intel oneAPI optimizations for high performance

2. **`OscController` (C++ Class - `src/AudioEngine/OscController.cpp`):** Manages bidirectional OSC communication with RME devices, supporting:
   - Comprehensive parameter control (volume, mute, routing, EQ, etc.)
   - Device state querying
   - Configuration application from JSON files
   - Event notifications and callbacks

3. **Configuration System (C++ Classes - `src/AudioEngine/Configuration.cpp`):**
   - JSON-based configuration loading/saving
   - Structured organization of device settings
   - Device state management (reading/writing)
   - Command-line parameter parsing

4. **FFmpeg Integration:**
   - Local integration of FFmpeg source code (no external dependencies)
   - Complete audio codec, format, and filtering functionality
   - Custom-built for optimized performance

5. **Supporting Components:**
   - `liblo` (C Library - `src/lo`): Lightweight OSC implementation
   - Command-line tools and examples
   - Mermaid-based architecture documentation

## Project Structure

* `./CMakeLists.txt`: Main CMake build script with FFmpeg and nlohmann/json integration
* `./CMakePresets.json`: Predefined configurations for CMake
* `./AudioEngine_ARCHITECTURE.md`: Detailed architecture documentation
* `./build/`: Build output directory
* `./src/`: Source code
  * `./src/AudioEngine/`: Core engine components
    * `AudioBuffer.h/cpp`: Audio data container with reference counting
    * `AudioNode.h/cpp`: Base class for processing nodes
    * `AsioManager.h/cpp`: ASIO hardware interface
    * `OscController.h/cpp`: OSC communication with RME devices
    * `Configuration.h/cpp`: Configuration management
    * `Connection.h/cpp`: Audio signal routing
    * `DeviceStateManager.h/cpp`: Device state querying
    * Node implementations: ASIO I/O, file I/O, FFmpeg processing
    * `MERMAID.md`: Architectural diagrams
  * `./src/ffmpeg-master/`: Integrated FFmpeg source code
  * `./src/lo/`: liblo OSC implementation
  * `./src/tools/`: Command-line utilities
    * `./src/tools/osc/`: OSC tools and scripts
    * `./src/tools/matlab/`: MATLAB integration tools
  * `./src/web/`: Web-based interface components
  * `./src/oscmix/`: Device-specific implementations
* `./include/`: External library headers
  * `./include/nlohmann/`: nlohmann/json headers (auto-downloaded)
* `./examples/`: Example configurations and usage patterns
* `./TODO.md`: Development task tracking

## Dependencies

* **CMake:** Version 3.15+ (for building)
* **C++ Compiler:** C++17 compliant (GCC, Clang, MSVC, Intel icpx)
* **ASIO SDK:** For hardware audio I/O (included)
* **FFmpeg:** Integrated source code (no external dependency)
* **nlohmann/json:** Automatically downloaded during the build process
* **Intel oneAPI (Optional but Recommended):**
  * Threading Building Blocks (TBB) for parallelism
  * Integrated Performance Primitives (IPP) for signal processing
  * Math Kernel Library (MKL) for optimized math operations
* **Doxygen (Optional):** For building documentation

## Building

The project uses CMake with the following key features:

1. **Automatic FFmpeg Integration:**
   - FFmpeg source code is built as part of the project
   - No external FFmpeg installation required

2. **Automatic JSON Library Integration:**
   - nlohmann/json is automatically downloaded and integrated
   - No manual installation required

3. **Intel oneAPI Detection and Integration:**
   - Automatically detects and uses Intel compilers if available
   - TBB, IPP, and MKL are automatically integrated when present

4. **Build Options:**
   - `USE_INTEL_COMPILER`: Prefer Intel oneAPI compiler (default: ON)
   - `USE_INTEL_TBB`: Use Threading Building Blocks (default: ON)
   - `USE_INTEL_IPP`: Use Integrated Performance Primitives (default: ON)
   - `USE_INTEL_MKL`: Use Math Kernel Library (default: ON)
   - `BUILD_LO_TOOLS`: Build OSC command-line tools (default: ON)
   - `BUILD_DOCUMENTATION`: Build Doxygen documentation (default: ON)

### Build Steps

```bash
# Configure with standard compiler
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Configure with Intel oneAPI compiler
cmake -S . -B build-intel -DCMAKE_C_COMPILER=icx -DCMAKE_CXX_COMPILER=icpx -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# Install
cmake --install build --prefix /path/to/install
```

Windows users can use the provided `build/build_script_oneapi.bat` script for Intel oneAPI builds.

## Configuration and Usage

### JSON Configuration

The system uses JSON configuration files to define:
- Audio processing graph (nodes and connections)
- Device settings (ASIO, sample rate, buffer size)
- OSC communication parameters
- RME device-specific settings

Example JSON configuration:
```json
{
  "asioDeviceName": "RME Fireface UCX II",
  "rmeOscIp": "127.0.0.1",
  "rmeOscPort": 7001,
  "sampleRate": 48000,
  "bufferSize": 512,

  "nodes": [
    { "name": "asio_input", "type": "ASIO_SOURCE", "params": { "channels": "0,1" } },
    { "name": "processor", "type": "FFMPEG_PROCESSOR", "params": {
      "filter_description": "equalizer=f=1000:width_type=q:width=1:g=0"
    }},
    { "name": "asio_output", "type": "ASIO_SINK", "params": { "channels": "0,1" } }
  ],

  "connections": [
    { "sourceName": "asio_input", "sourcePad": 0, "sinkName": "processor", "sinkPad": 0 },
    { "sourceName": "processor", "sourcePad": 0, "sinkName": "asio_output", "sinkPad": 0 }
  ],

  "rmeCommands": [
    { "address": "/1/channel/1/volume", "args": [0.0] },
    { "address": "/1/channel/2/volume", "args": [0.0] }
  ]
}
```

### Device State Management

The system can:
1. Query the current state of connected RME devices
2. Save device configurations to JSON files
3. Apply saved configurations to devices

This allows for:
- Creating device presets
- Backup and restore of device settings
- Programmatic control of device parameters

## Future Development

Key areas for future development include:
- GUI integration for configuration and monitoring
- Additional processing node types
- Enhanced device control capabilities
- Web interface improvements

See [`TODO.md`](TODO.md) for specific development tasks.

## License

This project uses multiple components with different licenses:
- liblo: LGPL v2.1 or later
- FFmpeg: LGPL v2.1 or later
- nlohmann/json: MIT
- Project-specific code: MIT
