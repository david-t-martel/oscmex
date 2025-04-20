# Building the AudioEngine Project with CMake

This document provides instructions on how to build the AudioEngine project using CMake, with a focus on performance optimization using Intel oneAPI.

## Prerequisites

1. **CMake:** Version 3.15 or higher. Download from [cmake.org](https://cmake.org/download/).
2. **C++ Compiler:** A C++17 compliant compiler (e.g., GCC, Clang, MSVC, Intel oneAPI C++/C Compilers).
3. **Intel oneAPI Base Toolkit (Recommended for Performance):** Provides TBB, IPP, and MKL integration for significantly enhanced performance. Download from [Intel oneAPI](https://www.intel.com/content/www/us/en/developer/tools/oneapi/base-toolkit-download.html).
4. **Doxygen (Optional):** Required only if you want to build the documentation (`BUILD_DOCUMENTATION=ON`).

## Project Configuration

The project now includes the following components:

- **FFmpeg:** The project uses the included FFmpeg source code in `src/ffmpeg-master` directory. No external FFmpeg installation is needed.
- **ASIO SDK:** Used for audio device integration.
- **liblo:** Included for OSC (Open Sound Control) communication.
- **Intel oneAPI Components:** Optional but recommended for performance optimization.

## Configuration Steps

It's recommended to perform an out-of-source build.

1. **Create a Build Directory:**
    Open a terminal or command prompt in the project's root directory (`c:\codedev\auricleinc\oscmex`) and run:

    ```bash
    mkdir build
    cd build
    ```

2. **Setup Intel oneAPI Environment (Recommended):**

    For optimal performance, set up the Intel oneAPI environment before running CMake:

    **On Windows:**
    ```cmd
    "C:\Program Files (x86)\Intel\oneAPI\setvars.bat" intel64 vs2022
    ```

    **On Linux/macOS:**
    ```bash
    source /opt/intel/oneapi/setvars.sh
    ```

3. **Run CMake Configuration:**
    From the `build` directory, run `cmake` pointing to the parent directory (where the main `CMakeLists.txt` is located).

    **Basic Configuration (using default system compiler):**
    ```bash
    cmake ..
    ```

    **Intel oneAPI Optimized Configuration (Recommended):**
    ```bash
    cmake .. -DCMAKE_C_COMPILER=icx -DCMAKE_CXX_COMPILER=icx -DUSE_INTEL_COMPILER=ON -DUSE_INTEL_TBB=ON -DUSE_INTEL_IPP=ON -DUSE_INTEL_MKL=ON
    ```

    **Windows with Visual Studio Generator and Intel Compilers:**
    ```cmd
    cmake .. -G "Ninja" -DCMAKE_C_COMPILER=icx -DCMAKE_CXX_COMPILER=icx -DUSE_INTEL_COMPILER=ON
    ```

    Or using Visual Studio generator:
    ```cmd
    cmake .. -G "Visual Studio 17 2022" -T "Intel C++ Compiler 19.2" -DUSE_INTEL_COMPILER=ON
    ```

    **Enabling Optional Components:**
    Add these to your `cmake` command line as needed:
    * `-DBUILD_LO_TOOLS=ON` (Builds `oscsend`, `oscdump`, etc.)
    * `-DBUILD_LO_EXAMPLES=ON` (Builds examples)
    * `-DBUILD_DOCUMENTATION=ON` (Builds documentation using Doxygen)
    * `-DVECTORIZATION_REPORT=ON` (Generates Intel compiler vectorization reports for performance tuning)

    **Full-Featured Configuration with All Optimizations:**
    ```bash
    cmake .. -DCMAKE_C_COMPILER=icx -DCMAKE_CXX_COMPILER=icx -DUSE_INTEL_COMPILER=ON -DUSE_INTEL_TBB=ON -DUSE_INTEL_IPP=ON -DUSE_INTEL_MKL=ON -DBUILD_LO_TOOLS=ON -DBUILD_LO_EXAMPLES=ON -DBUILD_DOCUMENTATION=ON
    ```

## Building the Project

After successful configuration, build the project from the `build` directory:

```bash
cmake --build . --config Release
```

For multi-threaded builds to speed up compilation:

```bash
cmake --build . --config Release -j8  # Use 8 parallel jobs
```

## FFmpeg Integration

The project now uses the FFmpeg source code included in the `src/ffmpeg-master` directory. This provides several advantages:

1. **No external dependencies:** No need to install FFmpeg separately
2. **Consistent API:** All developers will use the same FFmpeg version
3. **Custom optimization:** The FFmpeg libraries are built with the same optimization flags as the rest of the project

The FFmpeg components used are:
- `libavutil`: Common utilities
- `libavformat`: Container format handling
- `libavcodec`: Codec handling
- `libavfilter`: Audio filtering (used for real-time effect processing)
- `libswresample`: Audio resampling and conversion
- `libswscale`: Image scaling and conversion

The `FfmpegFilter` and `FfmpegProcessorNode` classes provide high-level interfaces to FFmpeg's powerful audio filtering capabilities.

## Debugging with Intel oneAPI

For debugging with Intel compilers, build with the Debug configuration:

```bash
cmake --build . --config Debug
```

To generate performance diagnostic reports when using Intel compilers:

```bash
# Configure with vectorization reporting
cmake .. -DCMAKE_C_COMPILER=icx -DCMAKE_CXX_COMPILER=icx -DVECTORIZATION_REPORT=ON
cmake --build . --config Release

# The vectorization reports will be generated in the build directory
```

## VS Code Integration

This project includes pre-configured VS Code settings for seamless development:

1. Install the "C/C++" and "CMake Tools" extensions in VS Code
2. Open the project folder in VS Code
3. Select the Intel oneAPI kit when prompted by CMake Tools
4. Use the CMake Tools buttons in the status bar to configure, build, and debug

## Output Files

The build output will be placed within the `build` directory:

* **Executables:** `build/bin/` (e.g., `AudioEngineApp.exe`, `oscsend.exe`)
* **Libraries:** `build/lib/` (e.g., `AudioEngine.dll`/`.so`, `liblo.a`/`.lib`)
* **Examples (if built):** `build/bin/examples/`
* **Documentation (if built):** `build/doc/html/`

## Performance Considerations

When using Intel oneAPI components:

* The Intel compiler automatically optimizes for the current CPU architecture (`-xHost` flag)
* TBB provides optimized multi-threading capabilities
* IPP accelerates signal processing operations
* MKL provides highly optimized math operations, especially for FFT and matrix calculations

For best performance, ensure the oneAPI environment is properly set up before building, and use the Release configuration for production builds.

## Troubleshooting

**Intel Compiler Not Found:**
Ensure the oneAPI environment is properly set up by running `setvars.bat` or `setvars.sh` before launching your IDE or build tools.

**Build Errors with Intel Compiler:**
If you encounter build errors with Intel compilers, try falling back to standard compilers but keeping the oneAPI libraries:

```bash
cmake .. -DUSE_INTEL_TBB=ON -DUSE_INTEL_IPP=ON -DUSE_INTEL_MKL=ON
```

**FFmpeg Build Issues:**
If you encounter issues with the FFmpeg build:
1. Check that the FFmpeg source directory exists at `src/ffmpeg-master`
2. Ensure the `config.h` and `avconfig.h` files are correctly set up
3. For specific codec issues, you may need to adjust the configuration in `src/ffmpeg-master/config.h`

**TBB/IPP/MKL Not Found:**
Verify that the ONEAPI_ROOT environment variable is correctly set and that the components are installed. You can check the CMake configuration output for component detection messages.
