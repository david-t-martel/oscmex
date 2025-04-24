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

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* Platform-specific types */
#if defined(_WIN32)
    typedef void *platform_thread_t;
    typedef void *platform_mutex_t;
    typedef void *platform_file_t;
    typedef void *platform_socket_t;
    typedef void *platform_dir_t;
#define PLATFORM_INVALID_THREAD NULL
#define PLATFORM_INVALID_MUTEX NULL
#define PLATFORM_INVALID_FILE NULL
#define PLATFORM_INVALID_SOCKET NULL
#define PLATFORM_INVALID_DIR NULL
#define PLATFORM_MAX_PATH 260
#else
typedef unsigned long platform_thread_t;
typedef void *platform_mutex_t;
typedef int platform_file_t;
typedef int platform_socket_t;
typedef void *platform_dir_t;
#define PLATFORM_INVALID_THREAD 0
#define PLATFORM_INVALID_MUTEX NULL
#define PLATFORM_INVALID_FILE -1
#define PLATFORM_INVALID_SOCKET -1
#define PLATFORM_INVALID_DIR NULL
#define PLATFORM_MAX_PATH 4096
#endif

    /* File open modes */
    typedef enum
    {
        PLATFORM_OPEN_READ = 1,
        PLATFORM_OPEN_WRITE = 2,
        PLATFORM_OPEN_APPEND = 4,
        PLATFORM_OPEN_BINARY = 8,
        PLATFORM_OPEN_CREATE = 16
    } platform_open_mode_t;

    /* Platform initialization and shutdown */
    int platform_init(void);
    void platform_cleanup(void);

    /* Error handling */
    int platform_errno(void);
    const char *platform_strerror(int error_code);

    /* Thread functions */
    int platform_thread_create(platform_thread_t *thread, void *(*start_routine)(void *), void *arg);
    int platform_thread_join(platform_thread_t thread, void **retval);
    void platform_thread_exit(void *retval);
    platform_thread_t platform_thread_self(void);
    int platform_thread_detach(platform_thread_t thread);

    /* Mutex functions */
    int platform_mutex_init(platform_mutex_t *mutex);
    int platform_mutex_lock(platform_mutex_t *mutex);
    int platform_mutex_unlock(platform_mutex_t *mutex);
    int platform_mutex_destroy(platform_mutex_t *mutex);

    /* File system functions */
    platform_file_t platform_open_file(const char *path, int mode);
    size_t platform_read_file(platform_file_t file, void *buffer, size_t size);
    size_t platform_write_file(platform_file_t file, const void *buffer, size_t size);
    int platform_close_file(platform_file_t file);
    int platform_flush_file(platform_file_t file);
    size_t platform_get_file_size(platform_file_t file);
    bool platform_file_exists(const char *path);
    int platform_delete_file(const char *path);
    int platform_rename_file(const char *old_path, const char *new_path);

    /* Directory functions */
    int platform_create_directory(const char *path);
    int platform_remove_directory(const char *path);
    bool platform_directory_exists(const char *path);
    platform_dir_t platform_open_directory(const char *path);
    int platform_read_directory(platform_dir_t dir, char *name, size_t size);
    int platform_close_directory(platform_dir_t dir);

    /* Path functions */
    int platform_path_join(char *dst, size_t size, const char *path1, const char *path2);
    int platform_path_directory(char *dst, size_t size, const char *path);
    int platform_path_filename(char *dst, size_t size, const char *path);
    int platform_path_extension(char *dst, size_t size, const char *path);
    int platform_get_app_data_dir(char *dst, size_t size);
    int platform_get_executable_dir(char *dst, size_t size);

    /* Directory creation utility (creates all needed parent directories) */
    int platform_ensure_directory(const char *path);

    /* Time functions */
    int platform_sleep(unsigned int milliseconds);
    int64_t platform_get_time_ms(void);
    int platform_format_time(char *dst, size_t size, const char *format);

    /* Network functions */
    int platform_socket_init(void);
    void platform_socket_cleanup(void);
    platform_socket_t platform_socket_create(int domain, int type, int protocol);
    int platform_socket_close(platform_socket_t socket);
    int platform_socket_bind(platform_socket_t socket, const char *address, uint16_t port);
    int platform_socket_listen(platform_socket_t socket, int backlog);
    platform_socket_t platform_socket_accept(platform_socket_t socket, char *address, size_t address_size, uint16_t *port);
    int platform_socket_connect(platform_socket_t socket, const char *address, uint16_t port);
    int platform_socket_send(platform_socket_t socket, const void *buffer, size_t size);
    int platform_socket_recv(platform_socket_t socket, void *buffer, size_t size);
    int platform_socket_sendto(platform_socket_t socket, const void *buffer, size_t size, const char *address, uint16_t port);
    int platform_socket_recvfrom(platform_socket_t socket, void *buffer, size_t size, char *address, size_t address_size, uint16_t *port);
    int platform_socket_set_option(platform_socket_t socket, int level, int option, const void *value, size_t size);
    int platform_socket_get_option(platform_socket_t socket, int level, int option, void *value, size_t *size);
    int platform_socket_set_blocking(platform_socket_t socket, bool blocking);
    int platform_socket_set_broadcast(platform_socket_t socket, bool broadcast);
    int platform_socket_join_multicast_group(platform_socket_t socket, const char *group);
    int platform_socket_leave_multicast_group(platform_socket_t socket, const char *group);

    /* Process functions */
    int platform_execute(const char *command, char *output, size_t output_size);

    /* Memory functions */
    void *platform_allocate(size_t size);
    void *platform_reallocate(void *ptr, size_t size);
    void platform_deallocate(void *ptr);

    /* MIDI functions */
    int platform_midi_init(void);
    int platform_midi_send(const unsigned char *data, size_t length);
    int platform_midi_receive(unsigned char *data, size_t max_length, size_t *actual_length);
    void platform_midi_cleanup(void);

    /* Configuration functions */
    int platform_load_config(const char *app_name, const char *key, char *value, size_t value_size);
    int platform_save_config(const char *app_name, const char *key, const char *value);

    /* Logging functions (for platform-specific implementations) */
    typedef enum
    {
        PLATFORM_LOG_DEBUG,
        PLATFORM_LOG_INFO,
        PLATFORM_LOG_WARNING,
        PLATFORM_LOG_ERROR
    } platform_log_level_t;

    int platform_log(platform_log_level_t level, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_H */
