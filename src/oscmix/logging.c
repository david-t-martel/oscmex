#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include "platform.h"

#define MAX_LOG_ENTRY_SIZE 1024
#define LOG_HISTORY_SIZE 100

// Log state
static FILE *log_file = NULL;
static int debug_mode = 0;
static log_level_t current_log_level = LOG_WARNING;
static void (*gui_log_callback)(log_level_t level, const char *message) = NULL;
static platform_mutex_t log_mutex;
static int log_mutex_initialized = 0;

// Circular buffer of recent log messages for GUI retrieval
static char *recent_logs[LOG_HISTORY_SIZE] = {NULL};
static int log_index = 0;
static int log_count = 0;

// Level names for output formatting
static const char *level_names[] = {
    "ERROR",
    "WARNING",
    "INFO",
    "DEBUG"};

// Colors for console output (if supported)
static const char *level_colors[] = {
    "\033[1;31m", // ERROR - bright red
    "\033[1;33m", // WARNING - bright yellow
    "\033[1;32m", // INFO - bright green
    "\033[1;36m"  // DEBUG - bright cyan
};
static const char *color_reset = "\033[0m";

int log_init(const char *filename, int debug_enabled, void (*callback)(log_level_t level, const char *message))
{
    // Create mutex for thread safety if not already initialized
    if (!log_mutex_initialized)
    {
        if (platform_mutex_init(&log_mutex) != 0)
        {
            fprintf(stderr, "Failed to initialize logging mutex\n");
            return -1;
        }
        log_mutex_initialized = 1;
    }

    platform_mutex_lock(&log_mutex);

    debug_mode = debug_enabled;
    gui_log_callback = callback;

    if (debug_enabled)
    {
        current_log_level = LOG_DEBUG;
    }

    // Initialize log history
    for (int i = 0; i < LOG_HISTORY_SIZE; i++)
    {
        if (recent_logs[i] != NULL)
        {
            free(recent_logs[i]);
            recent_logs[i] = NULL;
        }
    }
    log_index = 0;
    log_count = 0;

    // Clean up existing log file if open
    if (log_file)
    {
        fclose(log_file);
        log_file = NULL;
    }

    // Open log file if requested
    if (filename)
    {
        log_file = platform_fopen(filename, "a");
        if (!log_file)
        {
            fprintf(stderr, "Failed to open log file: %s\n", filename);
            platform_mutex_unlock(&log_mutex);
            return -1;
        }

        // Write header to log file
        char timestamp[64];
        if (platform_format_time(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S") == 0)
        {
            fprintf(log_file, "\n--- OSCMix Log Started: %s ---\n", timestamp);
        }
        else
        {
            fprintf(log_file, "\n--- OSCMix Log Started ---\n");
        }
        fflush(log_file);
    }

    platform_mutex_unlock(&log_mutex);

    log_info("Logging system initialized (debug=%s)", debug_enabled ? "on" : "off");
    return 0;
}

void log_cleanup(void)
{
    if (!log_mutex_initialized)
    {
        return;
    }

    platform_mutex_lock(&log_mutex);

    // Close log file if open
    if (log_file)
    {
        char timestamp[64];
        if (platform_format_time(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S") == 0)
        {
            fprintf(log_file, "--- OSCMix Log Ended: %s ---\n\n", timestamp);
        }
        else
        {
            fprintf(log_file, "--- OSCMix Log Ended ---\n\n");
        }
        fclose(log_file);
        log_file = NULL;
    }

    // Free log history
    for (int i = 0; i < LOG_HISTORY_SIZE; i++)
    {
        if (recent_logs[i])
        {
            free(recent_logs[i]);
            recent_logs[i] = NULL;
        }
    }
    log_count = 0;

    platform_mutex_unlock(&log_mutex);

    // Destroy mutex at the very end
    platform_mutex_destroy(&log_mutex);
    log_mutex_initialized = 0;
}

void log_set_level(log_level_t level)
{
    if (level <= LOG_DEBUG)
    {
        platform_mutex_lock(&log_mutex);
        current_log_level = level;
        platform_mutex_unlock(&log_mutex);
    }
}

log_level_t log_get_level(void)
{
    log_level_t level;
    platform_mutex_lock(&log_mutex);
    level = current_log_level;
    platform_mutex_unlock(&log_mutex);
    return level;
}

int log_set_debug(int enabled)
{
    int prev;
    platform_mutex_lock(&log_mutex);
    prev = debug_mode;
    debug_mode = enabled ? 1 : 0;

    // Also adjust the log level
    if (enabled && current_log_level < LOG_DEBUG)
    {
        current_log_level = LOG_DEBUG;
    }
    else if (!enabled && current_log_level > LOG_WARNING)
    {
        current_log_level = LOG_WARNING;
    }

    platform_mutex_unlock(&log_mutex);
    return prev;
}

void log_message_v(log_level_t level, const char *fmt, va_list args)
{
    // Skip messages above the current log level
    if (level > current_log_level)
    {
        return;
    }

    // Only lock if we're actually logging
    platform_mutex_lock(&log_mutex);

    // Format timestamp
    char timestamp[32];
    if (platform_format_time(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S") != 0)
    {
        // Fallback if platform_format_time fails
        time_t now = time(NULL);
        struct tm *timeinfo = localtime(&now);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    }

    // Format log message
    char message[MAX_LOG_ENTRY_SIZE];
    int prefix_len = snprintf(message, sizeof(message), "%s [%s] ",
                              timestamp, level_names[level]);

    vsnprintf(message + prefix_len, sizeof(message) - prefix_len, fmt, args);

// Write to stderr with colors if appropriate
#if defined(PLATFORM_POSIX)
    fprintf(stderr, "%s%s%s\n", level_colors[level], message, color_reset);
#else
    fprintf(stderr, "%s\n", message);
#endif

    // Write to log file if available
    if (log_file)
    {
        fprintf(log_file, "%s\n", message);
        fflush(log_file);
    }

    // Add to circular buffer
    if (recent_logs[log_index])
    {
        free(recent_logs[log_index]);
    }
    recent_logs[log_index] = strdup(message);
    log_index = (log_index + 1) % LOG_HISTORY_SIZE;
    if (log_count < LOG_HISTORY_SIZE)
    {
        log_count++;
    }

    // Call GUI callback if registered
    if (gui_log_callback)
    {
        gui_log_callback(level, message);
    }

    platform_mutex_unlock(&log_mutex);
}

void log_message(log_level_t level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_message_v(level, fmt, args);
    va_end(args);
}

int log_get_recent(const char **messages, int max_messages)
{
    int count = 0;

    if (!messages || max_messages <= 0)
    {
        return 0;
    }

    platform_mutex_lock(&log_mutex);

    // Calculate how many messages to return (minimum of requested, available, and buffer size)
    int available = log_count;
    if (available > max_messages)
    {
        available = max_messages;
    }

    // Find starting point in circular buffer
    int start = (log_index - available + LOG_HISTORY_SIZE) % LOG_HISTORY_SIZE;

    // Copy messages in chronological order
    for (int i = 0; i < available; i++)
    {
        int idx = (start + i) % LOG_HISTORY_SIZE;
        if (recent_logs[idx])
        {
            messages[count++] = recent_logs[idx];
        }
    }

    platform_mutex_unlock(&log_mutex);

    return count;
}

int log_clear_history(void)
{
    platform_mutex_lock(&log_mutex);

    // Free all log entries
    for (int i = 0; i < LOG_HISTORY_SIZE; i++)
    {
        if (recent_logs[i])
        {
            free(recent_logs[i]);
            recent_logs[i] = NULL;
        }
    }

    log_index = 0;
    log_count = 0;

    platform_mutex_unlock(&log_mutex);
    return 0;
}

int log_set_file(const char *filename)
{
    platform_mutex_lock(&log_mutex);

    // Close existing log file if any
    if (log_file)
    {
        char timestamp[64];
        if (platform_format_time(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S") == 0)
        {
            fprintf(log_file, "--- Log file closed: %s ---\n\n", timestamp);
        }
        else
        {
            fprintf(log_file, "--- Log file closed ---\n\n");
        }
        fclose(log_file);
        log_file = NULL;
    }

    // Open new log file if requested
    if (filename)
    {
        log_file = platform_fopen(filename, "a");
        if (!log_file)
        {
            platform_mutex_unlock(&log_mutex);
            return -1;
        }

        // Write header to the new log file
        char timestamp[64];
        if (platform_format_time(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S") == 0)
        {
            fprintf(log_file, "\n--- Log file opened: %s ---\n", timestamp);
        }
        else
        {
            fprintf(log_file, "\n--- Log file opened ---\n");
        }
        fflush(log_file);
    }

    platform_mutex_unlock(&log_mutex);
    return 0;
}

int log_create_filename(char *buffer, size_t size, const char *prefix)
{
    char timestamp[32];
    char app_dir[256];
    char log_dir[512];
    char base_name[128];

    if (!buffer || size == 0)
    {
        return -1;
    }

    // Get the application data directory
    if (platform_get_app_data_dir(app_dir, sizeof(app_dir)) != 0)
    {
        return -1;
    }

    // Create logs directory
    if (platform_path_join(log_dir, sizeof(log_dir), app_dir, "OSCMix/logs") != 0)
    {
        return -1;
    }

    // Ensure the logs directory exists
    if (platform_ensure_directory(log_dir) != 0)
    {
        return -1;
    }

    // Format timestamp for filename
    if (platform_format_time(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S") != 0)
    {
        // Fallback if platform_format_time fails
        time_t now = time(NULL);
        struct tm *timeinfo = localtime(&now);
        strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", timeinfo);
    }

    // Create base filename
    if (prefix && *prefix)
    {
        snprintf(base_name, sizeof(base_name), "%s_%s.log", prefix, timestamp);
    }
    else
    {
        snprintf(base_name, sizeof(base_name), "oscmix_%s.log", timestamp);
    }

    // Create a valid filename (remove invalid characters)
    char valid_name[128];
    if (platform_create_valid_filename(base_name, valid_name, sizeof(valid_name)) != 0)
    {
        // If it fails, use the original
        strncpy(valid_name, base_name, sizeof(valid_name) - 1);
        valid_name[sizeof(valid_name) - 1] = '\0';
    }

    // Join the directory and filename
    return platform_path_join(buffer, size, log_dir, valid_name);
}
