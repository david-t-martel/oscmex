#include "platform.h"

#if defined(PLATFORM_WINDOWS)
#include <shlobj.h> // For SHGetFolderPath
#endif

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
    snprintf(buffer, size, "%s/Library/Application Support", home);
#else
    // Linux/FreeBSD: ~/.local/share
    snprintf(buffer, size, "%s/.local/share", home);
#endif

    return 0;
#endif
}

int platform_ensure_directory(const char *path)
{
    if (!path || strlen(path) == 0)
        return -1;

    // Make a copy of the path that we can modify
    char dir_path[1024];
    strncpy(dir_path, path, sizeof(dir_path) - 1);
    dir_path[sizeof(dir_path) - 1] = '\0';

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
                    return -1;
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

#if defined(PLATFORM_WINDOWS)
int platform_socket_init(void)
{
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData);
}

void platform_socket_cleanup(void)
{
    WSACleanup();
}
#else
int platform_socket_init(void)
{
    return 0; // No initialization needed on POSIX
}

void platform_socket_cleanup(void)
{
    // No cleanup needed on POSIX
}
#endif

platform_socket_t platform_socket_create(int domain, int type, int protocol)
{
#if defined(PLATFORM_WINDOWS)
    return socket(domain, type, protocol);
#else
    return socket(domain, type, protocol);
#endif
}

int platform_socket_close(platform_socket_t socket)
{
#if defined(PLATFORM_WINDOWS)
    return closesocket(socket);
#else
    return close(socket);
#endif
}

// In platform.c
int platform_get_device_config_dir(char *buffer, size_t size)
{
    char app_dir[256];
    int result;

    result = platform_get_app_data_dir(app_dir, sizeof(app_dir));
    if (result != 0)
    {
        return result;
    }

    snprintf(buffer, size, "%s%cOSCMix%cdevice_config",
             app_dir, PLATFORM_PATH_SEPARATOR, PLATFORM_PATH_SEPARATOR);

    return platform_ensure_directory(buffer);
}

int platform_create_valid_filename(const char *input, char *output, size_t output_size)
{
    size_t i, j = 0;
    size_t input_len = strlen(input);

    if (!output || output_size == 0)
    {
        return -1;
    }

    for (i = 0; i < input_len && j < output_size - 1; i++)
    {
        char c = input[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-')
        {
            output[j++] = c;
        }
        else if (c == ' ' || c == '.' || c == ',')
        {
            output[j++] = '_';
        }
    }

    output[j] = '\0';
    return 0;
}
