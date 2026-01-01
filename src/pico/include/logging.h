#ifndef LOGGING_H
#define LOGGING_H

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

/**
 * Logging System with Multiple Levels
 * 
 * Provides structured logging with filtering by level.
 * Supports both USB serial and ESP32 forwarding.
 */

// =============================================================================
// Log Levels
// =============================================================================

typedef enum {
    LOG_LEVEL_ERROR = 0,    // Critical errors
    LOG_LEVEL_WARN = 1,     // Warnings
    LOG_LEVEL_INFO = 2,     // Important information
    LOG_LEVEL_DEBUG = 3,    // Debug information
    LOG_LEVEL_TRACE = 4     // Detailed traces
} log_level_t;

// =============================================================================
// Configuration
// =============================================================================

/**
 * Initialize logging system
 */
void logging_init(void);

/**
 * Set minimum log level (messages below this are filtered)
 * @param level Minimum level to log
 */
void logging_set_level(log_level_t level);

/**
 * Get current log level
 * @return Current minimum log level
 */
log_level_t logging_get_level(void);

/**
 * Enable/disable ESP32 forwarding
 * @param enable True to forward logs to ESP32
 */
void logging_set_forward_enabled(bool enable);

/**
 * Check if ESP32 forwarding is enabled
 * @return True if forwarding enabled
 */
bool logging_is_forward_enabled(void);

// =============================================================================
// Logging Functions
// =============================================================================

/**
 * Log a message at specified level
 * @param level Log level
 * @param format Printf-style format string
 */
void log_message(log_level_t level, const char* format, ...) __attribute__((format(printf, 2, 3)));

/**
 * Log a message with va_list
 * @param level Log level
 * @param format Printf-style format string
 * @param args Variable arguments list
 */
void log_message_va(log_level_t level, const char* format, va_list args);

// =============================================================================
// Convenience Macros
// =============================================================================

#define LOG_ERROR(fmt, ...) log_message(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  log_message(LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  log_message(LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) log_message(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_TRACE(fmt, ...) log_message(LOG_LEVEL_TRACE, fmt, ##__VA_ARGS__)

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * Get log level name
 * @param level Log level
 * @return Level name string (never NULL)
 */
const char* log_level_name(log_level_t level);

/**
 * Process pending log messages from ring buffer
 * Call this periodically (e.g., from Core 1 or background task) to drain the buffer
 * This ensures non-blocking logging by deferring actual printf() calls
 */
void logging_process_pending(void);

#endif // LOGGING_H
