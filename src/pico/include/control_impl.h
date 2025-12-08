/**
 * ECM Pico Firmware - Control System Internal Interface
 * 
 * Internal header for machine-specific control implementations.
 * Each machine type implements these functions in its own file:
 *   - control_dual_boiler.c
 *   - control_single_boiler.c
 *   - control_heat_exchanger.c
 * 
 * The control_common.c file calls these machine-specific functions.
 */

#ifndef CONTROL_IMPL_H
#define CONTROL_IMPL_H

#include <stdint.h>
#include <stdbool.h>
#include "state.h"

// =============================================================================
// Heating Strategy Types (shared across all machine types)
// =============================================================================

typedef enum {
    HEAT_BREW_ONLY = 0,        // Only brew boiler heats
    HEAT_SEQUENTIAL = 1,       // Brew first, steam after threshold
    HEAT_PARALLEL = 2,         // Both heat simultaneously
    HEAT_SMART_STAGGER = 3     // Both with limited combined duty
} heating_strategy_t;

// =============================================================================
// PID State (shared structure)
// =============================================================================

typedef struct {
    float kp, ki, kd;
    float setpoint;
    float setpoint_target;      // Target for ramping
    float integral;
    float last_error;
    float last_measurement;     // For derivative-on-measurement (avoids setpoint kick)
    float last_derivative;      // For derivative filtering
    float output;
    bool setpoint_ramping;      // Enable setpoint ramping
    float ramp_rate;            // Degrees per second
    bool first_run;             // True on first call (skips derivative to avoid spike)
} pid_state_t;

// =============================================================================
// Global PID State (defined in control_common.c, used by machine-specific code)
// =============================================================================

extern pid_state_t g_brew_pid;
extern pid_state_t g_steam_pid;
extern heating_strategy_t g_heating_strategy;

// =============================================================================
// Shared Helper Functions (implemented in control_common.c)
// =============================================================================

/**
 * Initialize a PID state structure with default values
 */
void pid_init(pid_state_t* pid, float setpoint);

/**
 * Compute PID output with derivative filtering and setpoint ramping
 */
float pid_compute(pid_state_t* pid, float process_value, float dt);

/**
 * Apply heating strategy to get final duty cycles (dual boiler only)
 */
void apply_heating_strategy(
    float brew_demand, float steam_demand,
    float brew_temp, float steam_temp,
    float* brew_duty, float* steam_duty
);

/**
 * Apply hardware outputs (SSRs, relays)
 */
void apply_hardware_outputs(uint8_t brew_heater, uint8_t steam_heater, uint8_t pump);

/**
 * Initialize hardware (PWM, GPIO) - returns true if successful
 */
bool init_hardware_outputs(void);

/**
 * Estimate power consumption (fallback when power meter unavailable)
 */
uint16_t estimate_power_watts(uint8_t brew_duty, uint8_t steam_duty);

// =============================================================================
// Machine-Specific Functions (implemented per machine type)
// =============================================================================

/**
 * Initialize machine-specific control parameters
 * Called from control_init() after common initialization
 */
void control_init_machine(void);

/**
 * Update machine-specific control logic
 * Called from control_update() each control cycle
 * 
 * @param mode Current machine mode (MODE_IDLE, MODE_BREW, MODE_STEAM)
 * @param brew_temp Current brew temperature (°C)
 * @param steam_temp Current steam temperature (°C)
 * @param group_temp Current group temperature (°C)
 * @param dt Time delta in seconds
 * @param brew_duty Output: brew heater duty cycle (0-100)
 * @param steam_duty Output: steam heater duty cycle (0-100)
 */
void control_update_machine(
    machine_mode_t mode,
    float brew_temp, float steam_temp, float group_temp,
    float dt,
    float* brew_duty, float* steam_duty
);

/**
 * Get machine-specific single boiler mode (only for single boiler)
 * Returns 0 for other machine types
 */
uint8_t control_get_machine_mode(void);

/**
 * Check if machine is switching modes (only for single boiler)
 * Returns false for other machine types
 */
bool control_is_machine_switching(void);

#endif // CONTROL_IMPL_H

