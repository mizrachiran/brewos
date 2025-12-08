/**
 * ECM Pico Firmware - Sensor Reading
 * 
 * Handles reading of all sensors (temperature, pressure, water level).
 * Uses hardware abstraction layer for real hardware or simulation.
 */

#include "pico/stdlib.h"
#include "sensors.h"
#include "config.h"
#include "hardware.h"
#include "sensor_utils.h"
#include "pcb_config.h"
#include "machine_config.h"
#include "power_meter.h"
#include <stdlib.h>
#include <math.h>

// Machine-type awareness: sensors are only read if they exist for this machine type
// This prevents false fault detection and unnecessary processing

// =============================================================================
// Filter Configuration
// =============================================================================

#define FILTER_SIZE_BREW_NTC    8   // Moving average samples for brew NTC
#define FILTER_SIZE_STEAM_NTC   8   // Moving average samples for steam NTC
#define FILTER_SIZE_PRESSURE    4   // Moving average samples for pressure

// =============================================================================
// Private State
// =============================================================================

static sensor_data_t g_sensor_data = {0};
static bool g_use_hardware = false;  // Use hardware abstraction (sim or real)

// Filter buffers
static float g_filter_buf_brew[FILTER_SIZE_BREW_NTC];
static float g_filter_buf_steam[FILTER_SIZE_STEAM_NTC];
static float g_filter_buf_pressure[FILTER_SIZE_PRESSURE];

// Filter structures
static moving_avg_filter_t g_filter_brew;
static moving_avg_filter_t g_filter_steam;
static moving_avg_filter_t g_filter_pressure;

// Simulation state (fallback if hardware abstraction is in simulation mode)
static float g_sim_brew_temp = 25.0f;
static float g_sim_steam_temp = 25.0f;
static bool g_sim_heating = false;

// Sensor fault tracking
static bool g_brew_ntc_fault = false;
static bool g_steam_ntc_fault = false;
static bool g_pressure_sensor_fault = false;

// Sensor error tracking (consecutive failures)
static uint16_t g_brew_ntc_error_count = 0;
static uint16_t g_steam_ntc_error_count = 0;
static uint16_t g_pressure_error_count = 0;
static uint16_t g_power_meter_error_count = 0;

#define SENSOR_ERROR_THRESHOLD 10  // Report error after 10 consecutive failures

// =============================================================================
// Helper Functions
// =============================================================================

/**
 * Read brew NTC thermistor
 * Returns NAN if sensor doesn't exist for this machine type
 */
static float read_brew_ntc(void) {
    // Machine-type check: HX machines don't have brew NTC
    if (!machine_has_brew_ntc()) {
        return NAN;  // Not present on this machine type
    }
    
    const pcb_config_t* pcb = pcb_config_get();
    if (!pcb || pcb->pins.adc_brew_ntc < 0) {
        return NAN;  // Not configured
    }
    
    // Get ADC channel (GPIO26=0, GPIO27=1, GPIO28=2, GPIO29=3)
    uint8_t adc_channel = pcb->pins.adc_brew_ntc - 26;
    if (adc_channel > 3) {
        return NAN;  // Invalid channel
    }
    
    // Read ADC
    uint16_t adc_value = hw_read_adc(adc_channel);
    
    // Convert to temperature
    float temp_c = ntc_adc_to_temp(
        adc_value,
        HW_ADC_VREF_VOLTAGE,
        NTC_SERIES_R_OHMS,
        NTC_R25_OHMS,
        NTC_B_VALUE
    );
    
    // Validate
    if (!sensor_validate_temp(temp_c, -10.0f, 200.0f)) {
        g_brew_ntc_fault = true;
        g_brew_ntc_error_count++;
        if (g_brew_ntc_error_count >= SENSOR_ERROR_THRESHOLD && 
            g_brew_ntc_error_count == SENSOR_ERROR_THRESHOLD) {
            DEBUG_PRINT("SENSOR ERROR: Brew NTC invalid reading (%.1fC) - %d consecutive failures\n", 
                       temp_c, g_brew_ntc_error_count);
        }
        return NAN;
    }
    
    // Valid reading - reset error count
    if (g_brew_ntc_error_count > 0) {
        DEBUG_PRINT("SENSOR: Brew NTC recovered after %d failures\n", g_brew_ntc_error_count);
    }
    g_brew_ntc_fault = false;
    g_brew_ntc_error_count = 0;
    return temp_c;
}

/**
 * Read steam NTC thermistor
 * Returns NAN if sensor doesn't exist for this machine type
 */
static float read_steam_ntc(void) {
    // Machine-type check: Single boiler machines don't have steam NTC
    if (!machine_has_steam_ntc()) {
        return NAN;  // Not present on this machine type
    }
    
    const pcb_config_t* pcb = pcb_config_get();
    if (!pcb || pcb->pins.adc_steam_ntc < 0) {
        return NAN;  // Not configured
    }
    
    // Get ADC channel
    uint8_t adc_channel = pcb->pins.adc_steam_ntc - 26;
    if (adc_channel > 3) {
        return NAN;  // Invalid channel
    }
    
    // Read ADC
    uint16_t adc_value = hw_read_adc(adc_channel);
    
    // Convert to temperature
    float temp_c = ntc_adc_to_temp(
        adc_value,
        HW_ADC_VREF_VOLTAGE,
        NTC_SERIES_R_OHMS,
        NTC_R25_OHMS,
        NTC_B_VALUE
    );
    
    // Validate
    if (!sensor_validate_temp(temp_c, -10.0f, 200.0f)) {
        g_steam_ntc_fault = true;
        g_steam_ntc_error_count++;
        if (g_steam_ntc_error_count >= SENSOR_ERROR_THRESHOLD && 
            g_steam_ntc_error_count == SENSOR_ERROR_THRESHOLD) {
            DEBUG_PRINT("SENSOR ERROR: Steam NTC invalid reading (%.1fC) - %d consecutive failures\n", 
                       temp_c, g_steam_ntc_error_count);
        }
        return NAN;
    }
    
    // Valid reading - reset error count
    if (g_steam_ntc_error_count > 0) {
        DEBUG_PRINT("SENSOR: Steam NTC recovered after %d failures\n", g_steam_ntc_error_count);
    }
    g_steam_ntc_fault = false;
    g_steam_ntc_error_count = 0;
    return temp_c;
}

/**
 * Read group temperature
 * Returns NAN as group head thermocouple support was removed (v2.24.3)
 */
static float read_group_thermocouple(void) {
    // Group head thermocouple (MAX31855) support removed in v2.24.3
    // Boiler NTC sensors provide sufficient temperature control
    return NAN;
}

/**
 * Read pressure sensor
 */
static float read_pressure(void) {
    const pcb_config_t* pcb = pcb_config_get();
    if (!pcb || pcb->pins.adc_pressure < 0) {
        return 0.0f;  // Not configured, return 0 bar
    }
    
    // Get ADC channel
    uint8_t adc_channel = pcb->pins.adc_pressure - 26;
    if (adc_channel > 3) {
        return 0.0f;  // Invalid channel
    }
    
    // Read ADC voltage
    float voltage = hw_read_adc_voltage(adc_channel);
    
    // Validate voltage range (sanity check)
    // Expected range: 0.3V to 2.7V (after voltage divider: 0.5V to 4.5V transducer)
    // Add some margin: 0.2V to 3.0V
    if (voltage < 0.2f || voltage > 3.0f) {
        g_pressure_sensor_fault = true;
        g_pressure_error_count++;
        if (g_pressure_error_count >= SENSOR_ERROR_THRESHOLD && 
            g_pressure_error_count == SENSOR_ERROR_THRESHOLD) {
            DEBUG_PRINT("SENSOR ERROR: Pressure sensor voltage out of range (%.2fV) - %d consecutive failures\n", 
                       voltage, g_pressure_error_count);
        }
        return 0.0f;  // Return 0 bar on error
    }
    
    // YD4060 pressure transducer:
    // - 0.5V = 0 bar
    // - 4.5V = 16 bar
    // - Voltage divider: 10kΩ / 15kΩ = 0.6 ratio
    // So ADC sees: V_adc = V_transducer * 0.6
    // V_transducer = V_adc / 0.6
    
    float v_transducer = voltage / 0.6f;
    
    // Validate transducer voltage range
    if (v_transducer < 0.3f || v_transducer > 4.7f) {
        g_pressure_sensor_fault = true;
        g_pressure_error_count++;
        if (g_pressure_error_count >= SENSOR_ERROR_THRESHOLD && 
            g_pressure_error_count == SENSOR_ERROR_THRESHOLD) {
            DEBUG_PRINT("SENSOR ERROR: Pressure transducer voltage out of range (%.2fV) - %d consecutive failures\n", 
                       v_transducer, g_pressure_error_count);
        }
        return 0.0f;
    }
    
    // Convert to pressure (0.5V to 4.5V = 0 to 16 bar)
    float pressure_bar = (v_transducer - 0.5f) * 16.0f / 4.0f;
    
    // Clamp to valid range
    if (pressure_bar < 0.0f) pressure_bar = 0.0f;
    if (pressure_bar > 16.0f) pressure_bar = 16.0f;
    
    // Valid reading - reset error count
    if (g_pressure_error_count > 0) {
        DEBUG_PRINT("SENSOR: Pressure sensor recovered after %d failures\n", g_pressure_error_count);
    }
    g_pressure_sensor_fault = false;
    g_pressure_error_count = 0;
    
    return pressure_bar;
}

/**
 * Read water level (digital inputs)
 */
static uint8_t read_water_level(void) {
    const pcb_config_t* pcb = pcb_config_get();
    if (!pcb) {
        return 100;  // Assume full if not configured
    }
    
    // Read level switches
    bool reservoir_ok = true;
    bool tank_level_ok = true;
    bool steam_level_ok = true;
    
    if (pcb->pins.input_reservoir >= 0) {
        reservoir_ok = hw_read_gpio(pcb->pins.input_reservoir);
    }
    
    if (pcb->pins.input_tank_level >= 0) {
        tank_level_ok = hw_read_gpio(pcb->pins.input_tank_level);
    }
    
    if (pcb->pins.input_steam_level >= 0) {
        steam_level_ok = hw_read_gpio(pcb->pins.input_steam_level);
    }
    
    // Simple level calculation (can be improved)
    // For now, return 100% if all OK, 0% if any critical switch is low
    if (!reservoir_ok) {
        return 0;  // Reservoir empty
    }
    
    if (!tank_level_ok) {
        return 20;  // Tank low
    }
    
    if (!steam_level_ok) {
        return 50;  // Steam boiler low
    }
    
    return 100;  // All OK
}

// =============================================================================
// Initialization
// =============================================================================

void sensors_init(void) {
    // Initialize filters
    filter_moving_avg_init(&g_filter_brew, g_filter_buf_brew, FILTER_SIZE_BREW_NTC);
    filter_moving_avg_init(&g_filter_steam, g_filter_buf_steam, FILTER_SIZE_STEAM_NTC);
    filter_moving_avg_init(&g_filter_pressure, g_filter_buf_pressure, FILTER_SIZE_PRESSURE);
    
    // Check if hardware abstraction is available
    g_use_hardware = true;  // Always use hardware abstraction (sim or real)
    
    // Initialize digital inputs for water level
    const pcb_config_t* pcb = pcb_config_get();
    if (pcb) {
        if (pcb->pins.input_reservoir >= 0) {
            hw_gpio_init_input(pcb->pins.input_reservoir, true, false);  // Pull-up
        }
        if (pcb->pins.input_tank_level >= 0) {
            hw_gpio_init_input(pcb->pins.input_tank_level, true, false);
        }
        if (pcb->pins.input_steam_level >= 0) {
            hw_gpio_init_input(pcb->pins.input_steam_level, true, false);
        }
    }
    
    // Initialize sensor data with default values
    g_sensor_data.brew_temp = 250;      // 25.0C
    g_sensor_data.steam_temp = 250;    // 25.0C
    g_sensor_data.group_temp = 250;    // 25.0C
    g_sensor_data.pressure = 0;        // 0.00 bar
    g_sensor_data.water_level = 80;    // 80%
    
    DEBUG_PRINT("Sensors initialized (hardware mode: %s)\n", 
                hw_is_simulation_mode() ? "SIMULATION" : "REAL");
}

// =============================================================================
// Sensor Reading
// =============================================================================

void sensors_read(void) {
    if (g_use_hardware) {
        // Use hardware abstraction layer (works in both sim and real mode)
        
        // Read and filter brew NTC
        float brew_temp_raw = read_brew_ntc();
        if (!isnan(brew_temp_raw)) {
            float brew_temp_filtered = filter_moving_avg_update(&g_filter_brew, brew_temp_raw);
            g_sensor_data.brew_temp = (int16_t)(brew_temp_filtered * 10.0f);
        } else {
            // Sensor fault - keep last valid value (filter maintains it)
            // Safety system will detect NAN and handle appropriately
        }
        
        // Read and filter steam NTC
        float steam_temp_raw = read_steam_ntc();
        if (!isnan(steam_temp_raw)) {
            float steam_temp_filtered = filter_moving_avg_update(&g_filter_steam, steam_temp_raw);
            g_sensor_data.steam_temp = (int16_t)(steam_temp_filtered * 10.0f);
        } else {
            // Sensor fault - keep last valid value
        }
        
        // Note: Group head thermocouple support removed (v2.24.3)
        // g_sensor_data.group_temp remains at default/last value (NAN data)
        
        // Read and filter pressure
        float pressure_raw = read_pressure();
        if (!g_pressure_sensor_fault) {
            // Only update filter if sensor is not in fault state
            float pressure_filtered = filter_moving_avg_update(&g_filter_pressure, pressure_raw);
            g_sensor_data.pressure = (uint16_t)(pressure_filtered * 100.0f);  // Convert to 0.01 bar units
        } else {
            // Sensor fault - keep last valid value (filter maintains it)
        }
        
        // Read water level
        g_sensor_data.water_level = read_water_level();
        
        // Read power meter (PZEM, JSY, Eastron, etc.)
        // power_meter_update() is non-blocking (uses timeout)
        // Power data doesn't need high frequency, but reading every cycle is fine
        if (power_meter_is_connected()) {
            power_meter_update();
            
            // Check for persistent errors
            const char* error = power_meter_get_error();
            if (error) {
                g_power_meter_error_count++;
                if (g_power_meter_error_count % 50 == 0) {
                    // Report every 50 errors to avoid spam
                    DEBUG_PRINT("SENSOR ERROR: Power meter: %s (count: %d)\n", error, g_power_meter_error_count);
                }
            } else {
                g_power_meter_error_count = 0;  // Reset on success
            }
        }
        
    } else {
        // Fallback: Old simulation mode (should not be used if hardware abstraction is available)
        // This is kept for backward compatibility
        
        // Target temperatures based on whether heating is "on"
        float brew_target = g_sim_heating ? 93.0f : 25.0f;
        float steam_target = g_sim_heating ? 140.0f : 25.0f;
        
        // Slowly approach target (thermal mass simulation)
        float rate = 0.1f;  // Degrees per 50ms read cycle
        
        if (g_sim_brew_temp < brew_target) {
            g_sim_brew_temp += rate;
            if (g_sim_brew_temp > brew_target) g_sim_brew_temp = brew_target;
        } else if (g_sim_brew_temp > brew_target) {
            g_sim_brew_temp -= rate * 0.3f;  // Cool slower
            if (g_sim_brew_temp < brew_target) g_sim_brew_temp = brew_target;
        }
        
        if (g_sim_steam_temp < steam_target) {
            g_sim_steam_temp += rate * 0.8f;
            if (g_sim_steam_temp > steam_target) g_sim_steam_temp = steam_target;
        } else if (g_sim_steam_temp > steam_target) {
            g_sim_steam_temp -= rate * 0.2f;
            if (g_sim_steam_temp < steam_target) g_sim_steam_temp = steam_target;
        }
        
        // Add small noise for realism
        float noise = ((float)(rand() % 10) - 5.0f) / 50.0f;
        
        g_sensor_data.brew_temp = (int16_t)((g_sim_brew_temp + noise) * 10);
        g_sensor_data.steam_temp = (int16_t)((g_sim_steam_temp + noise) * 10);
        g_sensor_data.group_temp = (int16_t)((g_sim_brew_temp - 5.0f + noise) * 10);
        g_sensor_data.pressure = 100 + (rand() % 20);  // ~1.0 bar
    }
}

void sensors_get_data(sensor_data_t* data) {
    if (data) {
        *data = g_sensor_data;
    }
}

// =============================================================================
// Individual Sensor Access
// =============================================================================

int16_t sensors_get_brew_temp(void) {
    return g_sensor_data.brew_temp;
}

int16_t sensors_get_steam_temp(void) {
    return g_sensor_data.steam_temp;
}

int16_t sensors_get_group_temp(void) {
    return g_sensor_data.group_temp;
}

uint16_t sensors_get_pressure(void) {
    return g_sensor_data.pressure;
}

uint8_t sensors_get_water_level(void) {
    return g_sensor_data.water_level;
}

// =============================================================================
// Simulation Control (for development)
// =============================================================================

void sensors_set_simulation(bool enable) {
    // This now controls the hardware abstraction layer's simulation mode
    hw_set_simulation_mode(enable);
    DEBUG_PRINT("Sensor simulation: %s\n", enable ? "enabled" : "disabled");
}

void sensors_sim_set_heating(bool heating) {
    g_sim_heating = heating;
    
    // If in simulation mode, set simulated ADC values to simulate heating
    if (hw_is_simulation_mode()) {
        // Calculate ADC values for target temperatures
        // This is approximate - in real use, the hardware abstraction layer
        // would handle simulation values
        
        // For brew: target 93°C when heating
        // For steam: target 140°C when heating
        // These would be set via hw_sim_set_adc() if needed
    }
}
