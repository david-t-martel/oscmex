/**
 * @file platform.h
 * @brief Platform abstraction layer for OSCMix
 *
 * This file provides platform-independent APIs for system-specific functionality
 * such as thread management, socket communication, file handling, and MIDI device
 * interactions. It enables code portability across Windows and POSIX systems.
 */

#ifndef PLATFORM_H
#define PLATFORM_H

/* Common includes for all platforms */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <signal.h>
#include <stdarg.h>

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
#include <direct.h>
#include <shlobj.h>

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
#define PLATFORM_ECONNREFUSED WSAECONNREFUSED

/* File handling */
#define platform_mkdir(path) _mkdir(path)
#define platform_access(path, mode) _access(path, mode)

/* Path handling */
#define PLATFORM_PATH_SEPARATOR '\\'
#define PLATFORM_PATH_SEPARATOR_STR "\\"
#define PLATFORM_MAX_PATH MAX_PATH

/* MIDI-specific functions */
typedef HMIDIIN platform_midiin_t;
typedef HMIDIOUT platform_midiout_t;
typedef unsigned(__stdcall *platform_thread_func_t)(void *);

/* Define signal constants for Windows if they're not already defined */
#ifndef SIGINT
#define SIGINT 2
#endif
#ifndef SIGTERM
#define SIGTERM 15
#endif

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
#include <errno.h>

/* Thread handling */
typedef pthread_t platform_thread_t;
typedef pthread_mutex_t platform_mutex_t;
typedef pthread_cond_t platform_event_t;

/* Socket handling */
typedef int platform_socket_t;
#define PLATFORM_INVALID_SOCKET -1
#define PLATFORM_SOCKET_ERROR -1
#define PLATFORM_ECONNREFUSED ECONNREFUSED

/* File handling */
#define platform_mkdir(path) mkdir(path, 0755)
#define platform_access(path, mode) access(path, mode)

/* Path handling */
#define PLATFORM_PATH_SEPARATOR '/'
#define PLATFORM_PATH_SEPARATOR_STR "/"
#define PLATFORM_MAX_PATH 4096

/* MIDI-specific functions - generic types for POSIX */
typedef int platform_midiin_t;
typedef int platform_midiout_t;
typedef void *(*platform_thread_func_t)(void *);

#endif

/* Platform-independent type definitions */

/**
 * @brief Platform-independent stream type
 */
typedef FILE platform_stream_t;

/* Function prototypes */

/*------------------------------------------------------------------------------
 * Safe String Functions
 *----------------------------------------------------------------------------*/

/**
 * @brief A safer version of strcpy
 *
 * Copies the source string to the destination buffer, ensuring null termination.
 * The copy will be truncated if the source string is longer than size-1 bytes.
 *
 * @param dst Destination buffer
 * @param src Source string
 * @param size Size of the destination buffer
 * @return Length of the source string (regardless of truncation)
 */
size_t strlcpy(char *dst, const char *src, size_t size);

/*------------------------------------------------------------------------------
 * File System Functions
 *----------------------------------------------------------------------------*/

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
 * @brief Gets the directory for device configuration files
 *
 * @param buffer Buffer to receive the path
 * @param size Size of the buffer
 * @return 0 on success, -1 on failure
 */
int platform_get_device_config_dir(char *buffer, size_t size);

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
 * @brief Creates a valid filename from potentially invalid input
 *
 * Removes characters that are not allowed in filenames on most filesystems.
 * Allowed characters are: alphanumeric, dash, underscore, and period.
 * Other characters are replaced with underscores or omitted.
 *
 * @param input Input string that may contain invalid characters
 * @param output Buffer to receive the valid filename
 * @param output_size Size of the output buffer
 * @return 0 on success, -1 on failure
 */
int platform_create_valid_filename(const char *input, char *output, size_t output_size);

/**
 * @brief Gets the current time as a formatted string
 *
 * @param buffer Buffer to receive the time string
 * @param size Size of the buffer
 * @param format Format string (strftime format)
 * @return 0 on success, -1 on failure
 */
int platform_format_time(char *buffer, size_t size, const char *format);

/**
 * @brief Opens a file using the platform's file API
 *
 * @param filename Name of the file to open
 * @param mode Mode to open the file in ("r", "w", etc.)
 * @return FILE pointer on success, NULL on failure
 */
FILE *platform_fopen(const char *filename, const char *mode);

/**
 * @brief Removes a file
 *
 * @param filename Name of the file to remove
 * @return 0 on success, -1 on failure
 */
int platform_remove(const char *filename);

/**
 * @brief Renames a file
 *
 * @param oldname Current name of the file
 * @param newname New name for the file
 * @return 0 on success, -1 on failure
 */
int platform_rename(const char *oldname, const char *newname);

/*------------------------------------------------------------------------------
 * Path Manipulation Functions
 *----------------------------------------------------------------------------*/

/**
 * @brief Joins two path components with the correct separator
 *
 * Handles cases where the first component ends with a separator or the second
 * component starts with a separator, ensuring only one separator is used.
 *
 * @param buffer Buffer to store the joined path
 * @param size Size of the buffer
 * @param part1 First part of the path
 * @param part2 Second part of the path
 * @return 0 on success, -1 on failure
 */
int platform_path_join(char *buffer, size_t size, const char *part1, const char *part2);

/**
 * @brief Joins two path components and ensures the directory exists
 *
 * @param buffer Buffer to store the joined path
 * @param size Size of the buffer
 * @param part1 First part of the path
 * @param part2 Second part of the path
 * @return 0 on success, -1 on failure
 */
int platform_ensure_path(char *buffer, size_t size, const char *part1, const char *part2);

/**
 * @brief Checks if a path is absolute
 *
 * @param path Path to check
 * @return 1 if absolute, 0 if relative
 */
int platform_path_is_absolute(const char *path);

/**
 * @brief Gets the basename (filename) from a path
 *
 * @param path Full path
 * @param buffer Buffer to store the basename
 * @param size Size of the buffer
 * @return 0 on success, -1 on failure
 */
int platform_path_basename(const char *path, char *buffer, size_t size);

/**
 * @brief Gets the dirname (directory) from a path
 *
 * @param path Full path
 * @param buffer Buffer to store the dirname
 * @param size Size of the buffer
 * @return 0 on success, -1 on failure
 */
int platform_path_dirname(const char *path, char *buffer, size_t size);

/*------------------------------------------------------------------------------
 * Thread and Synchronization Functions
 *----------------------------------------------------------------------------*/

/**
 * @brief Creates a new thread
 *
 * @param thread Pointer to store the thread handle
 * @param start_routine Thread entry point
 * @param arg Argument to pass to the thread function
 * @return 0 on success, error code on failure
 */
int platform_thread_create(platform_thread_t *thread, void *(*start_routine)(void *), void *arg);

/**
 * @brief Waits for a thread to complete
 *
 * @param thread Thread handle to wait for
 * @param retval Pointer to store return value
 * @return 0 on success, error code on failure
 */
int platform_thread_join(platform_thread_t thread, void **retval);

/**
 * @brief Sleeps for the specified number of milliseconds
 *
 * @param ms Number of milliseconds to sleep
 */
void platform_sleep_ms(unsigned long ms);

/**
 * @brief Initialize a mutex
 *
 * @param mutex Pointer to the mutex to initialize
 * @return 0 on success, non-zero on failure
 */
int platform_mutex_init(platform_mutex_t *mutex);

/**
 * @brief Destroy a mutex
 *
 * @param mutex Pointer to the mutex to destroy
 * @return 0 on success, non-zero on failure
 */
int platform_mutex_destroy(platform_mutex_t *mutex);

/**
 * @brief Lock a mutex
 *
 * @param mutex Pointer to the mutex to lock
 * @return 0 on success, non-zero on failure
 */
int platform_mutex_lock(platform_mutex_t *mutex);

/**
 * @brief Unlock a mutex
 *
 * @param mutex Pointer to the mutex to unlock
 * @return 0 on success, non-zero on failure
 */
int platform_mutex_unlock(platform_mutex_t *mutex);

/*------------------------------------------------------------------------------
 * Signal Handling Functions
 *----------------------------------------------------------------------------*/

/**
 * @brief Sets a signal handler for common termination signals
 *
 * Sets up a handler for SIGINT (Ctrl+C) and SIGTERM on both Windows and POSIX
 * platforms, making signal handling consistent across platforms.
 *
 * @param handler Function to call when a signal is received
 * @return 0 on success, -1 on failure
 */
int platform_set_signal_handler(void (*handler)(int));

/**
 * @brief Sets a cleanup handler to be called at program exit
 *
 * @param cleanup_func Function to call during program exit
 * @return 0 on success, -1 on failure
 */
int platform_set_cleanup_handler(void (*cleanup_func)(void));

/*------------------------------------------------------------------------------
 * Socket Functions
 *----------------------------------------------------------------------------*/

/**
 * @brief Initializes the socket subsystem
 *
 * @return 0 on success, non-zero on failure
 */
int platform_socket_init(void);

/**
 * @brief Cleans up the socket subsystem
 */
void platform_socket_cleanup(void);

/**
 * @brief Creates a socket
 *
 * @param domain Address family (AF_INET, etc.)
 * @param type Socket type (SOCK_STREAM, SOCK_DGRAM, etc.)
 * @param protocol Protocol (IPPROTO_TCP, etc.)
 * @return Socket handle on success, PLATFORM_INVALID_SOCKET on failure
 */
platform_socket_t platform_socket_create(int domain, int type, int protocol);

/**
 * @brief Closes a socket
 *
 * @param socket Socket handle to close
 * @return 0 on success, -1 on failure
 */
int platform_socket_close(platform_socket_t socket);

/**
 * @brief Sets a socket option
 *
 * @param socket Socket handle
 * @param level Option level (SOL_SOCKET, etc.)
 * @param optname Option name (SO_REUSEADDR, etc.)
 * @param optval Pointer to the option value
 * @param optlen Size of the option value
 * @return 0 on success, -1 on failure
 */
int platform_socket_setsockopt(platform_socket_t socket, int level, int optname,
                               const void *optval, socklen_t optlen);

/**
 * @brief Binds a socket to a local address and port
 *
 * @param socket Socket handle
 * @param address Local address to bind to (can be NULL for INADDR_ANY)
 * @param port Local port to bind to
 * @return 0 on success, -1 on failure
 */
int platform_socket_bind(platform_socket_t socket, const char *address, int port);

/**
 * @brief Connects a socket to a remote address and port
 *
 * @param socket Socket handle
 * @param address Remote address to connect to
 * @param port Remote port to connect to
 * @return 0 on success, -1 on failure
 */
int platform_socket_connect(platform_socket_t socket, const char *address, int port);

/**
 * @brief Open a socket
 *
 * @param address Address string in the format "udp!host!port"
 * @param server 1 for server (bind), 0 for client (connect)
 * @return Socket handle or PLATFORM_INVALID_SOCKET on error
 */
platform_socket_t platform_socket_open(const char *address, int server);

/**
 * @brief Send data on a socket
 *
 * @param sock Socket handle
 * @param buf Data buffer
 * @param len Data length
 * @param flags Send flags
 * @return Number of bytes sent or -1 on error
 */
ssize_t platform_socket_send(platform_socket_t sock, const void *buf, size_t len, int flags);

/**
 * @brief Receive data from a socket
 *
 * @param sock Socket handle
 * @param buf Data buffer
 * @param len Buffer size
 * @param flags Receive flags
 * @return Number of bytes received or -1 on error
 */
ssize_t platform_socket_recv(platform_socket_t sock, void *buf, size_t len, int flags);

/*------------------------------------------------------------------------------
 * Stream Functions
 *----------------------------------------------------------------------------*/

/**
 * @brief Get standard input stream
 *
 * @return Standard input stream
 */
platform_stream_t *platform_get_stdin(void);

/**
 * @brief Get standard output stream
 *
 * @return Standard output stream
 */
platform_stream_t *platform_get_stdout(void);

/**
 * @brief Read line from stream
 *
 * @param buf Buffer to read into
 * @param size Buffer size
 * @param stream Input stream
 * @return Buffer pointer or NULL on error or EOF
 */
char *platform_gets(char *buf, size_t size, platform_stream_t *stream);

/**
 * @brief Write formatted string to stream
 *
 * @param stream Output stream
 * @param format Format string
 * @param ... Format arguments
 * @return Number of characters written or negative on error
 */
int platform_printf(platform_stream_t *stream, const char *format, ...);

/**
 * @brief Write data to stream
 *
 * @param buf Data buffer
 * @param size Data size
 * @param stream Output stream
 * @return Number of items written
 */
size_t platform_write(const void *buf, size_t size, platform_stream_t *stream);

/**
 * @brief Read data from stream
 *
 * @param buf Buffer to read into
 * @param size Item size
 * @param stream Input stream
 * @return Number of items read
 */
size_t platform_read(void *buf, size_t size, platform_stream_t *stream);

/**
 * @brief Flush stream output
 *
 * @param stream Output stream
 * @return 0 on success, non-zero on failure
 */
int platform_flush(platform_stream_t *stream);

/**
 * @brief Check if stream has error
 *
 * @param stream Stream to check
 * @return Non-zero if error, 0 otherwise
 */
int platform_error(platform_stream_t *stream);

/*------------------------------------------------------------------------------
 * MIDI Functions
 *----------------------------------------------------------------------------*/

/**
 * @brief Initializes the MIDI subsystem
 *
 * @return 0 on success, non-zero on failure
 */
int platform_midi_init(void);

/**
 * @brief Cleans up the MIDI subsystem
 */
void platform_midi_cleanup(void);

/**
 * @brief Opens a MIDI input device
 *
 * @param device_name Name or ID of the MIDI device to open
 * @param handle Pointer to store the device handle
 * @return 0 on success, non-zero on failure
 */
int platform_midi_open_input(const char *device_name, platform_midiin_t *handle);

/**
 * @brief Opens a MIDI output device
 *
 * @param device_name Name or ID of the MIDI device to open
 * @param handle Pointer to store the device handle
 * @return 0 on success, non-zero on failure
 */
int platform_midi_open_output(const char *device_name, platform_midiout_t *handle);

/**
 * @brief Closes a MIDI input device
 *
 * @param handle Handle of the MIDI input device to close
 * @return 0 on success, non-zero on failure
 */
int platform_midi_close_input(platform_midiin_t handle);

/**
 * @brief Closes a MIDI output device
 *
 * @param handle Handle of the MIDI output device to close
 * @return 0 on success, non-zero on failure
 */
int platform_midi_close_output(platform_midiout_t handle);

/**
 * @brief Sends MIDI data to a MIDI output device
 *
 * @param handle Handle of the MIDI output device
 * @param data Pointer to the MIDI data to send
 * @param len Length of the MIDI data
 * @return 0 on success, non-zero on failure
 */
int platform_midi_send(platform_midiout_t handle, const void *data, size_t len);

/**
 * @brief Prepares and adds a buffer for MIDI input
 *
 * @param handle Handle of the MIDI input device
 * @param buffer Pointer to the buffer for MIDI data
 * @param len Length of the buffer
 * @return 0 on success, non-zero on failure
 */
int platform_midi_add_buffer(platform_midiin_t handle, void *buffer, size_t len);

/**
 * @brief Callback function type for MIDI input events
 */
typedef void (*platform_midi_callback_t)(void *data, size_t len, void *user_data);

/**
 * @brief Sets a callback function for MIDI input events
 *
 * @param handle Handle of the MIDI input device
 * @param callback Function to call when MIDI data is received
 * @param user_data User data to pass to the callback function
 * @return 0 on success, non-zero on failure
 */
int platform_midi_set_callback(platform_midiin_t handle, platform_midi_callback_t callback, void *user_data);

/*------------------------------------------------------------------------------
 * Error Handling Functions
 *----------------------------------------------------------------------------*/

/**
 * @brief Get platform error code
 *
 * @return Error code
 */
int platform_errno(void);

/**
 * @brief Get error message string
 *
 * @param errnum Error code
 * @return Error message string
 */
const char *platform_strerror(int errnum);

#endif /* PLATFORM_H */
