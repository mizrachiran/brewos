#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>  // For printf() used in DEBUG_PRINT and LOG_PRINT macros

// Include shared protocol definitions
#include "protocol_defs.h"

// =============================================================================
// BrewOS Pico Firmware Configuration
// =============================================================================

// -----------------------------------------------------------------------------
// Version
// -----------------------------------------------------------------------------
#define FIRMWARE_VERSION_MAJOR      0
#define FIRMWARE_VERSION_MINOR      8
#define FIRMWARE_VERSION_PATCH      8
#define PICO_VERSION_MAJOR      FIRMWARE_VERSION_MAJOR
#define PICO_VERSION_MINOR      FIRMWARE_VERSION_MINOR
#define PICO_VERSION_PATCH      FIRMWARE_VERSION_PATCH

// Build timestamp (set at compile time)
// Format: "Dec 12 2024" and "14:30:45"
#define BUILD_DATE      __DATE__
#define BUILD_TIME      __TIME__

// -----------------------------------------------------------------------------
// Safety Limits (NEVER exceed these)
// -----------------------------------------------------------------------------
#define SAFETY_MAX_BOILER_TEMP_C        165     // Absolute max temperature
#define SAFETY_MIN_WATER_LEVEL          10      // Minimum water level %
#define SAFETY_WATCHDOG_TIMEOUT_MS      2000    // Watchdog timeout (max 2000ms per SAF-002)
#define SAFETY_HEARTBEAT_TIMEOUT_MS     5000    // ESP32 heartbeat timeout

// -----------------------------------------------------------------------------
// UART - ESP32 Communication (Pico side pins)
// -----------------------------------------------------------------------------
#define ESP32_UART_ID           uart0
#define ESP32_UART_BAUD         PROTOCOL_BAUD_RATE
#define ESP32_UART_TX_PIN       0               // Pico TX -> ESP32 RX
#define ESP32_UART_RX_PIN       1               // Pico RX <- ESP32 TX

// -----------------------------------------------------------------------------
// Timing
// -----------------------------------------------------------------------------
#define CONTROL_LOOP_PERIOD_MS  100             // 10Hz control loop
#define STATUS_SEND_PERIOD_MS   250             // 4Hz status updates
#define SENSOR_READ_PERIOD_MS   50              // 20Hz sensor reads
#define BOOT_INFO_RESEND_MS     (5 * 60 * 1000) // Resend boot info every 5 minutes

// -----------------------------------------------------------------------------
// PID Defaults
// -----------------------------------------------------------------------------
#define PID_DEFAULT_KP          2.0f
#define PID_DEFAULT_KI          0.1f
#define PID_DEFAULT_KD          1.0f
#define PID_OUTPUT_MIN          0.0f
#define PID_OUTPUT_MAX          100.0f

// -----------------------------------------------------------------------------
// Temperature Defaults (in Celsius * 10 for 0.1C resolution)
// -----------------------------------------------------------------------------
#define DEFAULT_BREW_TEMP       930             // 93.0C
#define DEFAULT_STEAM_TEMP      1400            // 140.0C
#define DEFAULT_OFFSET_TEMP     -50             // -5.0C offset

// Temperature conversion macros (decicelsius <-> Celsius)
#define TEMP_DECI_TO_C(x)       ((float)(x) / 10.0f)      // e.g., 930 -> 93.0
#define TEMP_C_TO_DECI(x)       ((int16_t)((x) * 10.0f))  // e.g., 93.0 -> 930

// -----------------------------------------------------------------------------
// SSR/PWM Configuration
// -----------------------------------------------------------------------------
// Minimum duty cycle for Zero-Crossing SSRs (2ms min pulse at 50Hz = 10ms half-cycle)
// At 25Hz PWM (40ms period), 5% duty = 2ms - below this, ZC SSRs may skip cycles
#define SSR_MIN_DUTY_PERCENT    5.0f

// PID Derivative filter time constant (seconds)
// Lower = more responsive but noisier, higher = smoother but slower
#define PID_DERIVATIVE_FILTER_TAU   0.5f

// -----------------------------------------------------------------------------
// Hardware Simulation Mode
// -----------------------------------------------------------------------------
// Set to 1 to enable simulation mode (for development without hardware)
// Set to 0 to use real hardware
// Can also be overridden at compile time: -DHW_SIMULATION_MODE=0
#ifndef HW_SIMULATION_MODE
    #define HW_SIMULATION_MODE 1  // Default to simulation for development
#endif

// -----------------------------------------------------------------------------
// Logging
// -----------------------------------------------------------------------------
// Use the new structured logging system with multiple log levels
// The logging module provides LOG_ERROR, LOG_WARN, LOG_INFO, LOG_DEBUG, LOG_TRACE
// and automatically handles USB serial output and ESP32 forwarding when enabled.
//
// For backwards compatibility, we map old macros to new logging system:
// - DEBUG_PRINT -> LOG_DEBUG (debug-level information)
// - LOG_PRINT -> LOG_INFO (important operational logs)
//
// New code should use the logging.h macros directly:
//   LOG_ERROR() - Critical errors
//   LOG_WARN()  - Warnings
//   LOG_INFO()  - Important information
//   LOG_DEBUG() - Debug information
//   LOG_TRACE() - Detailed traces

#ifndef UNIT_TEST  // Real implementation for firmware
    #include "logging.h"
    #define DEBUG_PRINT(fmt, ...) LOG_DEBUG(fmt, ##__VA_ARGS__)
    #define LOG_PRINT(fmt, ...)   LOG_INFO(fmt, ##__VA_ARGS__)
#else  // Test environment - use simple printf
    #define DEBUG_PRINT(fmt, ...) printf(fmt, ##__VA_ARGS__)
    #define LOG_PRINT(fmt, ...)   printf(fmt, ##__VA_ARGS__)
#endif

#endif // CONFIG_H
