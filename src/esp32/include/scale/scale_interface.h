/**
 * BrewOS BLE Scale Interface
 * 
 * Common types and utilities for BLE scale support.
 * See scale_manager.h for the main scale interface.
 */

#ifndef SCALE_INTERFACE_H
#define SCALE_INTERFACE_H

#include <Arduino.h>

// =============================================================================
// Scale Types
// =============================================================================

typedef enum {
    SCALE_TYPE_UNKNOWN = 0,
    SCALE_TYPE_ACAIA,       // Acaia Lunar, Pearl, Pyxis, Cinco
    SCALE_TYPE_FELICITA,    // Felicita Arc, Parallel, Incline
    SCALE_TYPE_DECENT,      // Decent Scale
    SCALE_TYPE_TIMEMORE,    // Timemore Black Mirror
    SCALE_TYPE_HIROIA,      // Hiroia Jimmy
    SCALE_TYPE_BOOKOO,      // Bookoo Themis Mini, Themis Ultra
    SCALE_TYPE_GENERIC_WSS, // Standard Bluetooth Weight Scale Service
} scale_type_t;

// =============================================================================
// Scale State
// =============================================================================

typedef struct {
    bool connected;         // Connected to scale
    bool stable;            // Weight reading is stable
    float weight;           // Weight in grams
    float flow_rate;        // Calculated flow rate (g/s)
    int battery_percent;    // Battery level (0-100, -1 if unknown)
    uint32_t last_update_ms;
} scale_state_t;

// =============================================================================
// Scale Info (for discovery)
// =============================================================================

typedef struct {
    char name[32];          // BLE advertised name
    char address[18];       // MAC address (xx:xx:xx:xx:xx:xx)
    scale_type_t type;      // Detected scale type
    int rssi;               // Signal strength (dBm)
} scale_info_t;

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * Detect scale type from BLE advertised name
 * @param name BLE device name
 * @return Detected scale type
 */
scale_type_t detectScaleType(const char* name);

/**
 * Get human-readable name for scale type
 */
const char* getScaleTypeName(scale_type_t type);

#endif // SCALE_INTERFACE_H
