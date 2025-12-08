#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace BrewOS {

// =============================================================================
// SETTINGS - User configurable, persisted to NVS
// =============================================================================

struct TemperatureSettings {
    float brewSetpoint = 93.5f;        // °C
    float steamSetpoint = 145.0f;      // °C
    float brewOffset = 0.0f;           // Calibration offset
    float steamOffset = 0.0f;          // Calibration offset
    float ecoBrewTemp = 80.0f;         // Eco mode brew temp
    uint16_t ecoTimeoutMinutes = 30;   // Auto-eco after idle
    
    void toJson(JsonObject& obj) const;
    bool fromJson(JsonObjectConst obj);
};

struct BrewSettings {
    bool bbwEnabled = false;           // Brew-by-weight enabled
    float doseWeight = 18.0f;          // Input dose (g)
    float targetWeight = 36.0f;        // Target output (g)
    float stopOffset = 2.0f;           // Stop before target (g)
    bool autoTare = true;              // Auto-tare on portafilter
    float preinfusionTime = 0.0f;      // Pre-infusion seconds
    float preinfusionPressure = 2.0f;  // Pre-infusion bar
    
    void toJson(JsonObject& obj) const;
    bool fromJson(JsonObjectConst obj);
};

struct PowerSettings {
    uint16_t mainsVoltage = 220;       // 110, 220, 240
    float maxCurrent = 13.0f;          // Amps limit
    bool powerOnBoot = false;          // Auto power on
    
    void toJson(JsonObject& obj) const;
    bool fromJson(JsonObjectConst obj);
};

struct NetworkSettings {
    char wifiSsid[33] = {0};
    char wifiPassword[65] = {0};
    bool wifiConfigured = false;
    
    char hostname[32] = "brewos";
    
    void toJson(JsonObject& obj) const;
    bool fromJson(JsonObjectConst obj);
};

struct SystemSettings {
    bool setupComplete = false;         // First-run wizard completed
    
    void toJson(JsonObject& obj) const;
    bool fromJson(JsonObjectConst obj);
};

struct TimeSettings {
    bool useNTP = true;                 // Use NTP vs manual time
    char ntpServer[64] = "pool.ntp.org";
    int16_t utcOffsetMinutes = 0;       // Timezone offset in minutes from UTC
    bool dstEnabled = false;            // Daylight saving time
    int16_t dstOffsetMinutes = 60;      // DST offset (usually 60 min)
    
    void toJson(JsonObject& obj) const;
    bool fromJson(JsonObjectConst obj);
};

struct MQTTSettings {
    bool enabled = false;
    char broker[64] = {0};
    uint16_t port = 1883;
    char username[32] = {0};
    char password[64] = {0};
    char baseTopic[32] = "brewos";
    bool discovery = true;             // Home Assistant discovery
    
    void toJson(JsonObject& obj) const;
    bool fromJson(JsonObjectConst obj);
};

struct CloudSettings {
    bool enabled = false;
    char serverUrl[128] = {0};
    char deviceId[37] = {0};           // UUID
    char deviceKey[65] = {0};          // Secret key
    
    void toJson(JsonObject& obj) const;
    bool fromJson(JsonObjectConst obj);
};

struct ScaleSettings {
    bool enabled = true;
    char pairedAddress[18] = {0};      // MAC address
    char pairedName[32] = {0};
    uint8_t scaleType = 0;             // 0=unknown, 1=acaia, 2=felicita, etc.
    
    void toJson(JsonObject& obj) const;
    bool fromJson(JsonObjectConst obj);
};

struct MachineInfoSettings {
    char deviceName[32] = "BrewOS";    // User-friendly device name
    char machineBrand[32] = {0};       // e.g., "ECM", "La Marzocco"
    char machineModel[32] = {0};       // e.g., "Synchronika", "Linea Mini"
    char machineType[20] = "dual_boiler";  // dual_boiler, single_boiler, heat_exchanger
    
    void toJson(JsonObject& obj) const;
    bool fromJson(JsonObjectConst obj);
};

struct NotificationSettings {
    bool machineReady = true;
    bool waterEmpty = true;
    bool descaleDue = true;
    bool serviceDue = true;
    bool backflushDue = true;
    bool machineError = true;
    bool picoOffline = true;
    bool scheduleTriggered = true;
    bool brewComplete = false;
    
    void toJson(JsonObject& obj) const;
    bool fromJson(JsonObjectConst obj);
};

struct DisplaySettings {
    uint8_t brightness = 200;          // 0-255
    uint8_t screenTimeout = 30;        // Seconds, 0=never
    bool showShotTimer = true;
    bool showWeight = true;
    bool showPressure = true;
    
    void toJson(JsonObject& obj) const;
    bool fromJson(JsonObjectConst obj);
};

// User preferences - UI/UX settings synced across devices
struct UserPreferences {
    // Regional settings
    uint8_t firstDayOfWeek = 0;        // 0=Sunday, 1=Monday
    bool use24HourTime = false;
    uint8_t temperatureUnit = 0;       // 0=celsius, 1=fahrenheit
    
    // Energy cost settings
    float electricityPrice = 0.15f;    // Price per kWh
    char currency[4] = "USD";          // Currency code (USD, EUR, GBP, etc.)
    
    // Machine behavior
    uint8_t lastHeatingStrategy = 1;   // 0=BrewOnly, 1=Sequential, 2=Parallel, 3=SmartStagger
    
    // Flags
    bool initialized = false;          // True after first setup from browser
    
    void toJson(JsonObject& obj) const;
    bool fromJson(JsonObjectConst obj);
};

// =============================================================================
// SCHEDULE SETTINGS - Time-based automation
// =============================================================================

// Days of week bitmask
enum DayOfWeek : uint8_t {
    SUNDAY    = 0x01,
    MONDAY    = 0x02,
    TUESDAY   = 0x04,
    WEDNESDAY = 0x08,
    THURSDAY  = 0x10,
    FRIDAY    = 0x20,
    SATURDAY  = 0x40,
    WEEKDAYS  = MONDAY | TUESDAY | WEDNESDAY | THURSDAY | FRIDAY,
    WEEKENDS  = SATURDAY | SUNDAY,
    EVERY_DAY = 0x7F
};

// Schedule action types
enum ScheduleAction : uint8_t {
    ACTION_TURN_ON = 0,
    ACTION_TURN_OFF = 1
};

// Heating strategies (matches protocol_defs.h values)
// Note: Using different names to avoid macro conflicts with protocol_defs.h
enum HeatingStrategy : uint8_t {
    STRATEGY_BREW_ONLY = 0,
    STRATEGY_SEQUENTIAL = 1,
    STRATEGY_PARALLEL = 2,
    STRATEGY_SMART_STAGGER = 3
};

// Single schedule entry
struct ScheduleEntry {
    uint8_t id = 0;                    // Unique ID (1-10, 0 = unused)
    bool enabled = false;              // Schedule is active
    uint8_t days = EVERY_DAY;          // Day of week bitmask
    uint8_t hour = 7;                  // Hour (0-23)
    uint8_t minute = 0;                // Minute (0-59)
    ScheduleAction action = ACTION_TURN_ON;
    HeatingStrategy strategy = STRATEGY_SEQUENTIAL;  // Only used for TURN_ON
    char name[24] = {0};               // User-friendly name
    
    void toJson(JsonObject& obj) const;
    bool fromJson(JsonObjectConst obj);
    bool isValidForDay(uint8_t dayOfWeek) const;  // dayOfWeek: 0=Sun, 1=Mon, etc.
    bool matchesTime(uint8_t h, uint8_t m) const;
};

// Maximum number of schedules
constexpr size_t MAX_SCHEDULES = 10;

struct ScheduleSettings {
    ScheduleEntry schedules[MAX_SCHEDULES];
    uint8_t count = 0;                 // Number of active schedules
    
    // Auto power-off settings
    bool autoPowerOffEnabled = false;
    uint16_t autoPowerOffMinutes = 60; // Minutes of idle before power off (0 = disabled)
    
    void toJson(JsonObject& obj) const;
    bool fromJson(JsonObjectConst obj);
    
    // Helper methods
    ScheduleEntry* findById(uint8_t id);
    const ScheduleEntry* findById(uint8_t id) const;
    uint8_t addSchedule(const ScheduleEntry& entry);  // Returns new ID or 0 on failure
    bool removeSchedule(uint8_t id);
    uint8_t getNextId() const;
};

// All settings combined
struct Settings {
    TemperatureSettings temperature;
    BrewSettings brew;
    PowerSettings power;
    NetworkSettings network;
    TimeSettings time;
    MQTTSettings mqtt;
    CloudSettings cloud;
    ScaleSettings scale;
    DisplaySettings display;
    ScheduleSettings schedule;
    MachineInfoSettings machineInfo;
    NotificationSettings notifications;
    SystemSettings system;
    UserPreferences preferences;
    
    void toJson(JsonDocument& doc) const;
    bool fromJson(const JsonDocument& doc);
};

// =============================================================================
// STATISTICS - Counters and accumulators, persisted periodically
// =============================================================================

struct Statistics {
    // Lifetime stats
    uint32_t totalShots = 0;
    uint32_t totalSteamCycles = 0;
    float totalKwh = 0.0f;
    uint32_t totalOnTimeMinutes = 0;
    
    // Daily stats (reset at midnight or manually)
    uint16_t shotsToday = 0;
    float kwhToday = 0.0f;
    uint16_t onTimeToday = 0;          // Minutes
    
    // Maintenance
    uint32_t shotsSinceDescale = 0;
    uint32_t shotsSinceGroupClean = 0;
    uint32_t shotsSinceBackflush = 0;
    uint32_t lastDescaleTimestamp = 0;
    uint32_t lastGroupCleanTimestamp = 0;
    uint32_t lastBackflushTimestamp = 0;
    
    // Current session
    uint32_t sessionStartTimestamp = 0;
    uint16_t sessionShots = 0;
    
    void toJson(JsonObject& obj) const;
    bool fromJson(JsonObjectConst obj);
    
    void resetDaily();
    void recordMaintenance(const char* type);
};

// =============================================================================
// SHOT HISTORY - Ring buffer of recent shots
// =============================================================================

struct ShotRecord {
    uint32_t timestamp = 0;            // Unix timestamp
    float doseWeight = 0.0f;           // Input (g)
    float yieldWeight = 0.0f;          // Output (g)
    uint16_t durationMs = 0;           // Total time
    uint16_t preinfusionMs = 0;        // Pre-infusion time
    float avgFlowRate = 0.0f;          // g/s average
    float peakPressure = 0.0f;         // Max pressure (bar)
    float avgTemperature = 0.0f;       // Avg brew temp
    uint8_t rating = 0;                // User rating 0-5
    
    void toJson(JsonObject& obj) const;
    bool fromJson(JsonObjectConst obj);
    
    float ratio() const { return doseWeight > 0 ? yieldWeight / doseWeight : 0; }
};

// Maximum shots to store (balance memory vs history)
constexpr size_t MAX_SHOT_HISTORY = 50;

struct ShotHistory {
    ShotRecord shots[MAX_SHOT_HISTORY];
    uint8_t head = 0;                  // Next write position
    uint8_t count = 0;                 // Number of valid entries
    
    void addShot(const ShotRecord& shot);
    const ShotRecord* getShot(uint8_t index) const;  // 0 = most recent
    void toJson(JsonArray& arr) const;
    bool fromJson(JsonArrayConst arr);
    void clear();
};

// =============================================================================
// RUNTIME STATE - Volatile, not persisted
// =============================================================================

enum class MachineState : uint8_t {
    INIT = 0,
    IDLE,
    HEATING,
    READY,
    BREWING,
    STEAMING,
    COOLDOWN,
    ECO,
    FAULT
};

enum class MachineMode : uint8_t {
    STANDBY = 0,
    ON,
    ECO
};

struct RuntimeState {
    // Machine state
    MachineState state = MachineState::INIT;
    MachineMode mode = MachineMode::STANDBY;
    
    // Temperatures
    float brewTemp = 0.0f;
    float steamTemp = 0.0f;
    bool brewHeating = false;
    bool steamHeating = false;
    
    // Pressure & flow
    float pressure = 0.0f;
    float flowRate = 0.0f;
    
    // Power
    float powerWatts = 0.0f;
    float voltage = 0.0f;
    
    // Water
    uint8_t waterLevel = 100;          // 0-100%
    
    // Scale
    bool scaleConnected = false;
    float scaleWeight = 0.0f;
    float scaleFlowRate = 0.0f;
    bool scaleStable = false;
    
    // Active shot
    bool shotActive = false;
    uint32_t shotStartTime = 0;
    float shotWeight = 0.0f;
    
    // Connection status
    bool wifiConnected = false;
    bool mqttConnected = false;
    bool cloudConnected = false;
    bool picoConnected = false;
    
    // Pico firmware info
    char picoVersion[16] = {0};        // Version string "X.Y.Z"
    uint8_t picoResetReason = 0;       // 0=POR, 1=WDT, 2=SW, 3=DBG
    uint8_t machineType = 0;           // 0=unknown, 1=dual_boiler, 2=single_boiler, 3=heat_exchanger
    
    // Timestamps
    uint32_t lastUpdate = 0;
    uint32_t uptime = 0;
    
    void toJson(JsonObject& obj) const;
};

// Helper to convert enums to strings
const char* machineStateToString(MachineState state);
const char* machineModeToString(MachineMode mode);
MachineState stringToMachineState(const char* str);
MachineMode stringToMachineMode(const char* str);

} // namespace BrewOS

