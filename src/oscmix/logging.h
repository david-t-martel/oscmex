#ifndef LOGGING_H
#define LOGGING_H

#include <stdarg.h>
#include "platform.h" // Include platform.h for mutex type

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
 * @brief Set the log level
 *
 * @param level Maximum level to log
 */
void log_set_level(log_level_t level);

/**
 * @brief Get the current log level
 *
 * @return Current log level
 */
log_level_t log_get_level(void);

/**
 * @brief Enable or disable debug logging
 *
 * @param enabled 1 to enable, 0 to disable
 * @return Previous state
 */
int log_set_debug(int enabled);

/**
 * @brief Log a message
 *
 * @param level The log level
 * @param fmt Printf-style format string
 * @param ... Format arguments
 */
void log_message(log_level_t level, const char *fmt, ...);

/**
 * @brief Log a message with a va_list
 *
 * @param level The log level
 * @param fmt Printf-style format string
 * @param args Variable argument list
 */
void log_message_v(log_level_t level, const char *fmt, va_list args);

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

/**
 * @brief Clear the log history buffer
 *
 * @return 0 on success, non-zero on failure
 */
int log_clear_history(void);

/**
 * @brief Set a custom log file
 *
 * @param filename Path to log file (NULL to close current file)
 * @return 0 on success, non-zero on failure
 */
int log_set_file(const char *filename);

/**
 * @brief Create a valid log filename using platform functions
 *
 * @param buffer Buffer to store the filename
 * @param size Size of the buffer
 * @param prefix Optional prefix for the log filename
 * @return 0 on success, non-zero on failure
 */
int log_create_filename(char *buffer, size_t size, const char *prefix);

#endif /* LOGGING_H */
