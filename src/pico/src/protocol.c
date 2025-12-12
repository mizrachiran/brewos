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
static uint8_t g_rx_buffer[64];
static uint8_t g_rx_index = 0;
static uint8_t g_rx_length = 0;
static uint8_t g_tx_seq = 0;

static packet_callback_t g_packet_callback = NULL;

// Error tracking
static uint32_t g_crc_errors = 0;
static uint32_t g_packet_errors = 0;

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
    
    // Reset error counters
    g_crc_errors = 0;
    g_packet_errors = 0;
    
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
    
    uint8_t buffer[64];
    uint8_t idx = 0;
    
    // Build packet
    buffer[idx++] = PROTOCOL_SYNC_BYTE;
    buffer[idx++] = type;
    buffer[idx++] = length;
    buffer[idx++] = g_tx_seq++;
    
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
        .build_time = {0}
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
                
                if (packet.length > 0 && packet.length <= PROTOCOL_MAX_PAYLOAD) {
                    memcpy(packet.payload, &g_rx_buffer[3], packet.length);
                }
                
                // Validate packet length field
                if (packet.length > PROTOCOL_MAX_PAYLOAD) {
                    g_packet_errors++;
                    LOG_PRINT("Protocol: ERROR - Invalid packet length %d (max %d, total errors: %lu)\n", 
                               packet.length, PROTOCOL_MAX_PAYLOAD, g_packet_errors);
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
                    
                    DEBUG_PRINT("Protocol: RX packet type=0x%02X len=%d seq=%d\n",
                               packet.type, packet.length, packet.seq);
                    
                    // Call callback
                    if (g_packet_callback) {
                        g_packet_callback(&packet);
                    } else {
                        DEBUG_PRINT("Protocol: WARNING - No callback registered for packet 0x%02X\n",
                                   packet.type);
                    }
                } else {
                    g_crc_errors++;
                    if (g_crc_errors <= 5 || (g_crc_errors % 10 == 0)) {
                        LOG_PRINT("Protocol: CRC error (got=0x%04X exp=0x%04X, total: %lu)\n", 
                                 received_crc, expected_crc, g_crc_errors);
                    }
                }
                
                g_rx_state = RX_WAIT_SYNC;
            }
            break;
    }
    
    // Buffer overflow protection
    if (g_rx_index >= sizeof(g_rx_buffer)) {
        g_packet_errors++;
        LOG_PRINT("Protocol: ERROR - Buffer overflow, resetting state (total errors: %lu)\n", g_packet_errors);
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
    return g_crc_errors;
}

uint32_t protocol_get_packet_errors(void) {
    return g_packet_errors;
}

void protocol_reset_error_counters(void) {
    g_crc_errors = 0;
    g_packet_errors = 0;
}

