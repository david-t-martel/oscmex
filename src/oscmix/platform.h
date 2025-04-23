// platform.h
#ifndef PLATFORM_H
#define PLATFORM_H

/* Common includes for all platforms */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/* Platform detection and specific includes */
#if defined(_WIN32)
/* Windows platform */
#define PLATFORM_WINDOWS 1
#define _WIN32_WINNT 0x0601 /* Windows 7 or later */
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mmsystem.h>
#include <process.h>
#include <io.h>

/* Windows type definitions */
#ifdef _WIN64
typedef __int64 ssize_t;
#else
typedef int ssize_t;
#endif

/* Thread handling */
typedef HANDLE platform_thread_t;
typedef HANDLE platform_mutex_t;
typedef HANDLE platform_event_t;

/* Socket handling */
typedef SOCKET platform_socket_t;
#define PLATFORM_INVALID_SOCKET INVALID_SOCKET
#define PLATFORM_SOCKET_ERROR SOCKET_ERROR

/* File handling */
#define platform_mkdir(path) _mkdir(path)
#define platform_access(path, mode) _access(path, mode)

/* Path handling */
#define PLATFORM_PATH_SEPARATOR '\\'
#define PLATFORM_PATH_SEPARATOR_STR "\\"

/* Thread/sync functions */
#define platform_thread_create(t, f, a) ((*t = (platform_thread_t)_beginthreadex(NULL, 0, f, a, 0, NULL)) == 0)
#define platform_thread_join(t) WaitForSingleObject(t, INFINITE)
#define platform_sleep_ms(ms) Sleep(ms)

/* MIDI-specific functions */
typedef HMIDIIN platform_midiin_t;
typedef HMIDIOUT platform_midiout_t;

#else
/* POSIX platform (Linux, macOS, etc.) */
#define PLATFORM_POSIX 1
#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Thread handling */
typedef pthread_t platform_thread_t;
typedef pthread_mutex_t platform_mutex_t;
typedef pthread_cond_t platform_event_t;

/* Socket handling */
typedef int platform_socket_t;
#define PLATFORM_INVALID_SOCKET -1
#define PLATFORM_SOCKET_ERROR -1

/* File handling */
#define platform_mkdir(path) mkdir(path, 0755)
#define platform_access(path, mode) access(path, mode)

/* Path handling */
#define PLATFORM_PATH_SEPARATOR '/'
#define PLATFORM_PATH_SEPARATOR_STR "/"

/* Thread/sync functions */
#define platform_thread_create(t, f, a) pthread_create(t, NULL, f, a)
#define platform_thread_join(t) pthread_join(t, NULL)
#define platform_sleep_ms(ms) usleep((ms) * 1000)

/* MIDI-specific functions - not applicable for POSIX */
typedef int platform_midiin_t;
typedef int platform_midiout_t;
#endif

/* Common platform-independent functions */

/**
 * @brief Gets the application data directory path
 *
 * Returns the platform-specific directory where application data should be stored:
 * - Windows: %APPDATA% (e.g., C:\Users\username\AppData\Roaming)
 * - macOS: ~/Library/Application Support
 * - Linux: ~/.local/share
 *
 * @param buffer Buffer to receive the path
 * @param size Size of the buffer
 * @return 0 on success, -1 on failure
 */
int platform_get_app_data_dir(char *buffer, size_t size);

/**
 * @brief Ensures a directory exists, creating it if necessary
 *
 * Creates all directories in the path if they don't exist.
 *
 * @param path Directory path to create
 * @return 0 on success, -1 on failure
 */
int platform_ensure_directory(const char *path);

/**
 * @brief Gets the current time as a formatted string
 *
 * @param buffer Buffer to receive the time string
 * @param size Size of the buffer
 * @param format Format string (strftime format)
 * @return 0 on success, -1 on failure
 */
int platform_format_time(char *buffer, size_t size, const char *format);

// Socket functions
int platform_socket_init(void);
void platform_socket_cleanup(void);
platform_socket_t platform_socket_create(int domain, int type, int protocol);
int platform_socket_close(platform_socket_t socket);
int platform_setsockopt(platform_socket_t socket, int level, int optname, const void *optval, socklen_t optlen);

// File functions
FILE *platform_fopen(const char *filename, const char *mode);
int platform_remove(const char *filename);
int platform_rename(const char *oldname, const char *newname);

// MIDI functions
int platform_midi_init(void);
void platform_midi_cleanup(void);
int platform_midi_open_input(const char *device_name, void **handle);
int platform_midi_open_output(const char *device_name, void **handle);
int platform_midi_close_input(void *handle);
int platform_midi_close_output(void *handle);
int platform_midi_send(void *handle, const unsigned char *data, size_t length);

// In platform.h
int platform_get_device_config_dir(char *buffer, size_t size);
int platform_create_valid_filename(const char *input, char *output, size_t output_size);

#endif /* PLATFORM_H */
