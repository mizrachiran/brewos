/**
 * ECM Pico Firmware - Hardware Abstraction Layer Implementation
 * 
 * Provides hardware access with simulation mode support.
 * 
 * Simulation Mode:
 *   - Returns simulated values for development without hardware
 *   - Can be toggled at runtime or compile time
 *   - Useful for testing control logic
 */

#include "hardware.h"
#include "config.h"
#include "pcb_config.h"
#include <math.h>  // For isnan(), isinf()

// Pico SDK includes
#include "hardware/adc.h"
#include "hardware/spi.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

// =============================================================================
// Private State
// =============================================================================

static bool g_simulation_mode = HW_SIMULATION_MODE;
static bool g_initialized = false;

// Simulation state
static uint16_t g_sim_adc[HW_ADC_CHANNEL_COUNT] = {0};
static uint32_t g_sim_max31855 = 0;
static bool g_sim_gpio[32] = {0};  // 32 GPIO pins max

// Hardware state
static uint8_t g_pwm_slice_brew = 0xFF;  // Invalid slice number
static uint8_t g_pwm_slice_steam = 0xFF;
static bool g_pwm_initialized = false;

// PWM channel tracking - maps slice to the channel that was initialized
// This ensures we set the correct channel when using the legacy interface
#define MAX_PWM_SLICES 8
static uint8_t g_pwm_slice_channel[MAX_PWM_SLICES] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t g_pwm_slice_gpio[MAX_PWM_SLICES] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// =============================================================================
// Initialization
// =============================================================================

bool hw_init(void) {
    if (g_initialized) {
        return true;  // Already initialized
    }
    
    if (g_simulation_mode) {
        DEBUG_PRINT("Hardware: Simulation mode enabled\n");
        
        // Initialize simulation state
        for (int i = 0; i < HW_ADC_CHANNEL_COUNT; i++) {
            g_sim_adc[i] = HW_ADC_MAX_VALUE / 2;  // Mid-scale default
        }
        
        // Default MAX31855: 25.0C (room temperature)
        // Format: bit 31=sign, bits 30-18=temp (14-bit), bits 17-4=temp*4, bits 3-0=fault
        g_sim_max31855 = (25 << 18) & 0xFFFC0000;  // 25C in MAX31855 format
        
        g_initialized = true;
        return true;
    }
    
    // Initialize real hardware
    DEBUG_PRINT("Hardware: Initializing real hardware\n");
    
    // Initialize ADC
    adc_init();
    adc_set_temp_sensor_enabled(false);  // We don't use internal temp sensor
    
    // Note: Individual ADC channels are configured when first read
    
    // SPI and PWM will be initialized on first use
    
    g_initialized = true;
    DEBUG_PRINT("Hardware: Initialization complete\n");
    return true;
}

void hw_set_simulation_mode(bool enable) {
    g_simulation_mode = enable;
    DEBUG_PRINT("Hardware: Simulation mode %s\n", enable ? "enabled" : "disabled");
}

bool hw_is_simulation_mode(void) {
    return g_simulation_mode;
}

// =============================================================================
// ADC Implementation
// =============================================================================

uint16_t hw_read_adc(uint8_t channel) {
    if (!g_initialized) {
        hw_init();
    }
    
    if (channel >= HW_ADC_CHANNEL_COUNT) {
        DEBUG_PRINT("Hardware: Invalid ADC channel %d\n", channel);
        return 0;
    }
    
    if (g_simulation_mode) {
        return g_sim_adc[channel];
    }
    
    // Real hardware: Read ADC
    // ADC channels map to GPIO: 26=0, 27=1, 28=2, 29=3
    uint8_t gpio_pin = 26 + channel;
    
    // Select ADC input
    adc_gpio_init(gpio_pin);
    adc_select_input(channel);
    
    // Read ADC (12-bit, 0-4095)
    uint16_t value = adc_read();
    
    return value;
}

float hw_adc_to_voltage(uint16_t adc_value) {
    if (adc_value > HW_ADC_MAX_VALUE) {
        adc_value = HW_ADC_MAX_VALUE;
    }
    return (float)adc_value * HW_ADC_VREF_VOLTAGE / (float)HW_ADC_MAX_VALUE;
}

float hw_read_adc_voltage(uint8_t channel) {
    uint16_t adc_value = hw_read_adc(channel);
    return hw_adc_to_voltage(adc_value);
}

// =============================================================================
// SPI Implementation (MAX31855)
// =============================================================================

bool hw_spi_init_max31855(void) {
    // MAX31855 thermocouple support removed (v2.24.3)
    // This function kept for API compatibility but always returns false
    return false;
}

bool hw_spi_read_max31855(uint32_t* data) {
    // MAX31855 thermocouple support removed (v2.24.3)
    (void)data;
    return false;
}

bool hw_max31855_to_temp(uint32_t data, float* temp_c) {
    if (!temp_c) {
        return false;
    }
    
    // Check for fault
    if (data & 0x7) {  // Bits 0-2 indicate faults
        return false;
    }
    
    // Extract temperature (14-bit signed, bits 18-31)
    int16_t temp_raw = (int16_t)((data >> 18) & 0x3FFF);
    
    // Handle sign extension (bit 31 is sign)
    if (data & 0x80000000) {
        temp_raw |= 0xC000;  // Sign extend for negative values
    }
    
    // Convert to Celsius (LSB = 0.25C)
    *temp_c = (float)temp_raw * 0.25f;
    
    return true;
}

bool hw_max31855_is_fault(uint32_t data) {
    return (data & 0x7) != 0;  // Any fault bit set
}

uint8_t hw_max31855_get_fault(uint32_t data) {
    if (!hw_max31855_is_fault(data)) {
        return 0;  // No fault
    }
    
    // Fault codes: bit 0=OC, bit 1=SCG, bit 2=SCV
    if (data & 0x01) return 1;  // Open circuit
    if (data & 0x02) return 2;  // Short to GND
    if (data & 0x04) return 3;  // Short to VCC
    
    return 0;  // Should not reach here
}

// =============================================================================
// PWM Implementation
// =============================================================================

bool hw_pwm_init_ssr(uint8_t gpio_pin, uint8_t* slice_num) {
    if (!slice_num) {
        return false;
    }
    
    if (g_simulation_mode) {
        // In simulation, just return a fake slice number
        *slice_num = gpio_pin % MAX_PWM_SLICES;  // Use GPIO modulo as slice number for simulation
        // Track channel even in simulation for consistency
        if (*slice_num < MAX_PWM_SLICES) {
            g_pwm_slice_channel[*slice_num] = gpio_pin & 1;  // Simulate channel based on GPIO
            g_pwm_slice_gpio[*slice_num] = gpio_pin;
        }
        return true;
    }
    
    // Real hardware: Initialize PWM
    // Each GPIO has a PWM slice assigned
    *slice_num = pwm_gpio_to_slice_num(gpio_pin);
    uint8_t channel = pwm_gpio_to_channel(gpio_pin);
    
    // Track channel for this slice (used by hw_set_pwm_duty)
    if (*slice_num < MAX_PWM_SLICES) {
        g_pwm_slice_channel[*slice_num] = channel;
        g_pwm_slice_gpio[*slice_num] = gpio_pin;
    }
    
    // Set GPIO function to PWM
    gpio_set_function(gpio_pin, GPIO_FUNC_PWM);
    
    // Configure PWM
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 125.0f);  // 125MHz / 125 = 1MHz
    pwm_config_set_wrap(&config, 40000);     // 1MHz / 40000 = 25Hz
    pwm_config_set_phase_correct(&config, false);
    
    // Initialize PWM slice
    pwm_init(*slice_num, &config, false);
    
    // Set initial duty to 0 (only for the correct channel)
    pwm_set_chan_level(*slice_num, channel, 0);
    
    // Enable PWM
    pwm_set_enabled(*slice_num, true);
    
    DEBUG_PRINT("PWM: GPIO%d initialized on slice %d channel %c\n", 
                gpio_pin, *slice_num, channel == 0 ? 'A' : 'B');
    
    return true;
}

bool hw_pwm_init_ssr_ex(uint8_t gpio_pin, pwm_ssr_config_t* config) {
    if (!config) {
        return false;
    }
    
    config->gpio_pin = gpio_pin;
    config->initialized = false;
    
    if (g_simulation_mode) {
        config->slice = gpio_pin % MAX_PWM_SLICES;
        config->channel = gpio_pin & 1;  // Simulate channel based on GPIO
        config->initialized = true;
        return true;
    }
    
    // Real hardware: Initialize PWM
    config->slice = pwm_gpio_to_slice_num(gpio_pin);
    config->channel = pwm_gpio_to_channel(gpio_pin);
    
    // Also update the tracking arrays for legacy interface compatibility
    if (config->slice < MAX_PWM_SLICES) {
        g_pwm_slice_channel[config->slice] = config->channel;
        g_pwm_slice_gpio[config->slice] = gpio_pin;
    }
    
    // Set GPIO function to PWM
    gpio_set_function(gpio_pin, GPIO_FUNC_PWM);
    
    // Configure PWM
    pwm_config pwm_cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&pwm_cfg, 125.0f);  // 125MHz / 125 = 1MHz
    pwm_config_set_wrap(&pwm_cfg, 40000);     // 1MHz / 40000 = 25Hz
    pwm_config_set_phase_correct(&pwm_cfg, false);
    
    // Initialize PWM slice
    pwm_init(config->slice, &pwm_cfg, false);
    
    // Set initial duty to 0 (only for the correct channel)
    pwm_set_chan_level(config->slice, config->channel, 0);
    
    // Enable PWM
    pwm_set_enabled(config->slice, true);
    
    config->initialized = true;
    
    DEBUG_PRINT("PWM: GPIO%d initialized on slice %d channel %c (ex)\n", 
                gpio_pin, config->slice, config->channel == 0 ? 'A' : 'B');
    
    return true;
}

void hw_set_pwm_duty(uint8_t slice_num, float duty_percent) {
    // Clamp to valid range
    if (duty_percent < 0.0f) duty_percent = 0.0f;
    if (duty_percent > 100.0f) duty_percent = 100.0f;
    
    // Guard against NaN (undefined behavior if passed to calculations)
    if (isnan(duty_percent)) {
        duty_percent = 0.0f;
    }
    
    // Minimum duty cycle for Zero-Crossing SSRs
    // ZC-SSRs need at least one half-cycle (8.3ms at 60Hz, 10ms at 50Hz) to fire.
    // At 25Hz PWM (40ms period), SSR_MIN_DUTY_PERCENT% = min reliable pulse.
    // Below this threshold, force to 0% to prevent erratic firing.
    if (duty_percent > 0.0f && duty_percent < SSR_MIN_DUTY_PERCENT) {
        duty_percent = 0.0f;
    }
    
    if (g_simulation_mode) {
        // In simulation, just store the value (could be used for testing)
        return;
    }
    
    // Convert percentage to PWM level (0-40000 for 25Hz)
    uint16_t level = (uint16_t)((duty_percent / 100.0f) * 40000.0f);
    
    // Use tracked channel for this slice (defaults to channel A if not tracked)
    uint8_t channel = PWM_CHAN_A;
    if (slice_num < MAX_PWM_SLICES && g_pwm_slice_channel[slice_num] != 0xFF) {
        channel = g_pwm_slice_channel[slice_num];
    }
    
    // Set only the correct channel to avoid affecting other SSRs sharing this slice
    pwm_set_chan_level(slice_num, channel, level);
}

void hw_set_pwm_duty_ex(const pwm_ssr_config_t* config, float duty_percent) {
    if (!config || !config->initialized) {
        return;
    }
    
    // Clamp to valid range
    if (duty_percent < 0.0f) duty_percent = 0.0f;
    if (duty_percent > 100.0f) duty_percent = 100.0f;
    
    // Guard against NaN
    if (isnan(duty_percent)) {
        duty_percent = 0.0f;
    }
    
    // Minimum duty cycle for Zero-Crossing SSRs
    if (duty_percent > 0.0f && duty_percent < SSR_MIN_DUTY_PERCENT) {
        duty_percent = 0.0f;
    }
    
    if (g_simulation_mode) {
        // In simulation, just store the value (could be used for testing)
        return;
    }
    
    // Convert percentage to PWM level (0-40000 for 25Hz)
    uint16_t level = (uint16_t)((duty_percent / 100.0f) * 40000.0f);
    
    // Set only the correct channel
    pwm_set_chan_level(config->slice, config->channel, level);
}

float hw_get_pwm_duty(uint8_t slice_num) {
    if (g_simulation_mode) {
        return 0.0f;  // Not tracked in simulation
    }
    
    // Real hardware: Read current duty cycle
    // Note: Pico SDK doesn't provide a direct way to read PWM level
    // This would need to track the level ourselves, or use pwm_get_counter()
    // For now, return 0.0f as we don't track the level
    // TODO: Track PWM level when setting it
    (void)slice_num;  // Unused for now
    return 0.0f;
}

void hw_pwm_set_enabled(uint8_t slice_num, bool enable) {
    if (g_simulation_mode) {
        return;  // Nothing to do in simulation
    }
    
    pwm_set_enabled(slice_num, enable);
}

// =============================================================================
// GPIO Implementation
// =============================================================================

bool hw_gpio_init_output(uint8_t pin, bool initial_state) {
    if (g_simulation_mode) {
        g_sim_gpio[pin] = initial_state;
        return true;
    }
    
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, initial_state);
    return true;
}

bool hw_gpio_init_input(uint8_t pin, bool pull_up, bool pull_down) {
    if (g_simulation_mode) {
        return true;
    }
    
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    
    if (pull_up) {
        gpio_pull_up(pin);
    } else if (pull_down) {
        gpio_pull_down(pin);
    }
    
    return true;
}

void hw_set_gpio(uint8_t pin, bool state) {
    if (g_simulation_mode) {
        g_sim_gpio[pin] = state;
        return;
    }
    
    gpio_put(pin, state);
}

bool hw_read_gpio(uint8_t pin) {
    if (g_simulation_mode) {
        return g_sim_gpio[pin];
    }
    
    return gpio_get(pin);
}

void hw_toggle_gpio(uint8_t pin) {
    if (g_simulation_mode) {
        g_sim_gpio[pin] = !g_sim_gpio[pin];
        return;
    }
    
    gpio_put(pin, !gpio_get(pin));
}

// =============================================================================
// Simulation Helpers
// =============================================================================

void hw_sim_set_adc(uint8_t channel, uint16_t value) {
    if (channel < HW_ADC_CHANNEL_COUNT) {
        g_sim_adc[channel] = value;
    }
}

void hw_sim_set_max31855(uint32_t data) {
    g_sim_max31855 = data;
}

void hw_sim_set_gpio(uint8_t pin, bool state) {
    if (pin < 32) {
        g_sim_gpio[pin] = state;
    }
}

