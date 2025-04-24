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
static void (*gui_log_callback)(log_level_t level, const char *message) = NULL;
static platform_mutex_t log_mutex;

// Circular buffer of recent log messages for GUI retrieval
static char *recent_logs[LOG_HISTORY_SIZE] = {NULL};
static int log_index = 0;

// Level names for output formatting
static const char *level_names[] = {
    "ERROR",
    "WARNING",
    "INFO",
    "DEBUG"};

int log_init(const char *filename, int debug_enabled, void (*callback)(log_level_t level, const char *message))
{
    // Create mutex for thread safety
    if (platform_mutex_init(&log_mutex) != 0)
    {
        fprintf(stderr, "Failed to initialize logging mutex\n");
        return -1;
    }

    debug_mode = debug_enabled;
    gui_log_callback = callback;

    // Initialize log history
    for (int i = 0; i < LOG_HISTORY_SIZE; i++)
    {
        recent_logs[i] = NULL;
    }

    // Open log file if requested
    if (filename)
    {
        log_file = fopen(filename, "a");
        if (!log_file)
        {
            fprintf(stderr, "Failed to open log file: %s\n", filename);
            return -1;
        }

        // Write header to log file
        time_t now = time(NULL);
        struct tm *timeinfo = localtime(&now);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

        fprintf(log_file, "\n--- OSCMix Log Started: %s ---\n", timestamp);
        fflush(log_file);
    }

    log_info("Logging system initialized (debug=%s)", debug_enabled ? "on" : "off");
    return 0;
}

void log_cleanup(void)
{
    platform_mutex_lock(&log_mutex);

    // Close log file if open
    if (log_file)
    {
        time_t now = time(NULL);
        struct tm *timeinfo = localtime(&now);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

        fprintf(log_file, "--- OSCMix Log Ended: %s ---\n\n", timestamp);
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

    platform_mutex_unlock(&log_mutex);
    platform_mutex_destroy(&log_mutex);
}

void log_message(log_level_t level, const char *fmt, ...)
{
    // Skip debug messages if debug mode is off
    if (level == LOG_DEBUG && !debug_mode)
    {
        return;
    }

    // Skip info messages if verbose mode is off
    if (level == LOG_INFO && !debug_mode)
    {
        return;
    }

    platform_mutex_lock(&log_mutex);

    // Format timestamp
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    // Format log message
    char message[MAX_LOG_ENTRY_SIZE];
    int prefix_len = snprintf(message, sizeof(message), "%s [%s] ",
                              timestamp, level_names[level]);

    va_list args;
    va_start(args, fmt);
    vsnprintf(message + prefix_len, sizeof(message) - prefix_len, fmt, args);
    va_end(args);

    // Write to stderr
    fprintf(stderr, "%s\n", message);

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

    // Call GUI callback if registered
    if (gui_log_callback)
    {
        gui_log_callback(level, message);
    }

    platform_mutex_unlock(&log_mutex);
}

int log_get_recent(const char **messages, int max_messages)
{
    int count = 0;

    platform_mutex_lock(&log_mutex);

    // Start from the oldest entry in the circular buffer
    int start = log_index;
    for (int i = 0; i < LOG_HISTORY_SIZE && count < max_messages; i++)
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
