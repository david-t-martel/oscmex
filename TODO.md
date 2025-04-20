# TODO: Modular C++ Audio Engine Implementation

This document outlines the steps required to implement the modular C++ audio engine architecture, as detailed in the `audio_engine_diagram_v3` diagram.

**Important Context:** This plan describes the creation of a **new C++ application**. While it incorporates OSC concepts potentially shared with your MATLAB/MEX `oscmex` project ([`osc_rme.cpp`](c:\codedev\auricleinc\oscmex\src\tools\matlab\osc_rme.cpp), [`oscmex_test.m`](c:\codedev\auricleinc\oscmex\src\tools\matlab\oscmex_test.m)), it does not directly modify the `oscmex` codebase. The existing [`OscController`](c:\codedev\auricleinc\oscmex\src\AudioEngine\OscController.h) class implements some of the OSC functionalities described below. This C++ engine could potentially be controlled by or interact with MATLAB via MEX wrappers later, but the core engine itself is a separate C++ entity.

## I. Project Setup & Dependencies

- [ ] **Create C++ Project Structure:**
  - Set up a new C++ project using a build system like CMake.
  - Organize source files (.h/.cpp) logically (e.g., `src/core`, `src/nodes`, `src/managers`, `src/utils`).
- [ ] **Integrate Dependencies:**
  - **ASIO SDK:** Ensure headers are accessible to the compiler and link against any necessary libraries (if applicable). Add placeholders/wrappers if SDK cannot be directly included in a public repo.
  - **FFmpeg:** Include headers and link against required FFmpeg development libraries (`libavutil`, `libavformat`, `libavcodec`, `libavfilter`, `libswresample`). (Headers seem present in `c:\Program Files\ffmpeg\include`)
  - **C++ OSC Library:** Choose and integrate a library (e.g., `oscpack`, `liblo wrapper`). (`liblo` seems to be used in [`OscController.cpp`](c:\codedev\auricleinc\oscmex\src\AudioEngine\OscController.cpp)).
  - **C++ Configuration Library (Optional):** Choose and integrate a library for parsing config files (e.g., `nlohmann/json`, `yaml-cpp`, `toml++`) or implement robust command-line parsing.

## II. Core Component Implementation

- [ ] **Implement `AudioBuffer` Structure/Class:**
  - Define the `AudioBuffer` struct (as in `audio_engine_impl_v1`) or a more robust class.
  - Consider adding methods for allocation, copying, and potentially reference counting or integrating with a buffer pool.
  - Ensure it holds format, layout, sample rate, and frame count alongside data pointers.
- [ ] **Implement `AsioManager` Class:**
  - Adapt the detailed C++ implementation (`asio_ffmpeg_eq_code_v2`, `audio_engine_impl_v1`).
  - Replace ASIO SDK placeholders with actual SDK calls.
  - Modify `initDevice` to accept preferred sample rate/buffer size from `AudioEngine`.
  - Modify `createBuffers` to accept specific input/output channel index vectors provided by `AudioEngine`.
  - Implement `setCallback` to store the `AudioEngine`'s callback function pointer/lambda.
  - Implement `getBufferPointers` for `AudioEngine` to retrieve buffers during the callback.
  - Ensure robust error handling for all ASIO calls.
- [X] **Implement `OscController` Class:** (Partially implemented as [`OscController`](c:\codedev\auricleinc\oscmex\src\AudioEngine\OscController.h))
  - [X] Implement `configure` method to set target IP/Port for sending and listening port. (See [`OscController::configure`](c:\codedev\auricleinc\oscmex\src\AudioEngine\OscController.cpp?range=52-71) for sending, [`OscController::startReceiver`](c:\codedev\auricleinc\oscmex\src\AudioEngine\OscController.cpp?range=73-110) for receiving)
  - [X] Implement `sendCommand` using the chosen C++ OSC library (`liblo`) to format and send UDP packets. (See [`OscController::sendCommand`](c:\codedev\auricleinc\oscmex\src\AudioEngine\OscController.cpp?range=308-356), [`OscController::sendBatch`](c:\codedev\auricleinc\oscmex\src\AudioEngine\OscController.cpp?range=365-439))
  - [X] Implement listener thread (`OscListenThread`). (Handled by `lo_server_thread` within [`OscController::startReceiver`](c:\codedev\auricleinc\oscmex\src\AudioEngine\OscController.cpp?range=97-109))
  - [X] Use the chosen C++ OSC library (`liblo`) to bind a UDP socket and listen for packets in the thread. (Handled by `lo_server_thread_new` in [`OscController::startReceiver`](c:\codedev\auricleinc\oscmex\src\AudioEngine\OscController.cpp?range=81-95))
  - [X] Implement message parsing logic within the listener thread. (See [`OscController::handleOscMessage`](c:\codedev\auricleinc\oscmex\src\AudioEngine\OscController.cpp?range=128-239))
  - [X] Implement a dispatch mechanism (e.g., callback map, message queue to `AudioEngine`) to handle recognized OSC address patterns. (Uses `m_messageCallback` and `m_levelCallback` in [`OscController`](c:\codedev\auricleinc\oscmex\src\AudioEngine\OscController.h?range=397-398))
  - [X] Implement `startListening`, `stopListening`, `cleanup` methods. (See [`OscController::startReceiver`](c:\codedev\auricleinc\oscmex\src\AudioEngine\OscController.cpp?range=73-110), [`OscController::stopReceiver`](c:\codedev\auricleinc\oscmex\src\AudioEngine\OscController.cpp?range=111-120), [`OscController::cleanup`](c:\codedev\auricleinc\oscmex\src\AudioEngine\OscController.cpp?range=36-44))
  - [X] Implement RME-specific helper methods (e.g., `setMatrixCrosspointGain`) based on RME TotalMix FX OSC documentation, translating logical actions into specific OSC messages. (See [`OscController::setMatrixCrosspointGain`](c:\codedev\auricleinc\oscmex\src\AudioEngine\OscController.h?range=186-191), [`OscController::setChannelMute`](c:\codedev\auricleinc\oscmex\src\AudioEngine\OscController.h?range=199-209), etc.)
- [ ] **Implement `AudioEngine` Class:** (Basic structure/demonstration in [`main.cpp`](c:\codedev\auricleinc\oscmex\src\AudioEngine\main.cpp) and [`audioEngine_main.cpp`](c:\codedev\auricleinc\oscmex\src\AudioEngine\audioEngine_main.cpp))
  - Implement constructor/destructor, `initialize`, `run`, `stop`, `cleanup`.
  - Implement configuration parsing logic within `initialize` (using `ConfigurationParser`).
  - Implement node creation and storage (`m_nodes`, `m_nodeMap`).
  - Implement connection setup (`m_connections`, determine `m_processingOrder`).
  - Implement interaction with `AsioManager` (initialization, starting, stopping, setting callback).
  - [X] Implement interaction with `OscController` (sending setup commands, starting/stopping listener, potentially receiving dispatched commands). (See [`audioEngine_main.cpp`](c:\codedev\auricleinc\oscmex\src\AudioEngine\audioEngine_main.cpp?range=854-865) for sending, [`main.cpp`](c:\codedev\auricleinc\oscmex\src\AudioEngine\main.cpp?range=68-80) for receiving)
  - Implement `processAsioBlock` method (called by `AsioManager` callback).
  - Implement `runFileProcessingLoop` method (for non-ASIO mode).
  - **Crucially:** Implement the core logic within `processAsioBlock` / `runFileProcessingLoop` to manage data flow between connected nodes based on `m_connections` and `m_processingOrder`. This involves getting/setting buffers on nodes.
  - Implement basic buffer management strategy (or placeholder for future pool).
  - Implement thread management for file nodes.

## III. Audio Node Implementation

- [ ] **Define `AudioNode` Base Class:**
  - Finalize the pure virtual interface (`configure`, `start`, `process`, `stop`, `cleanup`, `getOutputBuffer`, `setInputBuffer`).
- [ ] **Implement `AsioSourceNode`:**
  - Implement `configure` (parse channel params, store indices).
  - Implement `receiveAsioData` (called by `AudioEngine`): Get raw buffers from `AsioManager`, convert to `m_internalFormat`/`m_internalLayout` (using `SwrContext` if needed), store in `m_outputBuffer`. Use mutex for thread safety.
  - Implement `getOutputBuffer`: Return data from `m_outputBuffer` (use mutex).
  - Implement `cleanup` (free `SwrContext`).
- [ ] **Implement `AsioSinkNode`:**
  - Implement `configure` (parse channel params, store indices).
  - Implement `setInputBuffer`: Store incoming `AudioBuffer` in `m_inputBuffer` (use mutex).
  - Implement `provideAsioData` (called by `AudioEngine`): Convert `m_inputBuffer` data to ASIO format (using `SwrContext` if needed), copy to raw ASIO buffers provided by `AsioManager`. Use mutex.
  - Implement `cleanup` (free `SwrContext`).
- [ ] **Implement `FfmpegProcessorNode`:**
  - Implement `configure` (parse `chain` param, call `m_ffmpegFilter->initGraph`).
  - Implement `setInputBuffer`: Store input buffer.
  - Implement `process`: Call `m_ffmpegFilter->process` using stored input/output buffers.
  - Implement `getOutputBuffer`: Return processed output buffer.
  - Implement `updateParameter`: Forward call to `m_ffmpegFilter->updateFilterParameter`.
  - Implement `cleanup`: Call `m_ffmpegFilter->cleanup`.
- [ ] **Implement `FileSourceNode`:**
  - Implement `configure` (get path, setup FFmpeg format/codec contexts).
  - Implement `start`: Launch `readerThreadFunc`.
  - Implement `readerThreadFunc`: Loop (`while m_running`), read (`av_read_frame`), decode (`avcodec_receive_frame`), resample (`swr_convert`), package into `AudioBuffer`, push to thread-safe output queue. Handle EOF.
  - Implement `getOutputBuffer`: Pop `AudioBuffer` from thread-safe queue (non-blocking or blocking).
  - Implement `stop`: Set `m_running` to false, join thread.
  - Implement `cleanup`: Clean up FFmpeg contexts, close file.
- [ ] **Implement `FileSinkNode`:**
  - Implement `configure` (get path, setup FFmpeg format/codec/muxer contexts, open file, write header).
  - Implement `start`: Launch `writerThreadFunc`.
  - Implement `setInputBuffer`: Push `AudioBuffer` to thread-safe input queue.
  - Implement `writerThreadFunc`: Loop (`while m_running or !queue.empty()`), pop `AudioBuffer`, resample (`swr_convert`), encode (`avcodec_send_frame`/`receive_packet`), write (`av_interleaved_write_frame`).
  - Implement `stop`: Signal thread to finish queue, flush encoder, write trailer (`av_write_trailer`), join thread.
  - Implement `cleanup`: Clean up FFmpeg contexts, close file.

## IV. Supporting Component Implementation

- [ ] **Implement `ConfigurationParser`:**
  - Choose config format (JSON recommended for structure).
  - Implement parsing logic using chosen library or manual parsing for CLI args.
  - Populate the `Configuration` struct robustly, including validation.
- [ ] **Implement Buffer Management (Basic -> Pool):**
  - Start with simple buffer allocation/deallocation within nodes or engine.
  - Refactor later to use a central `AudioBuffer` pool managed by `AudioEngine` for efficiency, especially in real-time paths.

## V. Integration & Workflow

- [ ] **Implement `main.cpp`:**
  - Instantiate core components (`ConfigurationParser`, `AudioEngine`).
  - Handle command-line arguments / config file loading.
  - Initialize `AudioEngine`.
  - Start `AudioEngine` (`run`).
  - Implement main loop (wait for shutdown signal).
  - Implement graceful shutdown (`stop`, `cleanup`).
- [ ] **Refine `AudioEngine` Processing Logic:**
  - Implement the graph traversal/data flow logic in `processAsioBlock` and `runFileProcessingLoop`. Ensure correct buffer handling between nodes.

## VI. GUI Integration Hooks

- [ ] **Define `EngineAPI`:** Specify the functions and data needed by a GUI (e.g., `getStatus()`, `getNodeParameters(nodeName)`, `updateNodeParameter(...)`, `getLevelMeterData()`, `registerStatusCallback(...)`).
- [ ] **Implement API Layer:** Add methods to `AudioEngine` corresponding to the API.
- [ ] **Implement Data Push/Callback:** Add thread-safe queues or callbacks within `AudioEngine` to push status updates and level meter data asynchronously to the GUI layer/API.

## VII. Testing & Refinement

- [ ] **Unit Testing:** Write tests for individual components where possible (e.g., `FfmpegFilter` parameter updates, configuration parsing).
- [ ] **Integration Testing:** Test common configurations (ASIO I/O, File I/O, combinations) with actual hardware and files.
- [ ] **OSC Testing:** Test sending commands (e.g., to RME) and receiving commands via the `OscController`.
- [ ] **Performance Profiling:** Profile real-time paths (`processAsioBlock`) and optimize critical sections. Check for audio dropouts under load.
- [ ] **Memory Checking:** Use tools (like Valgrind on Linux/macOS, Dr. Memory/AddressSanitizer on Windows) to check for memory leaks or errors.
- [ ] **Documentation:** Add Doxygen-style comments or other documentation for classes and methods.
