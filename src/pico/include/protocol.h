#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>  // For size_t
#include "config.h"  // Includes protocol_defs.h

// -----------------------------------------------------------------------------
// Packet Structure
// -----------------------------------------------------------------------------
// | SYNC (0xAA) | TYPE | LENGTH | SEQ | PAYLOAD... | CRC16 |
// |     1       |   1  |    1   |  1  |  0-56      |   2   |

typedef struct {
    uint8_t type;
    uint8_t length;
    uint8_t seq;
    uint8_t payload[PROTOCOL_MAX_PAYLOAD];
    uint16_t crc;
    bool valid;
} packet_t;

// -----------------------------------------------------------------------------
// Status Payload (MSG_STATUS = 0x01)
// -----------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
    int16_t brew_temp;          // Celsius * 10 (0.1C resolution)
    int16_t steam_temp;         // Celsius * 10
    int16_t group_temp;         // Celsius * 10 (if available)
    uint16_t pressure;          // Bar * 100 (0.01 bar resolution)
    int16_t brew_setpoint;      // Celsius * 10
    int16_t steam_setpoint;     // Celsius * 10
    uint8_t brew_output;        // 0-100%
    uint8_t steam_output;       // 0-100%
    uint8_t pump_output;        // 0-100%
    uint8_t state;              // Machine state (see STATE_* in protocol_defs.h)
    uint8_t flags;              // Status flags (see STATUS_FLAG_* in protocol_defs.h)
    uint8_t water_level;        // 0-100%
    uint16_t power_watts;       // Current power draw
    uint32_t uptime_ms;         // Milliseconds since boot
    uint32_t shot_start_timestamp_ms;  // Brew start timestamp (milliseconds since boot, 0 if not brewing)
    uint8_t heating_strategy;   // Current heating strategy (see HEAT_STRATEGY_* in protocol_defs.h)
    uint8_t cleaning_reminder;  // 1 if cleaning reminder is due (brew_count >= threshold), 0 otherwise
    uint16_t brew_count;        // Number of brews since last cleaning (for cleaning reminder)
} status_payload_t;

// -----------------------------------------------------------------------------
// Alarm Payload (MSG_ALARM = 0x02)
// -----------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
    uint8_t code;               // Alarm code (see ALARM_* in protocol_defs.h)
    uint8_t severity;           // 0=warning, 1=error, 2=critical
    uint16_t value;             // Associated value (e.g., temperature)
} alarm_payload_t;

// -----------------------------------------------------------------------------
// Boot Payload (MSG_BOOT = 0x03)
// -----------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
    uint8_t version_major;
    uint8_t version_minor;
    uint8_t version_patch;
    uint8_t machine_type;       // See MACHINE_TYPE_* in protocol_defs.h
    uint8_t pcb_type;           // See PCB_TYPE_* in pcb_config.h
    uint8_t pcb_version_major;
    uint8_t pcb_version_minor;
    uint32_t reset_reason;
} boot_payload_t;

// -----------------------------------------------------------------------------
// Config Payload (MSG_CONFIG = 0x05)
// -----------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
    int16_t brew_setpoint;      // Celsius * 10
    int16_t steam_setpoint;     // Celsius * 10
    int16_t temp_offset;        // Celsius * 10
    uint16_t pid_kp;            // * 100
    uint16_t pid_ki;            // * 100
    uint16_t pid_kd;            // * 100
    uint8_t heating_strategy;   // See HEAT_STRATEGY_* in protocol_defs.h
    uint8_t machine_type;       // See MACHINE_TYPE_* in protocol_defs.h
} config_payload_t;

// -----------------------------------------------------------------------------
// Command Payloads
// -----------------------------------------------------------------------------

// MSG_CMD_SET_TEMP (0x10)
typedef struct __attribute__((packed)) {
    uint8_t target;             // 0=brew, 1=steam
    int16_t temperature;        // Celsius * 10
} cmd_set_temp_t;

// MSG_CMD_SET_PID (0x11)
typedef struct __attribute__((packed)) {
    uint8_t target;             // 0=brew, 1=steam
    uint16_t kp;                // * 100
    uint16_t ki;                // * 100
    uint16_t kd;                // * 100
} cmd_set_pid_t;

// MSG_CMD_BREW (0x13)
typedef struct __attribute__((packed)) {
    uint8_t action;             // 0=stop, 1=start
} cmd_brew_t;

// MSG_CMD_MODE (0x14)
typedef struct __attribute__((packed)) {
    uint8_t mode;               // machine_mode_t: 0=MODE_IDLE, 1=MODE_BREW, 2=MODE_STEAM
} cmd_mode_t;

// MSG_CMD_CONFIG (0x15) - Variable payload based on config_type
typedef struct __attribute__((packed)) {
    uint8_t  config_type;     // Configuration category (see CONFIG_* in protocol_defs.h)
    uint8_t  data[];          // Variable payload based on config_type
} cmd_config_t;

// CONFIG_ENVIRONMENTAL (0x05) payload (for MSG_CMD_CONFIG)
typedef struct __attribute__((packed)) {
    uint16_t nominal_voltage;    // 120, 230, 240, etc.
    float    max_current_draw;   // 10.0, 16.0, etc. (4 bytes)
} config_environmental_t;  // 6 bytes

// MSG_ENV_CONFIG (0x08) payload - Environmental config response
typedef struct __attribute__((packed)) {
    uint16_t nominal_voltage;        // 120, 230, 240, etc. (V)
    float    max_current_draw;       // 10.0, 16.0, etc. (A) - 4 bytes
    // Calculated values (for information)
    float    brew_heater_current;    // Calculated: brew_heater_power / nominal_voltage
    float    steam_heater_current;   // Calculated: steam_heater_power / nominal_voltage
    float    max_combined_current;   // Calculated: max_current_draw * 0.95
} env_config_payload_t;  // 18 bytes

// MSG_STATISTICS (0x09) payload - Statistics response
typedef struct __attribute__((packed)) {
    // Overall statistics
    uint32_t total_brews;             // Total number of brews (all time)
    uint32_t total_brew_time_ms;      // Total brew time (all time)
    uint16_t avg_brew_time_ms;        // Average brew time (all time)
    uint16_t min_brew_time_ms;        // Shortest brew time
    uint16_t max_brew_time_ms;        // Longest brew time
    
    // Daily statistics
    uint16_t daily_count;              // Cups in last 24 hours
    uint16_t daily_avg_time_ms;       // Average brew time (last 24 hours)
    
    // Weekly statistics
    uint16_t weekly_count;             // Cups in last 7 days
    uint16_t weekly_avg_time_ms;      // Average brew time (last 7 days)
    
    // Monthly statistics
    uint16_t monthly_count;            // Cups in last 30 days
    uint16_t monthly_avg_time_ms;     // Average brew time (last 30 days)
    
    // Metadata
    uint32_t last_brew_timestamp;     // Timestamp of last brew (milliseconds since boot)
} statistics_payload_t;  // 28 bytes

// -----------------------------------------------------------------------------
// ACK Payload (MSG_ACK = 0x04)
// -----------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
    uint8_t cmd_type;    // Original command type
    uint8_t cmd_seq;     // Original command sequence
    uint8_t result;      // ACK result code (ACK_SUCCESS, ACK_ERROR_*, etc.)
    uint8_t reserved;    // Reserved for future use
} ack_payload_t;  // 4 bytes

// -----------------------------------------------------------------------------
// Functions
// -----------------------------------------------------------------------------

// Initialize protocol (UART)
void protocol_init(void);

// Process incoming data (call from loop)
void protocol_process(void);

// Send packets
bool protocol_send_status(const status_payload_t* status);
bool protocol_send_alarm(uint8_t code, uint8_t severity, uint16_t value);
bool protocol_send_boot(void);
bool protocol_send_config(const config_payload_t* config);
bool protocol_send_env_config(const env_config_payload_t* env_config);
bool protocol_send_statistics(const statistics_payload_t* stats);
bool protocol_send_ack(uint8_t for_type, uint8_t seq, uint8_t result);
bool protocol_send_debug(const char* message);

// Error tracking
uint32_t protocol_get_crc_errors(void);
uint32_t protocol_get_packet_errors(void);
void protocol_reset_error_counters(void);

// Packet callback
typedef void (*packet_callback_t)(const packet_t* packet);
void protocol_set_callback(packet_callback_t callback);

// CRC calculation
uint16_t protocol_crc16(const uint8_t* data, size_t length);

#endif // PROTOCOL_H
