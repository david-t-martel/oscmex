/**
 * @file platform_win.c
 * @brief Windows implementation of platform abstraction layer
 */

#include "platform.h"
#include "logging.h"
#include <windows.h>
#include <shlobj.h>
#include <time.h>
#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Windows-specific socket includes */
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

/* Windows-specific MIDI includes */
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

/* Global variables */
static HMIDIOUT midi_out_handle = NULL;
static HMIDIIN midi_in_handle = NULL;
static WSADATA wsa_data;
static int wsa_initialized = 0;

/**
 * @brief Initialize platform-specific functionality
 *
 * @return 0 on success, non-zero on failure
 */
int platform_init(void)
{
    int result;

    // Initialize Windows Sockets
    result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0)
    {
        return result;
    }

    wsa_initialized = 1;
    return 0;
}

/**
 * @brief Clean up platform-specific resources
 */
void platform_cleanup(void)
{
    // Clean up Windows Sockets
    if (wsa_initialized)
    {
        WSACleanup();
        wsa_initialized = 0;
    }

    // Clean up MIDI resources
    platform_midi_cleanup();
}

/**
 * @brief Get platform-specific error code
 *
 * @return Error code
 */
int platform_errno(void)
{
    int err = WSAGetLastError();
    if (err != 0)
    {
        return err;
    }

    return GetLastError();
}

/**
 * @brief Get string description of error code
 *
 * @param error_code Error code from platform_errno()
 * @return String description of the error
 */
const char *platform_strerror(int error_code)
{
    static char error_buffer[256];

    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        error_buffer,
        sizeof(error_buffer),
        NULL);

    // Remove trailing newline characters
    char *p = error_buffer + strlen(error_buffer) - 1;
    while (p >= error_buffer && (*p == '\r' || *p == '\n'))
    {
        *p-- = '\0';
    }

    return error_buffer;
}

/**
 * @brief Create a new thread
 *
 * @param thread Pointer to thread handle
 * @param start_routine Thread function
 * @param arg Argument to pass to thread function
 * @return 0 on success, non-zero on failure
 */
int platform_thread_create(platform_thread_t *thread, void *(*start_routine)(void *), void *arg)
{
    if (!thread || !start_routine)
    {
        return -1;
    }

    *thread = CreateThread(
        NULL, // Default security attributes
        0,    // Default stack size
        (LPTHREAD_START_ROUTINE)start_routine,
        arg, // Thread parameter
        0,   // Run immediately
        NULL // Thread ID (not needed)
    );

    if (*thread == NULL)
    {
        return GetLastError();
    }

    return 0;
}

/**
 * @brief Wait for thread to finish
 *
 * @param thread Thread handle
 * @param retval Pointer to store thread return value
 * @return 0 on success, non-zero on failure
 */
int platform_thread_join(platform_thread_t thread, void **retval)
{
    DWORD wait_result;
    DWORD exit_code;

    if (thread == PLATFORM_INVALID_THREAD)
    {
        return -1;
    }

    // Wait for thread to finish
    wait_result = WaitForSingleObject(thread, INFINITE);
    if (wait_result != WAIT_OBJECT_0)
    {
        return GetLastError();
    }

    // Get thread exit code if requested
    if (retval)
    {
        if (!GetExitCodeThread(thread, &exit_code))
        {
            return GetLastError();
        }
        *retval = (void *)(uintptr_t)exit_code;
    }

    // Close thread handle
    CloseHandle(thread);

    return 0;
}

/**
 * @brief Exit current thread
 *
 * @param retval Return value from thread
 */
void platform_thread_exit(void *retval)
{
    ExitThread((DWORD)(uintptr_t)retval);
}

/**
 * @brief Get current thread handle
 *
 * @return Thread handle
 */
platform_thread_t platform_thread_self(void)
{
    // Convert thread ID to pseudo-handle
    return GetCurrentThread();
}

/**
 * @brief Detach thread (run independently of calling thread)
 *
 * @param thread Thread handle
 * @return 0 on success, non-zero on failure
 */
int platform_thread_detach(platform_thread_t thread)
{
    if (thread == PLATFORM_INVALID_THREAD)
    {
        return -1;
    }

    if (!CloseHandle(thread))
    {
        return GetLastError();
    }

    return 0;
}

/**
 * @brief Initialize mutex
 *
 * @param mutex Pointer to mutex handle
 * @return 0 on success, non-zero on failure
 */
int platform_mutex_init(platform_mutex_t *mutex)
{
    if (!mutex)
    {
        return -1;
    }

    *mutex = CreateMutex(NULL, FALSE, NULL);
    if (*mutex == NULL)
    {
        return GetLastError();
    }

    return 0;
}

/**
 * @brief Lock mutex
 *
 * @param mutex Pointer to mutex handle
 * @return 0 on success, non-zero on failure
 */
int platform_mutex_lock(platform_mutex_t *mutex)
{
    DWORD wait_result;

    if (!mutex || *mutex == PLATFORM_INVALID_MUTEX)
    {
        return -1;
    }

    wait_result = WaitForSingleObject(*mutex, INFINITE);
    if (wait_result != WAIT_OBJECT_0)
    {
        return GetLastError();
    }

    return 0;
}

/**
 * @brief Unlock mutex
 *
 * @param mutex Pointer to mutex handle
 * @return 0 on success, non-zero on failure
 */
int platform_mutex_unlock(platform_mutex_t *mutex)
{
    if (!mutex || *mutex == PLATFORM_INVALID_MUTEX)
    {
        return -1;
    }

    if (!ReleaseMutex(*mutex))
    {
        return GetLastError();
    }

    return 0;
}

/**
 * @brief Destroy mutex
 *
 * @param mutex Pointer to mutex handle
 * @return 0 on success, non-zero on failure
 */
int platform_mutex_destroy(platform_mutex_t *mutex)
{
    if (!mutex || *mutex == PLATFORM_INVALID_MUTEX)
    {
        return -1;
    }

    if (!CloseHandle(*mutex))
    {
        return GetLastError();
    }

    *mutex = PLATFORM_INVALID_MUTEX;
    return 0;
}

/**
 * @brief Open file
 *
 * @param path File path
 * @param mode File open mode
 * @return File handle or PLATFORM_INVALID_FILE on failure
 */
platform_file_t platform_open_file(const char *path, int mode)
{
    DWORD access = 0;
    DWORD creation = 0;
    HANDLE handle;

    if (!path)
    {
        return PLATFORM_INVALID_FILE;
    }

    // Set access mode
    if (mode & PLATFORM_OPEN_READ)
    {
        access |= GENERIC_READ;
    }
    if (mode & PLATFORM_OPEN_WRITE)
    {
        access |= GENERIC_WRITE;
    }

    // Set creation disposition
    if (mode & PLATFORM_OPEN_CREATE)
    {
        creation = CREATE_ALWAYS;
    }
    else if (mode & PLATFORM_OPEN_APPEND)
    {
        creation = OPEN_ALWAYS;
    }
    else
    {
        creation = OPEN_EXISTING;
    }

    // Open file
    handle = CreateFileA(
        path,
        access,
        FILE_SHARE_READ,
        NULL,
        creation,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (handle == INVALID_HANDLE_VALUE)
    {
        return PLATFORM_INVALID_FILE;
    }

    // If append mode, seek to end of file
    if (mode & PLATFORM_OPEN_APPEND)
    {
        SetFilePointer(handle, 0, NULL, FILE_END);
    }

    return handle;
}

/**
 * @brief Read from file
 *
 * @param file File handle
 * @param buffer Buffer to read into
 * @param size Number of bytes to read
 * @return Number of bytes read or 0 on failure
 */
size_t platform_read_file(platform_file_t file, void *buffer, size_t size)
{
    DWORD bytes_read = 0;

    if (file == PLATFORM_INVALID_FILE || !buffer || size == 0)
    {
        return 0;
    }

    if (!ReadFile(file, buffer, (DWORD)size, &bytes_read, NULL))
    {
        return 0;
    }

    return (size_t)bytes_read;
}

/**
 * @brief Write to file
 *
 * @param file File handle
 * @param buffer Buffer to write from
 * @param size Number of bytes to write
 * @return Number of bytes written or 0 on failure
 */
size_t platform_write_file(platform_file_t file, const void *buffer, size_t size)
{
    DWORD bytes_written = 0;

    if (file == PLATFORM_INVALID_FILE || !buffer || size == 0)
    {
        return 0;
    }

    if (!WriteFile(file, buffer, (DWORD)size, &bytes_written, NULL))
    {
        return 0;
    }

    return (size_t)bytes_written;
}

/**
 * @brief Close file
 *
 * @param file File handle
 * @return 0 on success, non-zero on failure
 */
int platform_close_file(platform_file_t file)
{
    if (file == PLATFORM_INVALID_FILE)
    {
        return -1;
    }

    if (!CloseHandle(file))
    {
        return GetLastError();
    }

    return 0;
}

/**
 * @brief Flush file buffers
 *
 * @param file File handle
 * @return 0 on success, non-zero on failure
 */
int platform_flush_file(platform_file_t file)
{
    if (file == PLATFORM_INVALID_FILE)
    {
        return -1;
    }

    if (!FlushFileBuffers(file))
    {
        return GetLastError();
    }

    return 0;
}

/**
 * @brief Get file size
 *
 * @param file File handle
 * @return File size or 0 on failure
 */
size_t platform_get_file_size(platform_file_t file)
{
    DWORD size = 0;

    if (file == PLATFORM_INVALID_FILE)
    {
        return 0;
    }

    size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE)
    {
        return 0;
    }

    return (size_t)size;
}

/**
 * @brief Check if file exists
 *
 * @param path File path
 * @return true if file exists, false otherwise
 */
bool platform_file_exists(const char *path)
{
    DWORD attrs;

    if (!path)
    {
        return false;
    }

    attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }

    return (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

/**
 * @brief Delete file
 *
 * @param path File path
 * @return 0 on success, non-zero on failure
 */
int platform_delete_file(const char *path)
{
    if (!path)
    {
        return -1;
    }

    if (!DeleteFileA(path))
    {
        return GetLastError();
    }

    return 0;
}

/**
 * @brief Rename file
 *
 * @param old_path Old file path
 * @param new_path New file path
 * @return 0 on success, non-zero on failure
 */
int platform_rename_file(const char *old_path, const char *new_path)
{
    if (!old_path || !new_path)
    {
        return -1;
    }

    if (!MoveFileA(old_path, new_path))
    {
        return GetLastError();
    }

    return 0;
}

/**
 * @brief Create directory
 *
 * @param path Directory path
 * @return 0 on success, non-zero on failure
 */
int platform_create_directory(const char *path)
{
    if (!path)
    {
        return -1;
    }

    if (!CreateDirectoryA(path, NULL))
    {
        DWORD error = GetLastError();
        if (error == ERROR_ALREADY_EXISTS)
        {
            return 0;
        }
        return error;
    }

    return 0;
}

/**
 * @brief Remove directory
 *
 * @param path Directory path
 * @return 0 on success, non-zero on failure
 */
int platform_remove_directory(const char *path)
{
    if (!path)
    {
        return -1;
    }

    if (!RemoveDirectoryA(path))
    {
        return GetLastError();
    }

    return 0;
}

/**
 * @brief Check if directory exists
 *
 * @param path Directory path
 * @return true if directory exists, false otherwise
 */
bool platform_directory_exists(const char *path)
{
    DWORD attrs;

    if (!path)
    {
        return false;
    }

    attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }

    return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

/**
 * @brief Open directory for reading
 *
 * @param path Directory path
 * @return Directory handle or PLATFORM_INVALID_DIR on failure
 */
platform_dir_t platform_open_directory(const char *path)
{
    WIN32_FIND_DATAA *find_data;
    HANDLE handle;
    char search_path[MAX_PATH];

    if (!path)
    {
        return PLATFORM_INVALID_DIR;
    }

    // Allocate find data structure
    find_data = (WIN32_FIND_DATAA *)malloc(sizeof(WIN32_FIND_DATAA));
    if (!find_data)
    {
        return PLATFORM_INVALID_DIR;
    }

    // Create search path with wildcard
    if (strlen(path) > MAX_PATH - 3)
    {
        free(find_data);
        return PLATFORM_INVALID_DIR;
    }
    strcpy(search_path, path);
    if (search_path[strlen(search_path) - 1] != '/' &&
        search_path[strlen(search_path) - 1] != '\\')
    {
        strcat(search_path, "\\");
    }
    strcat(search_path, "*");

    // Find first file/directory
    handle = FindFirstFileA(search_path, find_data);
    if (handle == INVALID_HANDLE_VALUE)
    {
        free(find_data);
        return PLATFORM_INVALID_DIR;
    }

    // Return combined structure
    find_data->dwReserved0 = (DWORD)(uintptr_t)handle;
    return find_data;
}

/**
 * @brief Read directory entry
 *
 * @param dir Directory handle
 * @param name Buffer to store entry name
 * @param size Size of the buffer
 * @return 0 on success, 1 on end of directory, negative on error
 */
int platform_read_directory(platform_dir_t dir, char *name, size_t size)
{
    WIN32_FIND_DATAA *find_data = (WIN32_FIND_DATAA *)dir;
    HANDLE handle;

    if (!dir || !name || size == 0)
    {
        return -1;
    }

    handle = (HANDLE)(uintptr_t)find_data->dwReserved0;

    // Copy current entry
    strncpy(name, find_data->cFileName, size);
    name[size - 1] = '\0';

    // Append slash if directory
    if ((find_data->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
        size > strlen(name) + 1)
    {
        strcat(name, "/");
    }

    // Get next entry
    if (!FindNextFileA(handle, find_data))
    {
        DWORD error = GetLastError();
        if (error == ERROR_NO_MORE_FILES)
        {
            return 1; // End of directory
        }
        return -1;
    }

    return 0;
}

/**
 * @brief Close directory
 *
 * @param dir Directory handle
 * @return 0 on success, non-zero on failure
 */
int platform_close_directory(platform_dir_t dir)
{
    WIN32_FIND_DATAA *find_data = (WIN32_FIND_DATAA *)dir;
    HANDLE handle;

    if (!dir)
    {
        return -1;
    }

    handle = (HANDLE)(uintptr_t)find_data->dwReserved0;
    FindClose(handle);
    free(find_data);

    return 0;
}

/**
 * @brief Join two paths
 *
 * @param dst Destination buffer
 * @param size Size of destination buffer
 * @param path1 First path
 * @param path2 Second path
 * @return 0 on success, non-zero on failure
 */
int platform_path_join(char *dst, size_t size, const char *path1, const char *path2)
{
    if (!dst || size == 0 || !path1 || !path2)
    {
        return -1;
    }

    // Copy first path
    strncpy(dst, path1, size);
    dst[size - 1] = '\0';

    // Add separator if needed
    if (dst[strlen(dst) - 1] != '/' && dst[strlen(dst) - 1] != '\\')
    {
        if (strlen(dst) + 1 >= size)
        {
            return -1;
        }
        strcat(dst, "\\");
    }

    // Add second path
    if (strlen(dst) + strlen(path2) >= size)
    {
        return -1;
    }

    // Skip leading slashes in second path
    const char *p2 = path2;
    while (*p2 == '/' || *p2 == '\\')
    {
        p2++;
    }

    strcat(dst, p2);

    return 0;
}

/**
 * @brief Get directory part of path
 *
 * @param dst Destination buffer
 * @param size Size of destination buffer
 * @param path Path to process
 * @return 0 on success, non-zero on failure
 */
int platform_path_directory(char *dst, size_t size, const char *path)
{
    const char *last_sep;

    if (!dst || size == 0 || !path)
    {
        return -1;
    }

    // Find last separator
    last_sep = strrchr(path, '\\');
    if (!last_sep)
    {
        last_sep = strrchr(path, '/');
    }

    if (!last_sep)
    {
        // No directory part
        dst[0] = '.';
        dst[1] = '\0';
        return 0;
    }

    // Copy directory part
    if ((size_t)(last_sep - path) >= size)
    {
        return -1;
    }
    strncpy(dst, path, last_sep - path);
    dst[last_sep - path] = '\0';

    return 0;
}

/**
 * @brief Get filename part of path
 *
 * @param dst Destination buffer
 * @param size Size of destination buffer
 * @param path Path to process
 * @return 0 on success, non-zero on failure
 */
int platform_path_filename(char *dst, size_t size, const char *path)
{
    const char *last_sep;

    if (!dst || size == 0 || !path)
    {
        return -1;
    }

    // Find last separator
    last_sep = strrchr(path, '\\');
    if (!last_sep)
    {
        last_sep = strrchr(path, '/');
    }

    if (!last_sep)
    {
        // No directory part, use whole path
        last_sep = path - 1;
    }

    // Copy filename part
    if (strlen(last_sep + 1) >= size)
    {
        return -1;
    }
    strcpy(dst, last_sep + 1);

    return 0;
}

/**
 * @brief Get extension part of path
 *
 * @param dst Destination buffer
 * @param size Size of destination buffer
 * @param path Path to process
 * @return 0 on success, non-zero on failure
 */
int platform_path_extension(char *dst, size_t size, const char *path)
{
    const char *last_dot;
    const char *last_sep;

    if (!dst || size == 0 || !path)
    {
        return -1;
    }

    // Find last separator
    last_sep = strrchr(path, '\\');
    if (!last_sep)
    {
        last_sep = strrchr(path, '/');
    }

    if (!last_sep)
    {
        last_sep = path - 1;
    }

    // Find last dot after last separator
    last_dot = strrchr(last_sep + 1, '.');
    if (!last_dot)
    {
        // No extension
        dst[0] = '\0';
        return 0;
    }

    // Copy extension part
    if (strlen(last_dot) >= size)
    {
        return -1;
    }
    strcpy(dst, last_dot);

    return 0;
}

/**
 * @brief Get application data directory
 *
 * @param dst Destination buffer
 * @param size Size of destination buffer
 * @return 0 on success, non-zero on failure
 */
int platform_get_app_data_dir(char *dst, size_t size)
{
    if (!dst || size == 0)
    {
        return -1;
    }

    // Get AppData directory
    if (FAILED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, dst)))
    {
        return -1;
    }

    return 0;
}

/**
 * @brief Get executable directory
 *
 * @param dst Destination buffer
 * @param size Size of destination buffer
 * @return 0 on success, non-zero on failure
 */
int platform_get_executable_dir(char *dst, size_t size)
{
    DWORD path_len;

    if (!dst || size == 0)
    {
        return -1;
    }

    // Get executable path
    path_len = GetModuleFileNameA(NULL, dst, (DWORD)size);
    if (path_len == 0 || path_len >= size)
    {
        return -1;
    }

    // Find last separator
    char *last_sep = strrchr(dst, '\\');
    if (!last_sep)
    {
        return -1;
    }

    // Terminate at last separator
    *last_sep = '\0';

    return 0;
}

/**
 * @brief Create directories recursively
 *
 * @param path Directory path
 * @return 0 on success, non-zero on failure
 */
int platform_ensure_directory(const char *path)
{
    char temp[MAX_PATH];
    char *p = NULL;
    int len;

    if (!path)
    {
        return -1;
    }

    // Make a copy of the path
    len = strlen(path);
    if (len >= MAX_PATH)
    {
        return -1;
    }
    strcpy(temp, path);

    // Skip drive letter
    p = temp;
    if (len >= 2 && temp[1] == ':')
    {
        p += 2;
    }

    // Skip leading slashes
    while (*p == '/' || *p == '\\')
    {
        p++;
    }

    // Iterate through path components
    while (p && *p)
    {
        // Find next separator
        char *next = strpbrk(p, "/\\");
        if (next)
        {
            *next = '\0';
        }

        // Create directory if it doesn't exist
        if (!platform_directory_exists(temp))
        {
            if (platform_create_directory(temp) != 0)
            {
                // Check if it was created by another thread
                if (!platform_directory_exists(temp))
                {
                    return -1;
                }
            }
        }

        // Restore separator and move to next component
        if (next)
        {
            *next = '\\';
            p = next + 1;
        }
        else
        {
            p = NULL;
        }
    }

    return 0;
}

/**
 * @brief Sleep for a number of milliseconds
 *
 * @param milliseconds Number of milliseconds to sleep
 * @return 0 on success, non-zero on failure
 */
int platform_sleep(unsigned int milliseconds)
{
    Sleep(milliseconds);
    return 0;
}

/**
 * @brief Get current time in milliseconds
 *
 * @return Current time in milliseconds
 */
int64_t platform_get_time_ms(void)
{
    FILETIME ft;
    ULARGE_INTEGER uli;

    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;

    return (int64_t)(uli.QuadPart / 10000); // Convert to milliseconds
}

/**
 * @brief Format current time as string
 *
 * @param dst Destination buffer
 * @param size Size of destination buffer
 * @param format Format string (strftime format)
 * @return 0 on success, non-zero on failure
 */
int platform_format_time(char *dst, size_t size, const char *format)
{
    time_t t;
    struct tm *tm_info;

    if (!dst || size == 0 || !format)
    {
        return -1;
    }

    // Get current time
    time(&t);
    tm_info = localtime(&t);

    // Format time
    if (strftime(dst, size, format, tm_info) == 0)
    {
        return -1;
    }

    return 0;
}

/**
 * @brief Initialize socket subsystem
 *
 * @return 0 on success, non-zero on failure
 */
int platform_socket_init(void)
{
    // Already initialized in platform_init()
    return 0;
}

/**
 * @brief Clean up socket subsystem
 */
void platform_socket_cleanup(void)
{
    // Cleaned up in platform_cleanup()
}

/**
 * @brief Create socket
 *
 * @param domain Domain (AF_INET, AF_INET6)
 * @param type Type (SOCK_STREAM, SOCK_DGRAM)
 * @param protocol Protocol (IPPROTO_TCP, IPPROTO_UDP)
 * @return Socket handle or PLATFORM_INVALID_SOCKET on failure
 */
platform_socket_t platform_socket_create(int domain, int type, int protocol)
{
    SOCKET sock;

    sock = socket(domain, type, protocol);
    if (sock == INVALID_SOCKET)
    {
        return PLATFORM_INVALID_SOCKET;
    }

    return (platform_socket_t)sock;
}

/**
 * @brief Close socket
 *
 * @param socket Socket handle
 * @return 0 on success, non-zero on failure
 */
int platform_socket_close(platform_socket_t socket)
{
    if (socket == PLATFORM_INVALID_SOCKET)
    {
        return -1;
    }

    if (closesocket((SOCKET)socket) == SOCKET_ERROR)
    {
        return WSAGetLastError();
    }

    return 0;
}

/**
 * @brief Bind socket to address
 *
 * @param socket Socket handle
 * @param address Address to bind to
 * @param port Port to bind to
 * @return 0 on success, non-zero on failure
 */
int platform_socket_bind(platform_socket_t socket, const char *address, uint16_t port)
{
    struct sockaddr_in addr;

    if (socket == PLATFORM_INVALID_SOCKET)
    {
        return -1;
    }

    // Set up address structure
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (address && *address)
    {
        if (inet_pton(AF_INET, address, &addr.sin_addr) != 1)
        {
            return WSAGetLastError();
        }
    }
    else
    {
        addr.sin_addr.s_addr = INADDR_ANY;
    }

    // Bind socket
    if (bind((SOCKET)socket, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        return WSAGetLastError();
    }

    return 0;
}

/**
 * @brief Put socket in listening state
 *
 * @param socket Socket handle
 * @param backlog Maximum length of queue of pending connections
 * @return 0 on success, non-zero on failure
 */
int platform_socket_listen(platform_socket_t socket, int backlog)
{
    if (socket == PLATFORM_INVALID_SOCKET)
    {
        return -1;
    }

    if (listen((SOCKET)socket, backlog) == SOCKET_ERROR)
    {
        return WSAGetLastError();
    }

    return 0;
}

/**
 * @brief Accept incoming connection
 *
 * @param socket Listening socket handle
 * @param address Buffer to store client address
 * @param address_size Size of address buffer
 * @param port Pointer to store client port
 * @return New socket handle or PLATFORM_INVALID_SOCKET on failure
 */
platform_socket_t platform_socket_accept(platform_socket_t socket, char *address, size_t address_size, uint16_t *port)
{
    SOCKET client_sock;
    struct sockaddr_in addr;
    int addr_len = sizeof(addr);

    if (socket == PLATFORM_INVALID_SOCKET)
    {
        return PLATFORM_INVALID_SOCKET;
    }

    // Accept connection
    client_sock = accept((SOCKET)socket, (struct sockaddr *)&addr, &addr_len);
    if (client_sock == INVALID_SOCKET)
    {
        return PLATFORM_INVALID_SOCKET;
    }

    // Store client address if requested
    if (address && address_size > 0 && port)
    {
        if (inet_ntop(AF_INET, &addr.sin_addr, address, address_size) == NULL)
        {
            closesocket(client_sock);
            return PLATFORM_INVALID_SOCKET;
        }
        *port = ntohs(addr.sin_port);
    }

    return (platform_socket_t)client_sock;
}

/**
 * @brief Connect to remote host
 *
 * @param socket Socket handle
 * @param address Remote address
 * @param port Remote port
 * @return 0 on success, non-zero on failure
 */
int platform_socket_connect(platform_socket_t socket, const char *address, uint16_t port)
{
    struct sockaddr_in addr;

    if (socket == PLATFORM_INVALID_SOCKET || !address)
    {
        return -1;
    }

    // Set up address structure
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, address, &addr.sin_addr) != 1)
    {
        return WSAGetLastError();
    }

    // Connect to remote host
    if (connect((SOCKET)socket, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        return WSAGetLastError();
    }

    return 0;
}

/**
 * @brief Send data on connected socket
 *
 * @param socket Socket handle
 * @param buffer Data buffer
 * @param size Buffer size
 * @return Number of bytes sent or -1 on failure
 */
int platform_socket_send(platform_socket_t socket, const void *buffer, size_t size)
{
    int result;

    if (socket == PLATFORM_INVALID_SOCKET || !buffer || size == 0)
    {
        return -1;
    }

    result = send((SOCKET)socket, (const char *)buffer, (int)size, 0);
    if (result == SOCKET_ERROR)
    {
        return -1;
    }

    return result;
}

/**
 * @brief Receive data on connected socket
 *
 * @param socket Socket handle
 * @param buffer Data buffer
 * @param size Buffer size
 * @return Number of bytes received or -1 on failure
 */
int platform_socket_recv(platform_socket_t socket, void *buffer, size_t size)
{
    int result;

    if (socket == PLATFORM_INVALID_SOCKET || !buffer || size == 0)
    {
        return -1;
    }

    result = recv((SOCKET)socket, (char *)buffer, (int)size, 0);
    if (result == SOCKET_ERROR)
    {
        return -1;
    }

    return result;
}

/**
 * @brief Send data to specific address
 *
 * @param socket Socket handle
 * @param buffer Data buffer
 * @param size Buffer size
 * @param address Destination address
 * @param port Destination port
 * @return Number of bytes sent or -1 on failure
 */
int platform_socket_sendto(platform_socket_t socket, const void *buffer, size_t size, const char *address, uint16_t port)
{
    struct sockaddr_in addr;
    int result;

    if (socket == PLATFORM_INVALID_SOCKET || !buffer || size == 0 || !address)
    {
        return -1;
    }

    // Set up address structure
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, address, &addr.sin_addr) != 1)
    {
        return -1;
    }

    // Send data
    result = sendto((SOCKET)socket, (const char *)buffer, (int)size, 0, (struct sockaddr *)&addr, sizeof(addr));
    if (result == SOCKET_ERROR)
    {
        return -1;
    }

    return result;
}

/**
 * @brief Receive data from any address
 *
 * @param socket Socket handle
 * @param buffer Data buffer
 * @param size Buffer size
 * @param address Buffer to store source address
 * @param address_size Size of address buffer
 * @param port Pointer to store source port
 * @return Number of bytes received or -1 on failure
 */
int platform_socket_recvfrom(platform_socket_t socket, void *buffer, size_t size, char *address, size_t address_size, uint16_t *port)
{
    struct sockaddr_in addr;
    int addr_len = sizeof(addr);
    int result;

    if (socket == PLATFORM_INVALID_SOCKET || !buffer || size == 0)
    {
        return -1;
    }

    // Receive data
    result = recvfrom((SOCKET)socket, (char *)buffer, (int)size, 0, (struct sockaddr *)&addr, &addr_len);
    if (result == SOCKET_ERROR)
    {
        return -1;
    }

    // Store source address if requested
    if (address && address_size > 0 && port)
    {
        if (inet_ntop(AF_INET, &addr.sin_addr, address, address_size) == NULL)
        {
            return -1;
        }
        *port = ntohs(addr.sin_port);
    }

    return result;
}

/**
 * @brief Set socket option
 *
 * @param socket Socket handle
 * @param level Option level
 * @param option Option name
 * @param value Option value
 * @param size Size of option value
 * @return 0 on success, non-zero on failure
 */
int platform_socket_set_option(platform_socket_t socket, int level, int option, const void *value, size_t size)
{
    if (socket == PLATFORM_INVALID_SOCKET || !value || size == 0)
    {
        return -1;
    }

    if (setsockopt((SOCKET)socket, level, option, (const char *)value, (int)size) == SOCKET_ERROR)
    {
        return WSAGetLastError();
    }

    return 0;
}

/**
 * @brief Get socket option
 *
 * @param socket Socket handle
 * @param level Option level
 * @param option Option name
 * @param value Buffer for option value
 * @param size Pointer to size of option value buffer
 * @return 0 on success, non-zero on failure
 */
int platform_socket_get_option(platform_socket_t socket, int level, int option, void *value, size_t *size)
{
    int optlen;

    if (socket == PLATFORM_INVALID_SOCKET || !value || !size || *size == 0)
    {
        return -1;
    }

    optlen = (int)*size;
    if (getsockopt((SOCKET)socket, level, option, (char *)value, &optlen) == SOCKET_ERROR)
    {
        return WSAGetLastError();
    }

    *size = optlen;
    return 0;
}

/**
 * @brief Set socket blocking mode
 *
 * @param socket Socket handle
 * @param blocking Whether to use blocking mode
 * @return 0 on success, non-zero on failure
 */
int platform_socket_set_blocking(platform_socket_t socket, bool blocking)
{
    unsigned long mode = blocking ? 0 : 1;

    if (socket == PLATFORM_INVALID_SOCKET)
    {
        return -1;
    }

    if (ioctlsocket((SOCKET)socket, FIONBIO, &mode) == SOCKET_ERROR)
    {
        return WSAGetLastError();
    }

    return 0;
}

/**
 * @brief Set socket broadcast option
 *
 * @param socket Socket handle
 * @param broadcast Whether to enable broadcasting
 * @return 0 on success, non-zero on failure
 */
int platform_socket_set_broadcast(platform_socket_t socket, bool broadcast)
{
    BOOL opt = broadcast ? TRUE : FALSE;

    if (socket == PLATFORM_INVALID_SOCKET)
    {
        return -1;
    }

    if (setsockopt((SOCKET)socket, SOL_SOCKET, SO_BROADCAST, (const char *)&opt, sizeof(opt)) == SOCKET_ERROR)
    {
        return WSAGetLastError();
    }

    return 0;
}

/**
 * @brief Join multicast group
 *
 * @param socket Socket handle
 * @param group Multicast group address
 * @return 0 on success, non-zero on failure
 */
int platform_socket_join_multicast_group(platform_socket_t socket, const char *group)
{
    struct ip_mreq mreq;

    if (socket == PLATFORM_INVALID_SOCKET || !group)
    {
        return -1;
    }

    // Set up multicast request
    memset(&mreq, 0, sizeof(mreq));
    if (inet_pton(AF_INET, group, &mreq.imr_multiaddr) != 1)
    {
        return WSAGetLastError();
    }
    mreq.imr_interface.s_addr = INADDR_ANY;

    // Join multicast group
    if (setsockopt((SOCKET)socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&mreq, sizeof(mreq)) == SOCKET_ERROR)
    {
        return WSAGetLastError();
    }

    return 0;
}

/**
 * @brief Leave multicast group
 *
 * @param socket Socket handle
 * @param group Multicast group address
 * @return 0 on success, non-zero on failure
 */
int platform_socket_leave_multicast_group(platform_socket_t socket, const char *group)
{
    struct ip_mreq mreq;

    if (socket == PLATFORM_INVALID_SOCKET || !group)
    {
        return -1;
    }

    // Set up multicast request
    memset(&mreq, 0, sizeof(mreq));
    if (inet_pton(AF_INET, group, &mreq.imr_multiaddr) != 1)
    {
        return WSAGetLastError();
    }
    mreq.imr_interface.s_addr = INADDR_ANY;

    // Leave multicast group
    if (setsockopt((SOCKET)socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, (const char *)&mreq, sizeof(mreq)) == SOCKET_ERROR)
    {
        return WSAGetLastError();
    }

    return 0;
}

/**
 * @brief Execute command and capture output
 *
 * @param command Command to execute
 * @param output Buffer to store command output
 * @param output_size Size of output buffer
 * @return 0 on success, non-zero on failure
 */
int platform_execute(const char *command, char *output, size_t output_size)
{
    FILE *pipe;
    size_t bytes_read;

    if (!command || !output || output_size == 0)
    {
        return -1;
    }

    // Set buffer to empty
    output[0] = '\0';

    // Open command pipe
    pipe = _popen(command, "r");
    if (!pipe)
    {
        return -1;
    }

    // Read command output
    bytes_read = fread(output, 1, output_size - 1, pipe);
    if (bytes_read < output_size - 1)
    {
        output[bytes_read] = '\0';
    }
    else
    {
        output[output_size - 1] = '\0';
    }

    // Close pipe
    _pclose(pipe);

    return 0;
}

/**
 * @brief Allocate memory
 *
 * @param size Size of memory to allocate
 * @return Pointer to allocated memory or NULL on failure
 */
void *platform_allocate(size_t size)
{
    return malloc(size);
}

/**
 * @brief Reallocate memory
 *
 * @param ptr Pointer to previously allocated memory
 * @param size New size
 * @return Pointer to reallocated memory or NULL on failure
 */
void *platform_reallocate(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

/**
 * @brief Free allocated memory
 *
 * @param ptr Pointer to previously allocated memory
 */
void platform_deallocate(void *ptr)
{
    free(ptr);
}

/**
 * @brief Initialize MIDI interface
 *
 * @return 0 on success, non-zero on failure
 */
int platform_midi_init(void)
{
    // Placeholder - would scan for MIDI devices and open a connection
    return 0;
}

/**
 * @brief Send MIDI data
 *
 * @param data MIDI data buffer
 * @param length Length of data buffer
 * @return 0 on success, non-zero on failure
 */
int platform_midi_send(const unsigned char *data, size_t length)
{
    // Placeholder - would send MIDI data to device
    return 0;
}

/**
 * @brief Receive MIDI data
 *
 * @param data Buffer to store received data
 * @param max_length Maximum amount of data to receive
 * @param actual_length Pointer to store actual amount received
 * @return 0 on success, non-zero on failure
 */
int platform_midi_receive(unsigned char *data, size_t max_length, size_t *actual_length)
{
    // Placeholder - would receive MIDI data from device
    if (actual_length)
    {
        *actual_length = 0;
    }
    return 0;
}

/**
 * @brief Clean up MIDI interface
 */
void platform_midi_cleanup(void)
{
    // Placeholder - would close MIDI connection
}

/**
 * @brief Load configuration value
 *
 * @param app_name Application name
 * @param key Configuration key
 * @param value Buffer to store configuration value
 * @param value_size Size of value buffer
 * @return 0 on success, non-zero on failure
 */
int platform_load_config(const char *app_name, const char *key, char *value, size_t value_size)
{
    char reg_key[256];
    HKEY hkey;
    DWORD type;
    DWORD size = (DWORD)value_size;

    if (!app_name || !key || !value || value_size == 0)
    {
        return -1;
    }

    // Create registry key path
    if (snprintf(reg_key, sizeof(reg_key), "Software\\%s", app_name) >= sizeof(reg_key))
    {
        return -1;
    }

    // Open registry key
    if (RegOpenKeyExA(HKEY_CURRENT_USER, reg_key, 0, KEY_READ, &hkey) != ERROR_SUCCESS)
    {
        return -1;
    }

    // Read value
    if (RegQueryValueExA(hkey, key, NULL, &type, (LPBYTE)value, &size) != ERROR_SUCCESS || type != REG_SZ)
    {
        RegCloseKey(hkey);
        return -1;
    }

    // Ensure null termination
    if (size == value_size)
    {
        value[value_size - 1] = '\0';
    }

    RegCloseKey(hkey);
    return 0;
}

/**
 * @brief Save configuration value
 *
 * @param app_name Application name
 * @param key Configuration key
 * @param value Configuration value
 * @return 0 on success, non-zero on failure
 */
int platform_save_config(const char *app_name, const char *key, const char *value)
{
    char reg_key[256];
    HKEY hkey;
    DWORD disposition;

    if (!app_name || !key || !value)
    {
        return -1;
    }

    // Create registry key path
    if (snprintf(reg_key, sizeof(reg_key), "Software\\%s", app_name) >= sizeof(reg_key))
    {
        return -1;
    }

    // Create or open registry key
    if (RegCreateKeyExA(HKEY_CURRENT_USER, reg_key, 0, NULL, 0, KEY_WRITE, NULL, &hkey, &disposition) != ERROR_SUCCESS)
    {
        return -1;
    }

    // Write value
    if (RegSetValueExA(hkey, key, 0, REG_SZ, (const BYTE *)value, (DWORD)strlen(value) + 1) != ERROR_SUCCESS)
    {
        RegCloseKey(hkey);
        return -1;
    }

    RegCloseKey(hkey);
    return 0;
}

/**
 * @brief Log message with platform-specific implementation
 *
 * @param level Log level
 * @param format Format string
 * @param ... Variable arguments
 * @return 0 on success, non-zero on failure
 */
int platform_log(platform_log_level_t level, const char *format, ...)
{
    va_list args;
    char buf[1024];
    int result;
    static FILE *log_file = NULL;
    static CRITICAL_SECTION log_mutex;
    static int log_mutex_initialized = 0;
    static char log_path[MAX_PATH] = {0};

    // Initialize log mutex if needed
    if (!log_mutex_initialized)
    {
        InitializeCriticalSection(&log_mutex);
        log_mutex_initialized = 1;

        // Create log file in AppData directory
        char app_data[MAX_PATH];
        if (platform_get_app_data_dir(app_data, sizeof(app_data)) == 0)
        {
            if (platform_path_join(log_path, sizeof(log_path), app_data, "OSCMix") == 0)
            {
                platform_ensure_directory(log_path);
                char log_file_path[MAX_PATH];
                if (platform_path_join(log_file_path, sizeof(log_file_path), log_path, "oscmix.log") == 0)
                {
                    log_file = fopen(log_file_path, "a");
                }
            }
        }
    }

    // Format message
    va_start(args, format);
    result = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    if (result < 0 || result >= sizeof(buf))
    {
        return -1;
    }

    // Get current time
    char time_buf[64];
    platform_format_time(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S");

    // Enter critical section
    EnterCriticalSection(&log_mutex);

    // Output to console
    const char *level_str = "UNKNOWN";
    switch (level)
    {
    case PLATFORM_LOG_DEBUG:
        level_str = "DEBUG";
        break;
    case PLATFORM_LOG_INFO:
        level_str = "INFO";
        break;
    case PLATFORM_LOG_WARNING:
        level_str = "WARNING";
        break;
    case PLATFORM_LOG_ERROR:
        level_str = "ERROR";
        break;
    }

    fprintf(stderr, "[%s] [%s] %s\n", time_buf, level_str, buf);

    // Output to log file if available
    if (log_file)
    {
        fprintf(log_file, "[%s] [%s] %s\n", time_buf, level_str, buf);
        fflush(log_file);
    }

    // Leave critical section
    LeaveCriticalSection(&log_mutex);

    return 0;
}
