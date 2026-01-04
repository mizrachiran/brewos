#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>  // For size_t
#include "config.h"  // Includes protocol_defs.h

// -----------------------------------------------------------------------------
// Protocol Configuration
// -----------------------------------------------------------------------------
#define PROTOCOL_VERSION_MAJOR      1
#define PROTOCOL_VERSION_MINOR      1
#define PROTOCOL_PARSER_TIMEOUT_MS  500   // Reset parser if incomplete packet > 500ms
#define PROTOCOL_ACK_TIMEOUT_MS     1000  // Wait for ACK response
#define PROTOCOL_RETRY_COUNT        3     // Number of command retries
#define PROTOCOL_HANDSHAKE_TIMEOUT_MS 5000 // Handshake completion timeout
#define PROTOCOL_MAX_PENDING_CMDS   4     // Maximum pending commands awaiting ACK
#define PROTOCOL_BACKPRESSURE_THRESHOLD 3 // Send NACK when pending >= threshold

// -----------------------------------------------------------------------------
// Packet Structure
// -----------------------------------------------------------------------------
// | SYNC (0xAA) | TYPE | LENGTH | SEQ | PAYLOAD... | CRC16 |
// |     1       |   1  |    1   |  1  |  0-32      |   2   |

typedef struct {
    uint8_t type;
    uint8_t length;
    uint8_t seq;
    uint8_t payload[PROTOCOL_MAX_PAYLOAD];
    uint16_t crc;
    bool valid;
    uint32_t timestamp_ms;  // Packet receive timestamp for timeout tracking
} packet_t;

// Pending command structure for retry tracking
typedef struct {
    uint8_t type;                   // Command type
    uint8_t seq;                    // Sequence number
    uint8_t payload[PROTOCOL_MAX_PAYLOAD];
    uint8_t length;                 // Payload length
    uint8_t retry_count;            // Number of retries attempted
    uint32_t sent_time_ms;          // Timestamp when sent
    bool active;                    // Slot in use
} pending_cmd_t;

// Protocol diagnostics structure
typedef struct {
    uint32_t packets_received;      // Total valid packets received
    uint32_t packets_sent;          // Total packets sent
    uint32_t crc_errors;            // CRC validation failures
    uint32_t packet_errors;         // Invalid packet format
    uint32_t timeout_errors;        // Parser timeouts
    uint32_t sequence_errors;       // Sequence number issues
    uint32_t ack_timeouts;          // ACK response timeouts
    uint32_t retries;               // Command retry count
    uint32_t nacks_sent;            // Backpressure NACK sent
    uint32_t nacks_received;        // Backpressure NACK received
    uint32_t bytes_received;        // Total bytes received
    uint32_t bytes_sent;            // Total bytes sent
    uint8_t last_seq_received;      // Last sequence number received
    uint8_t last_seq_sent;          // Last sequence number sent
    uint8_t pending_cmd_count;      // Current pending commands
    bool handshake_complete;        // Protocol handshake completed
} protocol_stats_t;

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
    char build_date[12];        // "Dec 12 2024" (compile date)
    char build_time[7];        // "143045" (compile time, HHMMSS format, no colons)
    uint8_t protocol_version_major;  // Protocol version for compatibility check
    uint8_t protocol_version_minor;
} boot_payload_t;

// -----------------------------------------------------------------------------
// Protocol Handshake Payload (MSG_HANDSHAKE = 0x0C)
// -----------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
    uint8_t protocol_version_major;
    uint8_t protocol_version_minor;
    uint8_t capabilities;       // Bit flags for optional features
    uint8_t max_retry_count;    // Maximum retry attempts
    uint16_t ack_timeout_ms;    // ACK timeout in milliseconds
} handshake_payload_t;

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

// CONFIG_PREINFUSION (0x02) payload (for MSG_CMD_CONFIG)
typedef struct __attribute__((packed)) {
    uint8_t  enabled;            // 0=disabled, 1=enabled
    uint16_t on_time_ms;         // Pump ON duration (500-10000ms typical)
    uint16_t pause_time_ms;      // Soak/pause duration (0-30000ms typical)
} config_preinfusion_t;  // 5 bytes

// CONFIG_MACHINE_INFO (0x07) payload (for MSG_CMD_CONFIG)
typedef struct __attribute__((packed)) {
    char brand[16];               // Machine brand (null-terminated, max 15 chars)
    char model[16];               // Machine model (null-terminated, max 15 chars)
} config_machine_info_t;  // 32 bytes

// MSG_ENV_CONFIG (0x08) payload - Environmental config response
typedef struct __attribute__((packed)) {
    uint16_t nominal_voltage;        // 120, 230, 240, etc. (V)
    float    max_current_draw;       // 10.0, 16.0, etc. (A) - 4 bytes
    // Calculated values (for information)
    float    brew_heater_current;    // Calculated: brew_heater_power / nominal_voltage
    float    steam_heater_current;   // Calculated: steam_heater_power / nominal_voltage
    float    max_combined_current;   // Calculated: max_current_draw * 0.95
} env_config_payload_t;  // 18 bytes

// MSG_STATISTICS (0x09) payload - DEPRECATED
// Statistics are now tracked by ESP32 which has NTP for accurate timestamps.
// This message type is retained for protocol compatibility but is not used.
// Pico sends brew completion via ALARM_BREW_COMPLETED; ESP32 records statistics.
typedef struct __attribute__((packed)) {
    uint32_t total_brews;              // Total number of brews (all time)
    uint32_t total_brew_time_ms;       // Total brew time (all time)
    uint16_t avg_brew_time_ms;         // Average brew time (all time)
    uint16_t min_brew_time_ms;         // Shortest brew time
    uint16_t max_brew_time_ms;         // Longest brew time
    uint16_t daily_count;              // Cups in last 24 hours
    uint16_t daily_avg_time_ms;        // Average brew time (last 24 hours)
    uint16_t weekly_count;             // Cups in last 7 days
    uint16_t weekly_avg_time_ms;       // Average brew time (last 7 days)
    uint16_t monthly_count;            // Cups in last 30 days
    uint16_t monthly_avg_time_ms;      // Average brew time (last 30 days)
    uint32_t last_brew_timestamp;      // Timestamp of last brew
} statistics_payload_t;  // 28 bytes - DEPRECATED

// -----------------------------------------------------------------------------
// Diagnostics Payload (MSG_DIAGNOSTICS = 0x0A)
// -----------------------------------------------------------------------------
// Single diagnostic test result
typedef struct __attribute__((packed)) {
    uint8_t test_id;               // Test identifier (DIAG_TEST_*)
    uint8_t status;                // Result status (DIAG_STATUS_*)
    int16_t raw_value;             // Raw sensor reading (if applicable)
    int16_t expected_min;          // Expected minimum value
    int16_t expected_max;          // Expected maximum value
    char message[24];              // Result message (null-terminated)
} diag_result_payload_t;  // 32 bytes

// Diagnostic report header (followed by individual results)
typedef struct __attribute__((packed)) {
    uint8_t test_count;            // Number of tests in this report
    uint8_t pass_count;            // Tests passed
    uint8_t fail_count;            // Tests failed
    uint8_t warn_count;            // Tests with warnings
    uint8_t skip_count;            // Tests skipped
    uint8_t is_complete;           // 1 if all results sent, 0 if more coming
    uint16_t duration_ms;          // Total test duration
} diag_header_payload_t;  // 8 bytes

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
// DEPRECATED: Statistics are now tracked by ESP32. This function is retained for compatibility.
bool protocol_send_statistics(const statistics_payload_t* stats);
bool protocol_send_ack(uint8_t for_type, uint8_t seq, uint8_t result);
bool protocol_send_debug(const char* message);
// Log forwarding (MSG_LOG)
bool protocol_send_log(uint8_t level, const char* message);
// Diagnostics
bool protocol_send_diag_header(const diag_header_payload_t* header);
bool protocol_send_diag_result(const diag_result_payload_t* result);

// Error tracking
uint32_t protocol_get_crc_errors(void);
uint32_t protocol_get_packet_errors(void);
void protocol_reset_error_counters(void);

// Protocol diagnostics
void protocol_get_stats(protocol_stats_t* stats);
void protocol_reset_stats(void);

// Protocol state
bool protocol_is_ready(void);           // Returns true if handshake complete
bool protocol_handshake_complete(void); // Check handshake status
void protocol_request_handshake(void);  // Initiate handshake

// Packet callback
typedef void (*packet_callback_t)(const packet_t* packet);
void protocol_set_callback(packet_callback_t callback);

// CRC calculation
uint16_t protocol_crc16(const uint8_t* data, size_t length);

// Buffer access (for Class B RAM testing - reuses RX buffer)
// Returns pointer to RX buffer and its size
// WARNING: Only use when protocol is not actively receiving data
uint8_t* protocol_get_rx_buffer(size_t* buffer_size);

// -----------------------------------------------------------------------------
// Compile-time Payload Size Verification
// -----------------------------------------------------------------------------
// Static assertions to catch payload size mismatches at compile time
// These ensure all payloads fit within PROTOCOL_MAX_PAYLOAD (32 bytes)

_Static_assert(sizeof(config_payload_t) <= PROTOCOL_MAX_PAYLOAD,
               "config_payload_t exceeds PROTOCOL_MAX_PAYLOAD");
_Static_assert(sizeof(status_payload_t) <= PROTOCOL_MAX_PAYLOAD,
               "status_payload_t exceeds PROTOCOL_MAX_PAYLOAD");
_Static_assert(sizeof(env_config_payload_t) <= PROTOCOL_MAX_PAYLOAD,
               "env_config_payload_t exceeds PROTOCOL_MAX_PAYLOAD");
_Static_assert(sizeof(cmd_set_temp_t) <= PROTOCOL_MAX_PAYLOAD,
               "cmd_set_temp_t exceeds PROTOCOL_MAX_PAYLOAD");
_Static_assert(sizeof(cmd_set_pid_t) <= PROTOCOL_MAX_PAYLOAD,
               "cmd_set_pid_t exceeds PROTOCOL_MAX_PAYLOAD");
_Static_assert(sizeof(config_environmental_t) <= PROTOCOL_MAX_PAYLOAD,
               "config_environmental_t exceeds PROTOCOL_MAX_PAYLOAD");
_Static_assert(sizeof(config_preinfusion_t) <= PROTOCOL_MAX_PAYLOAD,
               "config_preinfusion_t exceeds PROTOCOL_MAX_PAYLOAD");
_Static_assert(sizeof(boot_payload_t) <= PROTOCOL_MAX_PAYLOAD,
               "boot_payload_t exceeds PROTOCOL_MAX_PAYLOAD");

#endif // PROTOCOL_H
