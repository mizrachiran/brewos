/**
 * Power Meter Driver for Raspberry Pi Pico 2
 * 
 * Supports external power metering modules via UART1 (GPIO6/7)
 * - PZEM-004T V3 (TTL UART, 9600 baud)
 * - JSY-MK-163T/194T (TTL UART, 4800 baud)
 * - Eastron SDM120/230 (RS485, various baud rates)
 * 
 * Data-driven architecture using Modbus register maps
 */

#ifndef POWER_METER_H
#define POWER_METER_H

#include <stdint.h>
#include <stdbool.h>

// =============================================================================
// DATA STRUCTURES
// =============================================================================

// Unified power meter reading structure
typedef struct {
    float voltage;        // Volts (RMS)
    float current;        // Amps (RMS)
    float power;          // Watts (Active)
    float energy_import;  // kWh (from grid)
    float energy_export;  // kWh (to grid - for solar/bidirectional)
    float frequency;      // Hz
    float power_factor;   // 0.0 - 1.0
    uint32_t timestamp;   // Timestamp when read (ms)
    bool valid;           // Reading successful
} power_meter_reading_t;

// Modbus register map configuration (data-driven approach)
typedef struct {
    const char* name;
    uint8_t slave_addr;
    uint32_t baud_rate;
    bool is_rs485;  // true = RS485, false = TTL UART
    
    // Register addresses and scaling factors
    uint16_t voltage_reg;
    float voltage_scale;
    
    uint16_t current_reg;
    float current_scale;
    
    uint16_t power_reg;
    float power_scale;
    
    uint16_t energy_reg;
    float energy_scale;
    bool energy_is_32bit;  // true if energy uses 2 consecutive registers
    
    uint16_t frequency_reg;
    float frequency_scale;
    
    uint16_t pf_reg;  // power factor register
    float pf_scale;
    
    uint8_t function_code;  // Modbus function code (0x03 or 0x04)
    uint8_t num_registers;  // Number of registers to read
} modbus_register_map_t;

// Power meter configuration (stored in flash)
typedef struct {
    bool enabled;
    uint8_t meter_index;  // Index into register maps array, or 0xFF for auto
    uint8_t slave_addr;   // Override slave address (0 = use default)
    uint32_t baud_rate;   // Override baud rate (0 = use default)
} power_meter_config_t;

// =============================================================================
// PUBLIC API
// =============================================================================

/**
 * Initialize power meter driver
 * @param config Configuration (NULL = load from flash)
 * @return true if initialization successful
 */
bool power_meter_init(const power_meter_config_t* config);

/**
 * Update power meter (call periodically, e.g., every 1 second)
 * Reads data from meter and updates internal state
 */
void power_meter_update(void);

/**
 * Get current power meter reading
 * @param reading Pointer to reading structure to fill
 * @return true if valid reading available
 */
bool power_meter_get_reading(power_meter_reading_t* reading);

/**
 * Check if power meter is connected and responding
 * @return true if meter responded recently
 */
bool power_meter_is_connected(void);

/**
 * Get meter name
 * @return Meter name string (e.g., "PZEM-004T V3")
 */
const char* power_meter_get_name(void);

/**
 * Auto-detect connected power meter
 * Tries all known meter configurations
 * @return true if meter detected
 */
bool power_meter_auto_detect(void);

/**
 * Save current configuration to flash
 * @return true if save successful
 */
bool power_meter_save_config(void);

/**
 * Load configuration from flash
 * @param config Pointer to config structure to fill
 * @return true if config loaded
 */
bool power_meter_load_config(power_meter_config_t* config);

/**
 * Get last error message
 * @return Error string, or NULL if no error
 */
const char* power_meter_get_error(void);

#endif // POWER_METER_H

