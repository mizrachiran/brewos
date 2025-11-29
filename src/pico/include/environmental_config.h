/**
 * ECM Pico Firmware - Environmental Configuration
 * 
 * Defines installation-specific electrical parameters (voltage, current limits).
 * These vary by installation location and can be set at compile-time or runtime.
 * 
 * See docs/pico/Environmental_Configuration.md for details.
 */

#ifndef ENVIRONMENTAL_CONFIG_H
#define ENVIRONMENTAL_CONFIG_H

#include <stdint.h>
#include "machine_electrical.h"  // For machine_electrical_t

// =============================================================================
// Environmental Electrical Configuration (Installation-specific)
// =============================================================================

typedef struct {
    // Nominal voltage (V) - from local electrical supply
    uint16_t nominal_voltage;        // 120, 230, 240, etc.
    
    // Maximum current draw limit (A) - from circuit breaker/installation
    // Set this to your circuit's safe limit (typically 10A or 16A)
    float    max_current_draw;       // e.g., 10.0f, 16.0f
    
    // Calculated values (computed from machine + environment at runtime)
    float    brew_heater_current;    // Calculated: machine.brew_heater_power / nominal_voltage
    float    steam_heater_current;   // Calculated: machine.steam_heater_power / nominal_voltage
    float    max_combined_current;   // Calculated: max_current_draw * 0.95 (5% safety margin)
    
} environmental_electrical_t;

// =============================================================================
// Complete Environmental Configuration
// =============================================================================

typedef struct {
    environmental_electrical_t electrical;  // Voltage, current limits (varies by installation)
    // Future: temperature units, timezone, locale, etc.
} environmental_config_t;

// =============================================================================
// Runtime Electrical State (computed from machine + environment)
// =============================================================================

typedef struct {
    // From machine config
    uint16_t brew_heater_power;
    uint16_t steam_heater_power;
    
    // From environmental config
    uint16_t nominal_voltage;
    float    max_current_draw;
    
    // Calculated values
    float    brew_heater_current;
    float    steam_heater_current;
    float    max_combined_current;
    
} electrical_state_t;

// =============================================================================
// Example Environmental Configurations
// =============================================================================

// Israel/Europe 230V with 10A limit
static const environmental_electrical_t ENV_230V_10A = {
    .nominal_voltage    = 230,
    .max_current_draw   = 10.0f,  // Direct limit: 10A
    // Calculated values computed at runtime
};

// Israel/Europe 230V with 16A limit (recommended for dual-boiler)
static const environmental_electrical_t ENV_230V_16A = {
    .nominal_voltage    = 230,
    .max_current_draw   = 16.0f,  // Direct limit: 16A
};

// USA 120V with 12A limit (15A breaker with 80% rule)
static const environmental_electrical_t ENV_120V_12A = {
    .nominal_voltage    = 120,
    .max_current_draw   = 12.0f,  // Direct limit: 12A
};

// USA 120V with 16A limit (20A breaker with 80% rule)
static const environmental_electrical_t ENV_120V_16A = {
    .nominal_voltage    = 120,
    .max_current_draw   = 16.0f,  // Direct limit: 16A
};

// =============================================================================
// Current Environmental Configuration Selection
// =============================================================================

// Select which environmental config to use at compile-time
// Can be overridden at runtime via flash storage
// Default: 230V/16A (typical for Europe/Israel)
#define ENVIRONMENTAL_ELECTRICAL_CONFIG ENV_230V_16A

// =============================================================================
// Functions
// =============================================================================

/**
 * Initialize electrical state from machine and environmental configs
 */
void electrical_state_init(electrical_state_t* state, 
                          const machine_electrical_t* machine,
                          const environmental_electrical_t* env);

/**
 * Get current electrical state
 */
void electrical_state_get(electrical_state_t* state);

/**
 * Set environmental configuration (can be called at runtime)
 */
void environmental_config_set(const environmental_electrical_t* config);

/**
 * Get current environmental configuration
 */
void environmental_config_get(environmental_electrical_t* config);

#endif // ENVIRONMENTAL_CONFIG_H

