/**
 * Pico Firmware - Communication Protocol
 * 
 * Binary protocol implementation for ESP32 communication.
 */

#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/watchdog.h"
#include "protocol.h"
#include "config.h"
#include "pcb_config.h"
#include "machine_config.h"
#include "gpio_init.h"
#include "bootloader.h"

// Reset reason codes
#define RESET_REASON_POWER_ON      0  // Power-on reset
#define RESET_REASON_WATCHDOG      1  // Watchdog timeout
#define RESET_REASON_SOFTWARE      2  // Software reset (watchdog_reboot)
#define RESET_REASON_DEBUG         3  // Debug reset (debugger attached)
#define RESET_REASON_UNKNOWN       255

// -----------------------------------------------------------------------------
// Private State
// -----------------------------------------------------------------------------

// Receive state machine
typedef enum {
    RX_WAIT_SYNC,
    RX_GOT_TYPE,
    RX_GOT_LENGTH,
    RX_GOT_SEQ,
    RX_READING_PAYLOAD,
    RX_READING_CRC
} rx_state_t;

static rx_state_t g_rx_state = RX_WAIT_SYNC;
// RX buffer sized for max packet: header (4) + payload (32) + CRC (2) = 38 bytes, round to 40 for safety
static uint8_t g_rx_buffer[40];
static uint8_t g_rx_index = 0;
static uint8_t g_rx_length = 0;
static uint8_t g_tx_seq = 0;
static uint32_t g_rx_last_byte_time = 0;  // Timestamp of last received byte
static uint8_t g_last_seq_received = 0xFF; // Track last sequence number

static packet_callback_t g_packet_callback = NULL;

// Protocol statistics
static protocol_stats_t g_stats = {0};

// Handshake state
static bool g_handshake_complete = false;
static uint32_t g_handshake_request_time = 0;

// Retry tracking - pending commands awaiting ACK
static pending_cmd_t g_pending_cmds[PROTOCOL_MAX_PENDING_CMDS] = {0};

// Backpressure state
static bool g_backpressure_active = false;

// -----------------------------------------------------------------------------
// CRC-16-CCITT
// -----------------------------------------------------------------------------
uint16_t protocol_crc16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    
    return crc;
}

// -----------------------------------------------------------------------------
// Buffer Access (for Class B RAM testing)
// -----------------------------------------------------------------------------

uint8_t* protocol_get_rx_buffer(size_t* buffer_size) {
    if (buffer_size) {
        *buffer_size = sizeof(g_rx_buffer);
    }
    return g_rx_buffer;
}

// -----------------------------------------------------------------------------
// Retry & Backpressure Helpers
// -----------------------------------------------------------------------------

// Add command to pending list for retry tracking
static bool add_pending_command(uint8_t type, uint8_t seq, const uint8_t* payload, uint8_t length) {
    // Find free slot
    for (int i = 0; i < PROTOCOL_MAX_PENDING_CMDS; i++) {
        if (!g_pending_cmds[i].active) {
            g_pending_cmds[i].type = type;
            g_pending_cmds[i].seq = seq;
            g_pending_cmds[i].length = length;
            g_pending_cmds[i].retry_count = 0;
            g_pending_cmds[i].sent_time_ms = to_ms_since_boot(get_absolute_time());
            g_pending_cmds[i].active = true;
            if (length > 0 && payload != NULL) {
                memcpy(g_pending_cmds[i].payload, payload, length);
            }
            g_stats.pending_cmd_count++;
            
            // Check backpressure threshold
            if (g_stats.pending_cmd_count >= PROTOCOL_BACKPRESSURE_THRESHOLD) {
                g_backpressure_active = true;
            }
            
            return true;
        }
    }
    return false; // No free slots
}

// Remove command from pending list (ACK received)
static void remove_pending_command(uint8_t seq) {
    for (int i = 0; i < PROTOCOL_MAX_PENDING_CMDS; i++) {
        if (g_pending_cmds[i].active && g_pending_cmds[i].seq == seq) {
            g_pending_cmds[i].active = false;
            g_stats.pending_cmd_count--;
            
            // Release backpressure if below threshold
            if (g_stats.pending_cmd_count < PROTOCOL_BACKPRESSURE_THRESHOLD) {
                g_backpressure_active = false;
            }
            break;
        }
    }
}

// Check for ACK timeouts and retry commands
static void process_pending_commands(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    for (int i = 0; i < PROTOCOL_MAX_PENDING_CMDS; i++) {
        if (!g_pending_cmds[i].active) continue;
        
        pending_cmd_t* cmd = &g_pending_cmds[i];
        
        // Check if ACK timeout expired
        if (now - cmd->sent_time_ms > PROTOCOL_ACK_TIMEOUT_MS) {
            if (cmd->retry_count < PROTOCOL_RETRY_COUNT) {
                // Retry command
                cmd->retry_count++;
                cmd->sent_time_ms = now;
                g_stats.retries++;
                
                LOG_WARN("Protocol: Retrying command 0x%02X seq=%d (retry %d of %d)\n",
                       cmd->type, cmd->seq, cmd->retry_count, PROTOCOL_RETRY_COUNT);
                
                // Resend packet
                uint8_t buffer[64];
                uint8_t idx = 0;
                buffer[idx++] = PROTOCOL_SYNC_BYTE;
                buffer[idx++] = cmd->type;
                buffer[idx++] = cmd->length;
                buffer[idx++] = cmd->seq;
                if (cmd->length > 0) {
                    memcpy(&buffer[idx], cmd->payload, cmd->length);
                    idx += cmd->length;
                }
                uint16_t crc = protocol_crc16(&buffer[1], 3 + cmd->length);
                buffer[idx++] = crc & 0xFF;
                buffer[idx++] = (crc >> 8) & 0xFF;
                uart_write_blocking(ESP32_UART_ID, buffer, idx);
                g_stats.bytes_sent += idx;
            } else {
                // Max retries exceeded
                g_stats.ack_timeouts++;
                LOG_ERROR("Protocol: Command 0x%02X seq=%d failed after %d retries\n",
                        cmd->type, cmd->seq, PROTOCOL_RETRY_COUNT);
                cmd->active = false;
                g_stats.pending_cmd_count--;
                
                // Release backpressure
                if (g_stats.pending_cmd_count < PROTOCOL_BACKPRESSURE_THRESHOLD) {
                    g_backpressure_active = false;
                }
            }
        }
    }
}

// Forward declaration
static bool send_packet(uint8_t type, const uint8_t* payload, uint8_t length);

// Send NACK for backpressure
static void send_nack(uint8_t for_type, uint8_t seq) {
    ack_payload_t nack = {
        .cmd_type = for_type,
        .cmd_seq = seq,
        .result = ACK_ERROR_BUSY,
        .reserved = 0
    };
    send_packet(MSG_NACK, (const uint8_t*)&nack, sizeof(ack_payload_t));
    g_stats.nacks_sent++;
    LOG_DEBUG("Protocol: Sent NACK for 0x%02X (busy)\n", for_type);
}

// -----------------------------------------------------------------------------
// Initialization
// -----------------------------------------------------------------------------
void protocol_init(void) {
    LOG_PRINT("Protocol: Initializing UART communication\n");
    
    // Initialize UART
    uart_init(ESP32_UART_ID, ESP32_UART_BAUD);
    
    // Get UART pins from PCB config
    const pcb_config_t* pcb = pcb_config_get();
    int8_t tx_pin = ESP32_UART_TX_PIN;
    int8_t rx_pin = ESP32_UART_RX_PIN;
    if (pcb) {
        tx_pin = pcb->pins.uart_esp32_tx;
        rx_pin = pcb->pins.uart_esp32_rx;
    }
    
    // Configure UART pins
    if (PIN_VALID(tx_pin)) {
        gpio_set_function(tx_pin, GPIO_FUNC_UART);
    } else {
        LOG_PRINT("Protocol: WARNING - Invalid TX pin (%d)\n", tx_pin);
    }
    if (PIN_VALID(rx_pin)) {
        gpio_set_function(rx_pin, GPIO_FUNC_UART);
    } else {
        LOG_PRINT("Protocol: WARNING - Invalid RX pin (%d)\n", rx_pin);
    }
    
    // Set UART format
    uart_set_format(ESP32_UART_ID, 8, 1, UART_PARITY_NONE);
    uart_set_hw_flow(ESP32_UART_ID, false, false);
    uart_set_fifo_enabled(ESP32_UART_ID, true);
    
    // Initialize statistics
    memset(&g_stats, 0, sizeof(protocol_stats_t));
    g_handshake_complete = false;
    g_handshake_request_time = 0;
    g_rx_last_byte_time = 0;
    g_last_seq_received = 0xFF;
    
    LOG_PRINT("Protocol: UART%d initialized at %d baud (TX=%d, RX=%d)\n",
                ESP32_UART_ID == uart0 ? 0 : 1,
                ESP32_UART_BAUD,
                tx_pin,
                rx_pin);
}

// -----------------------------------------------------------------------------
// Send Packet
// -----------------------------------------------------------------------------
static bool send_packet(uint8_t type, const uint8_t* payload, uint8_t length) {
    if (length > PROTOCOL_MAX_PAYLOAD) {
        LOG_PRINT("Protocol: ERROR - Packet too large (type=0x%02X, len=%d, max=%d)\n",
                  type, length, PROTOCOL_MAX_PAYLOAD);
        return false;
    }
    
    // Check if this is a command that requires ACK tracking
    bool needs_ack = (type >= MSG_CMD_SET_TEMP && type <= MSG_LOG);
    
    // If backpressure is active and this needs ACK, check if we can send
    if (needs_ack && g_backpressure_active) {
        LOG_WARN("Protocol: Backpressure active, deferring command 0x%02X\n", type);
        return false;  // Caller should retry later
    }
    
    uint8_t buffer[64];
    uint8_t idx = 0;
    
    // Build packet
    buffer[idx++] = PROTOCOL_SYNC_BYTE;
    buffer[idx++] = type;
    buffer[idx++] = length;
    uint8_t seq = g_tx_seq++;
    buffer[idx++] = seq;
    
    // Copy payload
    if (length > 0 && payload != NULL) {
        memcpy(&buffer[idx], payload, length);
        idx += length;
    }
    
    // Calculate CRC (over type, length, seq, payload)
    uint16_t crc = protocol_crc16(&buffer[1], 3 + length);
    buffer[idx++] = crc & 0xFF;
    buffer[idx++] = (crc >> 8) & 0xFF;
    
    // Send
    uart_write_blocking(ESP32_UART_ID, buffer, idx);
    
    // Update statistics
    g_stats.packets_sent++;
    g_stats.bytes_sent += idx;
    g_stats.last_seq_sent = seq;
    
    // Add to pending commands for retry tracking if needed
    if (needs_ack) {
        if (!add_pending_command(type, seq, payload, length)) {
            LOG_WARN("Protocol: Failed to track command 0x%02X (pending queue full)\n", type);
        }
    }
    
    return true;
}

// -----------------------------------------------------------------------------
// Public Send Functions
// -----------------------------------------------------------------------------

bool protocol_send_status(const status_payload_t* status) {
    return send_packet(MSG_STATUS, (const uint8_t*)status, sizeof(status_payload_t));
}

bool protocol_send_alarm(uint8_t code, uint8_t severity, uint16_t value) {
    alarm_payload_t alarm = {
        .code = code,
        .severity = severity,
        .value = value
    };
    return send_packet(MSG_ALARM, (const uint8_t*)&alarm, sizeof(alarm_payload_t));
}

/**
 * Get the reset reason from hardware registers
 * Returns one of the RESET_REASON_* codes
 */
static uint8_t get_reset_reason(void) {
    // Check if watchdog caused the reboot
    if (watchdog_caused_reboot()) {
        // watchdog_enable_caused_reboot() distinguishes between:
        // - true: watchdog_reboot() was called (software reset)
        // - false: watchdog timer expired (actual watchdog timeout)
        if (watchdog_enable_caused_reboot()) {
            return RESET_REASON_SOFTWARE;
        } else {
            return RESET_REASON_WATCHDOG;
        }
    }
    
    // If not watchdog, it's a power-on reset or debug reset
    // We can't easily distinguish between POR and debug from SDK
    // Default to power-on reset
    return RESET_REASON_POWER_ON;
}

bool protocol_send_boot(void) {
    const pcb_config_t* pcb = pcb_config_get();
    pcb_version_t pcb_ver = pcb ? pcb->version : (pcb_version_t){0, 0, 0};
    pcb_type_t pcb_type = pcb ? pcb->type : PCB_TYPE_UNKNOWN;
    uint8_t reset_reason = get_reset_reason();
    
    const char* reset_reason_str = "UNKNOWN";
    switch (reset_reason) {
        case RESET_REASON_POWER_ON: reset_reason_str = "POWER_ON"; break;
        case RESET_REASON_WATCHDOG: reset_reason_str = "WATCHDOG"; break;
        case RESET_REASON_SOFTWARE: reset_reason_str = "SOFTWARE"; break;
        case RESET_REASON_DEBUG: reset_reason_str = "DEBUG"; break;
    }
    
    LOG_PRINT("Protocol: Sending boot message (v%d.%d.%d, reset: %s)\n",
              FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR, FIRMWARE_VERSION_PATCH,
              reset_reason_str);
    
    boot_payload_t boot = {
        .version_major = FIRMWARE_VERSION_MAJOR,
        .version_minor = FIRMWARE_VERSION_MINOR,
        .version_patch = FIRMWARE_VERSION_PATCH,
        .machine_type = (uint8_t)machine_get_type(),  // From machine config
        .pcb_type = (uint8_t)pcb_type,
        .pcb_version_major = pcb_ver.major,
        .pcb_version_minor = pcb_ver.minor,
        .reset_reason = reset_reason,
        .build_date = {0},
        .build_time = {0},
        .protocol_version_major = PROTOCOL_VERSION_MAJOR,
        .protocol_version_minor = PROTOCOL_VERSION_MINOR
    };
    // Copy build date/time (compile-time constants)
    strncpy(boot.build_date, BUILD_DATE, sizeof(boot.build_date) - 1);
    strncpy(boot.build_time, BUILD_TIME, sizeof(boot.build_time) - 1);
    
    bool result = send_packet(MSG_BOOT, (const uint8_t*)&boot, sizeof(boot_payload_t));
    if (!result) {
        LOG_PRINT("Protocol: ERROR - Failed to send boot message\n");
    }
    return result;
}

bool protocol_send_config(const config_payload_t* config) {
    return send_packet(MSG_CONFIG, (const uint8_t*)config, sizeof(config_payload_t));
}

bool protocol_send_env_config(const env_config_payload_t* env_config) {
    return send_packet(MSG_ENV_CONFIG, (const uint8_t*)env_config, sizeof(env_config_payload_t));
}

bool protocol_send_statistics(const statistics_payload_t* stats) {
    return send_packet(MSG_STATISTICS, (const uint8_t*)stats, sizeof(statistics_payload_t));
}

bool protocol_send_ack(uint8_t for_type, uint8_t seq, uint8_t result) {
    ack_payload_t ack = {
        .cmd_type = for_type,
        .cmd_seq = seq,
        .result = result,
        .reserved = 0
    };
    return send_packet(MSG_ACK, (const uint8_t*)&ack, sizeof(ack_payload_t));
}

bool protocol_send_debug(const char* message) {
    size_t len = strlen(message);
    if (len > PROTOCOL_MAX_PAYLOAD) {
        len = PROTOCOL_MAX_PAYLOAD;
    }
    return send_packet(MSG_DEBUG, (const uint8_t*)message, len);
}

bool protocol_send_log(uint8_t level, const char* message) {
    if (!message) return false;
    
    size_t msg_len = strlen(message);
    if (msg_len > PROTOCOL_MAX_PAYLOAD - 1) {
        msg_len = PROTOCOL_MAX_PAYLOAD - 1;  // Leave room for level byte
    }
    
    // Payload format: [level (1 byte)] [message (rest)]
    uint8_t payload[PROTOCOL_MAX_PAYLOAD];
    payload[0] = level;
    memcpy(&payload[1], message, msg_len);
    
    return send_packet(MSG_LOG, payload, msg_len + 1);
}

bool protocol_send_diag_header(const diag_header_payload_t* header) {
    return send_packet(MSG_DIAGNOSTICS, (const uint8_t*)header, sizeof(diag_header_payload_t));
}

bool protocol_send_diag_result(const diag_result_payload_t* result) {
    return send_packet(MSG_DIAGNOSTICS, (const uint8_t*)result, sizeof(diag_result_payload_t));
}

// -----------------------------------------------------------------------------
// Receive Processing
// -----------------------------------------------------------------------------

static void process_byte(uint8_t byte) {
    // Update byte timestamp for timeout detection
    g_rx_last_byte_time = to_ms_since_boot(get_absolute_time());
    g_stats.bytes_received++;
    
    switch (g_rx_state) {
        case RX_WAIT_SYNC:
            if (byte == PROTOCOL_SYNC_BYTE) {
                g_rx_index = 0;
                g_rx_state = RX_GOT_TYPE;
            }
            break;
            
        case RX_GOT_TYPE:
            g_rx_buffer[g_rx_index++] = byte;  // type
            g_rx_state = RX_GOT_LENGTH;
            break;
            
        case RX_GOT_LENGTH:
            g_rx_buffer[g_rx_index++] = byte;  // length
            g_rx_length = byte;
            g_rx_state = RX_GOT_SEQ;
            break;
            
        case RX_GOT_SEQ:
            g_rx_buffer[g_rx_index++] = byte;  // seq
            if (g_rx_length > 0) {
                g_rx_state = RX_READING_PAYLOAD;
            } else {
                g_rx_state = RX_READING_CRC;
            }
            break;
            
        case RX_READING_PAYLOAD:
            g_rx_buffer[g_rx_index++] = byte;
            if (g_rx_index >= 3 + g_rx_length) {
                g_rx_state = RX_READING_CRC;
            }
            break;
            
        case RX_READING_CRC:
            g_rx_buffer[g_rx_index++] = byte;
            if (g_rx_index >= 3 + g_rx_length + 2) {
                // Complete packet received
                packet_t packet;
                packet.type = g_rx_buffer[0];
                packet.length = g_rx_buffer[1];
                packet.seq = g_rx_buffer[2];
                packet.timestamp_ms = g_rx_last_byte_time;
                
                if (packet.length > 0 && packet.length <= PROTOCOL_MAX_PAYLOAD) {
                    memcpy(packet.payload, &g_rx_buffer[3], packet.length);
                }
                
                // Validate packet length field
                if (packet.length > PROTOCOL_MAX_PAYLOAD) {
                    g_stats.packet_errors++;
                    LOG_PRINT("Protocol: ERROR - Invalid packet length %d (max %d, total errors: %lu)\n", 
                               packet.length, PROTOCOL_MAX_PAYLOAD, g_stats.packet_errors);
                    g_rx_state = RX_WAIT_SYNC;
                    g_rx_index = 0;
                    break;
                }
                
                // Extract received CRC (little-endian)
                uint16_t received_crc = g_rx_buffer[3 + packet.length] |
                                       (g_rx_buffer[4 + packet.length] << 8);
                
                // Calculate expected CRC
                uint16_t expected_crc = protocol_crc16(g_rx_buffer, 3 + packet.length);
                
                if (received_crc == expected_crc) {
                    packet.valid = true;
                    packet.crc = received_crc;
                    
                    // Sequence number validation (skip for status/control messages)
                    bool check_sequence = (packet.type >= MSG_CMD_SET_TEMP);
                    if (check_sequence && g_last_seq_received != 0xFF) {
                        uint8_t expected_seq = (g_last_seq_received + 1) & 0xFF;
                        if (packet.seq != expected_seq) {
                            g_stats.sequence_errors++;
                            LOG_WARN("Protocol: Sequence error (got=%d, expected=%d)\n",
                                   packet.seq, expected_seq);
                        }
                    }
                    g_last_seq_received = packet.seq;
                    
                    // Handle handshake message
                    if (packet.type == MSG_HANDSHAKE) {
                        g_handshake_complete = true;
                        g_stats.handshake_complete = true;
                        LOG_INFO("Protocol: Handshake complete\n");
                    }
                    
                    // Handle ACK messages - remove from pending commands
                    if (packet.type == MSG_ACK && packet.length >= sizeof(ack_payload_t)) {
                        ack_payload_t ack;
                        memcpy(&ack, packet.payload, sizeof(ack_payload_t));
                        remove_pending_command(ack.cmd_seq);
                        DEBUG_PRINT("Protocol: ACK received for seq=%d (result=%d)\n",
                                  ack.cmd_seq, ack.result);
                    }
                    
                    // Handle NACK messages - backpressure signal from ESP32
                    if (packet.type == MSG_NACK) {
                        g_stats.nacks_received++;
                        LOG_WARN("Protocol: NACK received (ESP32 busy)\n");
                        // ESP32 is busy - could slow down command sending
                    }
                    
                    // Call callback
                    if (g_packet_callback) {
                        g_packet_callback(&packet);
                    } else {
                        DEBUG_PRINT("Protocol: WARNING - No callback registered for packet 0x%02X\n",
                                   packet.type);
                    }
                } else {
                    // CRC validation failed
                    g_stats.crc_errors++;
                    if (g_stats.crc_errors <= 5 || (g_stats.crc_errors % 10 == 0)) {
                        LOG_PRINT("Protocol: CRC error (got=0x%04X exp=0x%04X, total: %lu) type=0x%02X len=%d seq=%d\n", 
                                 received_crc, expected_crc, g_stats.crc_errors,
                                 packet.type, packet.length, packet.seq);
                    }
                }
                
                g_rx_state = RX_WAIT_SYNC;
            }
            break;
    }
    
    // Buffer overflow protection
    if (g_rx_index >= sizeof(g_rx_buffer)) {
        g_stats.packet_errors++;
        LOG_PRINT("Protocol: ERROR - Buffer overflow, resetting state (total errors: %lu)\n", g_stats.packet_errors);
        g_rx_state = RX_WAIT_SYNC;
        g_rx_index = 0;
        g_rx_length = 0;
    }
}

void protocol_process(void) {
    // Skip packet processing when bootloader is active
    // Bootloader handles UART directly with its own protocol
    if (bootloader_is_active()) {
        return;
    }
    
    // Check for parser timeout - reset if incomplete packet has been waiting too long
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (g_rx_state != RX_WAIT_SYNC && g_rx_last_byte_time > 0) {
        if (now - g_rx_last_byte_time > PROTOCOL_PARSER_TIMEOUT_MS) {
            g_stats.timeout_errors++;
            LOG_WARN("Protocol: Parser timeout (state=%d, waited=%lums)\n",
                   g_rx_state, now - g_rx_last_byte_time);
            g_rx_state = RX_WAIT_SYNC;
            g_rx_index = 0;
            g_rx_length = 0;
            g_rx_last_byte_time = 0;
        }
    }
    
    // Process pending commands (retry logic)
    process_pending_commands();
    
    // Process all available bytes
    while (uart_is_readable(ESP32_UART_ID)) {
        uint8_t byte = uart_getc(ESP32_UART_ID);
        process_byte(byte);
    }
}

void protocol_set_callback(packet_callback_t callback) {
    g_packet_callback = callback;
}

uint32_t protocol_get_crc_errors(void) {
    return g_stats.crc_errors;
}

uint32_t protocol_get_packet_errors(void) {
    return g_stats.packet_errors;
}

void protocol_reset_error_counters(void) {
    g_stats.crc_errors = 0;
    g_stats.packet_errors = 0;
    g_stats.timeout_errors = 0;
    g_stats.sequence_errors = 0;
}

void protocol_get_stats(protocol_stats_t* stats) {
    if (stats) {
        memcpy(stats, &g_stats, sizeof(protocol_stats_t));
    }
}

void protocol_reset_stats(void) {
    memset(&g_stats, 0, sizeof(protocol_stats_t));
    g_handshake_complete = false;
}

bool protocol_is_ready(void) {
    return g_handshake_complete;
}

bool protocol_handshake_complete(void) {
    return g_handshake_complete;
}

void protocol_request_handshake(void) {
    handshake_payload_t handshake = {
        .protocol_version_major = PROTOCOL_VERSION_MAJOR,
        .protocol_version_minor = PROTOCOL_VERSION_MINOR,
        .capabilities = 0,  // No special capabilities yet
        .max_retry_count = PROTOCOL_RETRY_COUNT,
        .ack_timeout_ms = PROTOCOL_ACK_TIMEOUT_MS
    };
    send_packet(MSG_HANDSHAKE, (const uint8_t*)&handshake, sizeof(handshake_payload_t));
    g_handshake_request_time = to_ms_since_boot(get_absolute_time());
    LOG_INFO("Protocol: Handshake requested (v%d.%d)\n", 
           PROTOCOL_VERSION_MAJOR, PROTOCOL_VERSION_MINOR);
}

