# OSCMix Cross-Platform TODO List

This document outlines the changes needed to make OSCMix fully cross-platform compatible across Windows, macOS, and Linux.

## Build System

1. **Create a CMake-based build system**

```cmake
cmake_minimum_required(VERSION 3.10)
project(oscmix C)

# Set platform-specific libraries
if(WIN32)
    set(PLATFORM_LIBS ws2_32 winmm)
    add_definitions(-D_WIN32_WINNT=0x0601)
elseif(APPLE)
    set(PLATFORM_LIBS pthread)
else()
    set(PLATFORM_LIBS pthread rt)
endif()

add_executable(oscmix
    src/oscmix/oscmix.c
    src/oscmix/osc.c
    src/oscmix/socket.c
    src/oscmix/sysex.c
    src/oscmix/intpack.c
    src/oscmix/device.c
    src/oscmix/util.c
    $<IF:$<BOOL:${WIN32}>,src/oscmix/main_old.c,src/oscmix/main_unix.c>
)
target_link_libraries(oscmix m ${PLATFORM_LIBS})
```

## Code Refactoring

2. **Create a unified main.c file**

Merge `main_old.c` (Windows) and `main_unix.c` (POSIX) into a single file with conditional compilation:

```c
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
    #include <winsock2.h>
    #include <mmsystem.h>
    // Windows-specific code here
#else
    #include <fcntl.h>
    #include <pthread.h>
    #include <signal.h>
    #include <sys/time.h>
    #include <unistd.h>
    // Unix-specific code here
#endif

// Common code below
```

3. **Abstract MIDI I/O operations**

Create a platform-independent interface for MIDI operations:

```c
// Add to oscmix.h
#ifdef __cplusplus
extern "C" {
#endif

typedef struct MidiDevice MidiDevice;

MidiDevice* midi_open(const char* device_name);
void midi_close(MidiDevice* device);
int midi_send(MidiDevice* device, const void* data, size_t length);
int midi_set_callback(MidiDevice* device, void (*callback)(const unsigned char*, size_t));

#ifdef __cplusplus
}
#endif
```

4. **Improve error handling and logging**

Add standardized logging macros:

```c
#define LOG_ERROR(fmt, ...) fprintf(stderr, "ERROR: " fmt "\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) if(dflag) fprintf(stderr, "INFO: " fmt "\n", ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) if(dflag > 1) fprintf(stderr, "DEBUG: " fmt "\n", ##__VA_ARGS__)
```

5. **Add cross-platform socket initialization**

Create a common networking initialization function:

```c
int init_networking(void) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        LOG_ERROR("WSAStartup failed");
        return -1;
    }
#endif
    return 0;
}

void cleanup_networking(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}
```

6. **Add proper shutdown handling**

Implement consistent signal handling across platforms:

```c
void handle_signal(int sig) {
    LOG_INFO("Received signal %d, cleaning up...", sig);
    // Close sockets, MIDI devices, free memory
    exit(0);
}

// In main():
#ifdef _WIN32
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)handle_signal, TRUE);
#else
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
#endif
```

## Platform-Specific Implementations

7. **Windows**
   - Use the Windows Multimedia API for MIDI I/O
   - Use WinSock2 for networking

8. **macOS**
   - Use CoreMIDI for MIDI I/O
   - Use POSIX sockets for networking

9. **Linux**
   - Use ALSA for MIDI I/O
   - Use POSIX sockets for networking

## Testing

10. **Test on all platforms**
    - Verify connectivity to RME devices
    - Ensure parameter control works reliably
    - Check performance and stability
    - Validate error handling
