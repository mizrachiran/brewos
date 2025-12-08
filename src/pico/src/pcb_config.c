/**
 * ECM Pico Firmware - PCB Configuration Implementation
 * 
 * Provides runtime access to PCB configuration and validation.
 */

#include <stddef.h>
#include "pcb_config.h"

// =============================================================================
// Active PCB Configuration
// =============================================================================

const pcb_config_t* pcb_config_get(void) {
    return PCB_CONFIG_GET();
}

pcb_type_t pcb_get_type(void) {
    const pcb_config_t* config = pcb_config_get();
    return config ? config->type : PCB_TYPE_UNKNOWN;
}

pcb_version_t pcb_get_version(void) {
    const pcb_config_t* config = pcb_config_get();
    pcb_version_t version = {0, 0, 0};
    if (config) {
        version = config->version;
    }
    return version;
}

const char* pcb_get_name(void) {
    const pcb_config_t* config = pcb_config_get();
    return config ? config->name : "Unknown PCB";
}

// =============================================================================
// Pin Validation
// =============================================================================

bool pcb_validate_pins(void) {
    const pcb_config_t* config = pcb_config_get();
    if (!config) {
        return false;
    }
    
    const pcb_pin_config_t* pins = &config->pins;
    
    // Check for duplicate pins (basic validation)
    // This is a simple check - could be expanded
    int8_t used_pins[32] = {0};
    int pin_count = 0;
    
    // Helper to check and record pin
    #define CHECK_PIN(pin) \
        do { \
            if (PIN_VALID(pin)) { \
                for (int i = 0; i < pin_count; i++) { \
                    if (used_pins[i] == (pin)) { \
                        return false; /* Duplicate pin */ \
                    } \
                } \
                used_pins[pin_count++] = (pin); \
            } \
        } while (0)
    
    // Check all pins
    CHECK_PIN(pins->adc_brew_ntc);
    CHECK_PIN(pins->adc_steam_ntc);
    CHECK_PIN(pins->adc_pressure);
    CHECK_PIN(pins->adc_flow);
    CHECK_PIN(pins->adc_inlet_temp);
    CHECK_PIN(pins->spi_miso);
    CHECK_PIN(pins->spi_sck);
    CHECK_PIN(pins->spi_cs);
    CHECK_PIN(pins->spi_mosi);
    CHECK_PIN(pins->input_reservoir);
    CHECK_PIN(pins->input_tank_level);
    CHECK_PIN(pins->input_steam_level);
    CHECK_PIN(pins->input_brew_switch);
    CHECK_PIN(pins->input_steam_switch);
    CHECK_PIN(pins->input_water_mode);
    CHECK_PIN(pins->input_flow_pulse);
    CHECK_PIN(pins->input_emergency_stop);
    CHECK_PIN(pins->input_weight_stop);
    CHECK_PIN(pins->input_spare);
    CHECK_PIN(pins->relay_pump);
    CHECK_PIN(pins->relay_brew_solenoid);
    CHECK_PIN(pins->relay_water_led);
    CHECK_PIN(pins->relay_fill_solenoid);
    CHECK_PIN(pins->relay_spare);
    CHECK_PIN(pins->ssr_brew);
    CHECK_PIN(pins->ssr_steam);
    CHECK_PIN(pins->led_status);
    CHECK_PIN(pins->buzzer);
    CHECK_PIN(pins->uart_esp32_tx);
    CHECK_PIN(pins->uart_esp32_rx);
    CHECK_PIN(pins->uart_meter_tx);
    CHECK_PIN(pins->uart_meter_rx);
    CHECK_PIN(pins->i2c_sda);
    CHECK_PIN(pins->i2c_scl);
    CHECK_PIN(pins->input_weight_stop);
    CHECK_PIN(pins->input_spare);
    
    #undef CHECK_PIN
    
    return true;
}

