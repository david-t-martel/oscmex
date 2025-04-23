cmake_minimum_required(VERSION 3.10)
project(oscmix C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Enable verbose output for debugging if needed
# set(CMAKE_VERBOSE_MAKEFILE ON)

# Set global compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# Detect platform and set platform-specific libraries
if(WIN32)
    message(STATUS "Configuring for Windows")
    set(PLATFORM_LIBS ws2_32 winmm)
    add_definitions(-D_WIN32_WINNT=0x0601)
elseif(APPLE)
    message(STATUS "Configuring for macOS")
    set(PLATFORM_LIBS pthread)

    # MacOS uses CoreMIDI for MIDI support
    find_library(COREMIDI_LIBRARY CoreMIDI)

    if(COREMIDI_LIBRARY)
        list(APPEND PLATFORM_LIBS ${COREMIDI_LIBRARY})
        add_definitions(-DHAVE_COREMIDI)
    endif()
else()
    message(STATUS "Configuring for Linux/Unix")
    set(PLATFORM_LIBS pthread rt)

    # Check for ALSA (for Linux MIDI support)
    find_package(ALSA)

    if(ALSA_FOUND)
        include_directories(${ALSA_INCLUDE_DIRS})
        list(APPEND PLATFORM_LIBS ${ALSA_LIBRARIES})
        add_definitions(-DHAVE_ALSA)
    endif()
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Common source files for all platforms
set(COMMON_SOURCES
    oscmix.c
    osc.c
    socket.c
    sysex.c
    intpack.c
    device.c
    util.c
)

# Use the unified main.c file for all platforms
add_executable(oscmix
    main.c
    ${COMMON_SOURCES}
)

# Link with platform-specific libraries
target_link_libraries(oscmix m ${PLATFORM_LIBS})

# Installation rules
install(TARGETS oscmix DESTINATION bin)

# Testing
enable_testing()

# Add a simple test to check if the application starts
add_test(NAME oscmix_start
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/oscmix -h)

# Show summary of configuration
message(STATUS "")
message(STATUS "OSCMix Configuration Summary:")
message(STATUS "-----------------------------")
message(STATUS "Platform libraries: ${PLATFORM_LIBS}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "-----------------------------")
message(STATUS "")
