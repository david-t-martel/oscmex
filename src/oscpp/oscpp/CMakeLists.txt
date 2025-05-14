# CMakeLists.txt for the oscpp Library

cmake_minimum_required(VERSION 3.14)

# Set up vcpkg integration if available
if(DEFINED ENV{VCPKG_ROOT})
    set(VCPKG_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

    if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE ${VCPKG_TOOLCHAIN_FILE})
    endif()
endif()

# Set the project name and version
project(oscpp
    VERSION 1.0.0
    DESCRIPTION "Open Sound Control C++ Library"
    HOMEPAGE_URL "https://github.com/auricleinc/oscmex"
    LANGUAGES CXX)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(OSCPP_BUILD_EXAMPLES "Build example applications" ON)
option(OSCPP_BUILD_TESTS "Build test applications" ON)
option(OSCPP_INSTALL "Generate installation target" ON)
option(OSCPP_CROSS_COMPILE "Enable cross-compilation mode" OFF)
option(OSCPP_CREATE_PACKAGE "Create installable package" OFF)

# Allow developers to override/add compile flags
set(OSCPP_EXTRA_C_FLAGS "" CACHE STRING "Extra flags for C compiler")
set(OSCPP_EXTRA_CXX_FLAGS "" CACHE STRING "Extra flags for C++ compiler")

# Detect target platform
if(DEFINED OSCPP_PLATFORM)
    message(STATUS "Target platform explicitly set to: ${OSCPP_PLATFORM}")
else()
    if(WIN32)
        set(OSCPP_PLATFORM "WINDOWS")
    elseif(APPLE)
        set(OSCPP_PLATFORM "MACOS")
    elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
        set(OSCPP_PLATFORM "LINUX")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        set(OSCPP_PLATFORM "ARM64")
    else()
        message(WARNING "Unknown platform, defaulting to generic Unix-like system")
        set(OSCPP_PLATFORM "UNIX")
    endif()
endif()

# Detect Intel oneAPI compiler and set specific flags
if(CMAKE_CXX_COMPILER_ID MATCHES "Intel" OR CMAKE_CXX_COMPILER_ID MATCHES "IntelLLVM")
    message(STATUS "Intel oneAPI compiler detected for ${OSCPP_PLATFORM}")

    # Set Intel-specific flags
    if(OSCPP_PLATFORM STREQUAL "WINDOWS")
        add_definitions(-DUSE_INTEL_COMPILER -DINTEL_ONEAPI_WINDOWS)
        set(INTEL_SPECIFIC_FLAGS "-Qopenmp")
    elseif(OSCPP_PLATFORM STREQUAL "LINUX" OR OSCPP_PLATFORM STREQUAL "ARM64")
        add_definitions(-DUSE_INTEL_COMPILER -DINTEL_ONEAPI_LINUX)
        set(INTEL_SPECIFIC_FLAGS "-qopenmp")
    elseif(OSCPP_PLATFORM STREQUAL "MACOS")
        add_definitions(-DUSE_INTEL_COMPILER -DINTEL_ONEAPI_MACOS)
        set(INTEL_SPECIFIC_FLAGS "-qopenmp")
    endif()

    # Add Intel-specific optimization flags
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${INTEL_SPECIFIC_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${INTEL_SPECIFIC_FLAGS}")

    # Set optimization level based on build type
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

    # Handle cross-compilation cases
    if(OSCPP_CROSS_COMPILE)
        message(STATUS "Cross-compilation mode enabled for ${OSCPP_PLATFORM}")
        add_definitions(-DCROSS_COMPILE)

        # Additional platform-specific cross-compilation flags
        if(OSCPP_PLATFORM STREQUAL "ARM64")
            add_compile_options(-march=armv8-a)
        endif()
    endif()
endif()

# Platform-specific settings
if(OSCPP_PLATFORM STREQUAL "WINDOWS")
    # Add Windows socket library
    set(PLATFORM_LIBS ws2_32)

    # Disable min/max macros from Windows headers
    add_definitions(-DNOMINMAX -D_WINSOCK_DEPRECATED_NO_WARNINGS)
elseif(OSCPP_PLATFORM STREQUAL "MACOS")
    # Apple-specific settings if needed
    set(PLATFORM_LIBS "")
    add_definitions(-DMACOS_TARGET)
elseif(OSCPP_PLATFORM STREQUAL "LINUX")
    # Linux-specific settings
    set(PLATFORM_LIBS pthread)
    add_definitions(-DLINUX_TARGET)
elseif(OSCPP_PLATFORM STREQUAL "ARM64")
    # ARM64-specific settings
    set(PLATFORM_LIBS pthread)
    add_definitions(-DARM64_TARGET)
else()
    # Generic Unix-like system
    set(PLATFORM_LIBS pthread)
endif()

# Include directories - properly handle the project structure
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Define library sources (more specific glob patterns)
file(GLOB_RECURSE SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# Define library headers
file(GLOB_RECURSE HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/osc/*.h"
)

# Create the library
add_library(oscpp ${SOURCES} ${HEADERS})
add_library(osc::oscpp ALIAS oscpp)

# Add platform-specific libraries
target_link_libraries(oscpp PRIVATE ${PLATFORM_LIBS})

# Specify the include directories for the library
target_include_directories(oscpp
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Add custom build flags if provided
if(OSCPP_EXTRA_C_FLAGS)
    target_compile_options(oscpp PRIVATE ${OSCPP_EXTRA_C_FLAGS})
endif()

if(OSCPP_EXTRA_CXX_FLAGS)
    target_compile_options(oscpp PRIVATE ${OSCPP_EXTRA_CXX_FLAGS})
endif()

# Define target properties
set_target_properties(oscpp PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME oscpp
    DEBUG_POSTFIX "d"
)

# Set correct output directories for all platforms
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Detect standalone test build
option(OSC_STANDALONE_TEST "Build main.cpp as a standalone test executable" OFF)

if(OSC_STANDALONE_TEST)
    add_executable(oscpp_test ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
    target_link_libraries(oscpp_test PRIVATE oscpp)
    target_compile_definitions(oscpp_test PRIVATE OSC_STANDALONE_TEST)
endif()

# Examples
if(OSCPP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
if(OSCPP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
if(OSCPP_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    # Install headers
    install(DIRECTORY include/osc
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    # Install library
    install(TARGETS oscpp
        EXPORT oscppTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    # Install platform-specific files (e.g., configs, docs, etc.)
    if(OSCPP_PLATFORM STREQUAL "WINDOWS")
        # Windows-specific installation
        install(FILES LICENSE README.md
            DESTINATION share/oscpp/docs)

    # Install dependency DLLs if needed
    # install(FILES ${DEPENDENCY_DLLS} DESTINATION ${CMAKE_INSTALL_BINDIR})
    elseif(OSCPP_PLATFORM STREQUAL "LINUX" OR OSCPP_PLATFORM STREQUAL "ARM64")
        # Linux/ARM-specific installation
        install(FILES LICENSE README.md
            DESTINATION share/doc/oscpp)

        # Install config files
        install(FILES
            config_osc/rme_osc_commands.json
            DESTINATION share/oscpp/config)

    elseif(OSCPP_PLATFORM STREQUAL "MACOS")
        # macOS-specific installation
        install(FILES LICENSE README.md
            DESTINATION share/doc/oscpp)

        # macOS bundle related files
        # install(FILES Resources/Info.plist DESTINATION ${CMAKE_INSTALL_PREFIX})
    endif()

    # Export the targets
    install(EXPORT oscppTargets
        FILE oscppTargets.cmake
        NAMESPACE osc::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/oscpp
    )

    # Configure the config file
    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/oscppConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/oscppConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/oscpp
    )

    # Create package version file
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/oscppConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    # Install config files
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/oscppConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/oscppConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/oscpp
    )

    # Add packaging configuration
    if(OSCPP_CREATE_PACKAGE)
        message(STATUS "Configuring packaging for ${OSCPP_PLATFORM}")
        include(cmake/packaging.cmake)
    endif()
endif()

# Add a custom target to build for all platforms (when using Intel oneAPI cross-compilation)
if(CMAKE_CXX_COMPILER_ID MATCHES "Intel" OR CMAKE_CXX_COMPILER_ID MATCHES "IntelLLVM")
    add_custom_target(build_all_platforms
        COMMENT "Building for all platforms using Intel oneAPI cross-compilation"
    )

    # This is just a placeholder target - the actual multi-platform build is handled
    # through CMake presets or the build_cross.bat script.
    add_dependencies(build_all_platforms oscpp)
endif()
