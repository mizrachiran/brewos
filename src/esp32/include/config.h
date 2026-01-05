#ifndef CONFIG_H
#define CONFIG_H

// Include shared protocol definitions
#include "protocol_defs.h"

// =============================================================================
// BrewOS ESP32-S3 Configuration
// Target: UEDX48480021-MD80E (2.1" Round Knob Display)
// =============================================================================

// -----------------------------------------------------------------------------
// Version
// -----------------------------------------------------------------------------
#define ESP32_VERSION_MAJOR     0
#define ESP32_VERSION_MINOR     8
#define ESP32_VERSION_PATCH     32
#define ESP32_VERSION_PRERELEASE "beta.1"  // Empty string "" for stable releases
#define ESP32_VERSION           "0.8.32"

// Build timestamp (set at compile time)
// Format: "Dec 12 2024" and "14:30:45"
#define BUILD_DATE              __DATE__
#define BUILD_TIME              __TIME__

// Update channel: "stable" or "beta"
// Users on "stable" only get released versions (no pre-release suffix)
// Users on "beta" get all versions including pre-releases
#define ESP32_DEFAULT_CHANNEL   "beta"

// -----------------------------------------------------------------------------
// Display Configuration (included from display/display_config.h)
// -----------------------------------------------------------------------------
// See display/display_config.h for pin definitions and display settings

// -----------------------------------------------------------------------------
// WiFi Configuration
// -----------------------------------------------------------------------------
#define WIFI_AP_SSID            "BrewOS-Setup"
#define WIFI_AP_PASSWORD        "brewoscoffee"  // Min 8 characters
#define WIFI_AP_CHANNEL         1
#define WIFI_AP_MAX_CONNECTIONS 4
#define WIFI_AP_IP              IPAddress(192, 168, 4, 1)
#define WIFI_AP_GATEWAY         IPAddress(192, 168, 4, 1)
#define WIFI_AP_SUBNET          IPAddress(255, 255, 255, 0)

#define WIFI_CONNECT_TIMEOUT_MS 15000
#define WIFI_RECONNECT_INTERVAL 30000

// -----------------------------------------------------------------------------
// Web Server
// -----------------------------------------------------------------------------
#define WEB_SERVER_PORT         80
#define WEBSOCKET_PATH          "/ws"

// -----------------------------------------------------------------------------
// UART - Debug/Serial Communication (Hardware UART adapter)
// -----------------------------------------------------------------------------
#define DEBUG_UART_TX_PIN       37              // Hardware UART TX (GPIO37)
#define DEBUG_UART_RX_PIN       36              // Hardware UART RX (GPIO36)
#define DEBUG_UART_BAUD         115200

// -----------------------------------------------------------------------------
// UART - Pico Communication (ESP32 side pins)
// -----------------------------------------------------------------------------
#define PICO_UART_NUM           1               // UART1
#define PICO_UART_BAUD          PROTOCOL_BAUD_RATE
#define PICO_UART_TX_PIN        43              // ESP32 TX -> Pico RX (GPIO1)
#define PICO_UART_RX_PIN        44              // ESP32 RX <- Pico TX (GPIO0)

// Pico control pins
// NOTE: GPIO20 (D-) is repurposed from USB CDC for Pico reset control
// USB CDC is disabled to free up GPIO19/20 for GPIO functions
#define PICO_RUN_PIN            20              // Controls Pico RUN (reset) → J15 Pin 5
                                                // GPIO20 = USB D- (repurposed as GPIO)

// J15 Pin 6 - SPARE1: ESP32 GPIO9 ↔ Pico GPIO16 (4.7kΩ pull-down on Pico side)
#define SPARE1_PIN              9               // J15 Pin 6 - General purpose bidirectional I/O

// Brew-by-weight signal
// NOTE: GPIO19 (D+) is repurposed from USB CDC for weight stop signal
// USB CDC is disabled to free up GPIO19/20 for GPIO functions
#define WEIGHT_STOP_PIN         19              // ESP32 GPIO19 → J15 Pin 7 → Pico GPIO21 (4.7kΩ pull-down)
                                                // GPIO19 = USB D+ (repurposed as GPIO)
                                                // Set HIGH when target weight reached, LOW otherwise

// J15 Pin 8 - SPARE2: ESP32 GPIO22 ↔ Pico GPIO22 (4.7kΩ pull-down on Pico side)
#define SPARE2_PIN              22              // J15 Pin 8 - General purpose bidirectional I/O

// Buffer sizes
#define PICO_RX_BUFFER_SIZE     256

// -----------------------------------------------------------------------------
// OTA Configuration
// -----------------------------------------------------------------------------
#define OTA_FILE_PATH           "/pico_firmware.bin"
#define OTA_MAX_SIZE            (2 * 1024 * 1024)   // 2MB max

// GitHub OTA - Download firmware from GitHub releases
#define GITHUB_OWNER            "brewos-io"
#define GITHUB_REPO             "firmware"
#define GITHUB_ESP32_ASSET      "brewos_esp32.bin"
#define GITHUB_ESP32_LITTLEFS_ASSET "brewos_esp32_littlefs.bin"

// Pico firmware assets by machine type (UF2 format)
// Use .bin files for OTA (raw binary format that bootloader can flash directly)
// UF2 format is only for USB drag-and-drop flashing
#define GITHUB_PICO_DUAL_BOILER_ASSET     "brewos_dual_boiler.bin"
#define GITHUB_PICO_SINGLE_BOILER_ASSET   "brewos_single_boiler.bin"
#define GITHUB_PICO_HEAT_EXCHANGER_ASSET  "brewos_heat_exchanger.bin"

// GitHub API URL template: https://api.github.com/repos/OWNER/REPO/releases/tags/TAG
// Asset download URL: https://github.com/OWNER/REPO/releases/download/TAG/ASSET

// -----------------------------------------------------------------------------
// Debug
// -----------------------------------------------------------------------------
#define DEBUG_SERIAL            Serial
#define DEBUG_BAUD              115200

// Log levels (0=ERROR only, 1=+WARN, 2=+INFO, 3=+DEBUG)
// Using BREWOS_ prefix to avoid collision with NimBLE's LOG_LEVEL_* macros
// Note: If platform/platform.h is included first, it defines log_level_t and LOG_* macros
// In that case, we don't redefine the macros or enum constants to avoid conflicts
#ifndef PLATFORM_H
enum BrewOSLogLevel {
    BREWOS_LOG_ERROR = 0,
    BREWOS_LOG_WARN = 1,
    BREWOS_LOG_INFO = 2,
    BREWOS_LOG_DEBUG = 3
};

// Global log level - declared extern, defined in main.cpp
extern BrewOSLogLevel g_brewos_log_level;

// Log level control functions
void setLogLevel(BrewOSLogLevel level);
BrewOSLogLevel getLogLevel();
const char* logLevelToString(BrewOSLogLevel level);
BrewOSLogLevel stringToLogLevel(const char* str);

// Forward declarations for log manager
// Note: We include log_manager.h here to enable automatic log capture in LOG macros
// This is safe because we removed the config.h include from log_manager.h
#include "log_manager.h"

/**
 * BrewOS Unified Logging Macros
 * 
 * Three outputs:
 * 1. Serial - Always outputs for debugging
 * 2. Log Buffer - Writes to 50KB buffer when enabled (for download)
 * 3. WebSocket - Broadcasts to UI clients (INFO and above)
 * 
 * Note: Only defined if platform.h wasn't included first (avoids redefinition)
 */

#include <stdarg.h>  // For va_list in _brewos_log_broadcast

// Forward declaration for WebSocket broadcast (defined in web_server_broadcast.cpp)
extern "C" void platform_broadcast_log(const char* level, const char* message);

// Helper function to check if debug logs should be broadcast (defined in main.cpp)
extern bool should_broadcast_debug_logs(void);

// Helper to format and broadcast log message (used by LOG macros for INFO/WARN/ERROR)
// Optimized for non-DEBUG logs - no filtering needed
static inline void _brewos_log_broadcast(const char* level, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
static inline void _brewos_log_broadcast(const char* level, const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    platform_broadcast_log(level, buf);
}

// Helper to format and broadcast DEBUG log message (filters based on settings)
// Separate function to avoid string comparison overhead for non-DEBUG logs
static inline void _brewos_log_broadcast_debug(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
static inline void _brewos_log_broadcast_debug(const char* fmt, ...) {
    // Check setting before formatting (early return for better performance)
    if (!should_broadcast_debug_logs()) {
        return;  // Don't broadcast DEBUG logs if disabled
    }
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    platform_broadcast_log("debug", buf);
}

#define LOG_TAG                 "BrewOS"

// ERROR - always outputs (highest priority)
#define LOG_E(fmt, ...)         do { \
    Serial.printf("[%lu] E: " fmt "\n", millis(), ##__VA_ARGS__); \
    if (g_logManager && g_logManager->isEnabled()) { \
        log_manager_add_logf(BREWOS_LOG_ERROR, LOG_SOURCE_ESP32, fmt, ##__VA_ARGS__); \
    } \
    _brewos_log_broadcast("error", fmt, ##__VA_ARGS__); \
} while(0)

// WARN - outputs if level >= WARN
#define LOG_W(fmt, ...)         do { \
    if (g_brewos_log_level >= BREWOS_LOG_WARN) { \
        Serial.printf("[%lu] W: " fmt "\n", millis(), ##__VA_ARGS__); \
        if (g_logManager && g_logManager->isEnabled()) { \
            log_manager_add_logf(BREWOS_LOG_WARN, LOG_SOURCE_ESP32, fmt, ##__VA_ARGS__); \
        } \
        _brewos_log_broadcast("warn", fmt, ##__VA_ARGS__); \
    } \
} while(0)

// INFO - outputs if level >= INFO
#define LOG_I(fmt, ...)         do { \
    if (g_brewos_log_level >= BREWOS_LOG_INFO) { \
        Serial.printf("[%lu] I: " fmt "\n", millis(), ##__VA_ARGS__); \
        if (g_logManager && g_logManager->isEnabled()) { \
            log_manager_add_logf(BREWOS_LOG_INFO, LOG_SOURCE_ESP32, fmt, ##__VA_ARGS__); \
        } \
        _brewos_log_broadcast("info", fmt, ##__VA_ARGS__); \
    } \
} while(0)

// DEBUG - outputs if level >= DEBUG, broadcasts if debug logs enabled in settings
// Uses separate broadcast function that checks settings (avoids string comparison overhead)
#define LOG_D(fmt, ...)         do { \
    if (g_brewos_log_level >= BREWOS_LOG_DEBUG) { \
        Serial.printf("[%lu] D: " fmt "\n", millis(), ##__VA_ARGS__); \
        if (g_logManager && g_logManager->isEnabled()) { \
            log_manager_add_logf(BREWOS_LOG_DEBUG, LOG_SOURCE_ESP32, fmt, ##__VA_ARGS__); \
        } \
        _brewos_log_broadcast_debug(fmt, ##__VA_ARGS__); \
    } \
} while(0)
#else
// Platform.h was included first, so use its log_level_t and LOG_* macros
// Define BrewOSLogLevel as an alias for backward compatibility with existing code
// The enum constants (BREWOS_LOG_*) are already defined in platform.h
typedef log_level_t BrewOSLogLevel;

// Global log level - declared extern, defined in main.cpp
extern log_level_t g_brewos_log_level;

// Log level control functions (using log_level_t)
void setLogLevel(log_level_t level);
log_level_t getLogLevel();
const char* logLevelToString(log_level_t level);
log_level_t stringToLogLevel(const char* str);
#endif

#endif // CONFIG_H
