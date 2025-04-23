# Modular Audio Engine Design

This design facilitates flexible routing between hardware I/O (via ASIO), file I/O, and FFmpeg processing, configured externally and utilizing RME's OSC capabilities for hardware matrix routing.

## Core Concepts

1. **`AudioNode`**: An abstract base class representing any processing block, source, or sink.
    * Defines a common interface (e.g., `configure`, `process`, `getName`, `getInputPad`, `getOutputPad`).
    * Nodes have input and output "pads" for connecting.
    * Provides methods for buffer format validation and parameter updates.
    * Handles error reporting and state management.

2. **Node Types (Derived from `AudioNode`)**:
    * **`AsioSourceNode`**: Represents one or more logical ASIO input channels managed by `AsioManager`. Has output pads providing audio buffers.
    * **`AsioSinkNode`**: Represents one or more logical ASIO output channels managed by `AsioManager`. Has input pads consuming audio buffers.
    * **`FileSourceNode`**: Reads, decodes, and potentially resamples an audio file using integrated FFmpeg libraries. Has output pads. Runs in a dedicated thread.
    * **`FileSinkNode`**: Encodes and writes audio to a file using integrated FFmpeg libraries. Has input pads. Runs in a dedicated thread.
    * **`FfmpegProcessorNode`**: Wraps the `FfmpegFilter` instance. Has input and output pads. Handles FFmpeg filter chain execution and supports dynamic parameter updates through the AudioNode interface.

3. **`AudioEngine`**: The central coordinator.
    * **Responsibilities**:
        * Parses the application configuration from JSON files or command line.
        * Owns and manages the `AsioManager` instance.
        * Instantiates and configures all required `AudioNode` objects based on the configuration.
        * Manages connections between `AudioNode` pads based on the configuration.
        * Owns and manages the `OscController`.
        * Sends initial OSC commands to configure the RME TotalMix FX matrix via `OscController`.
        * Drives the audio processing loop:
            * If using ASIO, it gets triggered by the `AsioManager` callback.
            * If purely file-based, it runs its own loop.
        * Orchestrates data transfer between connected nodes (pulling from outputs, pushing to inputs).
        * Handles starting, stopping, and cleaning up all nodes and managers.
        * Manages necessary threading for file I/O nodes.
        * Manages buffer allocation and reuse between nodes.

4. **`AsioManager`**: (Modified from previous design)
    * Lower-level ASIO interaction.
    * Initializes the driver specified by `AudioEngine`.
    * Activates *only* the specific ASIO channels requested by the created `AsioSourceNode` and `AsioSinkNode` instances.
    * Provides the core `bufferSwitch` callback, which notifies the `AudioEngine` to run its processing cycle.

5. **`OscController`**:
    * Uses the LibLO OSC library for communication.
    * Provides high-level functions like `setChannelVolume()`, `setChannelMute()`, and `setMatrixCrosspointGain()`.
    * Takes target OSC device IP address and port (from configuration).
    * Formats and sends OSC messages via UDP to control the target device (e.g., TotalMix FX).
    * Can listen for incoming OSC messages and dispatch them (e.g., level meters, status updates).
    * Supports querying current device state for parameter values.
    * Can apply a complete Configuration to configure all aspects of the device at once.

6. **`Configuration`**:
    * A comprehensive data structure holding the parsed configuration.
    * Supports loading from and saving to JSON files.
    * Defines:
        * Global settings (e.g., ASIO device name, OSC IP address and port, sample rate, buffer size)
        * A list of nodes to create (name, type, type-specific parameters like file path, ASIO channels, filter description)
        * A list of connections between node pads (e.g., `nodeA:output0 -> nodeB:input0`)
        * A structured collection of OSC commands organized by functional areas (channels, matrix routing, EQ, etc.)
    * Provides methods to convert between normalized OSC values and human-readable values (e.g., dB).
    * Includes serialization/deserialization capabilities for all settings.

7. **`DeviceStateManager`**: Queries and manages device state.
    * Works with `OscController` to read the current state of the connected RME device.
    * Provides asynchronous querying with callbacks for UI responsiveness.
    * Converts device state into `Configuration` objects that can be saved to JSON.
    * Supports targeted parameter queries and batch operations.

8. **`FFmpeg Integration`**: (New component)
    * Uses the integrated FFmpeg source code rather than external libraries.
    * Provides complete audio codec, format, and filtering functionality.
    * Custom-built for optimized performance with the same compiler flags as the rest of the project.
    * Key components:
        * `libavformat`: Audio file container handling
        * `libavcodec`: Audio encoding/decoding
        * `libavfilter`: DSP processing including EQ, dynamics, and other effects
        * `libavutil`: Common utility functions
        * `libswresample`: Sample format conversion and resampling

## Dual-Phase Device Configuration

The system implements a two-phase device configuration approach:

1. **Hardware Connection & Auto-Configuration Phase**:
   * When the engine starts, it first connects to the specified audio device via `AsioManager`
   * Once connected, `AsioManager` queries the device driver for optimal settings:
     * Available sample rates
     * Optimal/preferred buffer sizes
     * Channel count and names
     * Device capabilities
   * This information is used to configure the engine's audio processing pipeline:
     * Setting the appropriate sample rate for all nodes
     * Configuring buffer sizes based on hardware capabilities
     * Creating appropriate audio format converters between nodes
   * The `AudioEngine::autoConfigureAsio()` method handles this auto-configuration when enabled

2. **Device-Specific Configuration Phase**:
   * After hardware connection and auto-configuration, device-specific settings are applied via OSC
   * The `OscController` sends commands defined in the `Configuration` to configure:
     * Channel volumes, mutes, and other parameters
     * Matrix routing between inputs and outputs
     * EQ settings, dynamics, and effects
     * Global device settings (main volume, dim, mono, etc.)
   * These settings can be applied from a saved configuration file or built programmatically
   * The `AudioEngine::sendOscCommands()` method handles this phase

This approach allows the engine to:

1. First establish a working connection to the hardware with optimal settings
2. Then apply user or application-specific configurations to the device
3. Support querying the current state through `DeviceStateManager` for persistence or UI feedback

## Configuration System

The JSON configuration system has been enhanced to provide:

1. **Structured Organization**:
   * Channel settings (inputs, playbacks, outputs) with per-channel parameters
   * Matrix routing with crosspoint gains
   * Global settings (mute, dim, mono, etc.)
   * EQ and dynamics settings
   * Effect settings (reverb, echo)

2. **Device State Management**:
   * Reading current device configuration
   * Saving device state to JSON files
   * Loading and applying saved configurations

3. **Extensibility**:
   * Support for different device types
   * Separation of device-specific and generic settings
   * Forward compatibility with future enhancements

## Workflow Example (ASIO In -> FFmpeg -> ASIO Out with OSC Control)

1. **Startup and Configuration**:
    * `main` parses command-line arguments and/or loads a JSON configuration file.
    * `ConfigurationParser` creates a `Configuration` object with nodes, connections, and OSC commands.
    * `AudioEngine` initializes based on the `Configuration`.
    * `AudioEngine` connects to the audio device and auto-configures based on hardware capabilities.
    * `OscController` sends commands to configure the RME device according to the loaded settings.
    * `AudioEngine` creates all nodes and connections for the processing graph.

2. **FFmpeg-based Processing**:
    * `FfmpegProcessorNode` initializes its filter graph using the integrated FFmpeg source code.
    * The filter graph may include complex processing chains with multiple filters.
    * Dynamic parameter updates can be received and applied to the filter chain in real-time.

3. **Real-time Processing**:
    * `AsioManager` triggers the `bufferSwitch` callback.
    * `AudioEngine` orchestrates the processing of all nodes in the graph.
    * Audio buffers flow through the nodes, getting processed by FFmpeg filters.
    * Processed audio is sent to outputs.

4. **Device State Management**:
    * At any point, `DeviceStateManager` can query the device state.
    * Current settings can be saved to a new JSON file.
    * Saved configurations can be loaded and applied to the device.

## Key Advantages of this Architecture

* **Modularity**: Clearly separates concerns (ASIO I/O, file I/O, processing, OSC control, engine logic). Easy to add new node types.
* **Flexibility**: Routing and processing are defined by configuration, not hardcoded.
* **Reusability**: Components like `FfmpegProcessorNode` and `OscController` can be reused.
* **Self-contained**: Integrated FFmpeg source eliminates external dependencies.
* **Optimized Performance**: Consistent compiler flags and optimizations across all components.
* **Comprehensive Device Control**: Complete RME device configuration via OSC.
* **Future GUI Support**: Architecture designed to accommodate a GUI layer in the future.

## Intel oneAPI Integration

The project has been enhanced with Intel oneAPI integration for optimized performance:

* **Intel C++ Compiler**: Provides advanced optimization and vectorization.
* **Threading Building Blocks (TBB)**: For parallel processing of audio data.
* **Integrated Performance Primitives (IPP)**: Accelerates common audio operations.
* **Math Kernel Library (MKL)**: Optimizes math operations, especially for FFT calculations.

This optimization layer works seamlessly with the FFmpeg integration for maximum performance.
