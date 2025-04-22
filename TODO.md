# TODO: Modular C++ Audio Engine Implementation

This document outlines the steps required to implement the modular C++ audio engine architecture, as detailed in the `audio_engine_diagram_v3` diagram.

**Important Context:** This plan describes the creation of a **new C++ application**. While it incorporates OSC concepts potentially shared with your MATLAB/MEX `oscmex` project ([`osc_rme.cpp`](c:\codedev\auricleinc\oscmex\src\tools\matlab\osc_rme.cpp), [`oscmex_test.m`](c:\codedev\auricleinc\oscmex\src\tools\matlab\oscmex_test.m)), it does not directly modify the `oscmex` codebase. The existing [`OscController`](c:\codedev\auricleinc\oscmex\src\AudioEngine\OscController.h) class implements some of the OSC functionalities described below. This C++ engine could potentially be controlled by or interact with MATLAB via MEX wrappers later, but the core engine itself is a separate C++ entity.

## I. Project Setup & Dependencies

- [X] **Create C++ Project Structure:**
  - [X] Set up a new C++ project using a build system like CMake.
  - [X] Organize source files (.h/.cpp) logically (e.g., `src/core`, `src/nodes`, `src/managers`, `src/utils`).
- [X] **Integrate Dependencies:**
  - [X] **ASIO SDK:** Ensure headers are accessible to the compiler and link against any necessary libraries (if applicable). Add placeholders/wrappers if SDK cannot be directly included in a public repo.
  - [X] **FFmpeg:** Include headers and link against required FFmpeg development libraries (`libavutil`, `libavformat`, `libavcodec`, `libavfilter`, `libswresample`). Integrated source code rather than external libraries.
  - [X] **C++ OSC Library:** Choose and integrate a library (e.g., `oscpack`, `liblo wrapper`). (`liblo` integrated into the project).
  - [X] **C++ Configuration Library:** Implemented using `nlohmann/json` auto-downloaded during build process.

## II. Core Component Implementation

- [X] **Implement `AudioBuffer` Structure/Class:**
  - [X] Define the `AudioBuffer` struct/class with allocation, copying methods.
  - [X] Implement reference counting for efficient buffer management.
  - [X] Include format, layout, sample rate, and frame count alongside data pointers.
- [ ] **Implement `AsioManager` Class:** *(Implementation example in sample_project.cpp)*
  - [X] Adapt the detailed C++ implementation.
  - [X] Replace ASIO SDK placeholders with actual SDK calls.
  - [X] Modify `initDevice` to accept preferred sample rate/buffer size from `AudioEngine`.
  - [ ] Modify `createBuffers` to accept specific input/output channel index vectors provided by `AudioEngine`.
  - [ ] Implement `setCallback` to store the `AudioEngine`'s callback function pointer/lambda.
  - [ ] Implement `getBufferPointers` for `AudioEngine` to retrieve buffers during the callback.
  - [ ] Enhance error handling for all ASIO calls.
  - [ ] Add channel discovery and selection capabilities as shown in `sample_project.cpp` using `findChannelIndices`.
  - [ ] Implement multi-channel ASIO device support with proper buffer handling.
- [X] **Implement `OscController` Class:** (Fully implemented as [`OscController`](c:\codedev\auricleinc\oscmex\src\AudioEngine\OscController.h))
  - [X] Implement `configure` method to set target IP/Port for sending and listening port.
  - [X] Implement `sendCommand` using `liblo` to format and send UDP packets.
  - [X] Implement listener thread (using `lo_server_thread`).
  - [X] Use `liblo` to bind a UDP socket and listen for packets in the thread.
  - [X] Implement message parsing logic within the listener thread.
  - [X] Implement a dispatch mechanism to handle recognized OSC address patterns.
  - [X] Implement `startReceiver`, `stopReceiver`, `cleanup` methods.
  - [X] Implement RME-specific helper methods.
  - [X] Implement `applyConfiguration` to apply complete Configuration to a device.
  - [X] Add device state querying capabilities.
- [ ] **Implement `AudioEngine` Class:** (Basic structure in [`main.cpp`](c:\codedev\auricleinc\oscmex\src\AudioEngine\main.cpp), [`audioEngine_main.cpp`](c:\codedev\auricleinc\oscmex\src\AudioEngine\audioEngine_main.cpp), and sample reference in `sample_project.cpp`)
  - [ ] Implement constructor/destructor, `initialize`, `run`, `stop`, `cleanup`.
  - [X] Implement configuration parsing logic within `initialize` (using `ConfigurationParser`).
  - [ ] Implement node creation and storage (`m_nodes`, `m_nodeMap`).
  - [ ] Implement connection setup (`m_connections`, determine `m_processingOrder`).
  - [ ] Implement interaction with `AsioManager` (initialization, starting, stopping, setting callback).
  - [X] Implement interaction with `OscController` (sending setup commands, starting/stopping listener, receiving dispatched commands).
  - [ ] Implement `processAsioBlock` method (called by `AsioManager` callback) based on the callback implementation in `sample_project.cpp`.
  - [ ] Implement `runFileProcessingLoop` method (for non-ASIO mode).
  - [ ] Implement the core data flow logic between connected nodes based on `m_connections` and `m_processingOrder`.
  - [X] Implement basic buffer management strategy with reference counting.
  - [ ] Implement thread management for file nodes.
  - [ ] Add clean shutdown handling with proper resource cleanup and thread joining.

## III. Audio Node Implementation

- [X] **Define `AudioNode` Base Class:**
  - [X] Finalize the pure virtual interface (`configure`, `start`, `process`, `stop`, `cleanup`, `getOutputBuffer`, `setInputBuffer`).
- [ ] **Implement `AsioSourceNode`:**
  - [X] Implement `configure` (parse channel params, store indices).
  - [ ] Implement `receiveAsioData`: Get raw buffers from `AsioManager`, convert to internal format/layout, store in output buffer.
  - [X] Implement `getOutputBuffer`: Return data from output buffer.
  - [X] Implement `cleanup` (free conversion contexts).
  - [ ] Add support for channel name discovery and selection as demonstrated in `sample_project.cpp`.
- [ ] **Implement `AsioSinkNode`:**
  - [X] Implement `configure` (parse channel params, store indices).
  - [X] Implement `setInputBuffer`: Store incoming `AudioBuffer`.
  - [ ] Implement `provideAsioData`: Convert input buffer data to ASIO format, copy to raw ASIO buffers.
  - [X] Implement `cleanup` (free conversion contexts).
  - [ ] Add support for channel name discovery and selection as demonstrated in `sample_project.cpp`.
- [X] **Implement `FfmpegProcessorNode`:** *(Implementation example in sample_project.cpp as `FfmpegFilter`)*
  - [X] Implement `configure` (parse `chain` param, call `m_ffmpegFilter->initGraph`).
  - [X] Implement `setInputBuffer`: Store input buffer.
  - [X] Implement `process`: Call `m_ffmpegFilter->process` using stored input/output buffers.
  - [X] Implement `getOutputBuffer`: Return processed output buffer.
  - [X] Implement `updateParameter`: Forward call to `m_ffmpegFilter->updateFilterParameter`.
  - [X] Implement `cleanup`: Call `m_ffmpegFilter->cleanup`.
  - [ ] Add support for dynamic filter chain configuration with parameter updates via OSC.
  - [ ] Enhance filter graph error handling and validation.
  - [ ] Implement proper resource management for FFmpeg contexts.
- [ ] **Implement `FileSourceNode`:**
  - [X] Implement `configure` (get path, setup FFmpeg format/codec contexts).
  - [ ] Implement `start`: Launch `readerThreadFunc`.
  - [ ] Implement `readerThreadFunc`: Read frames, decode, resample, package into `AudioBuffer`, push to thread-safe output queue.
  - [X] Implement `getOutputBuffer`: Pop `AudioBuffer` from thread-safe queue.
  - [ ] Implement `stop`: Set `m_running` to false, join thread.
  - [X] Implement `cleanup`: Clean up FFmpeg contexts, close file.
- [ ] **Implement `FileSinkNode`:**
  - [X] Implement `configure` (get path, setup FFmpeg format/codec/muxer contexts, open file, write header).
  - [ ] Implement `start`: Launch `writerThreadFunc`.
  - [X] Implement `setInputBuffer`: Push `AudioBuffer` to thread-safe input queue.
  - [ ] Implement `writerThreadFunc`: Process queue, resample, encode, write frames.
  - [ ] Implement `stop`: Signal thread to finish queue, flush encoder, write trailer, join thread.
  - [X] Implement `cleanup`: Clean up FFmpeg contexts, close file.

## IV. Supporting Component Implementation

- [X] **Implement `Configuration` and `ConfigurationParser`:**
  - [X] Implement JSON configuration format with nlohmann/json library.
  - [X] Create robust parsing logic for both files and command-line arguments.
  - [X] Add validation and structured organization of settings.
  - [X] Implement device-specific sections (channel settings, matrix routing, etc.)
  - [X] Add serialization/deserialization for all settings.
- [X] **Implement `DeviceStateManager`:**
  - [X] Implement device state querying functionality.
  - [X] Add support for saving and loading device configurations.
  - [X] Create asynchronous query interface with callbacks.
- [X] **Implement Buffer Management:**
  - [X] Implement reference-counted `AudioBuffer` class.
  - [X] Add thread-safe access methods.
  - [X] Add format validation and conversion capabilities.

## V. Integration & Workflow

- [ ] **Implement `main.cpp`:** *(Reference implementation in sample_project.cpp)*
  - [X] Instantiate core components (`ConfigurationParser`, `AudioEngine`).
  - [X] Handle command-line arguments / config file loading.
  - [ ] Initialize `AudioEngine` with complete processing graph.
  - [ ] Start `AudioEngine` (`run`).
  - [X] Implement main loop with command processing.
  - [ ] Implement graceful shutdown (`stop`, `cleanup`).
  - [ ] Add signal handling for clean shutdown (SIGINT, SIGTERM) similar to `sample_project.cpp`.
  - [ ] Implement clean resource management with proper shutdown sequence.
- [ ] **Refine `AudioEngine` Processing Logic:**
  - [ ] Implement process ordering for nodes based on connection graph.
  - [ ] Ensure efficient buffer transfer between nodes.
  - [ ] Add validation of buffer formats between connected nodes.
  - [ ] Create a robust callback system for ASIO processing as shown in `sample_project.cpp`.

## VI. OSC Integration

- [X] **Define OSC Communication Infrastructure:**
  - [X] Implement OSC message handling for filter parameter control as shown in `sample_project.cpp`.
  - [X] Create a thread-safe messaging system between OSC and audio processing threads.
  - [X] Add support for dynamic filter parameter updates via OSC messages.
  - [X] Implement bidirectional OSC communication with RME devices.
- [ ] **Implement OSC Message Handlers:**
  - [X] Create handlers for various message types (parameter updates, device control, etc.).
  - [ ] Add support for OSC address pattern matching and dispatching.
  - [ ] Implement error handling for malformed messages.
  - [ ] Create a message queue system for thread-safe communication.
- [ ] **Implement OSC Listener Thread:**
  - [ ] Setup UDP socket and binding as outlined in `osc_listener_thread_func` in `sample_project.cpp`.
  - [ ] Create message parsing and dispatch loop.
  - [ ] Add proper thread lifecycle management (start/stop/join).
  - [ ] Implement clean resource handling with socket cleanup on thread exit.
- [ ] **Enhance RME OSC Communication:**
  - [X] Implement message formatters for RME-specific commands.
  - [ ] Add support for sending messages to control RME device parameters.
  - [ ] Implement bi-directional parameter synchronization.
  - [ ] Create a periodic ping/monitoring system for RME connection status.

## VII. Testing & Refinement

- [ ] **Unit Testing:**
  - [ ] Create tests for individual components (filters, buffer management, configuration).
  - [ ] Add validation tests for parsing and serialization.
  - [ ] Test OSC message handling with simulated messages as shown in `sample_project.cpp`.
- [ ] **Integration Testing:**
  - [X] Test OSC communication with RME devices.
  - [ ] Test audio throughput with various node configurations.
  - [ ] Test file reading/writing with different formats and settings.
  - [ ] Test dynamic filter updates during audio processing.
- [ ] **Performance Optimization:**
  - [X] Implement Intel oneAPI optimizations.
  - [ ] Profile and optimize critical paths in audio processing chain.
  - [ ] Test for real-time performance under various loads.
  - [ ] Optimize buffer management and reduce unnecessary conversions.
- [X] **Documentation:**
  - [X] Update architecture documentation with FFmpeg integration.
  - [X] Create/update MERMAID diagrams for visualization.
  - [X] Document JSON configuration format and options.
  - [X] Update README with build instructions and dependencies.

## VIII. Future Enhancements

- [ ] **GUI Development:**
  - [ ] Design a graphical interface for configuration management.
  - [ ] Add real-time visualization of signal flow and levels.
  - [ ] Create configuration editor with parameter validation.
- [ ] **Additional Processing Nodes:**
  - [ ] Implement specialized DSP nodes for common tasks.
  - [ ] Add support for VST/VST3 plugins.
  - [ ] Create nodes for network audio streaming.
- [ ] **Enhanced Device Control:**
  - [ ] Support for additional RME device models/features.
  - [ ] Add support for other OSC-controllable devices.
  - [ ] Create device discovery and auto-configuration.
- [ ] **Web Interface Improvements:**
  - [ ] Enhance the existing web interface components.
  - [ ] Add remote configuration and monitoring capabilities.
  - [ ] Create RESTful API for external control.

## Implementation Notes from Sample Project

### AsioManager Implementation

The `sample_project.cpp` provides a useful reference for the `AsioManager` implementation with:

- Channel discovery via `findChannelIndices`
- Buffer creation and management for specific channels
- Callback system for audio processing
- Error handling and proper cleanup

### FFmpeg Filter Implementation

The `FfmpegFilter` class in `sample_project.cpp` demonstrates:

- Dynamic filter parameter updates via OSC
- Filter chain configuration using `avfilter_graph_parse_ptr`
- Proper thread-safe parameter update queuing
- Format conversion between interleaved and planar formats
- Error handling and resource management

### OSC Integration

The OSC integration in `sample_project.cpp` provides:

- Thread-safe communication between OSC and audio threads
- Message handling with proper type checking
- Dynamic parameter updates to running filters
- Bidirectional communication with RME devices

### Main Application Structure

The main application loop in `sample_project.cpp` shows:

- Clean signal handling for graceful shutdown
- Proper thread management and joining
- Resource cleanup in the correct order
- Error handling throughout the lifecycle

# TODO: Modular C++ Audio Engine Implementation

This document outlines the steps required to implement the standalone C++ modular audio engine architecture, as detailed in the `audio_engine_diagram_v3` diagram.

**Context:** This plan focuses on building a **new, standalone C++ application**.

## I. Project Setup & Dependencies

- [ ] **Create New C++ Project Structure:** (CMake recommended)
  - Directories: `engine/core`, `engine/nodes`, `engine/managers`, `engine/utils`, `osc/client`, `osc/server`, `config`, `tests`, `docs`.
- [ ] **Integrate Dependencies (CMake):**
  - [ ] ASIO SDK (Headers/Libs, provide path via CMake variable).
  - [ ] FFmpeg Dev Libs (Headers/Libs for `avutil`, `avformat`, `avcodec`, `avfilter`, `swresample`, find via CMake).
  - [ ] C++ OSC Library (e.g., `oscpack`, find/integrate via CMake).
  - [ ] C++ Configuration Library (e.g., `nlohmann/json`, find/integrate via CMake).

## II. Core Data Structures & Utilities

- [ ] **Implement `AudioBuffer` Structure/Class:** (`engine/core/audio_buffer.h/.cpp`)
  - [ ] Define core attributes (`data` pointers, `frames`, `sampleRate`, `format`, `channelLayout`). Use `AVSampleFormat` and `AVChannelLayout`.
  - [ ] Decide on memory ownership: raw pointers managed by pool OR internal storage (e.g., `std::vector<uint8_t>`) OR wrap `AVFrame`.
  - [ ] Implement move constructor/assignment operator. Disable copy constructor/assignment.
  - [ ] Add helper methods (e.g., `allocateData`, `freeData`, `getChannelCount`, `getBytesPerSample`, `isPlanar`).
  - [ ] (Optional) Consider reference counting if buffers might be shared across threads temporarily.
- [ ] **Implement `AudioBufferPool` Class:** (`engine/core/audio_buffer_pool.h/.cpp`)
  - [ ] Implement constructor: Takes max buffers, frames per buffer, channel count, format, layout; Pre-allocates `AudioBuffer` objects and their underlying data memory.
  - [ ] Implement `acquireBuffer()`: Returns a pointer (ideally smart pointer like `std::unique_ptr` with custom deleter) to an available buffer. Must be thread-safe.
  - [ ] Implement `releaseBuffer()`: Marks a buffer as available. Takes the buffer pointer/object. Must be thread-safe.
  - [ ] Implement destructor: Frees all allocated buffers and memory.
  - [ ] Choose thread-safety mechanism (e.g., `std::mutex` protecting available buffer list/queue, or lock-free queue).
- [ ] **Define `Connection` Structure:** (`engine/core/connection.h`)
  - [ ] Define struct with `sourceNode` (`std::weak_ptr<AudioNode>`), `sourcePad` (`int`), `sinkNode` (`std::weak_ptr<AudioNode>`), `sinkPad` (`int`).

## III. Manager Implementation

- [ ] **Implement `AsioManager` Class:** (`engine/managers/asio_manager.h/.cpp`)
  - [ ] Implement all methods defined in `audio_engine_impl_v1`.
  - [ ] Replace ASIO SDK placeholders with actual calls.
  - [ ] Implement `getBufferPointers` correctly based on internal `m_bufferInfos` and channel mapping.
  - [ ] Add robust error handling and logging.
- [ ] **Implement `RmeOscClient` Class:** (`osc/client/rme_osc_client.h/.cpp`)
  - [ ] Implement `configure`, `sendCommand`, `setMatrixCrosspointGain`, etc.
  - [ ] Integrate chosen C++ OSC library for sending UDP packets.
  - [ ] Add error handling for socket operations.
- [ ] **Implement `OscServer` Class:** (`osc/server/osc_server.h/.cpp`)
  - [ ] Implement `configure`, `startListening`, `stopListening`, `cleanup`.
  - [ ] Implement listener thread using chosen C++ OSC library (socket binding, receiving, parsing).
  - [ ] Implement message dispatcher (e.g., using `std::map<std::string, std::function<void(const std::vector<std::any>&)>>` for callbacks). Needs thread-safe way to register/unregister handlers if done dynamically.

## IV. Audio Node Implementation

- [ ] **Define `AudioNode` Base Class:** (`engine/nodes/audio_node.h`)
  - [ ] Finalize pure virtual interface (`configure`, `start`, `process`, `stop`, `cleanup`).
  - [ ] Refine `getOutputBuffer`/`setInputBuffer` interface (consider buffer ownership transfer, use `std::optional<AudioBuffer>` for return, take `AudioBuffer&&` for input).
  - [ ] Add virtual methods for querying input/output pad count and properties (e.g., expected format).
- [ ] **Implement `AsioSourceNode`:** (`engine/nodes/asio_source_node.h/.cpp`)
  - [ ] Implement `configure`: Parse "channels" param, validate indices, store `m_asioChannelIndices`. Allocate internal `m_outputBuffer` (or acquire from pool). Setup `SwrContext` if format conversion needed.
  - [ ] Implement `receiveAsioData`: Get raw pointers via `AsioManager::getBufferPointers`, perform conversion using `SwrContext` (if needed) into `m_outputBuffer.data`. Lock `m_bufferMutex`.
  - [ ] Implement `getOutputBuffer`: Lock `m_bufferMutex`, move `m_outputBuffer` content into a new buffer (acquired from pool?) and return it. Handle case where buffer wasn't ready.
  - [ ] Implement `cleanup`: Free `SwrContext`.
- [ ] **Implement `AsioSinkNode`:** (`engine/nodes/asio_sink_node.h/.cpp`)
  - [ ] Implement `configure`: Parse "channels" param, validate indices, store `m_asioChannelIndices`. Allocate internal conversion buffers if needed. Setup `SwrContext`.
  - [ ] Implement `setInputBuffer`: Lock `m_bufferMutex`, move received buffer into `m_inputBuffer`.
  - [ ] Implement `provideAsioData`: Lock `m_bufferMutex`, check if `m_inputBuffer` has data. If yes, convert using `SwrContext` (if needed) and copy result into the raw ASIO buffers provided via `AsioManager::getBufferPointers`. Release `m_inputBuffer` back to pool. If no data, write silence to ASIO buffers.
  - [ ] Implement `cleanup`: Free `SwrContext`.
- [ ] **Implement `FfmpegProcessorNode`:** (`engine/nodes/ffmpeg_processor_node.h/.cpp`)
  - [ ] Implement `configure`: Parse "chain" param, call `m_ffmpegFilter->initGraph`. Allocate internal buffers.
  - [ ] Implement `setInputBuffer`: Move buffer to `m_inputBuffer`. Lock `m_processMutex`.
  - [ ] Implement `process`: Lock `m_processMutex`. If `m_inputBuffer` exists, call `m_ffmpegFilter->process` using internal buffers. Store result in `m_outputBuffer`. Release input buffer.
  - [ ] Implement `getOutputBuffer`: Lock `m_processMutex`. If `m_outputBuffer` exists, move it to return value.
  - [ ] Implement `updateParameter`: Forward call to `m_ffmpegFilter->updateFilterParameter`.
  - [ ] Implement `cleanup`.
- [ ] **Implement `FileSourceNode`:** (`engine/nodes/file_source_node.h/.cpp`)
  - [ ] Implement `configure`: Get path, setup FFmpeg contexts (`avformat_open_input`, etc.).
  - [ ] Implement `start`: Launch `readerThreadFunc`.
  - [ ] Implement `readerThreadFunc`: Loop, read/decode/resample, push `AudioBuffer` (acquired from pool) to thread-safe output queue.
  - [ ] Implement `getOutputBuffer`: Pop `AudioBuffer` from queue (non-blocking). Return buffer to pool if moved out.
  - [ ] Implement `stop`/`cleanup`: Signal thread, join, free contexts.
  - [ ] Implement thread-safe queue.
- [ ] **Implement `FileSinkNode`:** (`engine/nodes/file_sink_node.h/.cpp`)
  - [ ] Implement `configure`: Get path/params, setup FFmpeg contexts (`avformat_alloc_output_context2`, `avcodec_find_encoder`, etc.), open file, write header.
  - [ ] Implement `start`: Launch `writerThreadFunc`.
  - [ ] Implement `setInputBuffer`: Move `AudioBuffer` to thread-safe input queue.
  - [ ] Implement `writerThreadFunc`: Loop, pop `AudioBuffer`, resample/encode/write, release buffer to pool.
  - [ ] Implement `stop`/`cleanup`: Signal thread, flush encoder, write trailer, join, free contexts.
  - [ ] Implement thread-safe queue.

## V. Audio Engine Core Logic

- [ ] **Implement `AudioEngine::initialize`:**
  - [ ] Instantiate `ConfigurationParser`, parse config.
  - [ ] Instantiate `RmeOscClient` and `OscServer` if configured. Configure them.
  - [ ] Call `createAndConfigureNodes`.
  - [ ] Call `setupConnections`.
  - [ ] Call `setupAsio` if needed (must happen *after* nodes are configured to know required channels).
  - [ ] Call `sendRmeCommands` if `RmeOscClient` active.
  - [ ] Start `OscServer` listener thread if active.
- [ ] **Implement `AudioEngine::createAndConfigureNodes`:**
  - [ ] Loop through `m_config.nodes`, create `std::shared_ptr<AudioNode>`, call `node->configure`, store in `m_nodes` and `m_nodeMap`.
- [ ] **Implement `AudioEngine::setupConnections`:**
  - [ ] Loop through `m_config.connections`, find nodes in `m_nodeMap`, create `Connection` structs, store in `m_connections`.
  - [ ] Implement `determineProcessingOrder`: Perform topological sort on nodes based on `m_connections` to create `m_processingOrder`. Handle cycles (error).
- [ ] **Implement `AudioEngine::processAsioBlock`:**
  - [ ] Notify relevant `AsioSourceNode`s via `receiveAsioData`.
  - [ ] Iterate through `m_processingOrder`:
    - For PROCESSOR nodes: Call `node->process()`.
    - For connections `A -> B`: `auto buffer = A->getOutputBuffer(); if(buffer) B->setInputBuffer(std::move(*buffer));` (Requires careful buffer ownership/pooling).
  - [ ] Notify relevant `AsioSinkNode`s via `provideAsioData`.
  - [ ] Handle buffer acquisition/release from pool.
- [ ] **Implement `AudioEngine::runFileProcessingLoop`:**
  - [ ] Similar graph processing logic as `processAsioBlock`, but driven by file node readiness and potentially running in its own thread.
- [ ] **Implement `AudioEngine::run`, `stop`, `cleanup`:** Orchestrate starting/stopping ASIO, file threads, OSC server, and cleaning up all resources.

## VI. Supporting Component Implementation

- [ ] **Implement `ConfigurationParser`:** (`config/parser.h/.cpp`)
  - [ ] Implement parsing logic for chosen format (e.g., JSON using `nlohmann/json`).
  - [ ] Add validation for configuration structure and parameters.

## VII. GUI Integration Hooks

- [ ] **Define & Implement `EngineAPI`:** (`engine/core/engine_api.h`)
  - [ ] Define interface methods in `AudioEngine`.
  - [ ] Implement thread-safe access to engine state for queries.
  - [ ] Implement mechanism for pushing updates (levels, status) to registered GUI callbacks/queues.

## VIII. Testing & Refinement

- [ ] **Unit Testing:** Add tests for config parsing, buffer pool, individual node configuration/processing logic.
- [ ] **Integration Testing:** Create test configurations for ASIO loopback, file->file, ASIO->file, file->ASIO, multi-node chains.
- [ ] **OSC Testing:** Use standard OSC tools to test `OscServer` receiving and verify `RmeOscClient` sending (monitor RME TotalMix FX).
- [ ] **Performance Profiling & Memory Checking.**
- [ ] **Documentation:** Doxygen comments, `docs/` folder updates.
