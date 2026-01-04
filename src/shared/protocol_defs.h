/**
 * Coffee Machine Controller - Shared Protocol Definitions
 * 
 * This file is shared between ESP32 and Pico firmware.
 * Any changes here must be compatible with both platforms.
 * 
 * Include path setup:
 *   ESP32: Add -I../../shared to build flags
 *   Pico:  Add ${CMAKE_SOURCE_DIR}/../shared to include_directories
 */

#ifndef PROTOCOL_DEFS_H
#define PROTOCOL_DEFS_H

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Version
// =============================================================================
// Protocol version - increment for breaking changes
// See docs/shared/Communication_Protocol.md for update strategy
#define PROTOCOL_VERSION    1

// =============================================================================
// UART Configuration
// =============================================================================
#define PROTOCOL_BAUD_RATE      921600

// =============================================================================
// Packet Structure
// =============================================================================
// | SYNC (0xAA) | TYPE | LENGTH | SEQ | PAYLOAD... | CRC16 |
// |     1       |   1  |    1   |  1  |   0-32     |   2   |

#define PROTOCOL_SYNC_BYTE      0xAA
#define PROTOCOL_MAX_PAYLOAD    32      // Reduced from 56 to save RAM (status_payload_t is 32 bytes)
#define PROTOCOL_HEADER_SIZE    4       // sync + type + length + seq
#define PROTOCOL_CRC_SIZE       2
#define PROTOCOL_MAX_PACKET     (PROTOCOL_HEADER_SIZE + PROTOCOL_MAX_PAYLOAD + PROTOCOL_CRC_SIZE)

// Note: All payload types fit within 32 bytes:
//   - status_payload_t: 32 bytes (exact fit)
//   - config_payload_t: 14 bytes
//   - env_config_payload_t: 18 bytes
//   - diag_result_payload_t: 32 bytes (exact fit)
//   - All command payloads: < 10 bytes
// Compile-time assertions in protocol.h verify payload sizes

// =============================================================================
// Message Types - Status/Response (0x00 - 0x0F)
// =============================================================================
#define MSG_PING                0x00    // Ping/pong for connectivity check
#define MSG_STATUS              0x01    // Machine status (temps, pressure, state)
#define MSG_ALARM               0x02    // Alarm/fault notification
#define MSG_BOOT                0x03    // Boot notification with version
#define MSG_ACK                 0x04    // Command acknowledgment
#define MSG_CONFIG              0x05    // Configuration dump
#define MSG_DEBUG               0x06    // Debug message (string)
#define MSG_DEBUG_RESP          0x07    // Debug response
#define MSG_ENV_CONFIG          0x08    // Environmental config (voltage, current limits)
#define MSG_STATISTICS          0x09    // Statistics response
#define MSG_DIAGNOSTICS         0x0A    // Diagnostics response
#define MSG_POWER_METER         0x0B    // Power meter reading (from Pico)
#define MSG_HANDSHAKE           0x0C    // Protocol handshake for version negotiation
#define MSG_NACK                0x0D    // Negative acknowledgment (busy/rejected)

// =============================================================================
// Diagnostic Test IDs
// Based on ECM_Control_Board_Specification_v2.20 hardware components
// =============================================================================
#define DIAG_TEST_ALL           0x00    // Run all tests

// Temperature Sensors (Section 3.1: T1, T2)
#define DIAG_TEST_BREW_NTC      0x01    // T1: Brew boiler NTC (GPIO26/ADC0)
#define DIAG_TEST_STEAM_NTC     0x02    // T2: Steam boiler NTC (GPIO27/ADC1)

// Pressure Sensor (Section 3.1: P1)
#define DIAG_TEST_PRESSURE      0x04    // P1: Pressure transducer YD4060 (GPIO28/ADC2)

// Water Level Sensors (Section 3.1: S1, S2, S3)
#define DIAG_TEST_WATER_LEVEL   0x05    // S1+S2: Water reservoir switch + tank float (GPIO2/3)
#define DIAG_TEST_STEAM_LEVEL   0x0E    // S3: Steam boiler level probe OPA342/TLV3201 (GPIO4)

// Brew Control Input (Section 3.1: S4)
#define DIAG_TEST_BREW_SWITCH   0x0F    // S4: Brew handle/lever switch (GPIO5)

// Heater Outputs - SSRs (Section 3.2: SSR1, SSR2)
#define DIAG_TEST_SSR_BREW      0x06    // SSR1: Brew heater solid-state relay (GPIO13)
#define DIAG_TEST_SSR_STEAM     0x07    // SSR2: Steam heater solid-state relay (GPIO14)

// Relay Outputs (Section 3.2: K1, K2, K3)
#define DIAG_TEST_RELAY_LED     0x10    // K1: Water status LED relay (GPIO10)
#define DIAG_TEST_RELAY_PUMP    0x08    // K2: Pump relay 16A (GPIO11)
#define DIAG_TEST_RELAY_SOLENOID 0x09   // K3: 3-way solenoid valve relay (GPIO12)

// Communication (Section 3.3)
#define DIAG_TEST_ESP32_COMM    0x0B    // ESP32 UART0 link (GPIO0/1)
#define DIAG_TEST_POWER_METER   0x0A    // Power meter UART1 (GPIO6/7) - PZEM, JSY, Eastron

// User Interface (Section 3.4)
#define DIAG_TEST_BUZZER        0x0C    // Piezo buzzer PWM (GPIO19)
#define DIAG_TEST_LED           0x0D    // Status LED green (GPIO15)

// Class B Safety Tests (IEC 60730/60335 Annex R)
#define DIAG_TEST_CLASS_B_ALL   0x30    // Run all Class B tests
#define DIAG_TEST_CLASS_B_RAM   0x31    // RAM March C- test
#define DIAG_TEST_CLASS_B_FLASH 0x32    // Flash CRC verification
#define DIAG_TEST_CLASS_B_CPU   0x33    // CPU register test
#define DIAG_TEST_CLASS_B_IO    0x34    // I/O state verification
#define DIAG_TEST_CLASS_B_CLOCK 0x35    // Clock frequency test
#define DIAG_TEST_CLASS_B_STACK 0x36    // Stack overflow detection
#define DIAG_TEST_CLASS_B_PC    0x37    // Program counter test

// Diagnostic result status
#define DIAG_STATUS_PASS        0x00    // Test passed
#define DIAG_STATUS_FAIL        0x01    // Test failed
#define DIAG_STATUS_WARN        0x02    // Test passed with warnings
#define DIAG_STATUS_SKIP        0x03    // Test skipped (not applicable)
#define DIAG_STATUS_RUNNING     0x04    // Test in progress

// =============================================================================
// ACK Result Codes
// =============================================================================
#define ACK_SUCCESS                0x00    // Command executed successfully
#define ACK_ERROR_INVALID         0x01    // Invalid command or parameters
#define ACK_ERROR_REJECTED        0x02    // Command rejected (e.g., safety, state)
#define ACK_ERROR_FAILED          0x03    // Command failed (e.g., hardware error)
#define ACK_ERROR_TIMEOUT         0x04    // Operation timed out
#define ACK_ERROR_BUSY           0x05    // System busy, try again later
#define ACK_ERROR_NOT_READY      0x06    // System not ready for this command

// =============================================================================
// Message Types - Commands (0x10 - 0x1F)
// =============================================================================
#define MSG_CMD_SET_TEMP        0x10    // Set temperature setpoint
#define MSG_CMD_SET_PID         0x11    // Set PID parameters
#define MSG_CMD_BREW            0x13    // Start/stop brew
#define MSG_CMD_MODE            0x14    // Set machine mode
#define MSG_CMD_CONFIG          0x15    // Update configuration
#define MSG_CMD_GET_CONFIG      0x16    // Request config dump
#define MSG_CMD_GET_ENV_CONFIG  0x17    // Request environmental config
#define MSG_CMD_CLEANING_START  0x18    // Start cleaning cycle
#define MSG_CMD_CLEANING_STOP   0x19    // Stop cleaning cycle
#define MSG_CMD_CLEANING_RESET  0x1A    // Reset brew counter
#define MSG_CMD_CLEANING_SET_THRESHOLD 0x1B  // Set cleaning reminder threshold
#define MSG_CMD_GET_STATISTICS  0x1C    // Request statistics
#define MSG_CMD_DEBUG           0x1D    // Debug command
#define MSG_CMD_SET_ECO         0x1E    // Set eco mode configuration
#define MSG_CMD_BOOTLOADER      0x1F    // Enter bootloader mode
#define MSG_CMD_DIAGNOSTICS     0x20    // Run diagnostic test(s)
#define MSG_CMD_POWER_METER_CONFIG 0x21 // Configure power meter
#define MSG_CMD_POWER_METER_DISCOVER 0x22 // Start power meter auto-discovery
#define MSG_CMD_GET_BOOT        0x23    // Request boot info (version, machine type)
#define MSG_CMD_LOG_CONFIG      0x24    // Configure log forwarding (Pico -> ESP32)
#define MSG_LOG                 0x25    // Log message from Pico

// =============================================================================
// Alarm Codes
// =============================================================================
#define ALARM_NONE              0x00
#define ALARM_OVER_TEMP         0x01    // Temperature exceeded limit
#define ALARM_WATER_LOW         0x02    // Water level too low
#define ALARM_SENSOR_FAIL       0x03    // Sensor read failure
#define ALARM_HEATER_FAIL       0x04    // Heater not responding
#define ALARM_WATCHDOG          0x05    // Watchdog reset occurred
#define ALARM_COMM_TIMEOUT      0x06    // Communication timeout

// =============================================================================
// Machine States
// =============================================================================
#define STATE_INIT              0       // Initializing
#define STATE_IDLE              1       // Machine on but not heating (MODE_IDLE)
#define STATE_HEATING           2       // Actively heating to setpoint
#define STATE_READY             3       // At temperature, ready to brew
#define STATE_BREWING           4       // Brewing in progress
#define STATE_FAULT             5       // Fault condition
#define STATE_SAFE              6       // Safe state (all outputs off)
#define STATE_ECO               7       // Eco mode (reduced temperature)

// =============================================================================
// Status Flags (bitfield)
// =============================================================================
#define STATUS_FLAG_BREWING     (1 << 0)
#define STATUS_FLAG_HEATING     (1 << 1)  // Heaters active (output > 0%)
#define STATUS_FLAG_PUMP_ON     (1 << 2)
#define STATUS_FLAG_WATER_LOW   (1 << 3)
#define STATUS_FLAG_ALARM       (1 << 4)

// =============================================================================
// Machine Types (Protocol IDs - matches machine_type_t enum)
// =============================================================================
// These are defined in machine_config.h as machine_type_t enum:
//   MACHINE_TYPE_UNKNOWN        = 0x00
//   MACHINE_TYPE_DUAL_BOILER    = 0x01
//   MACHINE_TYPE_SINGLE_BOILER  = 0x02
//   MACHINE_TYPE_HEAT_EXCHANGER = 0x03
//   MACHINE_TYPE_THERMOBLOCK    = 0x04
// Legacy defines removed to avoid conflict with machine_config.h enum

// =============================================================================
// Heating Strategies
// =============================================================================
#define HEAT_STRATEGY_BREW_ONLY     0   // Heat only brew boiler
#define HEAT_STRATEGY_SEQUENTIAL    1   // Brew first, then steam
#define HEAT_STRATEGY_PARALLEL      2   // Both simultaneously
#define HEAT_STRATEGY_SMART_STAGGER 3   // Power-aware staggering

// =============================================================================
// Configuration Types (for MSG_CMD_CONFIG)
// =============================================================================
#define CONFIG_HEATING_STRATEGY  0x01
#define CONFIG_PREINFUSION       0x02
#define CONFIG_STANDBY           0x03
#define CONFIG_TEMPS             0x04
#define CONFIG_ENVIRONMENTAL     0x05   // Environmental electrical config (voltage, current limits)
#define CONFIG_ECO               0x06   // Eco mode configuration
#define CONFIG_MACHINE_INFO      0x07   // Machine brand and model (source of truth on Pico)

// =============================================================================
// CRC-16-CCITT
// =============================================================================
// Polynomial: 0x1021, Initial: 0xFFFF
// Both ESP32 and Pico implement this identically

#ifdef __cplusplus
}
#endif

#endif // PROTOCOL_DEFS_H

