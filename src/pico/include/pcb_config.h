/**
 * ECM Pico Firmware - PCB Configuration
 * 
 * Centralized GPIO pin configuration for different PCB types and versions.
 * This allows the same firmware to support different hardware revisions.
 * 
 * PCB Type Selection:
 *   - Set PCB_TYPE at compile time (CMakeLists.txt or -DPCB_TYPE=...)
 *   - Each PCB type can have multiple versions
 *   - Pin assignments are validated at compile time
 * 
 * See docs/pico/Machine_Configurations.md for pin mapping details.
 */

#ifndef PCB_CONFIG_H
#define PCB_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

// =============================================================================
// PCB Type Definitions
// =============================================================================

typedef enum {
    PCB_TYPE_UNKNOWN = 0,
    PCB_TYPE_ECM_V1  = 1,  // ECM Synchronika V1 PCB
    PCB_TYPE_ECM_V2  = 2,  // ECM Synchronika V2 PCB (future)
    PCB_TYPE_CUSTOM  = 255
} pcb_type_t;

// =============================================================================
// PCB Version (within a type)
// =============================================================================

typedef struct {
    uint8_t major;  // Major revision (pinout changes)
    uint8_t minor;  // Minor revision (component changes, same pinout)
    uint8_t patch;  // Patch revision (bug fixes, same hardware)
} pcb_version_t;

// =============================================================================
// GPIO Pin Configuration
// =============================================================================

// Use -1 for pins that don't exist on this PCB
typedef struct {
    // ═══════════════════════════════════════════════════════════════
    // ANALOG INPUTS (ADC)
    // ═══════════════════════════════════════════════════════════════
    int8_t  adc_brew_ntc;        // ADC channel for brew boiler NTC thermistor
    int8_t  adc_steam_ntc;       // ADC channel for steam boiler NTC thermistor
    int8_t  adc_pressure;        // ADC channel for pressure transducer
    int8_t  adc_flow;            // ADC channel for flow meter (if analog)
    int8_t  adc_inlet_temp;      // ADC channel for inlet water temperature
    
    // ═══════════════════════════════════════════════════════════════
    // SPI (Thermocouple, etc.)
    // ═══════════════════════════════════════════════════════════════
    int8_t  spi_miso;            // SPI MISO (Master In, Slave Out)
    int8_t  spi_cs_thermocouple;  // SPI CS for thermocouple amplifier
    int8_t  spi_sck;             // SPI clock
    int8_t  spi_mosi;            // SPI MOSI (if needed)
    
    // ═══════════════════════════════════════════════════════════════
    // DIGITAL INPUTS
    // ═══════════════════════════════════════════════════════════════
    int8_t  input_reservoir;     // Reservoir empty switch
    int8_t  input_tank_level;   // Tank level sensor
    int8_t  input_steam_level;  // Steam boiler level probe
    int8_t  input_brew_switch;  // Brew button/switch
    int8_t  input_steam_switch; // Steam mode switch (single boiler)
    int8_t  input_water_mode;   // Water mode switch: 0=water tank, 1=plumbed (physical switch)
    int8_t  input_flow_pulse;   // Flow meter pulse output
    int8_t  input_emergency_stop; // Emergency stop button
    int8_t  input_weight_stop;  // WEIGHT_STOP signal from ESP32 (brew-by-weight, J15 Pin 7)
    int8_t  input_spare;        // SPARE input from ESP32 (reserved for future, J15 Pin 8)
    
    // ═══════════════════════════════════════════════════════════════
    // RELAY OUTPUTS
    // ═══════════════════════════════════════════════════════════════
    int8_t  relay_pump;          // Pump relay
    int8_t  relay_brew_solenoid; // 3-way brew solenoid
    int8_t  relay_water_led;     // Water LED indicator
    int8_t  relay_fill_solenoid; // Auto-fill solenoid (plumbed machines)
    int8_t  relay_spare;         // Spare relay
    
    // ═══════════════════════════════════════════════════════════════
    // SSR OUTPUTS (PWM for heating)
    // ═══════════════════════════════════════════════════════════════
    int8_t  ssr_brew;            // Brew boiler SSR (PWM)
    int8_t  ssr_steam;           // Steam boiler SSR (PWM)
    
    // ═══════════════════════════════════════════════════════════════
    // USER INTERFACE
    // ═══════════════════════════════════════════════════════════════
    int8_t  led_status;          // Status LED
    int8_t  buzzer;              // Buzzer/beeper
    
    // ═══════════════════════════════════════════════════════════════
    // COMMUNICATION
    // ═══════════════════════════════════════════════════════════════
    int8_t  uart_esp32_tx;       // UART TX to ESP32
    int8_t  uart_esp32_rx;       // UART RX from ESP32
    int8_t  uart_pzem_tx;        // UART TX to PZEM (power meter, optional)
    int8_t  uart_pzem_rx;        // UART RX from PZEM (power meter, optional)
    int8_t  i2c_sda;             // I2C SDA
    int8_t  i2c_scl;             // I2C SCL
    
    // ═══════════════════════════════════════════════════════════════
    // ESP32 CONTROL (for firmware updates)
    // ═══════════════════════════════════════════════════════════════
    // Note: Pico RUN and BOOTSEL pins are hardware control pins (not GPIO)
    // They are controlled by ESP32 via J15 pins 5 and 6, not accessible as GPIO
    // These are handled at the hardware level, not in GPIO configuration
    
} pcb_pin_config_t;

// =============================================================================
// Complete PCB Configuration
// =============================================================================

typedef struct {
    pcb_type_t      type;           // PCB type
    pcb_version_t   version;        // PCB version
    const char*     name;           // Human-readable name
    const char*     description;   // PCB description
    pcb_pin_config_t pins;          // GPIO pin assignments
} pcb_config_t;

// =============================================================================
// PCB Configuration Instances
// =============================================================================

// ECM Synchronika V1 PCB Configuration
// Pin assignments based on initial PCB design
static const pcb_config_t PCB_ECM_V1 = {
    .type = PCB_TYPE_ECM_V1,
    .version = { .major = 1, .minor = 0, .patch = 0 },
    .name = "ECM Synchronika V1",
    .description = "Initial PCB for ECM Synchronika dual boiler",
    
    .pins = {
        // Analog inputs (ADC)
        .adc_brew_ntc       = 26,  // GPIO26 = ADC0
        .adc_steam_ntc      = 27,  // GPIO27 = ADC1
        .adc_pressure       = 28,  // GPIO28 = ADC2
        .adc_flow           = -1,  // Not used
        .adc_inlet_temp     = -1,  // Not used
        
        // SPI
        .spi_miso           = 16,  // GPIO16 (SPI0 MISO)
        .spi_cs_thermocouple = 17, // GPIO17 (SPI0 CS)
        .spi_sck            = 18,  // GPIO18 (SPI0 SCK)
        .spi_mosi           = -1,  // Not used (MAX31855 is read-only)
        
        // Digital inputs
        .input_reservoir    = 2,   // GPIO2
        .input_tank_level   = 3,   // GPIO3
        .input_steam_level  = 4,   // GPIO4
        .input_brew_switch  = 5,   // GPIO5
        .input_steam_switch = -1,  // Not used (dual boiler)
        .input_flow_pulse   = -1,  // Not used
        .input_emergency_stop = -1, // Not used
        .input_weight_stop  = 21,  // GPIO21 (WEIGHT_STOP from ESP32, J15 Pin 7)
        .input_spare        = 22,  // GPIO22 (SPARE from ESP32, J15 Pin 8, reserved)
        
        // Relay outputs
        .relay_pump         = 11,  // GPIO11
        .relay_brew_solenoid = 12, // GPIO12
        .relay_water_led    = 10,  // GPIO10
        .relay_fill_solenoid = -1, // Not used
        .relay_spare        = 20,  // GPIO20
        
        // SSR outputs (PWM)
        .ssr_brew           = 13,  // GPIO13 (PWM)
        .ssr_steam          = 14,  // GPIO14 (PWM)
        
        // User interface
        .led_status         = 15,  // GPIO15 (Status LED)
        .buzzer             = 19,  // GPIO19 (Buzzer PWM)
        
        // Communication
        .uart_esp32_tx      = 0,   // GPIO0 (UART0 TX)
        .uart_esp32_rx      = 1,   // GPIO1 (UART0 RX)
        .uart_pzem_tx       = 6,   // GPIO6 (UART1 TX, optional)
        .uart_pzem_rx       = 7,   // GPIO7 (UART1 RX, optional)
        .i2c_sda            = 8,   // GPIO8 (I2C0 SDA)
        .i2c_scl            = 9,   // GPIO9 (I2C0 SCL)
        
        // Note: Pico RUN and BOOTSEL are hardware pins controlled by ESP32
        // via J15 pins 5 and 6. They are NOT GPIO pins and not configured here.
    }
};

// =============================================================================
// Active PCB Configuration Selection
// =============================================================================

// Select PCB type at compile time
// Override in CMakeLists.txt: -DPCB_TYPE=PCB_TYPE_ECM_V1
#ifndef PCB_TYPE
#define PCB_TYPE PCB_TYPE_ECM_V1
#endif

// Get the active PCB configuration
#define PCB_CONFIG_GET() \
    ((PCB_TYPE == PCB_TYPE_ECM_V1) ? &PCB_ECM_V1 : \
     ((void)0, (const pcb_config_t*)NULL))  // Unknown PCB type

// Convenience macros for active PCB config
#define PCB_PINS (PCB_CONFIG_GET()->pins)
#define PCB_TYPE_VALUE (PCB_CONFIG_GET()->type)
#define PCB_VERSION (PCB_CONFIG_GET()->version)
#define PCB_NAME (PCB_CONFIG_GET()->name)

// =============================================================================
// Pin Validation Macros
// =============================================================================

// Check if a pin is valid (not -1)
#define PIN_VALID(pin) ((pin) >= 0 && (pin) <= 28)

// Check if a pin is configured
#define PIN_CONFIGURED(pin) PIN_VALID(pin)

// =============================================================================
// Functions
// =============================================================================

/**
 * Get the active PCB configuration
 * Returns NULL if PCB type is not configured
 */
const pcb_config_t* pcb_config_get(void);

/**
 * Get PCB type
 */
pcb_type_t pcb_get_type(void);

/**
 * Get PCB version
 */
pcb_version_t pcb_get_version(void);

/**
 * Get PCB name string
 */
const char* pcb_get_name(void);

/**
 * Validate pin configuration
 * Returns true if all required pins are valid
 */
bool pcb_validate_pins(void);

#endif // PCB_CONFIG_H

