/**
 * ECM Pico Firmware - GPIO Initialization Implementation
 * 
 * Initializes all GPIO pins according to the active PCB configuration.
 */

#include "gpio_init.h"
#include "pcb_config.h"
#include "config.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"

// =============================================================================
// UART Initialization
// =============================================================================

void gpio_init_uart_esp32(void) {
    const pcb_config_t* pcb = pcb_config_get();
    if (!pcb) return;
    
    const pcb_pin_config_t* pins = &pcb->pins;
    
    if (PIN_VALID(pins->uart_esp32_tx)) {
        gpio_set_function(pins->uart_esp32_tx, GPIO_FUNC_UART);
    }
    if (PIN_VALID(pins->uart_esp32_rx)) {
        gpio_set_function(pins->uart_esp32_rx, GPIO_FUNC_UART);
    }
}

// =============================================================================
// ADC Initialization
// =============================================================================

void gpio_init_adc(void) {
    const pcb_config_t* pcb = pcb_config_get();
    if (!pcb) return;
    
    const pcb_pin_config_t* pins = &pcb->pins;
    
    // Initialize ADC hardware
    adc_init();
    
    // Configure ADC pins
    if (PIN_VALID(pins->adc_brew_ntc)) {
        adc_gpio_init(pins->adc_brew_ntc);
    }
    if (PIN_VALID(pins->adc_steam_ntc)) {
        adc_gpio_init(pins->adc_steam_ntc);
    }
    if (PIN_VALID(pins->adc_pressure)) {
        adc_gpio_init(pins->adc_pressure);
    }
    if (PIN_VALID(pins->adc_flow)) {
        adc_gpio_init(pins->adc_flow);
    }
    if (PIN_VALID(pins->adc_inlet_temp)) {
        adc_gpio_init(pins->adc_inlet_temp);
    }
}

// =============================================================================
// SPI Initialization
// =============================================================================

void gpio_init_spi(void) {
    const pcb_config_t* pcb = pcb_config_get();
    if (!pcb) return;
    
    const pcb_pin_config_t* pins = &pcb->pins;
    
    // Configure SPI pins (SPI0 by default)
    if (PIN_VALID(pins->spi_sck)) {
        gpio_set_function(pins->spi_sck, GPIO_FUNC_SPI);
    }
    if (PIN_VALID(pins->spi_mosi)) {
        gpio_set_function(pins->spi_mosi, GPIO_FUNC_SPI);
    }
    if (PIN_VALID(pins->spi_miso)) {
        gpio_set_function(pins->spi_miso, GPIO_FUNC_SPI);
    }
    if (PIN_VALID(pins->spi_cs)) {
        gpio_init(pins->spi_cs);
        gpio_set_dir(pins->spi_cs, GPIO_OUT);
        gpio_put(pins->spi_cs, 1);  // CS high (inactive)
    }
}

// =============================================================================
// I2C Initialization
// =============================================================================

void gpio_init_i2c(void) {
    const pcb_config_t* pcb = pcb_config_get();
    if (!pcb) return;
    
    const pcb_pin_config_t* pins = &pcb->pins;
    
    // Configure I2C pins (I2C0 by default)
    if (PIN_VALID(pins->i2c_sda)) {
        gpio_set_function(pins->i2c_sda, GPIO_FUNC_I2C);
        gpio_pull_up(pins->i2c_sda);
    }
    if (PIN_VALID(pins->i2c_scl)) {
        gpio_set_function(pins->i2c_scl, GPIO_FUNC_I2C);
        gpio_pull_up(pins->i2c_scl);
    }
}

// =============================================================================
// Digital Input Initialization
// =============================================================================

void gpio_init_inputs(void) {
    const pcb_config_t* pcb = pcb_config_get();
    if (!pcb) return;
    
    const pcb_pin_config_t* pins = &pcb->pins;
    
    // Configure all digital input pins with pull-up (switches typically connect to GND)
    #define INIT_INPUT_PULLUP(pin) \
        do { \
            if (PIN_VALID(pin)) { \
                gpio_init(pin); \
                gpio_set_dir(pin, GPIO_IN); \
                gpio_pull_up(pin); \
            } \
        } while (0)
    
    #define INIT_INPUT_PULLDOWN(pin) \
        do { \
            if (PIN_VALID(pin)) { \
                gpio_init(pin); \
                gpio_set_dir(pin, GPIO_IN); \
                gpio_pull_down(pin); \
            } \
        } while (0)
    
    // Inputs with pull-up (switches typically connect to GND)
    INIT_INPUT_PULLUP(pins->input_reservoir);
    INIT_INPUT_PULLUP(pins->input_tank_level);
    // GPIO4 (steam level) - no pull-up, driven by TLV3201 comparator
    if (PIN_VALID(pins->input_steam_level)) {
        gpio_init(pins->input_steam_level);
        gpio_set_dir(pins->input_steam_level, GPIO_IN);
        // No pull-up/pull-down - TLV3201 provides the signal
    }
    INIT_INPUT_PULLUP(pins->input_brew_switch);
    INIT_INPUT_PULLUP(pins->input_steam_switch);
    // Water mode switch: pull-down (LOW=water tank, HIGH=plumbed)
    if (PIN_VALID(pins->input_water_mode)) {
        gpio_init(pins->input_water_mode);
        gpio_set_dir(pins->input_water_mode, GPIO_IN);
        gpio_pull_down(pins->input_water_mode);  // Pull-down: HIGH = plumbed mode
    }
    INIT_INPUT_PULLUP(pins->input_flow_pulse);
    INIT_INPUT_PULLUP(pins->input_emergency_stop);
    
    // ESP32 signals (from J15)
    // GPIO21 (WEIGHT_STOP) - pull-down (normally LOW, ESP32 sets HIGH)
    if (PIN_VALID(pins->input_weight_stop)) {
        gpio_init(pins->input_weight_stop);
        gpio_set_dir(pins->input_weight_stop, GPIO_IN);
        gpio_pull_down(pins->input_weight_stop);
    }
    // GPIO22 (SPARE) - no pull-up/down by default (reserved for future)
    if (PIN_VALID(pins->input_spare)) {
        gpio_init(pins->input_spare);
        gpio_set_dir(pins->input_spare, GPIO_IN);
        // No pull-up/pull-down - reserved for future use
    }
    
    #undef INIT_INPUT_PULLUP
    #undef INIT_INPUT_PULLDOWN
}

// =============================================================================
// Output Initialization
// =============================================================================

void gpio_init_outputs(void) {
    const pcb_config_t* pcb = pcb_config_get();
    if (!pcb) return;
    
    const pcb_pin_config_t* pins = &pcb->pins;
    
    // Configure relay outputs
    #define INIT_OUTPUT(pin, initial) \
        do { \
            if (PIN_VALID(pin)) { \
                gpio_init(pin); \
                gpio_set_dir(pin, GPIO_OUT); \
                gpio_put(pin, initial); \
            } \
        } while (0)
    
    // Relays: OFF initially (active low or high depending on relay driver)
    INIT_OUTPUT(pins->relay_pump, 0);
    INIT_OUTPUT(pins->relay_brew_solenoid, 0);
    INIT_OUTPUT(pins->relay_water_led, 0);
    INIT_OUTPUT(pins->relay_fill_solenoid, 0);
    INIT_OUTPUT(pins->relay_spare, 0);
    
    // User interface
    INIT_OUTPUT(pins->led_status, 0);
    INIT_OUTPUT(pins->buzzer, 0);
    
    // Note: ESP32 control pins (RUN and BOOTSEL) are hardware pins, not GPIO
    // They are controlled by ESP32 via J15 pins 5 and 6, not initialized here
    
    #undef INIT_OUTPUT
}

// =============================================================================
// PWM Initialization (for SSR control)
// =============================================================================

void gpio_init_pwm(void) {
    const pcb_config_t* pcb = pcb_config_get();
    if (!pcb) return;
    
    const pcb_pin_config_t* pins = &pcb->pins;
    
    // Configure SSR pins for PWM
    if (PIN_VALID(pins->ssr_brew)) {
        gpio_set_function(pins->ssr_brew, GPIO_FUNC_PWM);
        // PWM slice and channel will be determined by the pin
        // Actual PWM setup happens in control module
    }
    if (PIN_VALID(pins->ssr_steam)) {
        gpio_set_function(pins->ssr_steam, GPIO_FUNC_PWM);
    }
}

// =============================================================================
// Complete GPIO Initialization
// =============================================================================

bool gpio_init_all(void) {
    // Validate PCB configuration first
    if (!pcb_validate_pins()) {
        return false;
    }
    
    // Initialize all GPIO subsystems
    gpio_init_uart_esp32();
    gpio_init_adc();
    gpio_init_spi();
    gpio_init_i2c();
    gpio_init_inputs();
    gpio_init_outputs();
    gpio_init_pwm();
    
    return true;
}

