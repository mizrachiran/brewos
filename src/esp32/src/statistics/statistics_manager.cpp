#include "statistics/statistics_manager.h"
#include "notifications/notification_manager.h"
#include <LittleFS.h>
#include <time.h>

namespace BrewOS {

// Maintenance thresholds for notifications
// Maintenance thresholds (Group Clean is now combined with Backflush)
constexpr uint32_t BACKFLUSH_WARNING_THRESHOLD = 80;   // Yellow at 80 shots
constexpr uint32_t BACKFLUSH_ALERT_THRESHOLD = 100;    // Red at 100 shots
constexpr uint32_t DESCALE_WARNING_THRESHOLD = 400;    // Yellow at 400 shots
constexpr uint32_t DESCALE_ALERT_THRESHOLD = 500;      // Red at 500 shots

// File paths
static const char* STATS_FILE = "/stats.json";
static const char* BREW_HISTORY_FILE = "/brew_history.json";
static const char* POWER_HISTORY_FILE = "/power_history.json";
static const char* DAILY_HISTORY_FILE = "/daily_history.json";

// =============================================================================
// JSON SERIALIZATION
// =============================================================================

void BrewRecord::toJson(JsonObject& obj) const {
    obj["timestamp"] = timestamp;
    obj["durationMs"] = durationMs;
    obj["yieldWeight"] = yieldWeight;
    obj["doseWeight"] = doseWeight;
    obj["peakPressure"] = peakPressure;
    obj["avgTemperature"] = avgTemperature;
    obj["avgFlowRate"] = avgFlowRate;
    obj["rating"] = rating;
    if (doseWeight > 0) {
        obj["ratio"] = ratio();
    }
}

bool BrewRecord::fromJson(JsonObjectConst obj) {
    if (obj["timestamp"].is<uint32_t>()) timestamp = obj["timestamp"];
    if (obj["durationMs"].is<uint16_t>()) durationMs = obj["durationMs"];
    if (obj["yieldWeight"].is<float>()) yieldWeight = obj["yieldWeight"];
    if (obj["doseWeight"].is<float>()) doseWeight = obj["doseWeight"];
    if (obj["peakPressure"].is<float>()) peakPressure = obj["peakPressure"];
    if (obj["avgTemperature"].is<float>()) avgTemperature = obj["avgTemperature"];
    if (obj["avgFlowRate"].is<float>()) avgFlowRate = obj["avgFlowRate"];
    if (obj["rating"].is<uint8_t>()) rating = obj["rating"];
    return timestamp > 0;
}

void PowerSample::toJson(JsonObject& obj) const {
    obj["timestamp"] = timestamp;
    obj["avgWatts"] = avgWatts;
    obj["maxWatts"] = maxWatts;
    obj["kwhConsumed"] = kwhConsumed;
}

bool PowerSample::fromJson(JsonObjectConst obj) {
    if (obj["timestamp"].is<uint32_t>()) timestamp = obj["timestamp"];
    if (obj["avgWatts"].is<float>()) avgWatts = obj["avgWatts"];
    if (obj["maxWatts"].is<float>()) maxWatts = obj["maxWatts"];
    if (obj["kwhConsumed"].is<float>()) kwhConsumed = obj["kwhConsumed"];
    return true;
}

void DailySummary::toJson(JsonObject& obj) const {
    obj["date"] = date;
    obj["shotCount"] = shotCount;
    obj["totalBrewTimeMs"] = totalBrewTimeMs;
    obj["totalKwh"] = totalKwh;
    obj["onTimeMinutes"] = onTimeMinutes;
    obj["steamCycles"] = steamCycles;
    obj["avgBrewTimeMs"] = avgBrewTimeMs;
}

bool DailySummary::fromJson(JsonObjectConst obj) {
    if (obj["date"].is<uint32_t>()) date = obj["date"];
    if (obj["shotCount"].is<uint16_t>()) shotCount = obj["shotCount"];
    if (obj["totalBrewTimeMs"].is<uint32_t>()) totalBrewTimeMs = obj["totalBrewTimeMs"];
    if (obj["totalKwh"].is<float>()) totalKwh = obj["totalKwh"];
    if (obj["onTimeMinutes"].is<uint16_t>()) onTimeMinutes = obj["onTimeMinutes"];
    if (obj["steamCycles"].is<uint8_t>()) steamCycles = obj["steamCycles"];
    if (obj["avgBrewTimeMs"].is<float>()) avgBrewTimeMs = obj["avgBrewTimeMs"];
    return true;
}

void PeriodStats::toJson(JsonObject& obj) const {
    obj["shotCount"] = shotCount;
    obj["totalBrewTimeMs"] = totalBrewTimeMs;
    obj["avgBrewTimeMs"] = avgBrewTimeMs;
    obj["minBrewTimeMs"] = minBrewTimeMs;
    obj["maxBrewTimeMs"] = maxBrewTimeMs;
    obj["totalKwh"] = totalKwh;
}

void LifetimeStats::toJson(JsonObject& obj) const {
    obj["totalShots"] = totalShots;
    obj["totalSteamCycles"] = totalSteamCycles;
    obj["totalKwh"] = totalKwh;
    obj["totalOnTimeMinutes"] = totalOnTimeMinutes;
    obj["totalBrewTimeMs"] = totalBrewTimeMs;
    obj["avgBrewTimeMs"] = avgBrewTimeMs;
    obj["minBrewTimeMs"] = minBrewTimeMs;
    obj["maxBrewTimeMs"] = maxBrewTimeMs;
    obj["firstShotTimestamp"] = firstShotTimestamp;
}

bool LifetimeStats::fromJson(JsonObjectConst obj) {
    if (obj["totalShots"].is<uint32_t>()) totalShots = obj["totalShots"];
    if (obj["totalSteamCycles"].is<uint32_t>()) totalSteamCycles = obj["totalSteamCycles"];
    if (obj["totalKwh"].is<float>()) totalKwh = obj["totalKwh"];
    if (obj["totalOnTimeMinutes"].is<uint32_t>()) totalOnTimeMinutes = obj["totalOnTimeMinutes"];
    if (obj["totalBrewTimeMs"].is<uint32_t>()) totalBrewTimeMs = obj["totalBrewTimeMs"];
    if (obj["avgBrewTimeMs"].is<float>()) avgBrewTimeMs = obj["avgBrewTimeMs"];
    if (obj["minBrewTimeMs"].is<float>()) minBrewTimeMs = obj["minBrewTimeMs"];
    if (obj["maxBrewTimeMs"].is<float>()) maxBrewTimeMs = obj["maxBrewTimeMs"];
    if (obj["firstShotTimestamp"].is<uint32_t>()) firstShotTimestamp = obj["firstShotTimestamp"];
    return true;
}

void MaintenanceStats::toJson(JsonObject& obj) const {
    obj["shotsSinceBackflush"] = shotsSinceBackflush;
    obj["shotsSinceGroupClean"] = shotsSinceGroupClean;
    obj["shotsSinceDescale"] = shotsSinceDescale;
    obj["lastBackflushTimestamp"] = lastBackflushTimestamp;
    obj["lastGroupCleanTimestamp"] = lastGroupCleanTimestamp;
    obj["lastDescaleTimestamp"] = lastDescaleTimestamp;
}

bool MaintenanceStats::fromJson(JsonObjectConst obj) {
    if (obj["shotsSinceBackflush"].is<uint32_t>()) shotsSinceBackflush = obj["shotsSinceBackflush"];
    if (obj["shotsSinceGroupClean"].is<uint32_t>()) shotsSinceGroupClean = obj["shotsSinceGroupClean"];
    if (obj["shotsSinceDescale"].is<uint32_t>()) shotsSinceDescale = obj["shotsSinceDescale"];
    if (obj["lastBackflushTimestamp"].is<uint32_t>()) lastBackflushTimestamp = obj["lastBackflushTimestamp"];
    if (obj["lastGroupCleanTimestamp"].is<uint32_t>()) lastGroupCleanTimestamp = obj["lastGroupCleanTimestamp"];
    if (obj["lastDescaleTimestamp"].is<uint32_t>()) lastDescaleTimestamp = obj["lastDescaleTimestamp"];
    return true;
}

void MaintenanceStats::recordMaintenance(const char* type, uint32_t timestamp) {
    if (strcmp(type, "backflush") == 0) {
        // Backflush includes group clean - reset both counters
        shotsSinceBackflush = 0;
        lastBackflushTimestamp = timestamp;
        shotsSinceGroupClean = 0;
        lastGroupCleanTimestamp = timestamp;
    } else if (strcmp(type, "descale") == 0) {
        shotsSinceDescale = 0;
        lastDescaleTimestamp = timestamp;
    }
}

void FullStatistics::toJson(JsonObject& obj) const {
    JsonObject lifetimeObj = obj["lifetime"].to<JsonObject>();
    lifetime.toJson(lifetimeObj);
    
    JsonObject dailyObj = obj["daily"].to<JsonObject>();
    daily.toJson(dailyObj);
    
    JsonObject weeklyObj = obj["weekly"].to<JsonObject>();
    weekly.toJson(weeklyObj);
    
    JsonObject monthlyObj = obj["monthly"].to<JsonObject>();
    monthly.toJson(monthlyObj);
    
    JsonObject maintObj = obj["maintenance"].to<JsonObject>();
    maintenance.toJson(maintObj);
    
    obj["sessionShots"] = sessionShots;
    obj["sessionStartTimestamp"] = sessionStartTimestamp;
}

// =============================================================================
// STATISTICS MANAGER IMPLEMENTATION
// =============================================================================

StatisticsManager& StatisticsManager::getInstance() {
    static StatisticsManager instance;
    return instance;
}

void StatisticsManager::begin() {
    Serial.println("[Stats] Initializing statistics manager...");
    
    loadFromFlash();
    
    // Initialize today tracking
    _todayStartTimestamp = getTodayMidnight();
    _lastPowerSampleTime = millis();
    _lastSaveTime = millis();
    
    Serial.printf("[Stats] Loaded: %lu total shots, %lu steam cycles, %.2f kWh\n",
                  _lifetime.totalShots, _lifetime.totalSteamCycles, _lifetime.totalKwh);
    Serial.printf("[Stats] Brew history: %d entries, Power samples: %d, Daily summaries: %d\n",
                  _brewHistoryCount, _powerSamplesCount, _dailySummariesCount);
}

void StatisticsManager::loop() {
    uint32_t now = millis();
    
    // Check for day change
    checkDayChange();
    
    // Power sampling every 5 minutes
    if (_machineIsOn && (now - _lastPowerSampleTime >= STATS_POWER_SAMPLE_INTERVAL)) {
        if (_powerSampleCount > 0) {
            PowerSample sample;
            sample.timestamp = time(nullptr);
            sample.avgWatts = _powerSampleSum / _powerSampleCount;
            sample.maxWatts = _powerSampleMax;
            
            // Calculate kWh for interval (watts * hours)
            float hoursElapsed = (float)STATS_POWER_SAMPLE_INTERVAL / 3600000.0f;
            sample.kwhConsumed = (sample.avgWatts / 1000.0f) * hoursElapsed;
            
            addPowerSample(sample);
            
            // Reset accumulators
            _powerSampleSum = 0;
            _powerSampleMax = 0;
            _powerSampleCount = 0;
        }
        _lastPowerSampleTime = now;
    }
    
    // Auto-save if dirty
    if (_dirty && (now - _lastSaveTime >= SAVE_INTERVAL)) {
        save();
    }
}

// =============================================================================
// BREW RECORDING
// =============================================================================

bool StatisticsManager::recordBrew(uint32_t durationMs, float yieldWeight, float doseWeight,
                                    float peakPressure, float avgTemp, float avgFlow) {
    // Validate duration - only record coffee brews (10-40s), filter out flushing/cleaning
    if (durationMs < STATS_MIN_BREW_TIME_MS || durationMs > STATS_MAX_BREW_TIME_MS) {
        Serial.printf("[Stats] Ignoring brew with invalid duration: %lu ms (valid range: %lu-%lu ms)\n", 
                      durationMs, STATS_MIN_BREW_TIME_MS, STATS_MAX_BREW_TIME_MS);
        return false;
    }
    
    uint32_t now = time(nullptr);
    
    // Create brew record
    BrewRecord record;
    record.timestamp = now;
    record.durationMs = durationMs;
    record.yieldWeight = yieldWeight;
    record.doseWeight = doseWeight;
    record.peakPressure = peakPressure;
    record.avgTemperature = avgTemp;
    record.avgFlowRate = avgFlow;
    
    addBrewRecord(record);
    
    // Update lifetime stats
    _lifetime.totalShots++;
    _lifetime.totalBrewTimeMs += durationMs;
    
    if (_lifetime.firstShotTimestamp == 0) {
        _lifetime.firstShotTimestamp = now;
    }
    
    // Update min/max
    if (_lifetime.totalShots == 1) {
        _lifetime.minBrewTimeMs = durationMs;
        _lifetime.maxBrewTimeMs = durationMs;
    } else {
        if (durationMs < _lifetime.minBrewTimeMs) _lifetime.minBrewTimeMs = durationMs;
        if (durationMs > _lifetime.maxBrewTimeMs) _lifetime.maxBrewTimeMs = durationMs;
    }
    
    // Recalculate average
    _lifetime.avgBrewTimeMs = (float)_lifetime.totalBrewTimeMs / _lifetime.totalShots;
    
    // Update maintenance counters
    _maintenance.shotsSinceBackflush++;
    _maintenance.shotsSinceGroupClean++;
    _maintenance.shotsSinceDescale++;
    
    // Update session
    _sessionShots++;
    
    _dirty = true;
    
    Serial.printf("[Stats] Recorded brew: %lu ms, total shots: %lu\n", durationMs, _lifetime.totalShots);
    
    // Check maintenance thresholds and trigger notifications
    checkMaintenanceThresholds();
    
    notifyChange();
    return true;
}

void StatisticsManager::recordSteamCycle() {
    _lifetime.totalSteamCycles++;
    _dirty = true;
    notifyChange();
}

void StatisticsManager::recordMaintenance(const char* type) {
    uint32_t now = time(nullptr);
    _maintenance.recordMaintenance(type, now);
    _dirty = true;
    save();  // Save immediately for maintenance events
    notifyChange();
    Serial.printf("[Stats] Recorded maintenance: %s\n", type);
}

void StatisticsManager::checkMaintenanceThresholds() {
    // Check backflush threshold (backflush + group clean every 100 shots)
    if (_maintenance.shotsSinceBackflush >= BACKFLUSH_ALERT_THRESHOLD) {
        notificationManager->backflushDue();
    }
    
    // Check descale threshold (every 500 shots)
    if (_maintenance.shotsSinceDescale >= DESCALE_ALERT_THRESHOLD) {
        uint32_t daysOverdue = 0;
        if (_maintenance.lastDescaleTimestamp > 0) {
            uint32_t now = time(nullptr);
            uint32_t daysSinceLast = (now - _maintenance.lastDescaleTimestamp) / 86400;
            // If it's been more than 60 days AND over threshold, calculate overdue
            if (daysSinceLast > 60) {
                daysOverdue = daysSinceLast - 60;
            }
        }
        notificationManager->descaleDue(daysOverdue);
    }
}

bool StatisticsManager::rateBrew(uint8_t index, uint8_t rating) {
    if (index >= _brewHistoryCount || rating > 5) return false;
    
    // Calculate actual index (0 = most recent)
    int actualIndex = (_brewHistoryHead - 1 - index + STATS_MAX_BREW_HISTORY) % STATS_MAX_BREW_HISTORY;
    _brewHistory[actualIndex].rating = rating;
    _dirty = true;
    return true;
}

// =============================================================================
// POWER TRACKING
// =============================================================================

void StatisticsManager::updatePower(float watts, bool isOn) {
    _currentWatts = watts;
    
    // Track machine on/off for on-time stats
    if (isOn && !_machineIsOn) {
        // Machine just turned on
        _machineOnStartTime = millis();
    } else if (!isOn && _machineIsOn) {
        // Machine just turned off - add on-time
        uint32_t onDurationMinutes = (millis() - _machineOnStartTime) / 60000;
        _lifetime.totalOnTimeMinutes += onDurationMinutes;
        _dirty = true;
    }
    _machineIsOn = isOn;
    
    if (isOn && watts > 0) {
        _powerSampleSum += watts;
        if (watts > _powerSampleMax) _powerSampleMax = watts;
        _powerSampleCount++;
        
        // Update total kWh (assuming this is called ~every 500ms)
        float kwhIncrement = (watts / 1000.0f) * (0.5f / 3600.0f);  // watts * hours
        _lifetime.totalKwh += kwhIncrement;
    }
}

// =============================================================================
// DATA ACCESS
// =============================================================================

void StatisticsManager::getFullStatistics(FullStatistics& stats) const {
    stats.lifetime = _lifetime;
    stats.maintenance = _maintenance;
    stats.sessionShots = _sessionShots;
    stats.sessionStartTimestamp = _sessionStartTimestamp;
    
    getDailyStats(stats.daily);
    getWeeklyStats(stats.weekly);
    getMonthlyStats(stats.monthly);
}

void StatisticsManager::getDailyStats(PeriodStats& stats) const {
    // Use cache if valid and same day
    time_t now = time(nullptr);
    if (now > 0) {
        struct tm* tm_now = localtime(&now);
        uint16_t currentDay = tm_now->tm_yday;
        
        if (!_statsCacheInvalid && _cachedDailyDay == currentDay && 
            _cachedDailyTimestamp > 0 && (now - _cachedDailyTimestamp) < 3600) {
            // Cache is valid (same day, less than 1 hour old)
            stats = _cachedDailyStats;
            return;
        }
    }
    
    // Recalculate and cache
    uint32_t dayAgo = now - 86400;  // 24 hours
    calculatePeriodStats(_cachedDailyStats, dayAgo);
    stats = _cachedDailyStats;
    _cachedDailyTimestamp = now;
    if (now > 0) {
        struct tm* tm_now = localtime(&now);
        _cachedDailyDay = tm_now->tm_yday;
    }
    _statsCacheInvalid = false;  // Mark cache as valid
}

void StatisticsManager::getWeeklyStats(PeriodStats& stats) const {
    // Use cache if valid (less than 1 hour old)
    time_t now = time(nullptr);
    if (!_statsCacheInvalid && _cachedWeeklyTimestamp > 0 && 
        (now - _cachedWeeklyTimestamp) < 3600) {
        stats = _cachedWeeklyStats;
        return;
    }
    
    // Recalculate and cache
    uint32_t weekAgo = now - (7 * 86400);  // 7 days
    calculatePeriodStats(_cachedWeeklyStats, weekAgo);
    stats = _cachedWeeklyStats;
    _cachedWeeklyTimestamp = now;
    _statsCacheInvalid = false;  // Mark cache as valid
}

void StatisticsManager::getMonthlyStats(PeriodStats& stats) const {
    // Use cache if valid (less than 1 hour old)
    time_t now = time(nullptr);
    if (!_statsCacheInvalid && _cachedMonthlyTimestamp > 0 && 
        (now - _cachedMonthlyTimestamp) < 3600) {
        stats = _cachedMonthlyStats;
        return;
    }
    
    // Recalculate and cache
    uint32_t monthAgo = now - (30 * 86400);  // 30 days
    calculatePeriodStats(_cachedMonthlyStats, monthAgo);
    stats = _cachedMonthlyStats;
    _cachedMonthlyTimestamp = now;
    _statsCacheInvalid = false;  // Mark cache as valid
}

void StatisticsManager::calculatePeriodStats(PeriodStats& stats, uint32_t startTimestamp) const {
    stats = PeriodStats();  // Reset
    
    bool first = true;
    for (size_t i = 0; i < _brewHistoryCount; i++) {
        int idx = (_brewHistoryHead - 1 - i + STATS_MAX_BREW_HISTORY) % STATS_MAX_BREW_HISTORY;
        const BrewRecord& record = _brewHistory[idx];
        
        if (record.timestamp >= startTimestamp) {
            stats.shotCount++;
            stats.totalBrewTimeMs += record.durationMs;
            
            if (first || record.durationMs < stats.minBrewTimeMs) {
                stats.minBrewTimeMs = record.durationMs;
            }
            if (first || record.durationMs > stats.maxBrewTimeMs) {
                stats.maxBrewTimeMs = record.durationMs;
            }
            first = false;
        }
    }
    
    if (stats.shotCount > 0) {
        stats.avgBrewTimeMs = (float)stats.totalBrewTimeMs / stats.shotCount;
    }
    
    // Calculate kWh from power samples
    for (size_t i = 0; i < _powerSamplesCount; i++) {
        int idx = (_powerSamplesHead - 1 - i + STATS_MAX_POWER_SAMPLES) % STATS_MAX_POWER_SAMPLES;
        const PowerSample& sample = _powerSamples[idx];
        
        if (sample.timestamp >= startTimestamp) {
            stats.totalKwh += sample.kwhConsumed;
        }
    }
}

void StatisticsManager::getBrewHistory(JsonArray& arr, size_t maxEntries) const {
    size_t count = min(maxEntries, (size_t)_brewHistoryCount);
    for (size_t i = 0; i < count; i++) {
        int idx = (_brewHistoryHead - 1 - i + STATS_MAX_BREW_HISTORY) % STATS_MAX_BREW_HISTORY;
        JsonObject obj = arr.add<JsonObject>();
        _brewHistory[idx].toJson(obj);
    }
}

void StatisticsManager::getPowerHistory(JsonArray& arr) const {
    for (size_t i = 0; i < _powerSamplesCount; i++) {
        // Return in chronological order (oldest first)
        int idx = (_powerSamplesHead - _powerSamplesCount + i + STATS_MAX_POWER_SAMPLES) % STATS_MAX_POWER_SAMPLES;
        JsonObject obj = arr.add<JsonObject>();
        _powerSamples[idx].toJson(obj);
    }
}

void StatisticsManager::getDailyHistory(JsonArray& arr) const {
    for (size_t i = 0; i < _dailySummariesCount; i++) {
        // Return in chronological order (oldest first)
        int idx = (_dailySummariesHead - _dailySummariesCount + i + STATS_MAX_DAILY_HISTORY) % STATS_MAX_DAILY_HISTORY;
        JsonObject obj = arr.add<JsonObject>();
        _dailySummaries[idx].toJson(obj);
    }
}

void StatisticsManager::getWeeklyBrewChart(JsonArray& arr) const {
    uint32_t now = time(nullptr);
    
    // Get shots per day for last 7 days
    struct tm* tm_now = localtime((time_t*)&now);
    int currentDayOfWeek = tm_now->tm_wday;  // 0 = Sunday
    
    const char* dayNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    
    for (int d = 6; d >= 0; d--) {
        uint32_t dayStart = now - (d * 86400);
        dayStart = dayStart - (dayStart % 86400);  // Round to midnight
        uint32_t dayEnd = dayStart + 86400;
        
        int dayOfWeek = (currentDayOfWeek - d + 7) % 7;
        
        int shotCount = 0;
        for (size_t i = 0; i < _brewHistoryCount; i++) {
            int idx = (_brewHistoryHead - 1 - i + STATS_MAX_BREW_HISTORY) % STATS_MAX_BREW_HISTORY;
            const BrewRecord& record = _brewHistory[idx];
            
            if (record.timestamp >= dayStart && record.timestamp < dayEnd) {
                shotCount++;
            }
        }
        
        JsonObject obj = arr.add<JsonObject>();
        obj["day"] = dayNames[dayOfWeek];
        obj["shots"] = shotCount;
    }
}

void StatisticsManager::getHourlyDistribution(JsonArray& arr) const {
    // Count shots by hour (all time)
    int hourCounts[24] = {0};
    
    for (size_t i = 0; i < _brewHistoryCount; i++) {
        int idx = (_brewHistoryHead - 1 - i + STATS_MAX_BREW_HISTORY) % STATS_MAX_BREW_HISTORY;
        const BrewRecord& record = _brewHistory[idx];
        
        if (record.timestamp > 0) {
            struct tm* tm_record = localtime((time_t*)&record.timestamp);
            hourCounts[tm_record->tm_hour]++;
        }
    }
    
    for (int h = 0; h < 24; h++) {
        JsonObject obj = arr.add<JsonObject>();
        obj["hour"] = h;
        obj["count"] = hourCounts[h];
    }
}

// =============================================================================
// PERSISTENCE
// =============================================================================

void StatisticsManager::loadFromFlash() {
    // Load lifetime stats
    if (LittleFS.exists(STATS_FILE)) {
        File file = LittleFS.open(STATS_FILE, "r");
        if (file) {
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, file);
            file.close();
            
            if (!error) {
                JsonObject lifetimeObj = doc["lifetime"];
                if (!lifetimeObj.isNull()) {
                    _lifetime.fromJson(lifetimeObj);
                }
                
                JsonObject maintObj = doc["maintenance"];
                if (!maintObj.isNull()) {
                    _maintenance.fromJson(maintObj);
                }
            }
        }
    }
    
    // Load brew history
    if (LittleFS.exists(BREW_HISTORY_FILE)) {
        File file = LittleFS.open(BREW_HISTORY_FILE, "r");
        if (file) {
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, file);
            file.close();
            
            if (!error) {
                JsonArray arr = doc.as<JsonArray>();
                _brewHistoryCount = 0;
                _brewHistoryHead = 0;
                
                for (JsonObject obj : arr) {
                    if (_brewHistoryCount < STATS_MAX_BREW_HISTORY) {
                        _brewHistory[_brewHistoryHead].fromJson(obj);
                        _brewHistoryHead = (_brewHistoryHead + 1) % STATS_MAX_BREW_HISTORY;
                        _brewHistoryCount++;
                    }
                }
            }
        }
    }
    
    // Load power history
    if (LittleFS.exists(POWER_HISTORY_FILE)) {
        File file = LittleFS.open(POWER_HISTORY_FILE, "r");
        if (file) {
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, file);
            file.close();
            
            if (!error) {
                JsonArray arr = doc.as<JsonArray>();
                _powerSamplesCount = 0;
                _powerSamplesHead = 0;
                
                for (JsonObject obj : arr) {
                    if (_powerSamplesCount < STATS_MAX_POWER_SAMPLES) {
                        _powerSamples[_powerSamplesHead].fromJson(obj);
                        _powerSamplesHead = (_powerSamplesHead + 1) % STATS_MAX_POWER_SAMPLES;
                        _powerSamplesCount++;
                    }
                }
            }
        }
    }
    
    // Load daily summaries
    if (LittleFS.exists(DAILY_HISTORY_FILE)) {
        File file = LittleFS.open(DAILY_HISTORY_FILE, "r");
        if (file) {
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, file);
            file.close();
            
            if (!error) {
                JsonArray arr = doc.as<JsonArray>();
                _dailySummariesCount = 0;
                _dailySummariesHead = 0;
                
                for (JsonObject obj : arr) {
                    if (_dailySummariesCount < STATS_MAX_DAILY_HISTORY) {
                        _dailySummaries[_dailySummariesHead].fromJson(obj);
                        _dailySummariesHead = (_dailySummariesHead + 1) % STATS_MAX_DAILY_HISTORY;
                        _dailySummariesCount++;
                    }
                }
            }
        }
    }
}

void StatisticsManager::saveToFlash() {
    // Yield before starting expensive file operations
    yield();
    
    // Save lifetime stats
    {
        JsonDocument doc;
        JsonObject lifetimeObj = doc["lifetime"].to<JsonObject>();
        _lifetime.toJson(lifetimeObj);
        
        JsonObject maintObj = doc["maintenance"].to<JsonObject>();
        _maintenance.toJson(maintObj);
        
        yield(); // Yield before file write
        
        File file = LittleFS.open(STATS_FILE, "w");
        if (file) {
            serializeJson(doc, file);
            file.close();
        }
        yield(); // Yield after file write
    }
    
    // Save brew history
    {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        
        // Yield periodically during large array building
        for (size_t i = 0; i < _brewHistoryCount; i++) {
            if (i > 0 && (i % 50 == 0)) {
                yield(); // Yield every 50 items to prevent blocking
            }
            int idx = (_brewHistoryHead - _brewHistoryCount + i + STATS_MAX_BREW_HISTORY) % STATS_MAX_BREW_HISTORY;
            JsonObject obj = arr.add<JsonObject>();
            _brewHistory[idx].toJson(obj);
        }
        
        yield(); // Yield before file write
        
        File file = LittleFS.open(BREW_HISTORY_FILE, "w");
        if (file) {
            serializeJson(doc, file);
            file.close();
        }
        yield(); // Yield after file write
    }
    
    // Save power history
    {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        
        // Yield periodically during large array building
        for (size_t i = 0; i < _powerSamplesCount; i++) {
            if (i > 0 && (i % 100 == 0)) {
                yield(); // Yield every 100 items to prevent blocking
            }
            int idx = (_powerSamplesHead - _powerSamplesCount + i + STATS_MAX_POWER_SAMPLES) % STATS_MAX_POWER_SAMPLES;
            JsonObject obj = arr.add<JsonObject>();
            _powerSamples[idx].toJson(obj);
        }
        
        yield(); // Yield before file write
        
        File file = LittleFS.open(POWER_HISTORY_FILE, "w");
        if (file) {
            serializeJson(doc, file);
            file.close();
        }
        yield(); // Yield after file write
    }
    
    // Save daily summaries
    {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        
        // Yield periodically during array building
        for (size_t i = 0; i < _dailySummariesCount; i++) {
            if (i > 0 && (i % 30 == 0)) {
                yield(); // Yield every 30 items to prevent blocking
            }
            int idx = (_dailySummariesHead - _dailySummariesCount + i + STATS_MAX_DAILY_HISTORY) % STATS_MAX_DAILY_HISTORY;
            JsonObject obj = arr.add<JsonObject>();
            _dailySummaries[idx].toJson(obj);
        }
        
        yield(); // Yield before file write
        
        File file = LittleFS.open(DAILY_HISTORY_FILE, "w");
        if (file) {
            serializeJson(doc, file);
            file.close();
        }
        yield(); // Yield after file write
    }
    
    Serial.println("[Stats] Saved to flash");
}

void StatisticsManager::save() {
    saveToFlash();
    _dirty = false;
    _lastSaveTime = millis();
}

void StatisticsManager::resetAll() {
    _lifetime = LifetimeStats();
    _maintenance = MaintenanceStats();
    _brewHistoryCount = 0;
    _brewHistoryHead = 0;
    _powerSamplesCount = 0;
    _powerSamplesHead = 0;
    _dailySummariesCount = 0;
    _dailySummariesHead = 0;
    _sessionShots = 0;
    
    // Delete files
    LittleFS.remove(STATS_FILE);
    LittleFS.remove(BREW_HISTORY_FILE);
    LittleFS.remove(POWER_HISTORY_FILE);
    LittleFS.remove(DAILY_HISTORY_FILE);
    
    Serial.println("[Stats] All statistics reset");
    notifyChange();
}

// =============================================================================
// SESSION MANAGEMENT
// =============================================================================

void StatisticsManager::startSession() {
    _sessionShots = 0;
    _sessionStartTimestamp = time(nullptr);
    _machineOnStartTime = millis();
    _machineIsOn = true;
}

void StatisticsManager::endSession() {
    // Finalize on-time
    if (_machineIsOn) {
        uint32_t onDurationMinutes = (millis() - _machineOnStartTime) / 60000;
        _lifetime.totalOnTimeMinutes += onDurationMinutes;
        _machineIsOn = false;
    }
    
    // Force save current power sample
    if (_powerSampleCount > 0) {
        PowerSample sample;
        sample.timestamp = time(nullptr);
        sample.avgWatts = _powerSampleSum / _powerSampleCount;
        sample.maxWatts = _powerSampleMax;
        float hoursElapsed = (float)(millis() - _lastPowerSampleTime) / 3600000.0f;
        sample.kwhConsumed = (sample.avgWatts / 1000.0f) * hoursElapsed;
        addPowerSample(sample);
    }
    
    save();
}

// =============================================================================
// HELPERS
// =============================================================================

void StatisticsManager::addBrewRecord(const BrewRecord& record) {
    _brewHistory[_brewHistoryHead] = record;
    _brewHistoryHead = (_brewHistoryHead + 1) % STATS_MAX_BREW_HISTORY;
    if (_brewHistoryCount < STATS_MAX_BREW_HISTORY) {
        _brewHistoryCount++;
    }
    // Invalidate stats cache when new brew is recorded
    _statsCacheInvalid = true;
}

void StatisticsManager::addPowerSample(const PowerSample& sample) {
    _powerSamples[_powerSamplesHead] = sample;
    _powerSamplesHead = (_powerSamplesHead + 1) % STATS_MAX_POWER_SAMPLES;
    if (_powerSamplesCount < STATS_MAX_POWER_SAMPLES) {
        _powerSamplesCount++;
    }
    // Invalidate stats cache when new power sample is added
    _statsCacheInvalid = true;
}

void StatisticsManager::addDailySummary(const DailySummary& summary) {
    _dailySummaries[_dailySummariesHead] = summary;
    _dailySummariesHead = (_dailySummariesHead + 1) % STATS_MAX_DAILY_HISTORY;
    if (_dailySummariesCount < STATS_MAX_DAILY_HISTORY) {
        _dailySummariesCount++;
    }
}

uint32_t StatisticsManager::getTodayMidnight() const {
    uint32_t now = time(nullptr);
    struct tm* tm_now = localtime((time_t*)&now);
    tm_now->tm_hour = 0;
    tm_now->tm_min = 0;
    tm_now->tm_sec = 0;
    return mktime(tm_now);
}

void StatisticsManager::checkDayChange() {
    uint32_t todayMidnight = getTodayMidnight();
    
    if (todayMidnight > _todayStartTimestamp && _todayStartTimestamp > 0) {
        // Day has changed - save yesterday's summary
        saveDailySummary();
        _todayStartTimestamp = todayMidnight;
        // Invalidate stats cache when day changes
        _statsCacheInvalid = true;
    }
}

void StatisticsManager::saveDailySummary() {
    // Calculate yesterday's stats from brew history
    uint32_t yesterdayStart = _todayStartTimestamp;
    uint32_t yesterdayEnd = yesterdayStart + 86400;
    
    DailySummary summary;
    summary.date = yesterdayStart;
    
    for (size_t i = 0; i < _brewHistoryCount; i++) {
        int idx = (_brewHistoryHead - 1 - i + STATS_MAX_BREW_HISTORY) % STATS_MAX_BREW_HISTORY;
        const BrewRecord& record = _brewHistory[idx];
        
        if (record.timestamp >= yesterdayStart && record.timestamp < yesterdayEnd) {
            summary.shotCount++;
            summary.totalBrewTimeMs += record.durationMs;
        }
    }
    
    if (summary.shotCount > 0) {
        summary.avgBrewTimeMs = (float)summary.totalBrewTimeMs / summary.shotCount;
    }
    
    // Get kWh from power samples
    for (size_t i = 0; i < _powerSamplesCount; i++) {
        int idx = (_powerSamplesHead - 1 - i + STATS_MAX_POWER_SAMPLES) % STATS_MAX_POWER_SAMPLES;
        const PowerSample& sample = _powerSamples[idx];
        
        if (sample.timestamp >= yesterdayStart && sample.timestamp < yesterdayEnd) {
            summary.totalKwh += sample.kwhConsumed;
        }
    }
    
    addDailySummary(summary);
    _dirty = true;
}

void StatisticsManager::onStatsChanged(StatsCallback callback) {
    _onStatsChanged = callback;
}

void StatisticsManager::notifyChange() {
    if (_onStatsChanged) {
        FullStatistics stats;
        getFullStatistics(stats);
        _onStatsChanged(stats);
    }
}

} // namespace BrewOS

