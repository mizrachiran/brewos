/**
 * Power Meter Driver Implementation (Raspberry Pi Pico 2)
 * 
 * Hardware: UART1 on GPIO6 (TX) and GPIO7 (RX), GPIO20 for RS485 DE/RE
 */

#include "power_meter.h"
#include <string.h>
#include <stdio.h>

#ifndef UNIT_TEST
// Real hardware environment
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "flash_config.h"

#define UART_ID uart1

#else
// Unit test environment - use mocked UART
// uart1 is defined in mock_hardware.h as ((void*)1)
#define UART_ID uart1

#endif

// =============================================================================
// HARDWARE CONFIGURATION
// =============================================================================

#define UART_TX_PIN 6
#define UART_RX_PIN 7
#define RS485_DE_RE_PIN 20

// Modbus protocol
#define MODBUS_FC_READ_HOLDING_REGS 0x03
#define MODBUS_FC_READ_INPUT_REGS   0x04

#define RESPONSE_TIMEOUT_MS 500
#define CONNECTION_TIMEOUT_MS 5000

// =============================================================================
// REGISTER MAPS FOR SUPPORTED METERS
// =============================================================================

static const modbus_register_map_t METER_MAPS[] = {
    // PZEM-004T V3
    {
        .name = "PZEM-004T V3",
        .slave_addr = 0xF8,
        .baud_rate = 9600,
        .is_rs485 = false,
        .voltage_reg = 0x0000,
        .voltage_scale = 0.1f,
        .current_reg = 0x0001,
        .current_scale = 0.001f,
        .power_reg = 0x0002,
        .power_scale = 1.0f,
        .energy_reg = 0x0003,
        .energy_scale = 1.0f,
        .energy_is_32bit = true,
        .frequency_reg = 0x0004,
        .frequency_scale = 0.1f,
        .pf_reg = 0x0005,
        .pf_scale = 0.01f,
        .function_code = MODBUS_FC_READ_INPUT_REGS,
        .num_registers = 10
    },
    // JSY-MK-163T
    {
        .name = "JSY-MK-163T",
        .slave_addr = 0x01,
        .baud_rate = 4800,
        .is_rs485 = false,
        .voltage_reg = 0x0048,
        .voltage_scale = 0.0001f,
        .current_reg = 0x0049,
        .current_scale = 0.0001f,
        .power_reg = 0x004A,
        .power_scale = 0.0001f,
        .energy_reg = 0x004B,
        .energy_scale = 0.001f,
        .energy_is_32bit = true,
        .frequency_reg = 0x0057,
        .frequency_scale = 0.01f,
        .pf_reg = 0x0056,
        .pf_scale = 0.001f,
        .function_code = MODBUS_FC_READ_HOLDING_REGS,
        .num_registers = 16
    },
    // JSY-MK-194T
    {
        .name = "JSY-MK-194T",
        .slave_addr = 0x01,
        .baud_rate = 4800,
        .is_rs485 = false,
        .voltage_reg = 0x0000,
        .voltage_scale = 0.01f,
        .current_reg = 0x0001,
        .current_scale = 0.01f,
        .power_reg = 0x0002,
        .power_scale = 0.1f,
        .energy_reg = 0x0003,
        .energy_scale = 0.01f,
        .energy_is_32bit = true,
        .frequency_reg = 0x0007,
        .frequency_scale = 0.01f,
        .pf_reg = 0x0008,
        .pf_scale = 0.001f,
        .function_code = MODBUS_FC_READ_HOLDING_REGS,
        .num_registers = 10
    },
    // Eastron SDM120
    {
        .name = "Eastron SDM120",
        .slave_addr = 0x01,
        .baud_rate = 2400,
        .is_rs485 = true,
        .voltage_reg = 0x0000,
        .voltage_scale = 1.0f,
        .current_reg = 0x0006,
        .current_scale = 1.0f,
        .power_reg = 0x000C,
        .power_scale = 1.0f,
        .energy_reg = 0x0048,
        .energy_scale = 1.0f,
        .energy_is_32bit = false,
        .frequency_reg = 0x0046,
        .frequency_scale = 1.0f,
        .pf_reg = 0x001E,
        .pf_scale = 1.0f,
        .function_code = MODBUS_FC_READ_INPUT_REGS,
        .num_registers = 2
    },
    // Eastron SDM230
    {
        .name = "Eastron SDM230",
        .slave_addr = 0x01,
        .baud_rate = 9600,
        .is_rs485 = true,
        .voltage_reg = 0x0000,
        .voltage_scale = 1.0f,
        .current_reg = 0x0006,
        .current_scale = 1.0f,
        .power_reg = 0x000C,
        .power_scale = 1.0f,
        .energy_reg = 0x0156,
        .energy_scale = 1.0f,
        .energy_is_32bit = false,
        .frequency_reg = 0x0046,
        .frequency_scale = 1.0f,
        .pf_reg = 0x001E,
        .pf_scale = 1.0f,
        .function_code = MODBUS_FC_READ_INPUT_REGS,
        .num_registers = 2
    }
};

#define METER_MAPS_COUNT (sizeof(METER_MAPS) / sizeof(METER_MAPS[0]))

// =============================================================================
// PRIVATE STATE
// =============================================================================

static bool initialized = false;
static const modbus_register_map_t* current_map = NULL;
static power_meter_reading_t last_reading = {0};
static uint32_t last_success_time = 0;
static char last_error[64] = {0};
static power_meter_config_t current_config = {0};

// =============================================================================
// MODBUS PROTOCOL HELPERS
// =============================================================================

static uint16_t modbus_crc16(const uint8_t* buffer, uint16_t length) {
    uint16_t crc = 0xFFFF;
    
    for (uint16_t i = 0; i < length; i++) {
        crc ^= buffer[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc;
}

static void set_rs485_direction(bool transmit) {
    if (current_map && current_map->is_rs485) {
        gpio_put(RS485_DE_RE_PIN, transmit);
        if (transmit) {
            sleep_us(100);  // Small delay for transceiver switching
        }
    }
}

static bool send_modbus_request(uint8_t slave_addr, uint8_t function_code, 
                                uint16_t start_reg, uint16_t num_regs) {
    uint8_t request[8];
    request[0] = slave_addr;
    request[1] = function_code;
    request[2] = (start_reg >> 8) & 0xFF;
    request[3] = start_reg & 0xFF;
    request[4] = (num_regs >> 8) & 0xFF;
    request[5] = num_regs & 0xFF;
    
    // Calculate CRC
    uint16_t crc = modbus_crc16(request, 6);
    request[6] = crc & 0xFF;
    request[7] = (crc >> 8) & 0xFF;
    
    // Send request
    set_rs485_direction(true);
    uart_write_blocking(UART_ID, request, 8);
    set_rs485_direction(false);
    
    return true;
}

static bool receive_modbus_response(uint8_t* buffer, int max_len, int* bytes_read) {
    *bytes_read = 0;
    uint32_t start_time = time_us_32() / 1000;  // Convert to ms
    
    while ((time_us_32() / 1000 - start_time) < RESPONSE_TIMEOUT_MS) {
        if (uart_is_readable(UART_ID)) {
            buffer[*bytes_read] = uart_getc(UART_ID);
            (*bytes_read)++;
            
            // Check if we have enough bytes for a complete response
            if (*bytes_read >= 5) {
                uint8_t expected_len = buffer[2] + 5;  // data length + header + crc
                if (*bytes_read >= expected_len) {
                    return true;
                }
            }
            
            if (*bytes_read >= max_len) {
                break;
            }
            
            start_time = time_us_32() / 1000;  // Reset timeout on each byte
        }
        sleep_ms(1);
    }
    
    return false;
}

static bool verify_modbus_response(const uint8_t* buffer, int length) {
    if (length < 5) return false;
    
    // Check slave address
    if (buffer[0] != current_map->slave_addr) return false;
    
    // Check function code
    if (buffer[1] != current_map->function_code) return false;
    
    // Verify CRC
    uint16_t received_crc = buffer[length - 2] | (buffer[length - 1] << 8);
    uint16_t calculated_crc = modbus_crc16(buffer, length - 2);
    
    return received_crc == calculated_crc;
}

static uint16_t extract_uint16(const uint8_t* buffer, int offset) {
    return (buffer[offset] << 8) | buffer[offset + 1];
}

static uint32_t extract_uint32(const uint8_t* buffer, int offset) {
    return ((uint32_t)buffer[offset] << 24) | 
           ((uint32_t)buffer[offset + 1] << 16) |
           ((uint32_t)buffer[offset + 2] << 8) | 
           buffer[offset + 3];
}

static bool parse_response(const uint8_t* buffer, int length, power_meter_reading_t* reading) {
    if (length < 5) return false;
    
    uint8_t byte_count = buffer[2];
    const uint8_t* data = &buffer[3];
    
    // Initialize reading
    memset(reading, 0, sizeof(power_meter_reading_t));
    
    // Calculate register offsets from start
    int voltage_offset = (current_map->voltage_reg - current_map->voltage_reg) * 2;
    int current_offset = (current_map->current_reg - current_map->voltage_reg) * 2;
    int power_offset = (current_map->power_reg - current_map->voltage_reg) * 2;
    int energy_offset = (current_map->energy_reg - current_map->voltage_reg) * 2;
    int frequency_offset = (current_map->frequency_reg - current_map->voltage_reg) * 2;
    int pf_offset = (current_map->pf_reg - current_map->voltage_reg) * 2;
    
    // Extract voltage
    if (voltage_offset >= 0 && voltage_offset + 1 < byte_count) {
        uint16_t raw = extract_uint16(data, voltage_offset);
        reading->voltage = raw * current_map->voltage_scale;
    }
    
    // Extract current
    if (current_offset >= 0 && current_offset + 1 < byte_count) {
        uint16_t raw = extract_uint16(data, current_offset);
        reading->current = raw * current_map->current_scale;
    }
    
    // Extract power
    if (power_offset >= 0 && power_offset + 1 < byte_count) {
        uint16_t raw = extract_uint16(data, power_offset);
        reading->power = raw * current_map->power_scale;
    }
    
    // Extract energy
    if (energy_offset >= 0 && energy_offset + (current_map->energy_is_32bit ? 3 : 1) < byte_count) {
        if (current_map->energy_is_32bit) {
            uint32_t raw = extract_uint32(data, energy_offset);
            reading->energy_import = raw * current_map->energy_scale / 1000.0f;  // Wh to kWh
        } else {
            uint16_t raw = extract_uint16(data, energy_offset);
            reading->energy_import = raw * current_map->energy_scale;
        }
    }
    
    // Extract frequency
    if (frequency_offset >= 0 && frequency_offset + 1 < byte_count) {
        uint16_t raw = extract_uint16(data, frequency_offset);
        reading->frequency = raw * current_map->frequency_scale;
    }
    
    // Extract power factor
    if (pf_offset >= 0 && pf_offset + 1 < byte_count) {
        uint16_t raw = extract_uint16(data, pf_offset);
        reading->power_factor = raw * current_map->pf_scale;
    }
    
    return true;
}

// =============================================================================
// PUBLIC FUNCTIONS
// =============================================================================

bool power_meter_init(const power_meter_config_t* config) {
    // Load or use provided config
    if (config) {
        current_config = *config;
    } else {
        if (!power_meter_load_config(&current_config)) {
            // No saved config, use defaults
            current_config.enabled = false;
            current_config.meter_index = 0xFF;  // Auto-detect
            current_config.slave_addr = 0;
            current_config.baud_rate = 0;
        }
    }
    
    if (!current_config.enabled) {
        return true;  // Disabled, nothing to do
    }
    
    // Select register map
    if (current_config.meter_index < METER_MAPS_COUNT) {
        current_map = &METER_MAPS[current_config.meter_index];
    } else if (current_config.meter_index == 0xFF) {
        // Auto-detect
        return power_meter_auto_detect();
    } else {
        snprintf(last_error, sizeof(last_error), "Invalid meter index");
        return false;
    }
    
    // Initialize UART
    uart_init(UART_ID, current_map->baud_rate);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
    
    // Configure RS485 direction pin if needed
    if (current_map->is_rs485) {
        gpio_init(RS485_DE_RE_PIN);
        gpio_set_dir(RS485_DE_RE_PIN, GPIO_OUT);
        gpio_put(RS485_DE_RE_PIN, 0);  // Start in receive mode
    }
    
    initialized = true;
    printf("Power meter initialized: %s @ %u baud\n", current_map->name, (unsigned int)current_map->baud_rate);
    
    return true;
}

void power_meter_update(void) {
    if (!initialized || !current_map) {
        return;
    }
    
    // Clear any stale data
    while (uart_is_readable(UART_ID)) {
        uart_getc(UART_ID);
    }
    
    // Send Modbus request
    if (!send_modbus_request(current_map->slave_addr, current_map->function_code,
                            current_map->voltage_reg, current_map->num_registers)) {
        snprintf(last_error, sizeof(last_error), "Failed to send request");
        return;
    }
    
    // Receive response
    uint8_t response_buffer[128];
    int bytes_read = 0;
    if (!receive_modbus_response(response_buffer, sizeof(response_buffer), &bytes_read)) {
        snprintf(last_error, sizeof(last_error), "No response from meter");
        return;
    }
    
    // Verify response
    if (!verify_modbus_response(response_buffer, bytes_read)) {
        snprintf(last_error, sizeof(last_error), "Invalid response");
        return;
    }
    
    // Parse response
    power_meter_reading_t reading;
    if (!parse_response(response_buffer, bytes_read, &reading)) {
        snprintf(last_error, sizeof(last_error), "Parse error");
        return;
    }
    
    // Success
    reading.timestamp = time_us_32() / 1000;
    reading.valid = true;
    last_reading = reading;
    last_success_time = reading.timestamp;
    last_error[0] = '\0';
}

bool power_meter_get_reading(power_meter_reading_t* reading) {
    if (!reading) return false;
    
    // Check if reading is fresh (within last 5 seconds)
    uint32_t now = time_us_32() / 1000;
    if (last_reading.valid && (now - last_success_time) < CONNECTION_TIMEOUT_MS) {
        *reading = last_reading;
        return true;
    }
    
    return false;
}

bool power_meter_is_connected(void) {
    if (!initialized) return false;
    
    uint32_t now = time_us_32() / 1000;
    return (now - last_success_time) < CONNECTION_TIMEOUT_MS;
}

const char* power_meter_get_name(void) {
    return current_map ? current_map->name : "None";
}

bool power_meter_auto_detect(void) {
    printf("Starting power meter auto-detection...\n");
    
    // Try each known meter configuration
    for (uint8_t i = 0; i < METER_MAPS_COUNT; i++) {
        const modbus_register_map_t* test_map = &METER_MAPS[i];
        printf("Trying %s @ %u baud...\n", test_map->name, (unsigned int)test_map->baud_rate);
        
        current_map = test_map;
        
        // Initialize UART for this config
        uart_deinit(UART_ID);
        uart_init(UART_ID, test_map->baud_rate);
        gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
        gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
        uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
        
        // Configure RS485 if needed
        if (test_map->is_rs485) {
            gpio_init(RS485_DE_RE_PIN);
            gpio_set_dir(RS485_DE_RE_PIN, GPIO_OUT);
            gpio_put(RS485_DE_RE_PIN, 0);
        }
        
        sleep_ms(100);  // Let UART settle
        
        // Try to read
        power_meter_reading_t test_reading;
        
        // Clear buffer
        while (uart_is_readable(UART_ID)) {
            uart_getc(UART_ID);
        }
        
        // Send request
        if (!send_modbus_request(test_map->slave_addr, test_map->function_code,
                                test_map->voltage_reg, test_map->num_registers)) {
            continue;
        }
        
        // Receive response
        uint8_t response_buffer[128];
        int bytes_read = 0;
        if (!receive_modbus_response(response_buffer, sizeof(response_buffer), &bytes_read)) {
            continue;
        }
        
        // Verify and parse
        if (verify_modbus_response(response_buffer, bytes_read) &&
            parse_response(response_buffer, bytes_read, &test_reading)) {
            
            // Verify reading is reasonable
            if (test_reading.voltage > 50 && test_reading.voltage < 300) {
                printf("Detected: %s\n", test_map->name);
                initialized = true;
                last_reading = test_reading;
                last_success_time = time_us_32() / 1000;
                
                // Save config
                current_config.enabled = true;
                current_config.meter_index = i;
                power_meter_save_config();
                
                return true;
            }
        }
        
        sleep_ms(200);  // Wait before next attempt
    }
    
    printf("No power meter detected\n");
    snprintf(last_error, sizeof(last_error), "Auto-detection failed");
    initialized = false;
    current_map = NULL;
    return false;
}

bool power_meter_save_config(void) {
#ifndef UNIT_TEST
    // Save to flash using flash_config system
    // TODO: Integrate with flash_config.c when available
    return true;
#else
    // In unit tests, always succeed
    return true;
#endif
}

bool power_meter_load_config(power_meter_config_t* config) {
#ifndef UNIT_TEST
    // Load from flash using flash_config system
    // TODO: Integrate with flash_config.c when available
    return false;
#else
    // In unit tests, always fail (no config)
    (void)config;
    return false;
#endif
}

const char* power_meter_get_error(void) {
    return last_error[0] != '\0' ? last_error : NULL;
}

