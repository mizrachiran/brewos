#pragma once

#include "state_types.h"
#include <Preferences.h>
#include <functional>

namespace BrewOS {

/**
 * StateManager - Central state management for BrewOS
 * 
 * Handles:
 * - Settings persistence (NVS)
 * - Statistics tracking and persistence
 * - Shot history (ring buffer)
 * - Runtime state
 * - Change notifications
 */
class StateManager {
public:
    static StateManager& getInstance();
    
    // Lifecycle
    void begin();
    void loop();  // Call periodically for auto-save, daily reset, etc.
    
    // ==========================================================================
    // SETTINGS - Persisted to NVS
    // ==========================================================================
    
    Settings& settings() { return _settings; }
    const Settings& settings() const { return _settings; }
    
    // Save all settings to NVS
    void saveSettings();
    
    // Save specific settings section
    void saveTemperatureSettings();
    void saveBrewSettings();
    void savePowerSettings();
    void saveNetworkSettings();
    void saveTimeSettings();
    void saveMQTTSettings();
    void saveCloudSettings();
    void saveScaleSettings();
    void saveDisplaySettings();
    void saveScheduleSettings();
    void saveMachineInfoSettings();
    void saveNotificationSettings();
    void saveSystemSettings();
    void saveUserPreferences();
    
    // Reset settings to defaults
    void resetSettings();
    void factoryReset();  // Reset everything including stats
    
    // ==========================================================================
    // STATISTICS - Persisted periodically
    // ==========================================================================
    
    Statistics& stats() { return _stats; }
    const Statistics& stats() const { return _stats; }
    
    // Save stats to NVS (call periodically or on important events)
    void saveStats();
    
    // Increment counters
    void recordShot();
    void recordSteamCycle();
    void addPowerUsage(float kwh);
    void recordMaintenance(const char* type);  // "descale", "groupClean", "backflush"
    
    // ==========================================================================
    // SHOT HISTORY - Persisted to flash
    // ==========================================================================
    
    const ShotHistory& shotHistory() const { return _shotHistory; }
    
    // Add a completed shot
    void addShotRecord(const ShotRecord& shot);
    
    // Rate a shot
    void rateShot(uint8_t index, uint8_t rating);
    
    // Clear history
    void clearShotHistory();
    
    // ==========================================================================
    // RUNTIME STATE - Volatile
    // ==========================================================================
    
    RuntimeState& state() { return _state; }
    const RuntimeState& state() const { return _state; }
    
    // Convenience methods
    void setMachineState(MachineState newState);
    void setMachineMode(MachineMode newMode);
    void updateTemperatures(float brew, float steam);
    void updatePressure(float pressure);
    void updatePower(float watts, float voltage);
    void updateScale(float weight, float flowRate, bool stable);
    
    // Shot tracking
    void startShot();
    void endShot();
    bool isShotActive() const { return _state.shotActive; }
    uint32_t getShotDuration() const;
    
    // ==========================================================================
    // SCHEDULE MANAGEMENT
    // ==========================================================================
    
    // Add or update a schedule
    uint8_t addSchedule(const ScheduleEntry& entry);
    bool updateSchedule(uint8_t id, const ScheduleEntry& entry);
    bool removeSchedule(uint8_t id);
    bool enableSchedule(uint8_t id, bool enabled);
    
    // Auto power-off settings
    void setAutoPowerOff(bool enabled, uint16_t minutes);
    bool getAutoPowerOffEnabled() const { return _settings.schedule.autoPowerOffEnabled; }
    uint16_t getAutoPowerOffMinutes() const { return _settings.schedule.autoPowerOffMinutes; }
    
    // Schedule callbacks (for triggering actions)
    using ScheduleCallback = std::function<void(const ScheduleEntry&)>;
    void onScheduleTriggered(ScheduleCallback callback);
    
    // Check schedules (called from loop)
    void checkSchedules();
    
    // Idle tracking for auto power-off
    void resetIdleTimer();  // Call on user activity
    bool isIdleTimeout() const;
    
    // ==========================================================================
    // CHANGE NOTIFICATIONS
    // ==========================================================================
    
    using SettingsCallback = std::function<void(const Settings&)>;
    using StatsCallback = std::function<void(const Statistics&)>;
    using StateCallback = std::function<void(const RuntimeState&)>;
    using ShotCallback = std::function<void(const ShotRecord&)>;
    
    void onSettingsChanged(SettingsCallback callback);
    void onStatsChanged(StatsCallback callback);
    void onStateChanged(StateCallback callback);
    void onShotCompleted(ShotCallback callback);
    
    // ==========================================================================
    // SERIALIZATION
    // ==========================================================================
    
    // Get full state as JSON (for WebSocket broadcast)
    void getFullStateJson(JsonDocument& doc) const;
    
    // Get specific sections as JSON
    void getSettingsJson(JsonDocument& doc) const;
    void getStatsJson(JsonObject& obj) const;
    void getStateJson(JsonObject& obj) const;
    void getShotHistoryJson(JsonArray& arr) const;
    
    // Apply settings from JSON (from web/cloud)
    bool applySettings(const JsonDocument& doc);
    bool applySettings(const char* section, const JsonObject& obj);

private:
    StateManager() = default;
    ~StateManager() = default;
    StateManager(const StateManager&) = delete;
    StateManager& operator=(const StateManager&) = delete;
    
    // Storage
    Preferences _prefs;
    
    // State
    Settings _settings;
    Statistics _stats;
    ShotHistory _shotHistory;
    RuntimeState _state;
    
    // Active shot tracking
    ShotRecord _currentShot;
    float _shotPeakPressure = 0;
    float _shotTempSum = 0;
    uint32_t _shotTempCount = 0;
    
    // Callbacks
    SettingsCallback _onSettingsChanged;
    StatsCallback _onStatsChanged;
    StateCallback _onStateChanged;
    ShotCallback _onShotCompleted;
    
    // Timing
    uint32_t _lastStatsSave = 0;
    uint32_t _lastDailyReset = 0;
    uint32_t _lastScheduleCheck = 0;
    uint32_t _lastActivityTime = 0;
    uint8_t _lastScheduleMinute = 255;  // Track last minute to avoid duplicate triggers
    static constexpr uint32_t STATS_SAVE_INTERVAL = 300000;  // 5 minutes
    static constexpr uint32_t SCHEDULE_CHECK_INTERVAL = 10000;  // 10 seconds
    
    // Schedule callback
    ScheduleCallback _onScheduleTriggered;
    
    // Persistence
    void loadSettings();
    void loadStats();
    void loadShotHistory();
    void saveShotHistory();
    void loadScheduleSettings();
    
    // Helpers
    void notifySettingsChanged();
    void notifyStatsChanged();
    void notifyStateChanged();
    void checkDailyReset();
};

} // namespace BrewOS

// Convenience macro (outside namespace for global access)
#define State BrewOS::StateManager::getInstance()

