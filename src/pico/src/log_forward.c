/**
 * Pico Firmware - Log Forwarding to ESP32
 * 
 * Forwards Pico log messages to ESP32 via UART protocol.
 * Setting is persisted to flash and survives reboots.
 * Controlled via web UI (dev mode only).
 */

#include "log_forward.h"
#include "logging.h"  // For logging_set_forward_enabled()
#include "config_persistence.h"
#include "protocol.h"
#include "config.h"
#include "pico/time.h"  // For time functions
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

// =============================================================================
// State
// =============================================================================

static bool g_enabled = false;
static bool g_initialized = false;
static bool g_pending_flash_write = false;
static bool g_pending_flash_value = false;
static uint32_t g_last_send_time = 0;
static const uint32_t LOG_SEND_MIN_INTERVAL_MS = 2;  // Minimum 2ms between log sends (allows up to 500 logs/sec)
                                                    // MSG_LOG doesn't require ACK, so we can be more permissive

// =============================================================================
// Public Functions
// =============================================================================

void log_forward_init(void) {
    // Load enabled state from flash
    g_enabled = config_persistence_get_log_forwarding();
    g_initialized = true;
    
    if (g_enabled) {
        // Use direct printf to avoid recursion during initialization
        printf("Log: Forwarding enabled (loaded from flash)\n");
    }
}

void log_forward_set_enabled(bool enabled) {
    if (!g_initialized) {
        return;
    }
    
    if (g_enabled == enabled) {
        return;  // No change
    }
    
    g_enabled = enabled;
    
    // Defer flash write to avoid blocking protocol handler
    // Flash write will happen in log_forward_process() called from main loop
    g_pending_flash_write = true;
    g_pending_flash_value = enabled;
    
    // Use direct printf to avoid recursion - this log message should not be forwarded
    // because we're in the middle of changing the forwarding state
    printf("Log: Forwarding %s\n", enabled ? "enabled" : "disabled");
}

// Process pending flash writes (call from main loop, not from interrupt/protocol handler)
void log_forward_process(void) {
    if (g_pending_flash_write) {
        g_pending_flash_write = false;
        // Now safe to do blocking flash write from main loop
        config_persistence_save_log_forwarding(g_pending_flash_value);
    }
}

bool log_forward_is_enabled(void) {
    return g_enabled;
}

void log_forward_send(uint8_t level, const char* message) {
    if (!g_enabled || !g_initialized || !message) {
        return;
    }
    
    // Rate limiting: Don't send logs too frequently to prevent protocol flooding
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - g_last_send_time < LOG_SEND_MIN_INTERVAL_MS) {
        return;  // Skip this log to prevent flooding
    }
    g_last_send_time = now;
    
    // Send via protocol
    // Note: MSG_LOG is excluded from ACK tracking to prevent protocol overload
    protocol_send_log(level, message);
}

void log_forward_sendf(uint8_t level, const char* format, ...) {
    if (!g_enabled || !g_initialized || !format) {
        return;
    }
    
    char buffer[200];  // Max log message size
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    log_forward_send(level, buffer);
}

void log_forward_handle_command(const uint8_t* payload, uint8_t length) {
    if (!payload || length < 1) {
        return;
    }
    
    bool enabled = payload[0] != 0;
    
    // Update both the log_forward state and the logging system's forwarding flag
    // We call logging_set_forward_enabled() which will:
    // 1. Set g_forward_enabled in logging.c
    // 2. Call log_forward_set_enabled() which persists to flash
    // The early return in log_forward_set_enabled() prevents infinite recursion
    logging_set_forward_enabled(enabled);
    
    // Send ACK (we don't have seq here, but let's send general ACK)
    // The ESP32 will update its state based on the command it sent
}

