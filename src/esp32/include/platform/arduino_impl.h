/**
 * Arduino/ESP32 Platform Implementation
 * 
 * Provides platform functions using Arduino framework.
 */

#ifndef ARDUINO_IMPL_H
#define ARDUINO_IMPL_H

#include <Arduino.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline uint32_t platform_millis(void) {
    return millis();
}

static inline void platform_delay(uint32_t ms) {
    delay(ms);
}

#ifdef __cplusplus
}
#endif

// Include log_manager.h for LogSource enum and function declaration
// This is safe - no circular dependency (log_manager.h includes config.h, but config.h doesn't include platform.h)
#include "log_manager.h"

// Include state_manager.h to check debug logs setting
// This is safe - state_manager.h doesn't include platform.h
#include "state/state_manager.h"

// Forward declaration for web server (defined in main.cpp)
// Note: We can't include web_server.h here due to potential circular dependencies,
// so we'll use a function pointer approach or check at runtime
extern class BrewWebServer* webServer;

// Helper function to broadcast log via WebSocket (defined in web_server_broadcast.cpp)
// This avoids needing to include web_server.h here
extern "C" void platform_broadcast_log(const char* level, const char* message);

/**
 * BrewOS Unified Logging System
 * 
 * This function handles all three log outputs:
 * 1. Serial - Always outputs to Serial for debugging
 * 2. Log Buffer - Writes to 50KB circular buffer when enabled (for download)
 * 3. WebSocket - Broadcasts to connected UI clients (INFO and above)
 * 
 * @param level Log level (DEBUG, INFO, WARN, ERROR)
 * @param fmt printf-style format string
 * @param ... Format arguments
 */
static inline void platform_log(log_level_t level, const char* fmt, ...) {
    // 1. Format the message once (reused for all outputs)
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    // Get level string for Serial output
    const char* level_str;
    switch(level) {
        case BREWOS_LOG_DEBUG: level_str = "D"; break;
        case BREWOS_LOG_INFO:  level_str = "I"; break;
        case BREWOS_LOG_WARN:  level_str = "W"; break;
        case BREWOS_LOG_ERROR: level_str = "E"; break;
        default: level_str = "?"; break;
    }
    
    // 2. Serial output (always enabled)
    Serial.printf("[%lu] %s: %s\n", millis(), level_str, buf);
    
    // 3. Log buffer (if enabled - writes to 50KB circular buffer for download)
    // Use log_manager_add_logf with %s to pass the already-formatted message
    // This avoids type conversion issues between log_level_t and BrewOSLogLevel
    log_manager_add_logf((int)level, LOG_SOURCE_ESP32, "%s", buf);
    
    // 4. WebSocket broadcast (INFO and above, or DEBUG if debug logs enabled)
    // This sends logs to connected UI clients in real-time
    bool shouldBroadcast = (level >= BREWOS_LOG_INFO);
    if (level == BREWOS_LOG_DEBUG) {
        // Check if debug logs are enabled in settings
        shouldBroadcast = State.settings().system.debugLogsEnabled;
    }
    
    if (shouldBroadcast) {
        const char* level_name;
        switch(level) {
            case BREWOS_LOG_DEBUG: level_name = "debug"; break;
            case BREWOS_LOG_INFO:  level_name = "info"; break;
            case BREWOS_LOG_WARN:  level_name = "warn"; break;
            case BREWOS_LOG_ERROR: level_name = "error"; break;
            default: level_name = "info"; break;
        }
        platform_broadcast_log(level_name, buf);
    }
}

#endif // ARDUINO_IMPL_H

