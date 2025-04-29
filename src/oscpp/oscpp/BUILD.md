# Building the OSCPP Library

This document explains how to build the OSCPP library from source, including cross-compilation options using Intel oneAPI.

## Prerequisites

- CMake 3.14 or higher
- A compatible C++ compiler (supporting C++17)
- Ninja build system (recommended)
- Intel oneAPI Toolkit (for Intel compiler and cross-compilation)

## Building with Visual Studio Code

The project includes pre-configured VS Code tasks for building with various configurations and targets.

### Setup Intel oneAPI Environment

Before building, make sure the Intel oneAPI environment is set up:

1. Open VS Code in the project directory
2. Run the "Intel oneAPI Environment Setup" task

### Using CMake Presets (Recommended)

The project includes a set of CMake presets for easy configuration:

1. **Open the Command Palette** (`Ctrl+Shift+P`)
2. Type `CMake: Select Configure Preset` and select a preset:
   - `default` - Default Intel oneAPI configuration (recommended)
   - `intel-oneapi-debug` - Intel compiler with debug settings
   - `intel-oneapi-release` - Intel compiler with release settings
   - `intel-cross-linux-release` - Cross-compile for Linux
   - `intel-cross-arm64-release` - Cross-compile for ARM64
   - `intel-cross-macos-release` - Cross-compile for macOS

3. **Build using the preset**:
   - Open the Command Palette again
   - Type `CMake: Select Build Preset` and select a build preset matching your configure preset
   - Click the "Build" button in the CMake Tools bar or run `CMake: Build` from the command palette

This is the easiest and recommended way to build the project, as it automatically handles environment setup and compiler configuration.

### Building with VS Code Tasks (Alternative)

You can also use the pre-defined VS Code tasks:

1. **Debug Build:**
   - Run the "CMake Configure (Intel oneAPI)" task
   - Then run the "CMake Build (Intel oneAPI)" task
   - Output binaries will be in the `build_intel/bin` directory

2. **Release Build:**
   - Run the "CMake Build Release (Intel oneAPI)" task
   - Output binaries will be in the `build_intel/bin` directory

3. **Build Examples:**
   - Run the "Build Examples (Intel oneAPI)" task
   - Example binaries will be in the `build_intel/bin` directory

4. **Run Tests:**
   - Run the "Run Tests (Intel oneAPI)" task
   - Test results will be displayed in the terminal

5. **Clean Build:**
   - Run the "Clean Build (Intel oneAPI)" task to remove the build directory

6. **Generate Documentation:**
   - Run the "Generate Documentation" task
   - Documentation will be generated in the `html` directory

### Cross-Compilation with Intel oneAPI

The project supports cross-compilation for Linux, ARM64, and macOS platforms using Intel oneAPI.

#### Using CMake Presets for Cross-Compilation

1. Open the Command Palette (`Ctrl+Shift+P`)
2. Type `CMake: Select Configure Preset` and select:
   - `intel-cross-linux-release` for Linux
   - `intel-cross-arm64-release` for ARM64
   - `intel-cross-macos-release` for macOS
3. Type `CMake: Build` to build for the selected platform

#### Using VS Code Tasks for Cross-Compilation

##### Linux (x86_64)

1. **Configure and Build:**
   - Run the "Cross-Compile Configure (Linux x86_64)" task
   - Then run the "Cross-Compile Build (Linux x86_64)" task
   - Output binaries will be in the `build_linux_x64/bin` directory

2. **Build Examples:**
   - Run the "Cross-Compile Examples (Linux x86_64)" task
   - Example binaries will be in the `build_linux_x64/bin` directory

##### ARM64 (aarch64)

1. **Configure and Build:**
   - Run the "Cross-Compile Configure (ARM64)" task
   - Then run the "Cross-Compile Build (ARM64)" task
   - Output binaries will be in the `build_arm64/bin` directory

2. **Build Examples:**
   - Run the "Cross-Compile Examples (ARM64)" task
   - Example binaries will be in the `build_arm64/bin` directory

##### macOS

1. **Configure and Build:**
   - Run the "Cross-Compile Configure (macOS)" task
   - Then run the "Cross-Compile Build (macOS)" task
   - Output binaries will be in the `build_macos/bin` directory

2. **Build Examples:**
   - Run the "Cross-Compile Examples (macOS)" task
   - Example binaries will be in the `build_macos/bin` directory

## Building with Command Line

### Using the Build Scripts

The project includes batch scripts for building on Windows:

#### Windows (Host Platform)

```batch
# Set up Intel oneAPI environment
"C:\Program Files (x86)\Intel\oneAPI\setvars.bat"

# Build with Intel oneAPI compiler
build_intel.bat
```

#### Cross-Compilation for All Platforms

```batch
# To build for all platforms
build_cross.bat

# To build for a specific platform (windows, linux, arm64, macos)
build_cross.bat linux

# To build for a specific platform with a specific build type
build_cross.bat arm64 Debug
```

### Using CMake Presets from Command Line

CMake presets can also be used from the command line:

```batch
# Configure and build using the default preset
cmake --preset default
cmake --build --preset default

# Configure and build for Windows using Intel oneAPI
cmake --preset intel-oneapi-release
cmake --build --preset intel-oneapi-release

# Configure and build for Linux using Intel oneAPI cross-compilation
cmake --preset intel-cross-linux-release
cmake --build --preset intel-cross-linux-release

# Configure and build for ARM64 using Intel oneAPI cross-compilation
cmake --preset intel-cross-arm64-release
cmake --build --preset intel-cross-arm64-release

# Configure and build for macOS using Intel oneAPI cross-compilation
cmake --preset intel-cross-macos-release
cmake --build --preset intel-cross-macos-release
```

### Using CMake Directly

You can also use CMake directly for more control:

#### Windows (Host Platform)

```batch
cmake -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=cmake/intel_oneapi.cmake -DCMAKE_BUILD_TYPE=Release -B build_intel .
cmake --build build_intel --config Release
```

#### Linux Cross-Compilation

```batch
cmake -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=cmake/intel_oneapi_cross_linux.cmake -DCMAKE_BUILD_TYPE=Release -B build_linux_x64 .
cmake --build build_linux_x64 --config Release
```

#### ARM64 Cross-Compilation

```batch
cmake -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=cmake/intel_oneapi_cross_arm64.cmake -DCMAKE_BUILD_TYPE=Release -B build_arm64 .
cmake --build build_arm64 --config Release
```

#### macOS Cross-Compilation

```batch
cmake -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=cmake/intel_oneapi_cross_macos.cmake -DCMAKE_BUILD_TYPE=Release -B build_macos .
cmake --build build_macos --config Release
```

## Build Options

The following CMake options can be used to customize the build:

| Option                | Description                   | Default |
| --------------------- | ----------------------------- | ------- |
| OSCPP_BUILD_EXAMPLES  | Build example applications    | ON      |
| OSCPP_BUILD_TESTS     | Build test applications       | ON      |
| OSCPP_INSTALL         | Generate installation target  | ON      |
| OSCPP_CROSS_COMPILE   | Enable cross-compilation mode | OFF     |
| OSCPP_CREATE_PACKAGE  | Create installable package    | OFF     |
| OSCPP_EXTRA_C_FLAGS   | Extra flags for C compiler    | ""      |
| OSCPP_EXTRA_CXX_FLAGS | Extra flags for C++ compiler  | ""      |

Example:

```batch
cmake -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=cmake/intel_oneapi.cmake -DCMAKE_BUILD_TYPE=Release -DOSCPP_BUILD_EXAMPLES=OFF -B build_intel .
```

## Packaging Binaries

To create installable packages for each platform, see [INSTALL.md](INSTALL.md).
