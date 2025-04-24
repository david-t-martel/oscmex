/**
 * @file platform.c
 * @brief Implementation of platform abstraction layer
 */

#include "platform.h"
#include "logging.h"

#ifdef PLATFORM_WINDOWS
#include <shlobj.h> // For SHGetFolderPath
#else
#include <netdb.h> // For POSIX networking functions
#endif

/* Initialize global state */
static int g_socket_initialized = 0;

/* Directory and path functions */

int platform_get_app_data_dir(char *buffer, size_t size)
{
    if (!buffer || size == 0)
        return -1;

#if defined(PLATFORM_WINDOWS)
    // Windows: %APPDATA%
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, buffer)))
    {
        return 0;
    }
    return -1;
#else
    // POSIX systems
    const char *home = getenv("HOME");
    if (!home)
        return -1;

#if defined(__APPLE__)
    // macOS: ~/Library/Application Support
    if (snprintf(buffer, size, "%s/Library/Application Support", home) >= (int)size)
        return -1;
#else
    // Linux/FreeBSD: ~/.local/share
    if (snprintf(buffer, size, "%s/.local/share", home) >= (int)size)
        return -1;
#endif

    return 0;
#endif
}

int platform_get_device_config_dir(char *buffer, size_t size)
{
    char app_dir[PLATFORM_MAX_PATH];
    int result;

    result = platform_get_app_data_dir(app_dir, sizeof(app_dir));
    if (result != 0)
    {
        log_error("Failed to get app data directory");
        return result;
    }

    // First join the paths
    if (platform_path_join(buffer, size, app_dir, "OSCMix") != 0)
    {
        log_error("Failed to join paths: %s and OSCMix", app_dir);
        return -1;
    }

    // Now create the directory
    if (platform_ensure_directory(buffer) != 0)
    {
        log_error("Failed to create directory: %s", buffer);
        return -1;
    }

    // Add the device_config part
    if (platform_path_join(buffer, size, buffer, "device_config") != 0)
    {
        log_error("Failed to join paths: %s and device_config", buffer);
        return -1;
    }

    // And ensure this directory exists too
    return platform_ensure_directory(buffer);
}

int platform_ensure_directory(const char *path)
{
    if (!path || strlen(path) == 0)
        return -1;

    // Make a copy of the path that we can modify
    char dir_path[PLATFORM_MAX_PATH];
    strncpy(dir_path, path, sizeof(dir_path) - 1);
    dir_path[sizeof(dir_path) - 1] = '\0';

    // Ensure the directory exists
    int result = platform_access(dir_path, 0);
    if (result == 0)
    {
        // Directory already exists
        return 0;
    }

    // Directory doesn't exist, create it
    char *p = dir_path;

    // Skip leading slashes
    if (*p == PLATFORM_PATH_SEPARATOR)
        p++;

#if defined(PLATFORM_WINDOWS)
    // Skip drive letter on Windows (e.g., "C:")
    if (strlen(p) >= 2 && p[1] == ':')
    {
        p += 2;
        if (*p == PLATFORM_PATH_SEPARATOR)
            p++;
    }
#endif

    // Create each directory in the path
    while (*p)
    {
        // Find the next path separator
        char *q = strchr(p, PLATFORM_PATH_SEPARATOR);
        if (q)
            *q = '\0'; // Temporarily terminate the string

        // Create this segment of the path if it doesn't exist
        if (strlen(dir_path) > 0)
        {
            if (platform_access(dir_path, 0) != 0)
            {
                if (platform_mkdir(dir_path) != 0)
                {
                    log_error("Failed to create directory: %s", dir_path);
                    return -1;
                }
            }
        }

        // Restore the path separator and move to the next segment
        if (q)
        {
            *q = PLATFORM_PATH_SEPARATOR;
            p = q + 1;
        }
        else
        {
            break;
        }
    }

    return 0;
}

int platform_create_valid_filename(const char *input, char *output, size_t output_size)
{
    size_t i, j = 0;
    size_t input_len;

    if (!input || !output || output_size == 0)
    {
        return -1;
    }

    input_len = strlen(input);

    for (i = 0; i < input_len && j < output_size - 1; i++)
    {
        char c = input[i];
        // Allow alphanumeric, dash, underscore, and period
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')
        {
            output[j++] = c;
        }
        else if (c == ' ' || c == ',' || c == '+' || c == '=')
        {
            // Replace common separators with underscore
            output[j++] = '_';
        }
        // Skip other characters
    }

    // Ensure output is not empty
    if (j == 0 && output_size > 4)
    {
        strcpy(output, "file");
        j = 4;
    }

    output[j] = '\0';
    return 0;
}

int platform_format_time(char *buffer, size_t size, const char *format)
{
    if (!buffer || size == 0 || !format)
        return -1;

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    if (!tm_info)
        return -1;

    if (strftime(buffer, size, format, tm_info) == 0)
        return -1;

    return 0;
}

/* Path manipulation functions */

int platform_path_join(char *buffer, size_t size, const char *part1, const char *part2)
{
    if (!buffer || size == 0 || !part1 || !part2)
        return -1;

    // Handle empty parts
    if (*part1 == '\0')
    {
        if (strlcpy(buffer, part2, size) >= size)
            return -1;
        return 0;
    }

    if (*part2 == '\0')
    {
        if (strlcpy(buffer, part1, size) >= size)
            return -1;
        return 0;
    }

    size_t len1 = strlen(part1);
    size_t len2 = strlen(part2);

    // Check if part1 ends with a separator
    bool has_separator = (len1 > 0 && part1[len1 - 1] == PLATFORM_PATH_SEPARATOR);

    // Check if part2 starts with a separator
    bool skip_separator = (len2 > 0 && part2[0] == PLATFORM_PATH_SEPARATOR);

    // Calculate required buffer size
    size_t required_size;

    if (has_separator && skip_separator)
    {
        // Both have separators, skip one
        required_size = len1 + len2; // +1 for null terminator, -1 for skipping separator
    }
    else if (!has_separator && !skip_separator)
    {
        // Neither has a separator, add one
        required_size = len1 + 1 + len2 + 1;
    }
    else
    {
        // One has a separator, use as is
        required_size = len1 + len2 + 1;
    }

    if (required_size > size)
    {
        return -1;
    }

    if (has_separator && skip_separator)
    {
        // Both have separators, skip one
        if (snprintf(buffer, size, "%s%s", part1, part2 + 1) >= (int)size)
            return -1;
    }
    else if (!has_separator && !skip_separator)
    {
        // Neither has a separator, add one
        if (snprintf(buffer, size, "%s%c%s", part1, PLATFORM_PATH_SEPARATOR, part2) >= (int)size)
            return -1;
    }
    else
    {
        // One has a separator, use as is
        if (snprintf(buffer, size, "%s%s", part1, part2) >= (int)size)
            return -1;
    }

    return 0;
}

int platform_path_is_absolute(const char *path)
{
    if (!path)
        return 0;

#if defined(PLATFORM_WINDOWS)
    // Windows: X:\ or \\server
    return (
        (strlen(path) >= 3 && path[1] == ':' && path[2] == PLATFORM_PATH_SEPARATOR) ||
        (strlen(path) >= 2 && path[0] == PLATFORM_PATH_SEPARATOR && path[1] == PLATFORM_PATH_SEPARATOR));
#else
    // POSIX: /path
    return (strlen(path) >= 1 && path[0] == PLATFORM_PATH_SEPARATOR);
#endif
}

int platform_path_basename(const char *path, char *buffer, size_t size)
{
    if (!path || !buffer || size == 0)
        return -1;

    const char *last_separator = strrchr(path, PLATFORM_PATH_SEPARATOR);
    if (last_separator)
    {
        // Skip the separator
        last_separator++;

        // Copy basename to buffer
        if (strlcpy(buffer, last_separator, size) >= size)
            return -1;
    }
    else
    {
        // No separator, copy the whole path
        if (strlcpy(buffer, path, size) >= size)
            return -1;
    }

    return 0;
}

int platform_path_dirname(const char *path, char *buffer, size_t size)
{
    if (!path || !buffer || size == 0)
        return -1;

    const char *last_separator = strrchr(path, PLATFORM_PATH_SEPARATOR);
    if (last_separator)
    {
        // Calculate length of dirname
        size_t len = last_separator - path;

        // Ensure buffer is big enough
        if (len >= size)
            return -1;

        // Copy dirname to buffer
        memcpy(buffer, path, len);
        buffer[len] = '\0';
    }
    else
    {
        // No separator, use current directory
        if (size < 2)
            return -1;

        buffer[0] = '.';
        buffer[1] = '\0';
    }

    return 0;
}

int platform_ensure_path(char *buffer, size_t size, const char *part1, const char *part2)
{
    int result = platform_path_join(buffer, size, part1, part2);
    if (result != 0)
    {
        log_error("Failed to join paths: %s and %s", part1, part2);
        return result;
    }

    return platform_ensure_directory(buffer);
}

/* Socket functions */

int platform_socket_init(void)
{
#if defined(PLATFORM_WINDOWS)
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0)
    {
        log_error("WSAStartup failed: %d", result);
    }
    return result;
#else
    return 0; // No initialization needed on POSIX
#endif
}

void platform_socket_cleanup(void)
{
#if defined(PLATFORM_WINDOWS)
    WSACleanup();
    log_debug("Socket subsystem cleaned up");
#endif
    // No cleanup needed on POSIX
}

platform_socket_t platform_socket_create(int domain, int type, int protocol)
{
    // Initialize socket subsystem if not already done
    if (!g_socket_initialized)
    {
        platform_socket_init();
        g_socket_initialized = 1;
    }

    platform_socket_t sock = socket(domain, type, protocol);
    if (sock == PLATFORM_INVALID_SOCKET)
    {
        log_error("Failed to create socket: %s", platform_strerror(platform_errno()));
    }
    return sock;
}

int platform_socket_close(platform_socket_t socket)
{
    if (socket == PLATFORM_INVALID_SOCKET)
        return -1;

#if defined(PLATFORM_WINDOWS)
    int result = closesocket(socket);
#else
    int result = close(socket);
#endif
    if (result != 0)
    {
        log_error("Failed to close socket: %s", platform_strerror(platform_errno()));
    }
    return result;
}

int platform_socket_setsockopt(platform_socket_t socket, int level, int optname,
                               const void *optval, socklen_t optlen)
{
    if (socket == PLATFORM_INVALID_SOCKET || !optval)
        return -1;

#if defined(PLATFORM_WINDOWS)
    int result = setsockopt(socket, level, optname, (const char *)optval, optlen);
#else
    int result = setsockopt(socket, level, optname, optval, optlen);
#endif
    if (result != 0)
    {
        log_error("Failed to set socket option: %s", platform_strerror(platform_errno()));
    }
    return result;
}

int platform_socket_bind(platform_socket_t socket, const char *address, int port)
{
    if (socket == PLATFORM_INVALID_SOCKET || port < 0 || port > 65535)
        return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);

    if (address)
    {
        if (inet_pton(AF_INET, address, &addr.sin_addr) != 1)
        {
            log_error("Invalid IP address: %s", address);
            return -1;
        }
    }
    else
    {
        addr.sin_addr.s_addr = INADDR_ANY;
    }

    int result = bind(socket, (struct sockaddr *)&addr, sizeof(addr));
    if (result != 0)
    {
        log_error("Failed to bind socket to %s:%d: %s",
                  address ? address : "0.0.0.0", port, platform_strerror(platform_errno()));
    }
    else
    {
        log_debug("Socket bound to %s:%d", address ? address : "0.0.0.0", port);
    }
    return result;
}

int platform_socket_connect(platform_socket_t socket, const char *address, int port)
{
    if (socket == PLATFORM_INVALID_SOCKET || !address || port < 0 || port > 65535)
        return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);

    if (inet_pton(AF_INET, address, &addr.sin_addr) != 1)
    {
        // Try to resolve hostname
        struct hostent *he = gethostbyname(address);
        if (!he)
        {
            log_error("Failed to resolve host: %s", address);
            return -1;
        }
        memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    }

    int result = connect(socket, (struct sockaddr *)&addr, sizeof(addr));
    if (result != 0)
    {
        log_error("Failed to connect socket to %s:%d: %s",
                  address, port, platform_strerror(platform_errno()));
    }
    else
    {
        log_debug("Socket connected to %s:%d", address, port);
    }
    return result;
}

platform_socket_t platform_socket_open(const char *address, int server)
{
    if (!address)
    {
        log_error("Invalid address: NULL");
        return PLATFORM_INVALID_SOCKET;
    }

    // Initialize socket subsystem if needed
    if (!g_socket_initialized)
    {
        platform_socket_init();
        g_socket_initialized = 1;
    }

    // Parse the address string (format: "udp!host!port")
    char proto[16], host[256], port_str[16];

    // Copy the address to a buffer we can modify
    char addr_buf[PLATFORM_MAX_PATH];
    if (strlcpy(addr_buf, address, sizeof(addr_buf)) >= sizeof(addr_buf))
    {
        log_error("Address string too long: %s", address);
        return PLATFORM_INVALID_SOCKET;
    }

    // Parse parts
    char *proto_part = strtok(addr_buf, "!");
    char *host_part = strtok(NULL, "!");
    char *port_part = strtok(NULL, "!");

    if (!proto_part || !host_part || !port_part)
    {
        log_error("Invalid address format: %s (expected proto!host!port)", address);
        return PLATFORM_INVALID_SOCKET;
    }

    if (strlcpy(proto, proto_part, sizeof(proto)) >= sizeof(proto) ||
        strlcpy(host, host_part, sizeof(host)) >= sizeof(host) ||
        strlcpy(port_str, port_part, sizeof(port_str)) >= sizeof(port_str))
    {
        log_error("Address component too long in: %s", address);
        return PLATFORM_INVALID_SOCKET;
    }

    int port = atoi(port_str);
    if (port <= 0 || port > 65535)
    {
        log_error("Invalid port number: %s", port_str);
        return PLATFORM_INVALID_SOCKET;
    }

    // Only UDP is supported for now
    if (strcmp(proto, "udp") != 0)
    {
        log_error("Unsupported protocol: %s (only udp is supported)", proto);
        return PLATFORM_INVALID_SOCKET;
    }

    // Create socket
    platform_socket_t sock = platform_socket_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == PLATFORM_INVALID_SOCKET)
    {
        log_error("Failed to create socket: %s", platform_strerror(platform_errno()));
        return PLATFORM_INVALID_SOCKET;
    }

    // Set up address structure
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (server)
    {
        // For server (receiver), bind to the port
        int optval = 1;

        // Allow address reuse
        if (platform_socket_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                                       &optval, sizeof(optval)) != 0)
        {
            log_warning("Failed to set SO_REUSEADDR: %s", platform_strerror(platform_errno()));
        }

        // Check if this is a multicast address
        if (strncmp(host, "224.", 4) == 0 || strncmp(host, "239.", 4) == 0)
        {
            struct ip_mreq mreq;

            // Set up the multicast group
            mreq.imr_multiaddr.s_addr = inet_addr(host);
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);

            // Join the multicast group
            if (platform_socket_setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                                           &mreq, sizeof(mreq)) != 0)
            {
                log_error("Failed to join multicast group: %s", platform_strerror(platform_errno()));
                platform_socket_close(sock);
                return PLATFORM_INVALID_SOCKET;
            }

            log_info("Joined multicast group: %s", host);
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
        }
        else if (strcmp(host, "0.0.0.0") == 0 || strcmp(host, "*") == 0)
        {
            // Bind to all interfaces
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
        }
        else
        {
            // Bind to specific interface
            addr.sin_addr.s_addr = inet_addr(host);
            if (addr.sin_addr.s_addr == INADDR_NONE)
            {
                log_error("Invalid IP address: %s", host);
                platform_socket_close(sock);
                return PLATFORM_INVALID_SOCKET;
            }
        }

        // Bind to the address
        if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0)
        {
            log_error("Failed to bind socket: %s", platform_strerror(platform_errno()));
            platform_socket_close(sock);
            return PLATFORM_INVALID_SOCKET;
        }

        log_info("Socket bound to %s:%d", host, port);
    }
    else
    {
        // For client (sender), connect to the destination
        addr.sin_addr.s_addr = inet_addr(host);

        if (addr.sin_addr.s_addr == INADDR_NONE)
        {
            // Try to resolve hostname
            struct hostent *he = gethostbyname(host);
            if (!he)
            {
                log_error("Failed to resolve host: %s", host);
                platform_socket_close(sock);
                return PLATFORM_INVALID_SOCKET;
            }
            memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
        }

        if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0)
        {
            log_error("Failed to connect socket: %s", platform_strerror(platform_errno()));
            platform_socket_close(sock);
            return PLATFORM_INVALID_SOCKET;
        }

        log_info("Socket connected to %s:%d", host, port);
    }

    return sock;
}

/**
 * @brief Send data on a socket
 *
 * @param sock Socket handle
 * @param buf Data buffer
 * @param len Data length
 * @param flags Send flags
 * @return Number of bytes sent or -1 on error
 */
ssize_t platform_socket_send(platform_socket_t sock, const void *buf, size_t len, int flags)
{
    if (sock == PLATFORM_INVALID_SOCKET || !buf || len == 0)
        return -1;

#if defined(PLATFORM_WINDOWS)
    return send(sock, (const char *)buf, (int)len, flags);
#else
    return send(sock, buf, len, flags);
#endif
}

/**
 * @brief Receive data from a socket
 *
 * @param sock Socket handle
 * @param buf Data buffer
 * @param len Buffer size
 * @param flags Receive flags
 * @return Number of bytes received or -1 on error
 */
ssize_t platform_socket_recv(platform_socket_t sock, void *buf, size_t len, int flags)
{
    if (sock == PLATFORM_INVALID_SOCKET || !buf || len == 0)
        return -1;

#if defined(PLATFORM_WINDOWS)
    return recv(sock, (char *)buf, (int)len, flags);
#else
    return recv(sock, buf, len, flags);
#endif
}

/* File functions */

FILE *platform_fopen(const char *filename, const char *mode)
{
    if (!filename || !mode)
        return NULL;

#if defined(PLATFORM_WINDOWS)
    FILE *fp;
    if (fopen_s(&fp, filename, mode) != 0)
    {
        log_error("Failed to open file: %s (mode %s)", filename, mode);
        return NULL;
    }
    return fp;
#else
    FILE *fp = fopen(filename, mode);
    if (!fp)
    {
        log_error("Failed to open file: %s (mode %s): %s",
                  filename, mode, strerror(errno));
    }
    return fp;
#endif
}

int platform_remove(const char *filename)
{
    if (!filename)
        return -1;

    int result = remove(filename);
    if (result != 0)
    {
        log_error("Failed to remove file: %s: %s",
                  filename, platform_strerror(platform_errno()));
    }
    return result;
}

int platform_rename(const char *oldname, const char *newname)
{
    if (!oldname || !newname)
        return -1;

    int result = rename(oldname, newname);
    if (result != 0)
    {
        log_error("Failed to rename file from %s to %s: %s",
                  oldname, newname, platform_strerror(platform_errno()));
    }
    return result;
}

/* Thread functions */

int platform_thread_create(platform_thread_t *thread, platform_thread_func_t func, void *arg)
{
    if (!thread || !func)
        return -1;

#if defined(PLATFORM_WINDOWS)
    *thread = (platform_thread_t)_beginthreadex(NULL, 0, func, arg, 0, NULL);
    return (*thread == 0) ? -1 : 0;
#else
    return pthread_create(thread, NULL, func, arg);
#endif
}

int platform_thread_join(platform_thread_t thread)
{
#if defined(PLATFORM_WINDOWS)
    DWORD result = WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return (result == WAIT_OBJECT_0) ? 0 : -1;
#else
    return pthread_join(thread, NULL);
#endif
}

void platform_sleep_ms(unsigned long ms)
{
#if defined(PLATFORM_WINDOWS)
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

/* Signal handling functions */

#if defined(PLATFORM_WINDOWS)
static BOOL WINAPI win32_signal_handler(DWORD ctrlType);
static void (*global_signal_handler)(int) = NULL;

int platform_set_signal_handler(void (*handler)(int))
{
    global_signal_handler = handler;
    if (!SetConsoleCtrlHandler(win32_signal_handler, TRUE))
    {
        log_error("Failed to set console control handler: %s", platform_strerror(GetLastError()));
        return -1;
    }
    log_debug("Set signal handler for Windows");
    return 0;
}

static BOOL WINAPI win32_signal_handler(DWORD ctrlType)
{
    int sig = 0;

    switch (ctrlType)
    {
    case CTRL_C_EVENT:
        sig = SIGINT; // Now defined in platform.h
        break;
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        sig = SIGTERM; // Now defined in platform.h
        break;
    default:
        return FALSE;
    }

    if (global_signal_handler)
    {
        global_signal_handler(sig);
        return TRUE;
    }

    return FALSE;
}
#else
int platform_set_signal_handler(void (*handler)(int))
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask); // Block all signals during handler

    if (sigaction(SIGINT, &sa, NULL) != 0)
    {
        log_error("Failed to set SIGINT handler: %s", strerror(errno));
        return -1;
    }
    if (sigaction(SIGTERM, &sa, NULL) != 0)
    {
        log_error("Failed to set SIGTERM handler: %s", strerror(errno));
        return -1;
    }

    log_debug("Set signal handlers for POSIX");
    return 0;
}
#endif

int platform_set_cleanup_handler(void (*cleanup_func)(void))
{
    if (!cleanup_func)
        return -1;

    atexit(cleanup_func);
    log_debug("Set cleanup handler");
    return 0;
}

/* MIDI functions */

#if defined(PLATFORM_WINDOWS)

typedef struct
{
    platform_midi_callback_t callback;
    void *user_data;
} midi_callback_info;

static void CALLBACK midi_input_callback(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance,
                                         DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    midi_callback_info *info = (midi_callback_info *)dwInstance;

    if (!info || !info->callback)
        return;

    if (wMsg == MIM_DATA)
    {
        // Short MIDI message
        DWORD message = (DWORD)dwParam1;
        unsigned char data[3];
        data[0] = (unsigned char)(message & 0xFF);
        data[1] = (unsigned char)((message >> 8) & 0xFF);
        data[2] = (unsigned char)((message >> 16) & 0xFF);

        info->callback(data, 3, info->user_data);
    }
    else if (wMsg == MIM_LONGDATA)
    {
        // SysEx message
        MIDIHDR *header = (MIDIHDR *)dwParam1;

        if (header && header->dwBytesRecorded > 0)
        {
            info->callback(header->lpData, header->dwBytesRecorded, info->user_data);

            // Prepare the buffer for reuse
            midiInPrepareHeader(hMidiIn, header, sizeof(MIDIHDR));
            midiInAddBuffer(hMidiIn, header, sizeof(MIDIHDR));
        }
    }
}

int platform_midi_init(void)
{
    return 0; // No global initialization needed for Windows MIDI
}

void platform_midi_cleanup(void)
{
    // No global cleanup needed for Windows MIDI
}

int platform_midi_open_input(const char *device_name, platform_midiin_t *handle)
{
    if (!device_name || !handle)
        return -1;

    UINT numDevs = midiInGetNumDevs();
    MIDIINCAPS caps;
    MMRESULT result;
    UINT deviceId = MIDI_MAPPER; // Default

    // Try to find device by name substring
    for (UINT i = 0; i < numDevs; i++)
    {
        if (midiInGetDevCaps(i, &caps, sizeof(MIDIINCAPS)) == MMSYSERR_NOERROR)
        {
            if (strstr(caps.szPname, device_name) != NULL)
            {
                deviceId = i;
                break;
            }
        }
    }

    // If not found by name, try as a device ID number
    if (deviceId == MIDI_MAPPER)
    {
        char *endptr;
        long num = strtol(device_name, &endptr, 10);
        if (*device_name != '\0' && *endptr == '\0' && num >= 0 && num < numDevs)
        {
            deviceId = (UINT)num;
        }
    }

    // Open the device
    result = midiInOpen(handle, deviceId, NULL, 0, CALLBACK_NULL);
    if (result != MMSYSERR_NOERROR)
    {
        log_error("Failed to open MIDI input device: %s", device_name);
        return -1;
    }

    log_info("Opened MIDI input device: %s (ID: %u)", device_name, deviceId);
    return 0;
}

int platform_midi_open_output(const char *device_name, platform_midiout_t *handle)
{
    if (!device_name || !handle)
        return -1;

    UINT numDevs = midiOutGetNumDevs();
    MIDIOUTCAPS caps;
    MMRESULT result;
    UINT deviceId = MIDI_MAPPER; // Default

    // Try to find device by name substring
    for (UINT i = 0; i < numDevs; i++)
    {
        if (midiOutGetDevCaps(i, &caps, sizeof(MIDIOUTCAPS)) == MMSYSERR_NOERROR)
        {
            if (strstr(caps.szPname, device_name) != NULL)
            {
                deviceId = i;
                break;
            }
        }
    }

    // If not found by name, try as a device ID number
    if (deviceId == MIDI_MAPPER)
    {
        char *endptr;
        long num = strtol(device_name, &endptr, 10);
        if (*device_name != '\0' && *endptr == '\0' && num >= 0 && num < numDevs)
        {
            deviceId = (UINT)num;
        }
    }

    // Open the device
    result = midiOutOpen(handle, deviceId, 0, 0, CALLBACK_NULL);
    if (result != MMSYSERR_NOERROR)
    {
        log_error("Failed to open MIDI output device: %s", device_name);
        return -1;
    }

    log_info("Opened MIDI output device: %s (ID: %u)", device_name, deviceId);
    return 0;
}

int platform_midi_close_input(platform_midiin_t handle)
{
    if (handle)
    {
        midiInStop(handle);
        midiInReset(handle);
        midiInClose(handle);
        log_debug("Closed MIDI input device");
    }
    return 0;
}

int platform_midi_close_output(platform_midiout_t handle)
{
    if (handle)
    {
        midiOutReset(handle);
        midiOutClose(handle);
        log_debug("Closed MIDI output device");
    }
    return 0;
}

int platform_midi_send(platform_midiout_t handle, const void *data, size_t len)
{
    const unsigned char *pos = (const unsigned char *)data;
    MMRESULT result;

    if (!handle || !data || len == 0)
        return -1;

    // Check if it's a SysEx message (starts with 0xF0)
    if (pos[0] == 0xF0)
    {
        MIDIHDR midiHdr;

        // Set up the MIDI output header
        ZeroMemory(&midiHdr, sizeof(MIDIHDR));
        midiHdr.lpData = (LPSTR)data;
        midiHdr.dwBufferLength = (DWORD)len;
        midiHdr.dwFlags = 0;

        result = midiOutPrepareHeader(handle, &midiHdr, sizeof(MIDIHDR));
        if (result != MMSYSERR_NOERROR)
        {
            log_error("Failed to prepare MIDI header: %u", result);
            return -1;
        }

        result = midiOutLongMsg(handle, &midiHdr, sizeof(MIDIHDR));
        if (result != MMSYSERR_NOERROR)
        {
            midiOutUnprepareHeader(handle, &midiHdr, sizeof(MIDIHDR));
            log_error("Failed to send MIDI long message: %u", result);
            return -1;
        }

        // Wait for the message to be sent
        int wait_count = 0;
        while (!(midiHdr.dwFlags & MHDR_DONE) && wait_count < 100)
        {
            Sleep(1);
            wait_count++;
        }

        result = midiOutUnprepareHeader(handle, &midiHdr, sizeof(MIDIHDR));
        if (result != MMSYSERR_NOERROR)
        {
            log_error("Failed to unprepare MIDI header: %u", result);
            return -1;
        }
    }
    else
    {
        // Short MIDI message
        DWORD message = 0;
        size_t copy_len = (len > 4) ? 4 : len;

        // Copy data to DWORD (little-endian)
        for (size_t i = 0; i < copy_len; i++)
        {
            message |= ((DWORD)pos[i]) << (8 * i);
        }

        result = midiOutShortMsg(handle, message);
        if (result != MMSYSERR_NOERROR)
        {
            log_error("Failed to send MIDI short message: %u", result);
            return -1;
        }
    }

    log_debug("Sent %zu bytes of MIDI data", len);
    return 0;
}

int platform_midi_add_buffer(platform_midiin_t handle, void *buffer, size_t len)
{
    if (!handle || !buffer || len == 0)
        return -1;

    MIDIHDR *midiHdr = (MIDIHDR *)calloc(1, sizeof(MIDIHDR));
    if (!midiHdr)
    {
        log_error("Failed to allocate MIDI header");
        return -1;
    }

    midiHdr->lpData = buffer;
    midiHdr->dwBufferLength = (DWORD)len;
    midiHdr->dwFlags = 0;

    MMRESULT result = midiInPrepareHeader(handle, midiHdr, sizeof(MIDIHDR));
    if (result != MMSYSERR_NOERROR)
    {
        log_error("Failed to prepare MIDI input header: %u", result);
        free(midiHdr);
        return -1;
    }

    result = midiInAddBuffer(handle, midiHdr, sizeof(MIDIHDR));
    if (result != MMSYSERR_NOERROR)
    {
        log_error("Failed to add MIDI input buffer: %u", result);
        midiInUnprepareHeader(handle, midiHdr, sizeof(MIDIHDR));
        free(midiHdr);
        return -1;
    }

    log_debug("Added %zu bytes MIDI input buffer", len);
    return 0;
}

int platform_midi_set_callback(platform_midiin_t handle, platform_midi_callback_t callback, void *user_data)
{
    if (!handle || !callback)
        return -1;

    midi_callback_info *info = (midi_callback_info *)calloc(1, sizeof(midi_callback_info));
    if (!info)
    {
        log_error("Failed to allocate MIDI callback info");
        return -1;
    }

    info->callback = callback;
    info->user_data = user_data;

    // Close and reopen with callback
    UINT deviceId;
    if (midiInGetID(handle, &deviceId) != MMSYSERR_NOERROR)
    {
        log_error("Failed to get MIDI device ID");
        free(info);
        return -1;
    }

    midiInStop(handle);
    midiInReset(handle);
    midiInClose(handle);

    MMRESULT result = midiInOpen(&handle, deviceId, (DWORD_PTR)midi_input_callback,
                                 (DWORD_PTR)info, CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR)
    {
        log_error("Failed to reopen MIDI device with callback: %u", result);
        free(info);
        return -1;
    }

    result = midiInStart(handle);
    if (result != MMSYSERR_NOERROR)
    {
        log_error("Failed to start MIDI input: %u", result);
        midiInClose(handle);
        free(info);
        return -1;
    }

    log_debug("Set MIDI input callback");
    return 0;
}

#else
// POSIX MIDI implementations would go here
// Basic stubs for Linux/macOS - would need to be expanded with platform-specific MIDI APIs

int platform_midi_init(void)
{
    log_info("MIDI subsystem initialized (POSIX stub)");
    return 0; // Not implemented
}

void platform_midi_cleanup(void)
{
    log_info("MIDI subsystem cleaned up (POSIX stub)");
    // Not implemented
}

int platform_midi_open_input(const char *device_name, platform_midiin_t *handle)
{
    if (!device_name || !handle)
        return -1;

    // For POSIX, we might open a device like /dev/midi
    *handle = open(device_name, O_RDONLY | O_NONBLOCK);
    if (*handle < 0)
    {
        log_error("Failed to open MIDI input device: %s: %s", device_name, strerror(errno));
        return -1;
    }

    log_info("Opened MIDI input device: %s", device_name);
    return 0;
}

int platform_midi_open_output(const char *device_name, platform_midiout_t *handle)
{
    if (!device_name || !handle)
        return -1;

    // For POSIX, we might open a device like /dev/midi
    *handle = open(device_name, O_WRONLY);
    if (*handle < 0)
    {
        log_error("Failed to open MIDI output device: %s: %s", device_name, strerror(errno));
        return -1;
    }

    log_info("Opened MIDI output device: %s", device_name);
    return 0;
}

int platform_midi_close_input(platform_midiin_t handle)
{
    if (handle >= 0)
    {
        close(handle);
        log_debug("Closed MIDI input device");
    }
    return 0;
}

int platform_midi_close_output(platform_midiout_t handle)
{
    if (handle >= 0)
    {
        close(handle);
        log_debug("Closed MIDI output device");
    }
    return 0;
}

int platform_midi_send(platform_midiout_t handle, const void *data, size_t len)
{
    if (handle < 0 || !data || len == 0)
        return -1;

    // Simple write to the file descriptor
    ssize_t written = write(handle, data, len);
    if (written != (ssize_t)len)
    {
        log_error("Failed to send MIDI data: %s", strerror(errno));
        return -1;
    }

    log_debug("Sent %zu bytes of MIDI data", len);
    return 0;
}

int platform_midi_add_buffer(platform_midiin_t handle, void *buffer, size_t len)
{
    // Not directly applicable to POSIX, would depend on the specific MIDI library
    log_info("MIDI add buffer not implemented for POSIX");
    return -1;
}

int platform_midi_set_callback(platform_midiin_t handle, platform_midi_callback_t callback, void *user_data)
{
    // Not directly applicable to POSIX, would depend on the specific MIDI library
    log_info("MIDI callback not implemented for POSIX");
    return -1;
}
#endif

/* Mutex functions */

int platform_mutex_init(platform_mutex_t *mutex)
{
    if (!mutex)
        return -1;

#if defined(PLATFORM_WINDOWS)
    *mutex = CreateMutex(NULL, FALSE, NULL);
    return (*mutex == NULL) ? -1 : 0;
#else
    return pthread_mutex_init(mutex, NULL);
#endif
}

int platform_mutex_destroy(platform_mutex_t *mutex)
{
    if (!mutex)
        return -1;

#if defined(PLATFORM_WINDOWS)
    if (*mutex != NULL)
    {
        CloseHandle(*mutex);
        *mutex = NULL;
    }
    return 0;
#else
    return pthread_mutex_destroy(mutex);
#endif
}

int platform_mutex_lock(platform_mutex_t *mutex)
{
    if (!mutex)
        return -1;

#if defined(PLATFORM_WINDOWS)
    DWORD result = WaitForSingleObject(*mutex, INFINITE);
    return (result == WAIT_OBJECT_0 || result == WAIT_ABANDONED) ? 0 : -1;
#else
    return pthread_mutex_lock(mutex);
#endif
}

int platform_mutex_unlock(platform_mutex_t *mutex)
{
    if (!mutex)
        return -1;

#if defined(PLATFORM_WINDOWS)
    return ReleaseMutex(*mutex) ? 0 : -1;
#else
    return pthread_mutex_unlock(mutex);
#endif
}

/**
 * @brief Get platform error code
 *
 * @return Error code
 */
int platform_errno(void)
{
#if defined(PLATFORM_WINDOWS)
    return WSAGetLastError();
#else
    return errno;
#endif
}

/**
 * @brief Get error message string
 *
 * @param errnum Error code
 * @return Error message string
 */
const char *platform_strerror(int errnum)
{
#if defined(PLATFORM_WINDOWS)
    static char buf[256];

    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errnum,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        buf,
        sizeof(buf),
        NULL);

    // Remove trailing newlines/carriage returns
    char *p = buf + strlen(buf) - 1;
    while (p >= buf && (*p == '\r' || *p == '\n'))
        *p-- = '\0';

    return buf;
#else
    return strerror(errnum);
#endif
}

/**
 * @brief Open a socket
 *
 * @param address Address string in the format "udp!host!port"
 * @param server 1 for server (bind), 0 for client (connect)
 * @return Socket handle or PLATFORM_INVALID_SOCKET on error
 */
platform_socket_t platform_socket_open(const char *address, int server)
{
    char proto[16], host[256], *port_str;
    int port, ret;
    struct sockaddr_in addr;
    platform_socket_t sock = PLATFORM_INVALID_SOCKET;

    // Initialize socket system if needed
    static int socket_initialized = 0;
    if (!socket_initialized)
    {
        platform_socket_init();
        socket_initialized = 1;
    }

    // Parse the address string (format: "udp!host!port")
    if (sscanf(address, "%15[^!]!%255[^!]!%ms", proto, host, &port_str) != 3)
    {
        log_error("Invalid address format: %s (expected proto!host!port)", address);
        return PLATFORM_INVALID_SOCKET;
    }

    port = atoi(port_str);
    free(port_str);

    // Only UDP is supported for now
    if (strcmp(proto, "udp") != 0)
    {
        log_error("Unsupported protocol: %s (only udp is supported)", proto);
        return PLATFORM_INVALID_SOCKET;
    }

    // Create socket
    sock = platform_socket_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == PLATFORM_INVALID_SOCKET)
    {
        log_error("Failed to create socket: %s", platform_strerror(platform_errno()));
        return PLATFORM_INVALID_SOCKET;
    }

    // Set up address structure
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (server)
    {
        // For server (receiver), bind to the port
        int optval = 1;

        // Allow address reuse
        if (platform_socket_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                                       (const char *)&optval, sizeof(optval)) != 0)
        {
            log_warning("Failed to set SO_REUSEADDR: %s", platform_strerror(platform_errno()));
        }

        // Check if this is a multicast address
        if (strncmp(host, "224.", 4) == 0 || strncmp(host, "239.", 4) == 0)
        {
            struct ip_mreq mreq;

            // Set up the multicast group
            mreq.imr_multiaddr.s_addr = inet_addr(host);
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);

            // Join the multicast group
            if (platform_socket_setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                                           (const char *)&mreq, sizeof(mreq)) != 0)
            {
                log_error("Failed to join multicast group: %s", platform_strerror(platform_errno()));
                platform_socket_close(sock);
                return PLATFORM_INVALID_SOCKET;
            }

            log_info("Joined multicast group: %s", host);
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
        }
        else if (strcmp(host, "0.0.0.0") == 0 || strcmp(host, "*") == 0)
        {
            // Bind to all interfaces
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
        }
        else
        {
            // Bind to specific interface
            addr.sin_addr.s_addr = inet_addr(host);
        }

        // Bind to the address
        if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0)
        {
            log_error("Failed to bind socket: %s", platform_strerror(platform_errno()));
            platform_socket_close(sock);
            return PLATFORM_INVALID_SOCKET;
        }

        log_info("Socket bound to %s:%d", host, port);
    }
    else
    {
        // For client (sender), connect to the destination
        addr.sin_addr.s_addr = inet_addr(host);

        if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0)
        {
            log_error("Failed to connect socket: %s", platform_strerror(platform_errno()));
            platform_socket_close(sock);
            return PLATFORM_INVALID_SOCKET;
        }

        log_info("Socket connected to %s:%d", host, port);
    }

    return sock;
}

/**
 * @brief Get standard input stream
 *
 * @return Standard input stream
 */
platform_stream_t *platform_get_stdin(void)
{
    return stdin;
}

/**
 * @brief Get standard output stream
 *
 * @return Standard output stream
 */
platform_stream_t *platform_get_stdout(void)
{
    return stdout;
}

/**
 * @brief Read line from stream
 *
 * @param buf Buffer to read into
 * @param size Buffer size
 * @param stream Input stream
 * @return Buffer pointer or NULL on error or EOF
 */
char *platform_gets(char *buf, size_t size, platform_stream_t *stream)
{
    if (!buf || !stream || size == 0)
        return NULL;

    if (fgets(buf, (int)size, stream) == NULL)
        return NULL;

    // Remove trailing newline if present
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
        buf[len - 1] = '\0';

    return buf;
}

/**
 * @brief Write formatted string to stream
 *
 * @param stream Output stream
 * @param format Format string
 * @param ... Format arguments
 * @return Number of characters written or negative on error
 */
int platform_printf(platform_stream_t *stream, const char *format, ...)
{
    if (!stream || !format)
        return -1;

    va_list args;
    int ret;

    va_start(args, format);
    ret = vfprintf(stream, format, args);
    va_end(args);

    return ret;
}

/**
 * @brief Write data to stream
 *
 * @param buf Data buffer
 * @param size Data size
 * @param stream Output stream
 * @return Number of items written
 */
size_t platform_write(const void *buf, size_t size, platform_stream_t *stream)
{
    if (!buf || !stream || size == 0)
        return 0;

    return fwrite(buf, 1, size, stream);
}

/**
 * @brief Read data from stream
 *
 * @param buf Buffer to read into
 * @param size Item size
 * @param stream Input stream
 * @return Number of items read
 */
size_t platform_read(void *buf, size_t size, platform_stream_t *stream)
{
    if (!buf || !stream || size == 0)
        return 0;

    return fread(buf, 1, size, stream);
}

/**
 * @brief Flush stream output
 *
 * @param stream Output stream
 * @return 0 on success, non-zero on failure
 */
int platform_flush(platform_stream_t *stream)
{
    if (!stream)
        return -1;

    return fflush(stream);
}

/**
 * @brief Check if stream has error
 *
 * @param stream Stream to check
 * @return Non-zero if error, 0 otherwise
 */
int platform_error(platform_stream_t *stream)
{
    if (!stream)
        return -1;

    return ferror(stream);
}

/**
 * @brief Create a thread
 *
 * @param thread Pointer to thread handle
 * @param start_routine Thread entry point
 * @param arg Thread argument
 * @return 0 on success, error code on failure
 */
int platform_thread_create(platform_thread_t *thread, void *(*start_routine)(void *), void *arg)
{
    if (!thread || !start_routine)
        return -1;

#if defined(PLATFORM_WINDOWS)
    unsigned thread_id;
    *thread = (HANDLE)_beginthreadex(NULL, 0,
                                     (unsigned(__stdcall *)(void *))start_routine,
                                     arg, 0, &thread_id);
    return (*thread == NULL) ? GetLastError() : 0;
#else
    return pthread_create(thread, NULL, start_routine, arg);
#endif
}

/**
 * @brief Join a thread
 *
 * @param thread Thread handle
 * @param retval Pointer to store return value
 * @return 0 on success, error code on failure
 */
int platform_thread_join(platform_thread_t thread, void **retval)
{
#if defined(PLATFORM_WINDOWS)
    DWORD result = WaitForSingleObject(thread, INFINITE);
    if (result != WAIT_OBJECT_0)
    {
        return GetLastError();
    }

    if (retval)
    {
        DWORD exit_code;
        GetExitCodeThread(thread, &exit_code);
        *retval = (void *)(uintptr_t)exit_code;
    }

    CloseHandle(thread);
    return 0;
#else
    return pthread_join(thread, retval);
#endif
}
