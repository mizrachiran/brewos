/**
 * ECM Coffee Machine Controller - Shared Protocol Definitions
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
#define ECM_PROTOCOL_VERSION    1

// =============================================================================
// UART Configuration
// =============================================================================
#define PROTOCOL_BAUD_RATE      921600

// =============================================================================
// Packet Structure
// =============================================================================
// | SYNC (0xAA) | TYPE | LENGTH | SEQ | PAYLOAD... | CRC16 |
// |     1       |   1  |    1   |  1  |   0-56     |   2   |

#define PROTOCOL_SYNC_BYTE      0xAA
#define PROTOCOL_MAX_PAYLOAD    56
#define PROTOCOL_HEADER_SIZE    4       // sync + type + length + seq
#define PROTOCOL_CRC_SIZE       2
#define PROTOCOL_MAX_PACKET     (PROTOCOL_HEADER_SIZE + PROTOCOL_MAX_PAYLOAD + PROTOCOL_CRC_SIZE)

// Note: config_payload_t is 60 bytes, exceeding PROTOCOL_MAX_PAYLOAD
// Environmental config should be sent separately or config_payload_t should be split

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
#define MSG_CMD_BOOTLOADER      0x1F    // Enter bootloader mode

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
#define HEAT_STRATEGY_STEAM_PRIORITY 3  // Prioritize steam boiler
#define HEAT_STRATEGY_SMART_STAGGER 4   // Power-aware staggering

// =============================================================================
// Configuration Types (for MSG_CMD_CONFIG)
// =============================================================================
#define CONFIG_HEATING_STRATEGY  0x01
#define CONFIG_PREINFUSION       0x02
#define CONFIG_STANDBY           0x03
#define CONFIG_TEMPS             0x04
#define CONFIG_ENVIRONMENTAL     0x05   // Environmental electrical config (voltage, current limits)

// =============================================================================
// CRC-16-CCITT
// =============================================================================
// Polynomial: 0x1021, Initial: 0xFFFF
// Both ESP32 and Pico implement this identically

#ifdef __cplusplus
}
#endif

#endif // PROTOCOL_DEFS_H

