# Modular Audio Engine Design

This design facilitates flexible routing between hardware I/O (via ASIO), file I/O, and FFmpeg processing, configured externally and utilizing RME's OSC capabilities for hardware matrix routing.

## Core Concepts

1. **`AudioNode`**: An abstract base class representing any processing block, source, or sink.
    * Defines a common interface (e.g., `configure`, `process`, `getName`, `getInputPad`, `getOutputPad`).
    * Nodes have input and output "pads" for connecting.

2. **Node Types (Derived from `AudioNode`)**:
    * **`AsioSourceNode`**: Represents one or more logical ASIO input channels managed by `AsioManager`. Has output pads providing audio buffers.
    * **`AsioSinkNode`**: Represents one or more logical ASIO output channels managed by `AsioManager`. Has input pads consuming audio buffers.
    * **`FileSourceNode`**: Reads, decodes, and potentially resamples an audio file using `libavformat`/`libavcodec`/`libswresample`. Has output pads. Likely runs in a dedicated thread.
    * **`FileSinkNode`**: Encodes and writes audio to a file using `libavformat`/`libavcodec`/`libswresample`. Has input pads. Likely runs in a dedicated thread.
    * **`FfmpegProcessorNode`**: Wraps an `FfmpegFilter` instance (from the previous design `asio_ffmpeg_eq_code_v2`). Has input and output pads. Handles the FFmpeg filter chain execution and dynamic parameter updates.

3. **`AudioEngine`**: The central coordinator.
    * **Responsibilities**:
        * Parses the application configuration (from file or command line).
        * Owns and manages the `AsioManager` instance.
        * Instantiates and configures all required `AudioNode` objects based on the configuration.
        * Manages connections between `AudioNode` pads based on the configuration.
        * Owns and manages the `RmeOscController`.
        * Sends initial OSC commands to configure the RME TotalMix FX matrix via `RmeOscController`.
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

5. **`RmeOscController`**:
    * Uses a C++ OSC library (e.g., `oscpack`).
    * Provides a method like `sendMatrixCommand(oscAddress, args)` or higher-level functions like `setMatrixCrosspointGain(inputChannel, outputChannel, gainDb)`.
    * Takes target RME IP address and port (from configuration).
    * Formats and sends OSC messages via UDP to control TotalMix FX. Primarily used during initialization by the `AudioEngine` based on the configuration file's routing requirements.

6. **`Configuration`**:
    * A data structure holding the parsed configuration.
    * Defines:
        * Global settings (e.g., ASIO device name, RME OSC target).
        * A list of nodes to create (name, type, type-specific parameters like file path, ASIO channels, filter description).
        * A list of connections between node pads (e.g., `nodeA:output0 -> nodeB:input0`).
        * A list of RME OSC commands or logical routing rules to execute at startup.

7. **Configuration Parser**: Reads a file (e.g., JSON, YAML) or command-line arguments and populates the `Configuration` structure.

## Workflow Example (ASIO In -> FFmpeg -> ASIO Out)

1. **Startup**:
    * `main` creates `AudioEngine`, `ConfigurationParser`, `RmeOscController`.
    * `ConfigurationParser` reads config (defining `asio_in`, `ffmpeg_proc`, `asio_out` nodes, connections, and RME routing).
    * `AudioEngine` parses config struct.
    * `AudioEngine` tells `RmeOscController` to send OSC commands to TotalMix FX (e.g., route physical input X to the ASIO channel used by `asio_in`, route the ASIO channel used by `asio_out` to physical output Y).
    * `AudioEngine` initializes `AsioManager` for the required ASIO channels.
    * `AudioEngine` creates `AsioSourceNode` (using ASIO input channels), `FfmpegProcessorNode` (with filter chain), `AsioSinkNode` (using ASIO output channels).
    * `AudioEngine` establishes internal connections: `asio_in` output -> `ffmpeg_proc` input, `ffmpeg_proc` output -> `asio_out` input.
    * `AudioEngine` starts `AsioManager`.

2. **Real-time Loop (`bufferSwitch` triggers `AudioEngine::processBlock`)**:
    * `AudioEngine` notes which `AsioSourceNode` has data and which `AsioSinkNode` needs data.
    * `AudioEngine` requests buffer from `asio_in`.
    * `AudioEngine` passes buffer to `ffmpeg_proc`'s input, calls its process method.
    * `AudioEngine` retrieves processed buffer from `ffmpeg_proc`'s output.
    * `AudioEngine` passes processed buffer to `asio_out`'s input.
    * `AudioEngine` signals ASIO output ready (via `AsioManager`).

## Key Advantages of this Architecture

* **Modularity**: Clearly separates concerns (ASIO I/O, file I/O, processing, OSC control, engine logic). Easy to add new node types.
* **Flexibility**: Routing and processing are defined by configuration, not hardcoded.
* **Reusability**: Nodes like `FfmpegProcessorNode` can be reused.
* **Testability**: Individual nodes can potentially be tested more easily.

## Challenges

* **Complexity**: More classes and interactions than the previous design.
* **Buffer Management**: Requires a robust system for managing audio buffers passed between nodes, potentially involving pooling and reference counting.
* **Connection Logic**: Implementing the connection routing within the `AudioEngine` needs care.
* **Configuration Parsing**: Requires implementing or integrating a parser for the chosen format.
* **Threading**: Managing threads for file I/O adds complexity.

This architecture provides the foundation for the highly flexible, configurable system you described.
