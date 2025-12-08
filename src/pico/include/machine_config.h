/**
 * BrewOS - Machine Configuration
 * 
 * Defines machine types (Dual Boiler, Single Boiler, Heat Exchanger) and
 * their feature sets. This enables the same firmware to support different
 * espresso machine architectures.
 * 
 * Supported Machine Examples:
 *   - Dual Boiler: ECM Synchronika, Profitec Pro 700, Decent DE1
 *   - Single Boiler: Rancilio Silvia, Gaggia Classic
 *   - Heat Exchanger: E61 HX machines, Bezzera BZ10
 * 
 * Machine Type Selection:
 *   - Set MACHINE_TYPE at compile time (CMakeLists.txt or -DMACHINE_TYPE=...)
 *   - Features are determined by machine type
 *   - Control, safety, and sensor code adapts automatically
 */

#ifndef MACHINE_CONFIG_H
#define MACHINE_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

// =============================================================================
// Machine Type Enumeration
// =============================================================================

typedef enum {
    MACHINE_TYPE_UNKNOWN        = 0x00,
    MACHINE_TYPE_DUAL_BOILER    = 0x01,  // Two boilers: brew + steam (e.g., Profitec Pro 700, ECM Synchronika)
    MACHINE_TYPE_SINGLE_BOILER  = 0x02,  // One boiler: switches between brew/steam (e.g., Rancilio Silvia, Gaggia Classic)
    MACHINE_TYPE_HEAT_EXCHANGER = 0x03,  // Steam boiler with passive HX (e.g., E61 HX machines, Bezzera BZ10)
    MACHINE_TYPE_THERMOBLOCK    = 0x04,  // Flow heater, no boiler (future)
} machine_type_t;

// =============================================================================
// Machine Features Structure
// =============================================================================

/**
 * Defines what hardware features are present on this machine type.
 * Used by control, safety, and sensor code to adapt behavior.
 */
typedef struct {
    // ═══════════════════════════════════════════════════════════════
    // IDENTITY
    // ═══════════════════════════════════════════════════════════════
    machine_type_t type;
    const char*    name;
    const char*    description;
    
    // ═══════════════════════════════════════════════════════════════
    // BOILER CONFIGURATION
    // ═══════════════════════════════════════════════════════════════
    uint8_t  num_boilers;           // 0, 1, or 2
    bool     has_brew_boiler;       // Separate brew boiler with heater
    bool     has_steam_boiler;      // Separate steam boiler with heater
    bool     is_heat_exchanger;     // Brew water heated passively via HX
    
    // ═══════════════════════════════════════════════════════════════
    // TEMPERATURE SENSORS
    // ═══════════════════════════════════════════════════════════════
    bool     has_brew_ntc;          // Brew boiler temperature sensor
    bool     has_steam_ntc;         // Steam boiler temperature sensor
    
    // ═══════════════════════════════════════════════════════════════
    // CONTROL CHARACTERISTICS
    // ═══════════════════════════════════════════════════════════════
    bool     needs_mode_switching;  // Single boiler: switches between brew/steam mode
    bool     steam_provides_brew_heat; // HX: steam boiler provides brew heat
    
    // ═══════════════════════════════════════════════════════════════
    // WATER SYSTEM
    // ═══════════════════════════════════════════════════════════════
    bool     has_steam_level_probe; // Steam boiler water level
    bool     has_auto_fill;         // Automatic steam boiler fill
    
    // ═══════════════════════════════════════════════════════════════
    // OUTPUTS
    // ═══════════════════════════════════════════════════════════════
    uint8_t  num_ssrs;              // Number of SSR outputs (0, 1, or 2)
    bool     has_separate_steam_ssr;// Separate SSR for steam boiler
    
} machine_features_t;

// =============================================================================
// Machine Electrical Specifications
// =============================================================================

/**
 * Defines machine-specific electrical specifications.
 * These are hardware-fixed values from manufacturer specs.
 * Used for power calculations and heating strategy decisions.
 */
typedef struct {
    uint16_t brew_heater_power;     // Brew heater power in Watts (0 if none)
    uint16_t steam_heater_power;    // Steam heater power in Watts (0 if none)
    // Future expansion:
    // uint16_t pump_power;         // Pump motor power rating
    // uint16_t other_loads_power;  // Other electrical loads
} machine_electrical_t;

// =============================================================================
// Single Boiler Configuration
// =============================================================================

/**
 * Configuration specific to single boiler machines.
 * These machines switch between brew and steam mode using the same boiler.
 */
typedef struct {
    float    brew_setpoint;         // Brew mode setpoint (°C)
    float    steam_setpoint;        // Steam mode setpoint (°C)
    uint16_t mode_switch_delay_ms;  // Delay when switching modes (cooldown/heatup)
    bool     auto_return_to_brew;   // Automatically return to brew mode after steaming
    uint16_t steam_timeout_s;       // Auto-return timeout (seconds, 0=disabled)
} single_boiler_config_t;

// =============================================================================
// Heat Exchanger Configuration
// =============================================================================

/**
 * Control mode for heat exchanger machines.
 * 
 * Traditional HX machines (like PRO ELIND ECO) use a pressurestat - 
 * a mechanical pressure switch that controls the heater directly.
 * Modern retrofits may add an NTC temperature sensor or pressure transducer.
 * 
 * HX_CONTROL_TEMPERATURE:
 *   - Requires steam NTC sensor
 *   - Full PID control of heater via SSR
 *   - Best for modern retrofits
 * 
 * HX_CONTROL_PRESSURE:
 *   - Requires pressure transducer
 *   - PID control based on boiler pressure
 *   - Good when NTC is not available
 * 
 * HX_CONTROL_PRESSURESTAT:
 *   - Heater controlled by external pressurestat (NOT our SSR)
 *   - Our controller monitors only - does NOT control heater
 *   - No rewiring required - add smart monitoring to existing machine
 *   - SSR output should be disconnected or not wired to heater
 */
typedef enum {
    HX_CONTROL_TEMPERATURE = 0,  // PID based on steam NTC temperature (modern retrofit)
    HX_CONTROL_PRESSURE = 1,     // PID based on pressure transducer
    HX_CONTROL_PRESSURESTAT = 2, // External pressurestat controls heater (monitor only)
} hx_control_mode_t;

/**
 * Configuration specific to heat exchanger machines.
 * Steam boiler provides heat, brew temp is monitored via group thermocouple.
 */
typedef struct {
    // Control mode
    hx_control_mode_t control_mode;     // How the steam boiler is controlled
    
    // Temperature-based control (HX_CONTROL_TEMPERATURE)
    float    steam_setpoint;            // Steam boiler setpoint (°C)
    
    // Pressure-based control (HX_CONTROL_PRESSURE)
    float    pressure_setpoint_bar;     // Target pressure (bar)
    float    pressure_hysteresis_bar;   // Hysteresis for pressure control
    
    // Pressurestat mode (HX_CONTROL_PRESSURESTAT)
    // In this mode, we monitor but don't control the heater
    // The external pressurestat handles on/off control
    bool     pressurestat_has_feedback; // True if we can read pressurestat state
} heat_exchanger_config_t;

// =============================================================================
// Complete Machine Configuration
// =============================================================================

typedef struct {
    machine_features_t features;
    machine_electrical_t electrical;
    
    // Mode-specific configuration (only one applies based on machine type)
    union {
        single_boiler_config_t single_boiler;
        heat_exchanger_config_t heat_exchanger;
    } mode_config;
    
} machine_config_t;

// =============================================================================
// Machine Configuration Instances
// =============================================================================

// Dual Boiler Configuration (e.g., ECM Synchronika, Profitec Pro 700)
static const machine_config_t MACHINE_CONFIG_DUAL_BOILER = {
    .features = {
        .type                   = MACHINE_TYPE_DUAL_BOILER,
        .name                   = "Dual Boiler",
        .description            = "Two independent boilers (brew + steam)",
        
        .num_boilers            = 2,
        .has_brew_boiler        = true,
        .has_steam_boiler       = true,
        .is_heat_exchanger      = false,
        
        .has_brew_ntc           = true,
        .has_steam_ntc          = true,
        
        .needs_mode_switching   = false,
        .steam_provides_brew_heat = false,
        
        .has_steam_level_probe  = true,
        .has_auto_fill          = false,
        
        .num_ssrs               = 2,
        .has_separate_steam_ssr = true,
    },
    .electrical = {
        .brew_heater_power      = 1500,  // Typical dual boiler brew heater (ECM Synchronika)
        .steam_heater_power     = 1000,  // Typical dual boiler steam heater (ECM Synchronika)
    },
    .mode_config = { 0 },  // Not used for dual boiler
};

// Rancilio Silvia Style (Single Boiler)
static const machine_config_t MACHINE_CONFIG_SINGLE_BOILER = {
    .features = {
        .type                   = MACHINE_TYPE_SINGLE_BOILER,
        .name                   = "Single Boiler",
        .description            = "One boiler, switches between brew/steam mode",
        
        .num_boilers            = 1,
        .has_brew_boiler        = true,   // Same boiler used for both
        .has_steam_boiler       = false,  // No separate steam boiler
        .is_heat_exchanger      = false,
        
        .has_brew_ntc           = true,   // Single NTC on the boiler
        .has_steam_ntc          = false,  // No separate steam sensor
        
        .needs_mode_switching   = true,   // Must switch between brew/steam setpoint
        .steam_provides_brew_heat = false,
        
        .has_steam_level_probe  = false,  // No separate steam boiler
        .has_auto_fill          = false,
        
        .num_ssrs               = 1,      // Single SSR for the boiler
        .has_separate_steam_ssr = false,
    },
    .electrical = {
        .brew_heater_power      = 1200,   // Typical single boiler heater (Rancilio Silvia)
        .steam_heater_power     = 0,      // Same heater used for both (accounted in brew)
    },
    .mode_config.single_boiler = {
        .brew_setpoint          = 93.0f,
        .steam_setpoint         = 140.0f,
        .mode_switch_delay_ms   = 5000,   // 5 second delay for thermal stabilization
        .auto_return_to_brew    = true,
        .steam_timeout_s        = 120,    // Return to brew after 2 minutes
    },
};

// E61 Heat Exchanger Style
static const machine_config_t MACHINE_CONFIG_HEAT_EXCHANGER = {
    .features = {
        .type                   = MACHINE_TYPE_HEAT_EXCHANGER,
        .name                   = "Heat Exchanger",
        .description            = "Steam boiler with passive heat exchanger for brew",
        
        .num_boilers            = 1,      // Only the steam boiler
        .has_brew_boiler        = false,  // No active brew boiler
        .has_steam_boiler       = true,   // Steam boiler with heater
        .is_heat_exchanger      = true,   // Brew water via HX
        
        .has_brew_ntc           = false,  // No brew boiler to measure
        .has_steam_ntc          = true,   // Steam boiler temperature
        
        .needs_mode_switching   = false,  // Steam and brew available simultaneously
        .steam_provides_brew_heat = true, // Steam boiler heats the HX
        
        .has_steam_level_probe  = true,   // Steam boiler level
        .has_auto_fill          = true,   // Often plumbed
        
        .num_ssrs               = 1,      // Only steam boiler SSR
        .has_separate_steam_ssr = false,  // It's the only SSR
    },
    .electrical = {
        .brew_heater_power      = 0,      // No separate brew heater (passive HX)
        .steam_heater_power     = 1400,   // Typical HX steam boiler heater (Bezzera BZ10)
    },
    .mode_config.heat_exchanger = {
        // Control mode - default to temperature PID (modern retrofit)
        .control_mode           = HX_CONTROL_TEMPERATURE,
        
        // Temperature PID settings
        .steam_setpoint         = 125.0f, // Lower than pure steam due to HX
        
        // Pressure PID settings (if HX_CONTROL_PRESSURE)
        .pressure_setpoint_bar  = 1.0f,   // ~1 bar for typical HX
        .pressure_hysteresis_bar = 0.1f,  // 0.1 bar hysteresis
        
        // Pressurestat mode (if HX_CONTROL_PRESSURESTAT)
        .pressurestat_has_feedback = false,
    },
};

// =============================================================================
// Active Machine Configuration Selection
// =============================================================================

// Select machine type at compile time
// Override in CMakeLists.txt: -DMACHINE_TYPE=DUAL_BOILER
#ifndef MACHINE_TYPE
#define MACHINE_TYPE DUAL_BOILER
#endif

// Concatenation macros for selecting config
#define _MACHINE_CONFIG_CONCAT(a, b) a ## b
#define MACHINE_CONFIG_CONCAT(a, b) _MACHINE_CONFIG_CONCAT(a, b)

// Get the active machine configuration
#define MACHINE_CONFIG_GET() \
    (&MACHINE_CONFIG_CONCAT(MACHINE_CONFIG_, MACHINE_TYPE))

// =============================================================================
// API Functions
// =============================================================================

/**
 * Get the active machine configuration
 */
const machine_config_t* machine_config_get(void);

/**
 * Get machine type
 */
machine_type_t machine_get_type(void);

/**
 * Get machine features
 */
const machine_features_t* machine_get_features(void);

/**
 * Check if machine has a specific feature
 */
bool machine_has_brew_boiler(void);
bool machine_has_steam_boiler(void);
bool machine_is_heat_exchanger(void);
bool machine_needs_mode_switching(void);
bool machine_has_brew_ntc(void);
bool machine_has_steam_ntc(void);

/**
 * Get machine name string
 */
const char* machine_get_name(void);

/**
 * Get single boiler config (only valid for MACHINE_TYPE_SINGLE_BOILER)
 */
const single_boiler_config_t* machine_get_single_boiler_config(void);

/**
 * Get heat exchanger config (only valid for MACHINE_TYPE_HEAT_EXCHANGER)
 */
const heat_exchanger_config_t* machine_get_hx_config(void);

/**
 * Get machine electrical specifications
 */
const machine_electrical_t* machine_get_electrical(void);

#endif // MACHINE_CONFIG_H

