/**
 * Status Change Detector
 * 
 * Detects meaningful changes in device status to avoid sending redundant updates
 * to cloud and MQTT. Only sends updates when data actually changes or on periodic
 * heartbeats to maintain connectivity.
 */

#ifndef STATUS_CHANGE_DETECTOR_H
#define STATUS_CHANGE_DETECTOR_H

#include "ui/ui.h"
#include <stdint.h>
#include <stdbool.h>

// =============================================================================
// Change Detection Thresholds
// =============================================================================

// Temperature change threshold (degrees Celsius)
// Only trigger update if temperature changes by this amount
#define STATUS_TEMP_THRESHOLD 0.5f

// Pressure change threshold (bar)
#define STATUS_PRESSURE_THRESHOLD 0.1f

// Power change threshold (watts)
#define STATUS_POWER_THRESHOLD 10.0f

// Weight change threshold (grams)
#define STATUS_WEIGHT_THRESHOLD 0.5f

// Flow rate change threshold (ml/s)
#define STATUS_FLOW_RATE_THRESHOLD 0.1f

// =============================================================================
// Changed Fields Structure
// Tracks which specific fields changed for delta updates
// =============================================================================

struct ChangedFields {
    bool machine_state;
    bool machine_mode;
    bool heating_strategy;
    bool is_heating;
    bool is_brewing;
    bool temps;              // Any temperature changed
    bool pressure;
    bool power;
    bool scale_weight;
    bool scale_flow_rate;
    bool scale_connected;
    bool brew_time;
    bool target_weight;
    bool connections;        // Any connection status changed
    bool water_low;
    bool alarm;
    bool cleaning;
    bool wifi;               // WiFi details changed
    bool mqtt;               // MQTT config changed
    bool stats;              // Statistics changed
    bool esp32;              // ESP32 info changed
    
    ChangedFields() : machine_state(false), machine_mode(false), heating_strategy(false),
                      is_heating(false), is_brewing(false), temps(false), pressure(false),
                      power(false), scale_weight(false), scale_flow_rate(false),
                      scale_connected(false), brew_time(false), target_weight(false),
                      connections(false), water_low(false), alarm(false), cleaning(false),
                      wifi(false), mqtt(false), stats(false), esp32(false) {}
    
    bool anyChanged() const {
        return machine_state || machine_mode || heating_strategy || is_heating || is_brewing ||
               temps || pressure || power || scale_weight || scale_flow_rate || scale_connected ||
               brew_time || target_weight || connections || water_low || alarm || cleaning ||
               wifi || mqtt || stats || esp32;
    }
};

// =============================================================================
// Status Change Detector Class
// =============================================================================

class StatusChangeDetector {
public:
    StatusChangeDetector();
    
    /**
     * Check if the current status has changed meaningfully from the previous status
     * @param current Current device status
     * @param changedFields Optional pointer to populate with specific changed fields
     * @return true if any meaningful field changed, false otherwise
     */
    bool hasChanged(const ui_state_t& current, ChangedFields* changedFields = nullptr);
    
    /**
     * Get which specific fields changed (for delta updates)
     * Must be called after hasChanged() to get accurate results
     * @param current Current device status
     * @return ChangedFields structure indicating which fields changed
     */
    ChangedFields getChangedFields(const ui_state_t& current);
    
    /**
     * Reset the detector (useful after reconnection or initialization)
     * Forces next check to return true
     */
    void reset();
    
    /**
     * Enable/disable debug logging of what changed
     */
    void setDebug(bool enabled) { _debug = enabled; }

private:
    // Previous status values for comparison
    ui_state_t _previous;
    bool _initialized;
    bool _debug;
    
    /**
     * Compare two float values with a threshold
     */
    bool floatChanged(float current, float previous, float threshold) const;
};

#endif // STATUS_CHANGE_DETECTOR_H

