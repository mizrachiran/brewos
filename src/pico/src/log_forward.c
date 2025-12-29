/**
 * Pico Firmware - Log Forwarding to ESP32
 * 
 * Forwards Pico log messages to ESP32 via UART protocol.
 * Setting is persisted to flash and survives reboots.
 * Controlled via web UI (dev mode only).
 */

#include "log_forward.h"
#include "config_persistence.h"
#include "protocol.h"
#include "config.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

// =============================================================================
// State
// =============================================================================

static bool g_enabled = false;
static bool g_initialized = false;

// =============================================================================
// Public Functions
// =============================================================================

void log_forward_init(void) {
    // Load enabled state from flash
    g_enabled = config_persistence_get_log_forwarding();
    g_initialized = true;
    
    if (g_enabled) {
        LOG_PRINT("Log: Forwarding enabled (loaded from flash)\n");
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
    
    // Persist to flash
    config_persistence_save_log_forwarding(enabled);
    
    LOG_PRINT("Log: Forwarding %s\n", enabled ? "enabled" : "disabled");
}

bool log_forward_is_enabled(void) {
    return g_enabled;
}

void log_forward_send(uint8_t level, const char* message) {
    if (!g_enabled || !g_initialized || !message) {
        return;
    }
    
    // Send via protocol
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
    log_forward_set_enabled(enabled);
    
    // Send ACK (we don't have seq here, but let's send general ACK)
    // The ESP32 will update its state based on the command it sent
}

