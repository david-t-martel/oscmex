# TODO: Modular C++ Audio Engine Implementation (Detailed Plan v2)

This document provides a detailed breakdown of tasks required to implement the standalone C++ modular audio engine and associated OSC components, using **liblo** and its C++ wrapper (`liblo++`). Architecture details in `docs/audio_engine_diagram_v3.md` (needs update).

**Context:** Build a standalone C++ `AudioEngine` application and potentially separate, reusable C++ libraries/tools for OSC communication (`OscClient`, `OscServer`) based on `liblo`. Assume ASIO SDK source, FFmpeg source (master branch), and Nlohmann JSON header are included directly in the project repository (e.g., under `vendor/`).

---

## Phase I: Project Foundation & Core Dependencies

### 1. Project Structure & Build System (CMake)

    -   [ ] **Initialize Git Repository:** If not already done.
    -   [ ] **Create Directory Structure:**
        -   `src/` (Contains subdirs for libraries and app)
        -   `src/lib_osc_client/`
        -   `src/lib_osc_server/`
        -   `src/lib_audio_engine_core/` (Engine, Nodes, Managers, Utils)
        -   `src/app_audio_engine/` (Main executable source)
        -   `src/tool_osc_client/` (Optional standalone tool source)
        -   `vendor/` (For included dependencies)
        -   `vendor/asio_sdk/` (Copy ASIO SDK source here)
        -   `vendor/ffmpeg/` (Copy FFmpeg source here)
        -   `vendor/nlohmann/` (Copy `json.hpp` here)
        -   `vendor/liblo/` (Copy liblo source here, including C++ wrapper)
        -   `config/` (Example configuration files)
        -   `tests/` (Unit and integration tests)
        -   `docs/` (Architecture diagrams, API docs, etc.)
        -   `build/` (CMake build output directory - add to `.gitignore`)
    -   [ ] **Create Root `CMakeLists.txt`:**
        -   Set `CMAKE_CXX_STANDARD 17` and `CMAKE_CXX_STANDARD_REQUIRED ON`.
        -   Define project name and version.
        -   Add `vendor/` subdirectories using `add_subdirectory()`.
        -   Add `src/` subdirectories using `add_subdirectory()`.
        -   Add `tests/` subdirectory (optional, enable via CMake option).
    -   [ ] **Configure ASIO SDK Build (in `vendor/asio_sdk/CMakeLists.txt` if needed, or root):**
        -   Set up target(s) for ASIO library based on its build system (might require platform checks for Windows COM/DirectX dependencies). Name the target clearly (e.g., `asio_lib`).
    -   [ ] **Configure FFmpeg Build (in `vendor/ffmpeg/CMakeLists.txt` or root):**
        -   Use FFmpeg's configure script mechanism or its (experimental) CMake support if available in the master branch version used.
        -   **Crucially:** Disable unnecessary components (`--disable-programs`, `--disable-ffplay`, `--disable-ffprobe`, `--disable-network`, `--disable-everything`, then selectively enable required components).
        -   **Enable:** `libavutil`, `libavformat`, `libavcodec`, `libavfilter`, `libswresample`. Enable required demuxers (e.g., wav, flac), decoders (e.g., pcm, flac), muxers, encoders, filters (e.g., equalizer, acompressor, aformat), protocols (e.g., file).
        -   Define FFmpeg library targets (e.g., `avutil`, `avformat`, etc.).
    -   [ ] **Configure liblo Build (in `vendor/liblo/CMakeLists.txt` or root):**
        -   Set up targets for C core (`liblo`) and C++ wrapper (`liblo++` if separate). Ensure C++ wrapper is enabled. Link `pthread` on POSIX.
    -   [ ] **Configure Nlohmann JSON:**
        -   Define an `INTERFACE` library target for the header, setting its include directory.
    -   [ ] **Define Project Library Targets:**
        -   `lib_osc_client`: Links against `liblo++` (or `liblo`).
        -   `lib_osc_server`: Links against `liblo++` (or `liblo`).
        -   `lib_audio_engine_core`: Links against `asio_lib`, FFmpeg libs (`avutil`, etc.), potentially `lib_osc_client`/`lib_osc_server` if tightly coupled (or link later in app).
    -   [ ] **Define Project Executable Targets:**
        -   `app_audio_engine`: Links against `lib_audio_engine_core`, `lib_osc_client`, `lib_osc_server`, Nlohmann JSON target.
        -   `tool_osc_client` (Optional): Links against `lib_osc_client`.
        -   `tests`: Links against core/node libraries and a testing framework (e.g., Google Test, Catch2).
    -   [ ] **Set Include Directories** correctly for all targets.

## II. OSC Component Implementation (liblo++)

- [ ] **Implement `OscClient` Library (`lib_osc_client`):**
  - [ ] Define `OscClient` class (`osc/client/osc_client.h/.cpp`). Include `<lo/lo++.h>`.
  - [ ] `configure(target_url)`: Create and store `lo::Address m_target;`. Handle potential `lo::AddressError`.
  - [ ] `sendCommand(address, args)`:
    - Create `lo::Message msg;`.
    - Implement robust `std::any` to `lo::Message::add()` type mapping (float 'f', int32 'i', string 's', bool 'T'/'F', blob 'b', double 'd', int64 'h', char 'c', etc.). Handle errors for unsupported types in `args`.
    - Call `m_target.send(address.c_str(), msg);`. Check return value for errors (`-1`).
  - [ ] Add RME helper methods (e.g., `sendRmeMatrixGain(in, out, gain_db)`).
- [ ] **Implement `OscServer` Library (`lib_osc_server`):**
  - [ ] Define `OscServer` class (`osc/server/osc_server.h/.cpp`). Include `<lo/lo++.h>`.
  - [ ] Define callback type: `using OscCallback = std::function<void(const std::string&, const std::vector<std::any>&)>;`
  - [ ] Member variables: `std::unique_ptr<lo::ServerThread> m_serverThread;`, `std::map<std::string, OscCallback> m_callbacks;`, `std::mutex m_callbackMutex;`, `std::atomic<bool> m_isRunning;`, `int m_listenPort;`.
  - [ ] `configure(port)`: Store port.
  - [ ] `startListening()`:
    - Check if already running.
    - Create `m_serverThread = std::make_unique<lo::ServerThread>(m_listenPort);`. Handle `lo::ServerError`.
    - Register a default internal handler using `m_serverThread->add_method(nullptr, nullptr, internal_static_handler, this);`. This catches all messages if specific ones aren't registered.
    - Call `m_serverThread->start();`. Check for errors. Set `m_isRunning = true;`.
  - [ ] Implement `internal_static_handler(path, types, argv, argc, msg, user_data)`:
    - Cast `user_data` to `OscServer*`.
    - Parse `path`, `types`, `argv`/`argc` or `msg` into `std::string address` and `std::vector<std::any> args`.
    - Call `this->dispatchMessage(address, args);`.
  - [ ] Implement `registerCallback(address_pattern, callback)`:
    - Lock `m_callbackMutex`.
    - Store `callback` in `m_callbacks[address_pattern]`.
    - *Alternative:* Directly use `m_serverThread->add_method(address_pattern.c_str(), type_string, internal_static_handler_for_specific_path, this);` if dynamic registration isn't needed after start. This leverages liblo's matching.
    - Unlock mutex.
  - [ ] Implement `dispatchMessage(address, args)`:
    - Lock `m_callbackMutex`.
    - Find best matching callback in `m_callbacks` (requires implementing matching logic if not using `add_method` per pattern).
    - Unlock mutex.
    - If found, call the callback.
  - [ ] Implement `stopListening()`: Set `m_isRunning = false;`, call `m_serverThread->stop();` (destructor handles join). Reset `m_serverThread`.
  - [ ] Implement `cleanup()`: Call `stopListening`.

## III. Core Audio Engine Implementation (`lib_audio_engine_core`)

- [ ] **Implement `AudioBuffer` & `AudioBufferPool`:**
  - [ ] Finalize `AudioBuffer` implementation (e.g., AVFrame wrapper with RAII).
  - [ ] Implement `AudioBufferPool` with acquire/release using smart pointers and chosen thread-safety mechanism.
- [ ] **Implement `AsioManager` Class:**
  - [ ] Implement fully using ASIO SDK source. Pay attention to Windows specifics (COM, HWND for `asioInit`).
  - [ ] Implement channel index mapping in `createBuffers` and buffer retrieval in `getBufferPointers`.
- [ ] **Implement `AudioNode` Base & Derived Classes:**
  - [ ] **Base:** Finalize interface. Add `getSampleRate()`, `getBufferSize()`, `getInternalFormat()`, `getInternalLayout()` accessors.
  - [ ] **`AsioSourceNode`:** Implement fully. `configure` needs to get actual ASIO format/layout from `AsioManager` to setup `SwrContext` correctly if needed. `receiveAsioData` performs the conversion into an `AudioBuffer` acquired from the pool. `getOutputBuffer` moves the prepared buffer.
  - [ ] **`AsioSinkNode`:** Implement fully. `configure` gets ASIO format/layout. `setInputBuffer` takes ownership of buffer. `provideAsioData` converts buffer content (if needed) and copies to raw ASIO pointers. Releases buffer back to pool.
  - [ ] **`FfmpegProcessorNode`:** Implement fully. `configure` parses chain, initializes `FfmpegFilter`. `setInputBuffer`/`getOutputBuffer` manage buffer ownership (acquire from/release to pool). `process` executes filter graph. `updateParameter` forwards to `FfmpegFilter`.
  - [ ] **`FileSourceNode`:** Implement fully. `configure` sets up FFmpeg contexts. `readerThreadFunc` loop reads/decodes/resamples into buffers acquired from pool, pushes to queue. `getOutputBuffer` pops from queue. `stop`/`cleanup` manage thread and contexts. Implement `ThreadSafeQueue<AudioBuffer>`.
  - [ ] **`FileSinkNode`:** Implement fully. `configure` sets up FFmpeg contexts/file. `writerThreadFunc` loop pops from queue, resamples/encodes/writes, releases buffer to pool. `stop`/`cleanup` manage thread, flush, trailer, contexts. Implement `ThreadSafeQueue<AudioBuffer>`.
- [ ] **Implement `AudioEngine` Class:**
  - [ ] Implement core methods.
  - [ ] `initialize`: Parse config. Instantiate managers/nodes based on config presence. Register OSC callbacks if `OscServer` active. Send RME commands if `OscClient` active.
  - [ ] `determineProcessingOrder`: Implement topological sort based on `m_connections`. Store result in `m_processingOrder`. Detect cycles.
  - [ ] `processAsioBlock`/`runFileProcessingLoop`: Implement graph execution: Iterate `m_processingOrder`. For each node, check dependencies based on `m_connections`. Acquire input buffers (from preceding nodes' `getOutputBuffer`), call `node->setInputBuffer`, then call `node->process()`. Handle buffer acquisition/release from pool.
  - [ ] Implement thread-safe methods for `EngineAPI`.
  - [ ] Implement OSC handler methods (e.g., `handleEngineParamUpdate`) registered with `OscServer`. These methods need to interact with nodes thread-safely (e.g., call `FfmpegProcessorNode::updateParameter`).

## IV. Supporting Component Implementation

- [ ] **Implement `ConfigurationParser`:** (`config/parser.h/.cpp`)
  - [ ] Use Nlohmann JSON. Implement loading from file/string.
  - [ ] Add validation logic using JSON schema or manual checks. Validate node types, parameters, connection names, OSC settings.
- [ ] **Implement Thread-Safe Queue:** (`engine/utils/thread_safe_queue.h`)
  - [ ] Implement template class with blocking push/try_pop/wait_pop.

## V. Main Application (`app_audio_engine`)

- [ ] **Implement `main.cpp`:**
  - [ ] Handle CLI args (`--config`).
  - [ ] Instantiate/run/cleanup `AudioEngine`.
  - [ ] Implement main run loop waiting for `g_shutdown_requested`.

## VI. GUI Integration Hooks

- [ ] **Define & Implement `EngineAPI`:**
  - [ ] Define clear C++ interface in `AudioEngine`.
  - [ ] Implement thread-safe methods.
- [ ] **Implement Data Push/Callback:**
  - [ ] Implement mechanism (e.g., queue polled by GUI thread, or direct callbacks with mutexes) for level meters/status.

## VII. Testing & Refinement

- [ ] **Unit Testing (CTest + Google Test/Catch2):**
  - [ ] Test `OscClient`/`OscServer` with loopback connection.
  - [ ] Test `ConfigurationParser` extensively.
  - [ ] Test `AudioBufferPool`.
  - [ ] Test individual node `configure` and basic `process` logic (mocking dependencies).
- [ ] **Integration Testing:**
  - [ ] Test `app_audio_engine` with various `config.json` files covering different node graphs and OSC configurations.
  - [ ] Verify audio output/files for correctness.
  - [ ] Test OSC control responsiveness.
- [ ] **Performance & Memory:**
  - [ ] Profile `processAsioBlock` under load. Identify bottlenecks.
  - [ ] Use memory analysis tools. Ensure `AudioBufferPool` prevents leaks.
- [ ] **Documentation:**
  - [ ] Doxygen comments.
  - [ ] Create/update `docs/` files (config format, OSC APIs, RME details, build guide, architecture diagram).
  - [ ] Update `README.md`.
