/**
 * ECM Pico Firmware - Heat Exchanger Control Implementation
 * 
 * Control logic for heat exchanger machines (e.g., E61 HX):
 * - Steam boiler only has active PID control
 * - Brew water temperature is passive (heated via heat exchanger)
 * - Group thermocouple monitors brew water temperature
 * 
 * Supports three control modes:
 * - HX_CONTROL_TEMPERATURE: PID based on steam NTC (modern retrofit)
 * - HX_CONTROL_PRESSURE: PID based on pressure transducer
 * - HX_CONTROL_PRESSURESTAT: External pressurestat controls heater (monitor only)
 */

#include "pico/stdlib.h"
#include "control_impl.h"
#include "config.h"
#include "machine_config.h"
#include "sensors.h"
#include <math.h>

// =============================================================================
// Private State
// =============================================================================

static hx_control_mode_t g_hx_control_mode = HX_CONTROL_TEMPERATURE;
static float g_pressure_setpoint_bar = 1.0f;
static float g_pressure_hysteresis_bar = 0.1f;

// Separate PID gains for pressure control (tuned for pressure process variable)
// Pressure PV is typically 0.8-1.5 bar, much smaller than temperature (80-150°C)
// These gains are scaled appropriately for the smaller error range
static pid_state_t g_pressure_pid;
static bool g_pressure_pid_initialized = false;

// PID scaling factors for pressure mode
// Multiply pressure error by this to get equivalent "temperature-like" error
// This allows using similar PID response characteristics
#define PRESSURE_TO_TEMP_SCALE  100.0f  // 0.1 bar error -> 10°C equivalent

// =============================================================================
// Heat Exchanger Initialization
// =============================================================================

void control_init_machine(void) {
    const heat_exchanger_config_t* hx_config = machine_get_hx_config();
    
    // HX: Only steam boiler has active control (unless pressurestat mode)
    g_brew_pid.setpoint = 0;  // Not used (passive HX)
    
    // HX uses single heater (steam boiler)
    g_heating_strategy = HEAT_BREW_ONLY;  // Only one SSR active
    
    if (hx_config) {
        g_hx_control_mode = hx_config->control_mode;
        g_pressure_setpoint_bar = hx_config->pressure_setpoint_bar;
        g_pressure_hysteresis_bar = hx_config->pressure_hysteresis_bar;
        
        switch (g_hx_control_mode) {
            case HX_CONTROL_TEMPERATURE:
                g_steam_pid.setpoint = hx_config->steam_setpoint;
                g_steam_pid.setpoint_target = g_steam_pid.setpoint;
                LOG_PRINT("Control: HX mode - TEMPERATURE PID\n");
                DEBUG_PRINT("  Steam setpoint: %.1fC\n", g_steam_pid.setpoint);
                break;
                
            case HX_CONTROL_PRESSURE:
                // Initialize pressure-specific PID with scaled gains
                // Pressure control uses error scaling to work with temperature-tuned PID
                if (!g_pressure_pid_initialized) {
                    pid_init(&g_pressure_pid, g_pressure_setpoint_bar * PRESSURE_TO_TEMP_SCALE);
                    // Adjust gains for pressure - typically needs higher Kp due to faster dynamics
                    g_pressure_pid.kp = PID_DEFAULT_KP * 1.5f;
                    g_pressure_pid.ki = PID_DEFAULT_KI * 0.5f;  // Less integral (faster response)
                    g_pressure_pid.kd = PID_DEFAULT_KD * 2.0f;  // More derivative (damping)
                    g_pressure_pid_initialized = true;
                }
                g_pressure_pid.setpoint = g_pressure_setpoint_bar * PRESSURE_TO_TEMP_SCALE;
                g_pressure_pid.setpoint_target = g_pressure_pid.setpoint;
                LOG_PRINT("Control: HX mode - PRESSURE PID\n");
                DEBUG_PRINT("  Pressure setpoint: %.2f bar (scaled SP: %.1f)\n", 
                           g_pressure_setpoint_bar, g_pressure_pid.setpoint);
                break;
                
            case HX_CONTROL_PRESSURESTAT:
                // No PID control - pressurestat handles it
                g_steam_pid.setpoint = 0;
                LOG_PRINT("Control: HX mode - PRESSURESTAT (monitor only)\n");
                DEBUG_PRINT("  Heater controlled by external pressurestat\n");
                break;
        }
        
        DEBUG_PRINT("  Control mode: %d\n", hx_config->control_mode);
        DEBUG_PRINT("  Steam setpoint: %.1fC\n", hx_config->steam_setpoint);
        DEBUG_PRINT("  Target group temp: %.1fC\n", hx_config->group_setpoint);
        DEBUG_PRINT("  Ready state config: %d\n", hx_config->ready_state_config);
    } else {
        // Default to temperature control
        g_hx_control_mode = HX_CONTROL_TEMPERATURE;
        g_steam_pid.setpoint = TEMP_DECI_TO_C(DEFAULT_STEAM_TEMP);
        g_steam_pid.setpoint_target = g_steam_pid.setpoint;
        DEBUG_PRINT("Control: HX mode - TEMPERATURE PID (default)\n");
    }
}

// =============================================================================
// Heat Exchanger Control Update
// =============================================================================

void control_update_machine(
    machine_mode_t mode,
    float brew_temp, float steam_temp, float group_temp,
    float dt,
    float* brew_duty, float* steam_duty
) {
    (void)mode;       // HX doesn't change behavior based on mode
    (void)brew_temp;  // HX has no brew boiler NTC
    
    // HX uses steam SSR only (no brew heater)
    *brew_duty = 0.0f;
    
    float demand = 0.0f;
    
    switch (g_hx_control_mode) {
        case HX_CONTROL_TEMPERATURE:
            // PID based on steam boiler temperature
            if (!isnan(steam_temp) && !isinf(steam_temp)) {
                demand = pid_compute(&g_steam_pid, steam_temp, dt);
            }
            break;
            
        case HX_CONTROL_PRESSURE: {
            // PID based on pressure transducer with error scaling
            // We scale the pressure to a "temperature-like" range so PID gains
            // tuned for temperature work reasonably for pressure control.
            sensor_data_t sensors;
            sensors_get_data(&sensors);
            float pressure_bar = sensors.pressure / 100.0f;  // Convert from 0.01 bar units
            
            if (pressure_bar >= 0.0f && pressure_bar <= 16.0f) {
                // Scale pressure to temperature-equivalent range for PID
                float scaled_pressure = pressure_bar * PRESSURE_TO_TEMP_SCALE;
                demand = pid_compute(&g_pressure_pid, scaled_pressure, dt);
            }
            break;
        }
            
        case HX_CONTROL_PRESSURESTAT:
            // External pressurestat controls the heater
            // We don't output any demand - the pressurestat does it
            // This mode is for monitoring only
            demand = 0.0f;
            
            // Note: In pressurestat mode, the heater is wired through
            // the pressurestat, NOT through our SSR. Our SSR should be
            // bypassed or not connected to the heater.
            break;
    }
    
    *steam_duty = demand;
    
    // Future enhancement: Cascade control using group_temp
    // Could adjust steam_setpoint based on group_temp deviation
    // from target to improve brew water temperature stability
    (void)group_temp;
}

// =============================================================================
// Heat Exchanger Mode Functions (not applicable)
// =============================================================================

uint8_t control_get_machine_mode(void) {
    return 0;  // HX doesn't have mode switching
}

bool control_is_machine_switching(void) {
    return false;  // HX doesn't switch modes
}

