/**
 * ECM Pico Firmware - Machine Configuration Implementation
 * 
 * Provides runtime access to machine configuration based on compile-time
 * MACHINE_TYPE selection.
 */

#include "machine_config.h"
#include <stddef.h>

// =============================================================================
// Static Configuration Pointer
// =============================================================================

// The active configuration is determined at compile time via MACHINE_TYPE
static const machine_config_t* g_machine_config = NULL;

// =============================================================================
// Initialization (called once at boot)
// =============================================================================

static void ensure_config_initialized(void) {
    if (g_machine_config == NULL) {
        g_machine_config = MACHINE_CONFIG_GET();
    }
}

// =============================================================================
// API Implementation
// =============================================================================

const machine_config_t* machine_config_get(void) {
    ensure_config_initialized();
    return g_machine_config;
}

machine_type_t machine_get_type(void) {
    ensure_config_initialized();
    return g_machine_config ? g_machine_config->features.type : MACHINE_TYPE_UNKNOWN;
}

const machine_features_t* machine_get_features(void) {
    ensure_config_initialized();
    return g_machine_config ? &g_machine_config->features : NULL;
}

bool machine_has_brew_boiler(void) {
    ensure_config_initialized();
    return g_machine_config ? g_machine_config->features.has_brew_boiler : false;
}

bool machine_has_steam_boiler(void) {
    ensure_config_initialized();
    return g_machine_config ? g_machine_config->features.has_steam_boiler : false;
}

bool machine_is_heat_exchanger(void) {
    ensure_config_initialized();
    return g_machine_config ? g_machine_config->features.is_heat_exchanger : false;
}

bool machine_needs_mode_switching(void) {
    ensure_config_initialized();
    return g_machine_config ? g_machine_config->features.needs_mode_switching : false;
}

bool machine_has_brew_ntc(void) {
    ensure_config_initialized();
    return g_machine_config ? g_machine_config->features.has_brew_ntc : false;
}

bool machine_has_steam_ntc(void) {
    ensure_config_initialized();
    return g_machine_config ? g_machine_config->features.has_steam_ntc : false;
}

const char* machine_get_name(void) {
    ensure_config_initialized();
    return g_machine_config ? g_machine_config->features.name : "Unknown";
}

const single_boiler_config_t* machine_get_single_boiler_config(void) {
    ensure_config_initialized();
    if (g_machine_config && g_machine_config->features.type == MACHINE_TYPE_SINGLE_BOILER) {
        return &g_machine_config->mode_config.single_boiler;
    }
    return NULL;
}

const heat_exchanger_config_t* machine_get_hx_config(void) {
    ensure_config_initialized();
    if (g_machine_config && g_machine_config->features.type == MACHINE_TYPE_HEAT_EXCHANGER) {
        return &g_machine_config->mode_config.heat_exchanger;
    }
    return NULL;
}

const machine_electrical_t* machine_get_electrical(void) {
    ensure_config_initialized();
    return g_machine_config ? &g_machine_config->electrical : NULL;
}

