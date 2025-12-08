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
#define ESP32_VERSION_MINOR     4
#define ESP32_VERSION_PATCH     4
#define ESP32_VERSION_PRERELEASE "beta.1"  // Empty string "" for stable releases
#define ESP32_VERSION           "0.4.4"

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
// UART - Pico Communication (ESP32 side pins)
// -----------------------------------------------------------------------------
#define PICO_UART_NUM           1               // UART1
#define PICO_UART_BAUD          PROTOCOL_BAUD_RATE
#define PICO_UART_TX_PIN        17              // ESP32 TX -> Pico RX (GPIO1)
#define PICO_UART_RX_PIN        18              // ESP32 RX <- Pico TX (GPIO0)

// Pico control pins for OTA
#define PICO_RUN_PIN            8               // Controls Pico RUN (reset) → J15 Pin 5
#define PICO_BOOTSEL_PIN        9               // Controls Pico BOOTSEL → J15 Pin 6

// Brew-by-weight signal
#define WEIGHT_STOP_PIN         10              // ESP32 GPIO10 → J15 Pin 7 (WEIGHT_STOP) → Pico GPIO21
                                                // Set HIGH when target weight reached, LOW otherwise

// Buffer sizes
#define PICO_RX_BUFFER_SIZE     256

// -----------------------------------------------------------------------------
// OTA Configuration
// -----------------------------------------------------------------------------
#define OTA_FILE_PATH           "/pico_firmware.bin"
#define OTA_MAX_SIZE            (2 * 1024 * 1024)   // 2MB max

// GitHub OTA - Download firmware from GitHub releases
#define GITHUB_OWNER            "mizrachiran"
#define GITHUB_REPO             "brewos"
#define GITHUB_ESP32_ASSET      "brewos_esp32.bin"

// Pico firmware assets by machine type (UF2 format)
#define GITHUB_PICO_DUAL_BOILER_ASSET     "brewos_dual_boiler.uf2"
#define GITHUB_PICO_SINGLE_BOILER_ASSET   "brewos_single_boiler.uf2"
#define GITHUB_PICO_HEAT_EXCHANGER_ASSET  "brewos_heat_exchanger.uf2"

// GitHub API URL template: https://api.github.com/repos/OWNER/REPO/releases/tags/TAG
// Asset download URL: https://github.com/OWNER/REPO/releases/download/TAG/ASSET

// -----------------------------------------------------------------------------
// Debug
// -----------------------------------------------------------------------------
#define DEBUG_SERIAL            Serial
#define DEBUG_BAUD              115200

// Log levels (0=ERROR only, 1=+WARN, 2=+INFO, 3=+DEBUG)
enum LogLevel {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN = 1,
    LOG_LEVEL_INFO = 2,
    LOG_LEVEL_DEBUG = 3
};

// Global log level - declared extern, defined in main.cpp
extern LogLevel g_log_level;

// Log level control functions
void setLogLevel(LogLevel level);
LogLevel getLogLevel();
const char* logLevelToString(LogLevel level);
LogLevel stringToLogLevel(const char* str);

// Log macros with level checking
#define LOG_TAG                 "BrewOS"
#define LOG_E(fmt, ...)         Serial.printf("[%lu] E: " fmt "\n", millis(), ##__VA_ARGS__)
#define LOG_W(fmt, ...)         do { if (g_log_level >= LOG_LEVEL_WARN) Serial.printf("[%lu] W: " fmt "\n", millis(), ##__VA_ARGS__); } while(0)
#define LOG_I(fmt, ...)         do { if (g_log_level >= LOG_LEVEL_INFO) Serial.printf("[%lu] I: " fmt "\n", millis(), ##__VA_ARGS__); } while(0)
#define LOG_D(fmt, ...)         do { if (g_log_level >= LOG_LEVEL_DEBUG) Serial.printf("[%lu] D: " fmt "\n", millis(), ##__VA_ARGS__); } while(0)

#endif // CONFIG_H
