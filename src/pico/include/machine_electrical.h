/**
 * ECM Pico Firmware - Machine Electrical Configuration
 * 
 * Defines machine-specific electrical specifications (heater power ratings).
 * These are hardware-specific and fixed for each machine model.
 * 
 * See docs/pico/Machine_Configurations.md for details.
 */

#ifndef MACHINE_ELECTRICAL_H
#define MACHINE_ELECTRICAL_H

#include <stdint.h>

// =============================================================================
// Machine Electrical Specifications (Hardware-specific, fixed)
// =============================================================================

typedef struct {
    // Heater power ratings (W) - from machine manufacturer specs
    // These vary by machine model (e.g., ECM Synchronika, Rancilio Silvia, etc.)
    uint16_t brew_heater_power;      // e.g., 1500W (ECM Synchronika), 1200W (other machine)
    uint16_t steam_heater_power;     // e.g., 1000W (ECM Synchronika), 1400W (other machine)
    
    // Additional machine-specific electrical specs (future expansion)
    // uint16_t pump_power;           // Pump motor power rating
    // uint16_t other_loads_power;    // Other electrical loads
    
} machine_electrical_t;

// =============================================================================
// Machine-Specific Configurations
// =============================================================================

// ECM Synchronika (Dual Boiler)
static const machine_electrical_t MACHINE_ECM_SYNCHRONIKA_ELECTRICAL = {
    .brew_heater_power  = 1500,  // ECM Synchronika brew boiler (from manufacturer specs)
    .steam_heater_power = 1000,  // ECM Synchronika steam boiler (from manufacturer specs)
};

// TODO: Add other machine types as they are supported
// Rancilio Silvia, Generic HX, etc.

// =============================================================================
// Current Machine Selection
// =============================================================================

// Select which machine electrical config to use
// This should match the machine type being built
#define MACHINE_ELECTRICAL_CONFIG MACHINE_ECM_SYNCHRONIKA_ELECTRICAL

#endif // MACHINE_ELECTRICAL_H

