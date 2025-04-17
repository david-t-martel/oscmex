# AudioEngine / OSCMex Project

## Overview

This project combines a C++ audio processing library (`AudioEngine`) with the `liblo` C library for Open Sound Control (OSC) communication. The primary goal is to create a flexible audio engine controllable via OSC messages, potentially targeting specific hardware like RME audio interfaces (as suggested by configuration files like `rme_osc_config.md`).

The project includes the core `AudioEngine`, the underlying `liblo` library, command-line OSC tools, examples, and potentially a web-based interface.

## Architecture

*(Note: A detailed architecture document, potentially `src/AudioEngine/AudioEngine_ARCHITECTURE.md`, should be created to elaborate on the internal design of the `AudioEngine` library itself.)*

The project is structured as follows:

1. **`AudioEngine` (C++ Library - `src/AudioEngine`):** The core component responsible for audio processing tasks. It leverages Intel oneAPI libraries (TBB for parallelism, IPP for signal processing, MKL for math kernels) for high performance. It is designed to be controlled externally.
2. **`liblo` (C Library - `src/lo`):** A lightweight, cross-platform implementation of the Open Sound Control protocol. It handles the packing, unpacking, sending, and receiving of OSC messages over network protocols (UDP, TCP) or Unix sockets.
3. **OSC Control Layer:** `AudioEngine` exposes its functionality via an OSC interface, allowing external applications or controllers (like MATLAB scripts, dedicated control surfaces, or the included web interface) to manage its parameters and state. The specific OSC command set for controlling RME devices via an intermediary like `oscmix` is partially documented in `rme_osc_config.md`.
4. **Command-Line Tools (`src/tools`):** Utilities like `oscsend` and `oscdump` (built from `liblo` sources) for sending and monitoring OSC messages, useful for testing and debugging.
5. **Examples (`src/examples`):** Sample code demonstrating how to use the `liblo` library (both C and C++ APIs).
6. **Web Interface (`src/web`):** (Optional) A web-based frontend using HTML, CSS, and JavaScript (potentially compiled to WebAssembly via Emscripten) to provide a graphical user interface for controlling the `AudioEngine` via OSC over WebSockets or MIDI.
7. **Build System (CMake):** A unified CMake build system manages the compilation of all components across different platforms.

## Project Components / Directory Structure

* `./CMakeLists.txt`: The main CMake build script for the entire project.
* `./CMakePresets.json`: Predefined configurations for CMake (e.g., Debug/Release, Intel compilers).
* `./BUILD_INSTRUCTIONS.md`: Detailed steps for configuring and building the project using CMake.
* `./build/`: Default directory for build output (contains build scripts like `build_script_oneapi.bat`).
* `./src/`: Contains all source code.
  * `./src/AudioEngine/`: Source code for the C++ `AudioEngine` library.
  * `./src/lo/`: Source code for the `liblo` C library.
  * `./src/tools/`: Source code for command-line OSC tools.
  * `./src/examples/`: Source code for `liblo` examples.
  * `./src/web/`: Source code for the web interface (`index.html`, `oscmix.js`, etc.).
  * `./src/main.cpp`: Example main application entry point using the `AudioEngine` library.
* `./doc/`: Contains Doxygen configuration for generating API documentation.
* `./INSTALL`: Basic installation instructions (points to `BUILD_INSTRUCTIONS.md`).
* `./README.md`: This file.
* `./rme_osc_config.md`: Documentation on OSC commands for RME interfaces (via `oscmix`).
* `./LICENSE` or `COPYING`: *(Should contain the project's license - currently missing, likely LGPL based on liblo)*.
* `./AUTHORS`: *(Should list project authors - currently missing)*.

## Dependencies

* **CMake:** Version 3.15+ (for building).
* **C++ Compiler:** C++17 compliant (GCC, Clang, MSVC, Intel icpx).
* **C Compiler:** For `liblo` (GCC, Clang, MSVC, Intel icx).
* **Intel oneAPI Base Toolkit:** Recommended for optimal performance. Provides:
  * Intel Threading Building Blocks (TBB)
  * Intel Integrated Performance Primitives (IPP)
  * Intel Math Kernel Library (MKL)
* **Doxygen (Optional):** Required only to build documentation (`BUILD_DOCUMENTATION=ON`).
* **Ninja (Recommended):** A fast build system often used with CMake.

## Building

Please refer to **`BUILD_INSTRUCTIONS.md`** for detailed steps on how to configure and build the project using CMake.

Key CMake options include:

* `CMAKE_BUILD_TYPE`: `Debug` or `Release`.
* `CMAKE_C_COMPILER` / `CMAKE_CXX_COMPILER`: Specify compilers (e.g., `icx`/`icpx`).
* `BUILD_LO_TOOLS`: Build command-line tools (Default: ON).
* `BUILD_LO_EXAMPLES`: Build examples (Default: ON).
* `BUILD_DOCUMENTATION`: Build Doxygen documentation (Default: ON).

A build script (`build/build_script_oneapi.bat`) is provided as an example for building with Intel oneAPI on Windows.

## Usage / Control

The `AudioEngine` is designed to be controlled via OSC messages. Refer to the `AudioEngine`'s specific documentation (once created) and potentially `rme_osc_config.md` for details on the OSC address space and expected arguments.

The command-line tools (`oscsend`, `oscdump`) or the web interface (`src/web/index.html`) can be used for interaction.

## License

*(The license file `COPYING` or `LICENSE` needs to be added. The core `liblo` library is licensed under the LGPL v2.1 or later. The license for the `AudioEngine` component should be clarified.)*

## Authors & Contributing

*(The `AUTHORS` file needs to be added, listing original `liblo` authors and contributors to the `AudioEngine` project.)*

Contributions are welcome. Please follow standard coding practices and consider adding tests for new functionality.
