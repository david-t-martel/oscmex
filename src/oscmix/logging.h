#ifndef LOGGING_H
#define LOGGING_H

#include <stdarg.h>

// Log levels
typedef enum
{
    LOG_ERROR = 0,   // Critical errors (always logged)
    LOG_WARNING = 1, // Warnings (always logged)
    LOG_INFO = 2,    // Informational messages (logged if verbose)
    LOG_DEBUG = 3    // Debug messages (logged if debug enabled)
} log_level_t;

/**
 * @brief Initialize the logging system
 *
 * @param filename Path to log file (NULL for stderr only)
 * @param debug_enabled Whether to enable debug-level logging
 * @param gui_callback Function to call when logging for GUI notification
 * @return 0 on success, non-zero on failure
 */
int log_init(const char *filename, int debug_enabled, void (*gui_callback)(log_level_t level, const char *message));

/**
 * @brief Clean up the logging system
 */
void log_cleanup(void);

/**
 * @brief Log a message
 *
 * @param level The log level
 * @param fmt Printf-style format string
 * @param ... Format arguments
 */
void log_message(log_level_t level, const char *fmt, ...);

// Convenience macros
#define log_error(fmt, ...) log_message(LOG_ERROR, fmt, ##__VA_ARGS__)
#define log_warning(fmt, ...) log_message(LOG_WARNING, fmt, ##__VA_ARGS__)
#define log_info(fmt, ...) log_message(LOG_INFO, fmt, ##__VA_ARGS__)
#define log_debug(fmt, ...) log_message(LOG_DEBUG, fmt, ##__VA_ARGS__)

/**
 * @brief Get last N log messages for GUI display
 *
 * @param messages Array to fill with message pointers
 * @param max_messages Maximum number of messages to retrieve
 * @return Number of messages retrieved
 */
int log_get_recent(const char **messages, int max_messages);

#endif /* LOGGING_H */
