/**
 * Status Change Detector Implementation
 */

#include "utils/status_change_detector.h"
#include "log_manager.h"
#include <string.h>
#include <math.h>

StatusChangeDetector::StatusChangeDetector() 
    : _initialized(false), _debug(false) {
    // Initialize previous state to zeros
    memset(&_previous, 0, sizeof(ui_state_t));
}

void StatusChangeDetector::reset() {
    _initialized = false;
    memset(&_previous, 0, sizeof(ui_state_t));
}

bool StatusChangeDetector::floatChanged(float current, float previous, float threshold) const {
    return fabsf(current - previous) >= threshold;
}

ChangedFields StatusChangeDetector::getChangedFields(const ui_state_t& current) {
    ChangedFields fields;
    
    // First call - all fields changed
    if (!_initialized) {
        fields.machine_state = true;
        fields.machine_mode = true;
        fields.heating_strategy = true;
        fields.is_heating = true;
        fields.is_brewing = true;
        fields.temps = true;
        fields.pressure = true;
        fields.power = true;
        fields.scale_weight = true;
        fields.scale_flow_rate = true;
        fields.scale_connected = true;
        fields.brew_time = true;
        fields.target_weight = true;
        fields.connections = true;
        fields.water_low = true;
        fields.alarm = true;
        fields.cleaning = true;
        fields.wifi = true;
        fields.mqtt = true;
        fields.stats = true;
        fields.esp32 = true;
        return fields;
    }
    
    // Machine state and mode
    if (current.machine_state != _previous.machine_state) {
        fields.machine_state = true;
        // Mode is derived from state, so mark it too
        fields.machine_mode = true;
    }
    
    // Heating strategy
    if (current.heating_strategy != _previous.heating_strategy) {
        fields.heating_strategy = true;
    }
    
    // Heating/brewing flags
    if (current.is_heating != _previous.is_heating) {
        fields.is_heating = true;
    }
    
    if (current.is_brewing != _previous.is_brewing) {
        fields.is_brewing = true;
    }
    
    // Temperatures - check if any changed
    if (floatChanged(current.brew_temp, _previous.brew_temp, STATUS_TEMP_THRESHOLD) ||
        floatChanged(current.brew_setpoint, _previous.brew_setpoint, STATUS_TEMP_THRESHOLD) ||
        floatChanged(current.steam_temp, _previous.steam_temp, STATUS_TEMP_THRESHOLD) ||
        floatChanged(current.steam_setpoint, _previous.steam_setpoint, STATUS_TEMP_THRESHOLD) ||
        floatChanged(current.group_temp, _previous.group_temp, STATUS_TEMP_THRESHOLD)) {
        fields.temps = true;
    }
    
    // Pressure
    if (floatChanged(current.pressure, _previous.pressure, STATUS_PRESSURE_THRESHOLD)) {
        fields.pressure = true;
    }
    
    // Power
    if (abs((int)current.power_watts - (int)_previous.power_watts) >= (int)STATUS_POWER_THRESHOLD) {
        fields.power = true;
    }
    
    // Scale weight
    if (floatChanged(current.brew_weight, _previous.brew_weight, STATUS_WEIGHT_THRESHOLD)) {
        fields.scale_weight = true;
    }
    
    // Flow rate
    if (floatChanged(current.flow_rate, _previous.flow_rate, STATUS_FLOW_RATE_THRESHOLD)) {
        fields.scale_flow_rate = true;
    }
    
    // Scale connected
    if (current.scale_connected != _previous.scale_connected) {
        fields.scale_connected = true;
    }
    
    // Brew time (when brewing)
    if (current.is_brewing && current.brew_time_ms != _previous.brew_time_ms) {
        fields.brew_time = true;
    }
    
    // Target weight
    if (floatChanged(current.target_weight, _previous.target_weight, STATUS_WEIGHT_THRESHOLD)) {
        fields.target_weight = true;
    }
    
    // Connection status - check if any changed
    if (current.pico_connected != _previous.pico_connected ||
        current.wifi_connected != _previous.wifi_connected ||
        current.mqtt_connected != _previous.mqtt_connected ||
        current.scale_connected != _previous.scale_connected ||
        current.cloud_connected != _previous.cloud_connected) {
        fields.connections = true;
    }
    
    // Water level
    if (current.water_low != _previous.water_low) {
        fields.water_low = true;
    }
    
    // Alarm
    if (current.alarm_active != _previous.alarm_active ||
        current.alarm_code != _previous.alarm_code) {
        fields.alarm = true;
    }
    
    // Cleaning
    if (current.cleaning_reminder != _previous.cleaning_reminder ||
        current.brew_count != _previous.brew_count) {
        fields.cleaning = true;
    }
    
    // WiFi details
    if (current.wifi_ap_mode != _previous.wifi_ap_mode ||
        strcmp(current.wifi_ip, _previous.wifi_ip) != 0 ||
        abs(current.wifi_rssi - _previous.wifi_rssi) >= 10) {
        fields.wifi = true;
    }
    
    // MQTT and stats are considered changed if we detect other changes
    // They'll be included in full status updates
    
    return fields;
}

bool StatusChangeDetector::hasChanged(const ui_state_t& current, ChangedFields* changedFields) {
    // First call - always return true and store current state
    if (!_initialized) {
        memcpy(&_previous, &current, sizeof(ui_state_t));
        _initialized = true;
        if (changedFields) {
            // Mark all fields as changed on first call
            *changedFields = ChangedFields();
            changedFields->machine_state = true;
            changedFields->machine_mode = true;
            changedFields->heating_strategy = true;
            changedFields->is_heating = true;
            changedFields->is_brewing = true;
            changedFields->temps = true;
            changedFields->pressure = true;
            changedFields->power = true;
            changedFields->scale_weight = true;
            changedFields->scale_flow_rate = true;
            changedFields->scale_connected = true;
            changedFields->brew_time = true;
            changedFields->target_weight = true;
            changedFields->connections = true;
            changedFields->water_low = true;
            changedFields->alarm = true;
            changedFields->cleaning = true;
            changedFields->wifi = true;
            changedFields->mqtt = true;
            changedFields->stats = true;
            changedFields->esp32 = true;
        }
        if (_debug) {
            LOG_D("StatusChangeDetector: Initialized");
        }
        return true;
    }
    
    bool changed = false;
    const char* changedField = nullptr;
    
    // Initialize changedFields if provided
    if (changedFields) {
        *changedFields = ChangedFields();
    }
    
    // Machine state - always check (critical)
    if (current.machine_state != _previous.machine_state) {
        changed = true;
        changedField = "machine_state";
        if (changedFields) changedFields->machine_state = true;
    }
    
    // Heating strategy
    if (current.heating_strategy != _previous.heating_strategy) {
        changed = true;
        changedField = "heating_strategy";
        if (changedFields) changedFields->heating_strategy = true;
    }
    
    // Heating/brewing flags
    if (current.is_heating != _previous.is_heating) {
        changed = true;
        changedField = "is_heating";
        if (changedFields) changedFields->is_heating = true;
    }
    
    if (current.is_brewing != _previous.is_brewing) {
        changed = true;
        changedField = "is_brewing";
        if (changedFields) changedFields->is_brewing = true;
    }
    
    // Temperatures - with threshold (grouped as "temps")
    bool tempChanged = false;
    if (floatChanged(current.brew_temp, _previous.brew_temp, STATUS_TEMP_THRESHOLD) ||
        floatChanged(current.brew_setpoint, _previous.brew_setpoint, STATUS_TEMP_THRESHOLD) ||
        floatChanged(current.steam_temp, _previous.steam_temp, STATUS_TEMP_THRESHOLD) ||
        floatChanged(current.steam_setpoint, _previous.steam_setpoint, STATUS_TEMP_THRESHOLD) ||
        floatChanged(current.group_temp, _previous.group_temp, STATUS_TEMP_THRESHOLD)) {
        changed = true;
        tempChanged = true;
        changedField = "temps";
        if (changedFields) changedFields->temps = true;
    }
    
    // Pressure - with threshold
    if (floatChanged(current.pressure, _previous.pressure, STATUS_PRESSURE_THRESHOLD)) {
        changed = true;
        changedField = "pressure";
        if (changedFields) changedFields->pressure = true;
    }
    
    // Power - with threshold
    if (abs((int)current.power_watts - (int)_previous.power_watts) >= (int)STATUS_POWER_THRESHOLD) {
        changed = true;
        changedField = "power";
        if (changedFields) changedFields->power = true;
    }
    
    // Brewing info - always check when brewing
    if (current.is_brewing) {
        // Brew time changes every update when brewing, but we only care about significant changes
        // Actually, let's always update when brewing to ensure real-time updates
        if (current.brew_time_ms != _previous.brew_time_ms) {
            changed = true;
            changedField = "brew_time";
            if (changedFields) changedFields->brew_time = true;
        }
    }
    
    // Weight - with threshold
    if (floatChanged(current.brew_weight, _previous.brew_weight, STATUS_WEIGHT_THRESHOLD)) {
        changed = true;
        changedField = "scale_weight";
        if (changedFields) changedFields->scale_weight = true;
    }
    
    // Flow rate - with threshold
    if (floatChanged(current.flow_rate, _previous.flow_rate, STATUS_FLOW_RATE_THRESHOLD)) {
        changed = true;
        changedField = "scale_flow_rate";
        if (changedFields) changedFields->scale_flow_rate = true;
    }
    
    // Target weight changes
    if (floatChanged(current.target_weight, _previous.target_weight, STATUS_WEIGHT_THRESHOLD)) {
        changed = true;
        changedField = "target_weight";
        if (changedFields) changedFields->target_weight = true;
    }
    
    // Connection status - always check (grouped as "connections")
    bool connChanged = false;
    if (current.pico_connected != _previous.pico_connected ||
        current.wifi_connected != _previous.wifi_connected ||
        current.mqtt_connected != _previous.mqtt_connected ||
        current.scale_connected != _previous.scale_connected ||
        current.cloud_connected != _previous.cloud_connected) {
        changed = true;
        connChanged = true;
        changedField = "connections";
        if (changedFields) {
            changedFields->connections = true;
            changedFields->scale_connected = (current.scale_connected != _previous.scale_connected);
        }
    }
    
    // Water level and alarms - always check
    if (current.water_low != _previous.water_low) {
        changed = true;
        changedField = "water_low";
        if (changedFields) changedFields->water_low = true;
    }
    
    // Alarm (grouped)
    if (current.alarm_active != _previous.alarm_active || current.alarm_code != _previous.alarm_code) {
        changed = true;
        changedField = "alarm";
        if (changedFields) changedFields->alarm = true;
    }
    
    // Cleaning reminder
    if (current.cleaning_reminder != _previous.cleaning_reminder) {
        changed = true;
        changedField = "cleaning";
        if (changedFields) changedFields->cleaning = true;
    }
    
    // WiFi (grouped - RSSI, AP mode, IP)
    bool wifiChanged = false;
    if (abs(current.wifi_rssi - _previous.wifi_rssi) >= 10 ||
        current.wifi_ap_mode != _previous.wifi_ap_mode ||
        strcmp(current.wifi_ip, _previous.wifi_ip) != 0) {
        changed = true;
        wifiChanged = true;
        changedField = "wifi";
        if (changedFields) changedFields->wifi = true;
    }
    
    // MQTT config changes (if we track them separately)
    // For now, mqtt connection status is part of "connections"
    
    // Stats and ESP32 info are not in ui_state_t directly, so we don't check them here
    // They would be set externally if needed
    
    // If something changed, update previous state
    if (changed) {
        memcpy(&_previous, &current, sizeof(ui_state_t));
        if (_debug && changedField) {
            LOG_D("StatusChangeDetector: Change detected in %s", changedField);
        }
    }
    
    return changed;
}

