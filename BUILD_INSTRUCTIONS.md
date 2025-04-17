# Building the AudioEngine Project with CMake

This document provides instructions on how to build the AudioEngine project using CMake.

## Prerequisites

1. **CMake:** Version 3.15 or higher. Download from [cmake.org](https://cmake.org/download/).
2. **C++ Compiler:** A C++17 compliant compiler (e.g., GCC, Clang, MSVC, Intel oneAPI C++/C Compilers).
3. **Intel oneAPI Base Toolkit (Optional but Recommended):** Required for TBB, IPP, and MKL integration. Ensure the environment is set up correctly (e.g., by running `setvars.bat` or `source setvars.sh`). Download from [Intel oneAPI](https://www.intel.com/content/www/us/en/developer/tools/oneapi/base-toolkit-download.html).
4. **Doxygen (Optional):** Required only if you want to build the documentation (`BUILD_DOCUMENTATION=ON`).

## Configuration Steps

It's recommended to perform an out-of-source build.

1. **Create a Build Directory:**
    Open a terminal or command prompt in the project's root directory (`c:\codedev\auricleinc\oscmex`) and run:

    ```bash
    mkdir build
    cd build
    ```

2. **Run CMake Configuration:**
    From the `build` directory, run `cmake` pointing to the parent directory (where the main `CMakeLists.txt` is located).

    **Basic Configuration (using default system compiler):**

    ```bash
    cmake ..
    ```

    **Configuration with Intel oneAPI Compilers (Recommended):**
    Ensure the oneAPI environment is active (`setvars.bat`/`setvars.sh`).

    ```bash
    cmake .. -DCMAKE_C_COMPILER=icx -DCMAKE_CXX_COMPILER=icpx
    ```

    *(On Windows with Visual Studio, you might need to specify the generator as well, e.g., `-G "Visual Studio 17 2022"`)*

    **Enabling Optional Components:**
    You can enable building tools, examples, and documentation using `-D` flags during configuration. Add these to your `cmake` command line as needed:
    * `-DBUILD_LO_TOOLS=ON` (Builds `oscsend`, `oscdump`, etc.)
    * `-DBUILD_LO_EXAMPLES=ON` (Builds examples like `cpp_example`, `example_server`, etc.)
    * `-DBUILD_DOCUMENTATION=ON` (Builds documentation using Doxygen)

    **Example Configuration Enabling Everything with Intel Compilers:**

    ```bash
    cmake .. -DCMAKE_C_COMPILER=icx -DCMAKE_CXX_COMPILER=icpx -DBUILD_LO_TOOLS=ON -DBUILD_LO_EXAMPLES=ON -DBUILD_DOCUMENTATION=ON
    ```

## Building the Project

After successful configuration, build the project from the `build` directory:

```bash
cmake --build .
```

Alternatively, on Linux/macOS, you can often use `make`:

```bash
make
```

Or open the generated solution/project file in your IDE (like Visual Studio or Xcode).

## Output Files

The build output will be placed within the `build` directory:

* **Executables:** `build/bin/` (e.g., `AudioEngineApp.exe`, `oscsend.exe`)
* **Libraries:** `build/lib/` (e.g., `AudioEngine.dll`/`.so`, `liblo.a`/`.lib`)
* **Examples (if built and installed):** `build/bin/examples/`
* **Documentation (if built):** `build/doc/html/`
