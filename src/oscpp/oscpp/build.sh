#!/bin/bash
# OSCPP Build Script for macOS/Linux
# =================================
# This script builds the OSCPP library with CMake
# Usage: ./build.sh [clean] [debug|release] [examples|no-examples] [tests|no-tests] [install] [verbose] [preset=name]

set -e # Exit on error

# Parse command line arguments
CLEAN=0
BUILD_TYPE="Release"
BUILD_EXAMPLES="ON"
BUILD_TESTS="ON"
INSTALL="OFF"
VERBOSE="OFF"
PLATFORM="x86_64"
USE_PRESET=0
PRESET_NAME=""

# Process arguments
for arg in "$@"; do
    case $arg in
    clean)
        CLEAN=1
        ;;
    debug)
        BUILD_TYPE="Debug"
        ;;
    release)
        BUILD_TYPE="Release"
        ;;
    examples)
        BUILD_EXAMPLES="ON"
        ;;
    no-examples)
        BUILD_EXAMPLES="OFF"
        ;;
    tests)
        BUILD_TESTS="ON"
        ;;
    no-tests)
        BUILD_TESTS="OFF"
        ;;
    install)
        INSTALL="ON"
        ;;
    verbose)
        VERBOSE="ON"
        ;;
    arm64)
        PLATFORM="arm64"
        ;;
    x86_64)
        PLATFORM="x86_64"
        ;;
    preset=*)
        USE_PRESET=1
        PRESET_NAME="${arg#preset=}"
        ;;
    *)
        echo "Unknown option: $arg"
        echo "Usage: ./build.sh [clean] [debug|release] [examples|no-examples] [tests|no-tests] [install] [verbose] [x86_64|arm64] [preset=name]"
        exit 1
        ;;
    esac
done

# Check if CMake is installed
if ! command -v cmake &>/dev/null; then
    echo "Error: CMake not found. Please install CMake and ensure it's in your PATH."
    exit 1
fi

# Check for presets support (CMake 3.20+)
CMAKE_VERSION=$(cmake --version | head -n1 | sed 's/cmake version //')
CMAKE_MAJOR=$(echo $CMAKE_VERSION | cut -d. -f1)
CMAKE_MINOR=$(echo $CMAKE_VERSION | cut -d. -f2)
CMAKE_SUPPORTS_PRESETS=0

if [ "$CMAKE_MAJOR" -gt 3 ] || ([ "$CMAKE_MAJOR" -eq 3 ] && [ "$CMAKE_MINOR" -ge 20 ]); then
    CMAKE_SUPPORTS_PRESETS=1
fi

# Check if presets file exists
PRESETS_FILE_EXISTS=0
if [ -f "CMakePresets.json" ]; then
    PRESETS_FILE_EXISTS=1
fi

# Detect OS
if [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macOS"
    TOOLCHAIN_FILE="cmake/macos.cmake"
    if [ "$PLATFORM" == "arm64" ]; then
        CMAKE_EXTRA_ARGS="-DCMAKE_OSX_ARCHITECTURES=arm64"
    elif [ "$(uname -m)" == "arm64" ] && [ "$PLATFORM" == "x86_64" ]; then
        # Building Intel binary on Apple Silicon
        CMAKE_EXTRA_ARGS="-DCMAKE_OSX_ARCHITECTURES=x86_64"
    fi
else
    OS="Linux"
    TOOLCHAIN_FILE="cmake/linux.cmake"
fi

# Determine if we should use presets
USING_PRESETS=0
if [ $USE_PRESET -eq 1 ]; then
    if [ $CMAKE_SUPPORTS_PRESETS -eq 0 ]; then
        echo "Error: CMake presets require CMake 3.20 or newer. Your version does not support presets."
        exit 1
    fi
    if [ $PRESETS_FILE_EXISTS -eq 0 ]; then
        echo "Error: CMakePresets.json not found in current directory."
        exit 1
    fi
    USING_PRESETS=1
elif [ $CMAKE_SUPPORTS_PRESETS -eq 1 ] && [ $PRESETS_FILE_EXISTS -eq 1 ]; then
    # Auto-select preset based on OS and build type
    if [[ "$OSTYPE" == "darwin"* ]]; then
        if [ "$BUILD_TYPE" == "Debug" ]; then
            PRESET_NAME="macos-debug"
            USING_PRESETS=1
        else
            PRESET_NAME="macos-release"
            USING_PRESETS=1
        fi
    else
        if [ "$BUILD_TYPE" == "Debug" ]; then
            PRESET_NAME="linux-debug"
            USING_PRESETS=1
        else
            PRESET_NAME="linux-release"
            USING_PRESETS=1
        fi
    fi
fi

# Display build information
echo "========================================"
echo "OSCPP Library Build Script for $OS"
echo "========================================"
echo "Build configuration:"
if [ $USING_PRESETS -eq 1 ]; then
    echo "  Using CMake preset: $PRESET_NAME"
else
    echo "  Build type: $BUILD_TYPE"
    echo "  Build examples: $BUILD_EXAMPLES"
    echo "  Build tests: $BUILD_TESTS"
    echo "  Install: $INSTALL"
    echo "  Platform: $PLATFORM"
fi
echo "========================================"

# Create build directory
mkdir -p build

# Clean if requested
if [ $CLEAN -eq 1 ]; then
    echo "Cleaning build directory..."
    if [ $USING_PRESETS -eq 1 ]; then
        case $PRESET_NAME in
        macos-debug)
            rm -rf build/macos-debug/CMakeCache.txt build/macos-debug/CMakeFiles
            ;;
        macos-release)
            rm -rf build/macos-release/CMakeCache.txt build/macos-release/CMakeFiles
            ;;
        linux-debug)
            rm -rf build/linux-debug/CMakeCache.txt build/linux-debug/CMakeFiles
            ;;
        linux-release)
            rm -rf build/linux-release/CMakeCache.txt build/linux-release/CMakeFiles
            ;;
        *)
            echo "Unknown preset: $PRESET_NAME"
            echo "Performing general clean..."
            rm -rf build/CMakeCache.txt build/CMakeFiles
            ;;
        esac
    else
        rm -rf build/CMakeCache.txt build/CMakeFiles
    fi
fi

# Configure and build using either presets or traditional CMake
if [ $USING_PRESETS -eq 1 ]; then
    echo "Using CMake preset: $PRESET_NAME"

    # Configure
    echo "Configuring with preset..."
    cmake --preset $PRESET_NAME

    # Build
    echo "Building with preset..."
    if [ "$VERBOSE" = "ON" ]; then
        cmake --build --preset $PRESET_NAME --verbose
    else
        cmake --build --preset $PRESET_NAME
    fi

    # Run tests if built
    if [ "$BUILD_TESTS" = "ON" ]; then
        echo "Running tests..."
        ctest --preset $PRESET_NAME || {
            echo "Warning: Some tests failed."
        }
    fi

    # Install if requested (not supported directly with presets)
    if [ "$INSTALL" = "ON" ]; then
        echo "Installing..."
        # Determine the build directory from preset name
        case $PRESET_NAME in
        macos-debug)
            cmake --install build/macos-debug --config Debug
            ;;
        macos-release)
            cmake --install build/macos-release --config Release
            ;;
        linux-debug)
            cmake --install build/linux-debug --config Debug
            ;;
        linux-release)
            cmake --install build/linux-release --config Release
            ;;
        *)
            echo "Unknown preset for installation. Please install manually."
            exit 1
            ;;
        esac
        echo "Installation complete. Files are in 'install' directory."
    fi
else
    # Traditional CMake workflow
    cd build

    # Configure CMake
    echo "Configuring CMake..."
    CMAKE_ARGS=".."

    # Add toolchain file
    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=../$TOOLCHAIN_FILE"

    # Add other args
    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_BUILD_TYPE=$BUILD_TYPE"
    CMAKE_ARGS="$CMAKE_ARGS -DOSCPP_BUILD_EXAMPLES=$BUILD_EXAMPLES"
    CMAKE_ARGS="$CMAKE_ARGS -DOSCPP_BUILD_TESTS=$BUILD_TESTS"
    CMAKE_ARGS="$CMAKE_ARGS -DOSCPP_INSTALL=$INSTALL"
    CMAKE_ARGS="$CMAKE_ARGS $CMAKE_EXTRA_ARGS"

    # Add installation path if installing
    if [ "$INSTALL" = "ON" ]; then
        CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=../install"
    fi

    # Configure
    echo "Running: cmake $CMAKE_ARGS"
    cmake $CMAKE_ARGS

    # Build
    echo "Building $BUILD_TYPE configuration..."
    if [ "$VERBOSE" = "ON" ]; then
        cmake --build . --config $BUILD_TYPE --verbose
    else
        cmake --build . --config $BUILD_TYPE
    fi

    # Run tests if built
    if [ "$BUILD_TESTS" = "ON" ]; then
        echo "Running tests..."
        ctest -C $BUILD_TYPE --output-on-failure || {
            echo "Warning: Some tests failed."
        }
    fi

    # Install if requested
    if [ "$INSTALL" = "ON" ]; then
        echo "Installing..."
        cmake --install . --config $BUILD_TYPE
        echo "Installation complete. Files are in 'install' directory."
    fi

    cd ..
fi

echo "========================================"
echo "Build completed successfully!"
echo "========================================"
echo "You can find:"

if [ $USING_PRESETS -eq 1 ]; then
    case $PRESET_NAME in
    macos-debug)
        echo "Using preset: macos-debug"
        echo "- Library files in build/macos-debug/lib"
        if [ "$BUILD_EXAMPLES" = "ON" ]; then
            echo "- Example executables in build/macos-debug/bin"
        fi
        if [ "$BUILD_TESTS" = "ON" ]; then
            echo "- Test executables in build/macos-debug/bin/tests"
        fi
        ;;
    macos-release)
        echo "Using preset: macos-release"
        echo "- Library files in build/macos-release/lib"
        if [ "$BUILD_EXAMPLES" = "ON" ]; then
            echo "- Example executables in build/macos-release/bin"
        fi
        if [ "$BUILD_TESTS" = "ON" ]; then
            echo "- Test executables in build/macos-release/bin/tests"
        fi
        ;;
    linux-debug)
        echo "Using preset: linux-debug"
        echo "- Library files in build/linux-debug/lib"
        if [ "$BUILD_EXAMPLES" = "ON" ]; then
            echo "- Example executables in build/linux-debug/bin"
        fi
        if [ "$BUILD_TESTS" = "ON" ]; then
            echo "- Test executables in build/linux-debug/bin/tests"
        fi
        ;;
    linux-release)
        echo "Using preset: linux-release"
        echo "- Library files in build/linux-release/lib"
        if [ "$BUILD_EXAMPLES" = "ON" ]; then
            echo "- Example executables in build/linux-release/bin"
        fi
        if [ "$BUILD_TESTS" = "ON" ]; then
            echo "- Test executables in build/linux-release/bin/tests"
        fi
        ;;
    esac
else
    echo "- Library files in build/lib"
    if [ "$BUILD_EXAMPLES" = "ON" ]; then
        echo "- Example executables in build/bin"
    fi
    if [ "$BUILD_TESTS" = "ON" ]; then
        echo "- Test executables in build/bin/tests"
    fi
fi

if [ "$INSTALL" = "ON" ]; then
    echo "- Installed files in install"
fi
echo "========================================"

exit 0
