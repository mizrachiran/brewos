#ifndef LOG_FORWARD_H
#define LOG_FORWARD_H

#include <stdint.h>
#include <stdbool.h>

// Log levels (matches ESP32 LogLevel enum)
#define LOG_FWD_ERROR   0
#define LOG_FWD_WARN    1
#define LOG_FWD_INFO    2
#define LOG_FWD_DEBUG   3

/**
 * Initialize log forwarding
 * Loads enabled state from flash
 */
void log_forward_init(void);

/**
 * Enable or disable log forwarding to ESP32
 * Setting is persisted to flash
 * @param enabled true to enable, false to disable
 */
void log_forward_set_enabled(bool enabled);

/**
 * Check if log forwarding is enabled
 * @return true if enabled
 */
bool log_forward_is_enabled(void);

/**
 * Forward a log message to ESP32 (if enabled)
 * @param level Log level (LOG_FWD_ERROR, LOG_FWD_WARN, LOG_FWD_INFO, LOG_FWD_DEBUG)
 * @param message Log message
 */
void log_forward_send(uint8_t level, const char* message);

/**
 * Forward a formatted log message to ESP32 (if enabled)
 * @param level Log level
 * @param format printf-style format string
 * @param ... format arguments
 */
void log_forward_sendf(uint8_t level, const char* format, ...);

/**
 * Process log forwarding command from ESP32
 * @param payload Command payload (1 byte: enabled flag)
 * @param length Payload length
 */
void log_forward_handle_command(const uint8_t* payload, uint8_t length);

/**
 * Process pending flash writes (call from main loop)
 * Deferred flash writes to avoid blocking protocol handler
 */
void log_forward_process(void);

#endif // LOG_FORWARD_H

