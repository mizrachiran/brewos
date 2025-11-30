#include "state/state_manager.h"
#include <LittleFS.h>
#include <time.h>

namespace BrewOS {

// NVS namespace names (max 15 chars)
static const char* NVS_SETTINGS = "settings";
static const char* NVS_STATS = "stats";

// LittleFS paths
static const char* SHOT_HISTORY_FILE = "/shot_history.json";

StateManager& StateManager::getInstance() {
    static StateManager instance;
    return instance;
}

void StateManager::begin() {
    Serial.println("[State] Initializing...");
    
    // Load persisted data
    loadSettings();
    loadStats();
    loadShotHistory();
    
    // Initialize session
    _stats.sessionStartTimestamp = time(nullptr);
    _stats.sessionShots = 0;
    _state.uptime = millis();
    
    Serial.printf("[State] Loaded %d settings, %lu total shots, %d history entries\n",
        1, _stats.totalShots, _shotHistory.count);
}

void StateManager::loop() {
    // Update uptime
    _state.uptime = millis();
    
    // Periodic stats save
    if (millis() - _lastStatsSave > STATS_SAVE_INTERVAL) {
        _lastStatsSave = millis();
        saveStats();
    }
    
    // Daily reset check
    checkDailyReset();
}

// =============================================================================
// SETTINGS PERSISTENCE
// =============================================================================

void StateManager::loadSettings() {
    _prefs.begin(NVS_SETTINGS, true);  // Read-only
    
    // Temperature
    _settings.temperature.brewSetpoint = _prefs.getFloat("brewSP", 93.5f);
    _settings.temperature.steamSetpoint = _prefs.getFloat("steamSP", 145.0f);
    _settings.temperature.brewOffset = _prefs.getFloat("brewOff", 0.0f);
    _settings.temperature.steamOffset = _prefs.getFloat("steamOff", 0.0f);
    _settings.temperature.ecoBrewTemp = _prefs.getFloat("ecoTemp", 80.0f);
    _settings.temperature.ecoTimeoutMinutes = _prefs.getUShort("ecoTimeout", 30);
    
    // Brew
    _settings.brew.bbwEnabled = _prefs.getBool("bbwEnabled", false);
    _settings.brew.doseWeight = _prefs.getFloat("doseWt", 18.0f);
    _settings.brew.targetWeight = _prefs.getFloat("targetWt", 36.0f);
    _settings.brew.stopOffset = _prefs.getFloat("stopOff", 2.0f);
    _settings.brew.autoTare = _prefs.getBool("autoTare", true);
    _settings.brew.preinfusionTime = _prefs.getFloat("preinf", 0.0f);
    _settings.brew.preinfusionPressure = _prefs.getFloat("preinfP", 2.0f);
    
    // Power
    _settings.power.mainsVoltage = _prefs.getUShort("voltage", 220);
    _settings.power.maxCurrent = _prefs.getFloat("maxCurr", 13.0f);
    _settings.power.powerOnBoot = _prefs.getBool("pwrBoot", false);
    
    // Network
    _prefs.getString("wifiSsid", _settings.network.wifiSsid, sizeof(_settings.network.wifiSsid));
    _prefs.getString("wifiPass", _settings.network.wifiPassword, sizeof(_settings.network.wifiPassword));
    _settings.network.wifiConfigured = _prefs.getBool("wifiCfg", false);
    _prefs.getString("hostname", _settings.network.hostname, sizeof(_settings.network.hostname));
    if (strlen(_settings.network.hostname) == 0) {
        strcpy(_settings.network.hostname, "brewos");
    }
    
    // MQTT
    _settings.mqtt.enabled = _prefs.getBool("mqttEn", false);
    _prefs.getString("mqttBrk", _settings.mqtt.broker, sizeof(_settings.mqtt.broker));
    _settings.mqtt.port = _prefs.getUShort("mqttPort", 1883);
    _prefs.getString("mqttUser", _settings.mqtt.username, sizeof(_settings.mqtt.username));
    _prefs.getString("mqttPass", _settings.mqtt.password, sizeof(_settings.mqtt.password));
    _prefs.getString("mqttTopic", _settings.mqtt.baseTopic, sizeof(_settings.mqtt.baseTopic));
    if (strlen(_settings.mqtt.baseTopic) == 0) {
        strcpy(_settings.mqtt.baseTopic, "brewos");
    }
    _settings.mqtt.discovery = _prefs.getBool("mqttDisc", true);
    
    // Cloud
    _settings.cloud.enabled = _prefs.getBool("cloudEn", false);
    _prefs.getString("cloudUrl", _settings.cloud.serverUrl, sizeof(_settings.cloud.serverUrl));
    _prefs.getString("devId", _settings.cloud.deviceId, sizeof(_settings.cloud.deviceId));
    _prefs.getString("devKey", _settings.cloud.deviceKey, sizeof(_settings.cloud.deviceKey));
    
    // Scale
    _settings.scale.enabled = _prefs.getBool("scaleEn", true);
    _prefs.getString("scaleAddr", _settings.scale.pairedAddress, sizeof(_settings.scale.pairedAddress));
    _prefs.getString("scaleName", _settings.scale.pairedName, sizeof(_settings.scale.pairedName));
    _settings.scale.scaleType = _prefs.getUChar("scaleType", 0);
    
    // Display
    _settings.display.brightness = _prefs.getUChar("dispBri", 200);
    _settings.display.screenTimeout = _prefs.getUChar("dispTO", 30);
    _settings.display.showShotTimer = _prefs.getBool("showTimer", true);
    _settings.display.showWeight = _prefs.getBool("showWt", true);
    _settings.display.showPressure = _prefs.getBool("showPres", true);
    
    _prefs.end();
}

void StateManager::saveSettings() {
    saveTemperatureSettings();
    saveBrewSettings();
    savePowerSettings();
    saveNetworkSettings();
    saveMQTTSettings();
    saveCloudSettings();
    saveScaleSettings();
    saveDisplaySettings();
}

void StateManager::saveTemperatureSettings() {
    _prefs.begin(NVS_SETTINGS, false);
    _prefs.putFloat("brewSP", _settings.temperature.brewSetpoint);
    _prefs.putFloat("steamSP", _settings.temperature.steamSetpoint);
    _prefs.putFloat("brewOff", _settings.temperature.brewOffset);
    _prefs.putFloat("steamOff", _settings.temperature.steamOffset);
    _prefs.putFloat("ecoTemp", _settings.temperature.ecoBrewTemp);
    _prefs.putUShort("ecoTimeout", _settings.temperature.ecoTimeoutMinutes);
    _prefs.end();
    notifySettingsChanged();
}

void StateManager::saveBrewSettings() {
    _prefs.begin(NVS_SETTINGS, false);
    _prefs.putBool("bbwEnabled", _settings.brew.bbwEnabled);
    _prefs.putFloat("doseWt", _settings.brew.doseWeight);
    _prefs.putFloat("targetWt", _settings.brew.targetWeight);
    _prefs.putFloat("stopOff", _settings.brew.stopOffset);
    _prefs.putBool("autoTare", _settings.brew.autoTare);
    _prefs.putFloat("preinf", _settings.brew.preinfusionTime);
    _prefs.putFloat("preinfP", _settings.brew.preinfusionPressure);
    _prefs.end();
    notifySettingsChanged();
}

void StateManager::savePowerSettings() {
    _prefs.begin(NVS_SETTINGS, false);
    _prefs.putUShort("voltage", _settings.power.mainsVoltage);
    _prefs.putFloat("maxCurr", _settings.power.maxCurrent);
    _prefs.putBool("pwrBoot", _settings.power.powerOnBoot);
    _prefs.end();
    notifySettingsChanged();
}

void StateManager::saveNetworkSettings() {
    _prefs.begin(NVS_SETTINGS, false);
    _prefs.putString("wifiSsid", _settings.network.wifiSsid);
    _prefs.putString("wifiPass", _settings.network.wifiPassword);
    _prefs.putBool("wifiCfg", _settings.network.wifiConfigured);
    _prefs.putString("hostname", _settings.network.hostname);
    _prefs.end();
    notifySettingsChanged();
}

void StateManager::saveMQTTSettings() {
    _prefs.begin(NVS_SETTINGS, false);
    _prefs.putBool("mqttEn", _settings.mqtt.enabled);
    _prefs.putString("mqttBrk", _settings.mqtt.broker);
    _prefs.putUShort("mqttPort", _settings.mqtt.port);
    _prefs.putString("mqttUser", _settings.mqtt.username);
    _prefs.putString("mqttPass", _settings.mqtt.password);
    _prefs.putString("mqttTopic", _settings.mqtt.baseTopic);
    _prefs.putBool("mqttDisc", _settings.mqtt.discovery);
    _prefs.end();
    notifySettingsChanged();
}

void StateManager::saveCloudSettings() {
    _prefs.begin(NVS_SETTINGS, false);
    _prefs.putBool("cloudEn", _settings.cloud.enabled);
    _prefs.putString("cloudUrl", _settings.cloud.serverUrl);
    _prefs.putString("devId", _settings.cloud.deviceId);
    _prefs.putString("devKey", _settings.cloud.deviceKey);
    _prefs.end();
    notifySettingsChanged();
}

void StateManager::saveScaleSettings() {
    _prefs.begin(NVS_SETTINGS, false);
    _prefs.putBool("scaleEn", _settings.scale.enabled);
    _prefs.putString("scaleAddr", _settings.scale.pairedAddress);
    _prefs.putString("scaleName", _settings.scale.pairedName);
    _prefs.putUChar("scaleType", _settings.scale.scaleType);
    _prefs.end();
    notifySettingsChanged();
}

void StateManager::saveDisplaySettings() {
    _prefs.begin(NVS_SETTINGS, false);
    _prefs.putUChar("dispBri", _settings.display.brightness);
    _prefs.putUChar("dispTO", _settings.display.screenTimeout);
    _prefs.putBool("showTimer", _settings.display.showShotTimer);
    _prefs.putBool("showWt", _settings.display.showWeight);
    _prefs.putBool("showPres", _settings.display.showPressure);
    _prefs.end();
    notifySettingsChanged();
}

void StateManager::resetSettings() {
    _settings = Settings();  // Reset to defaults
    saveSettings();
}

void StateManager::factoryReset() {
    _prefs.begin(NVS_SETTINGS, false);
    _prefs.clear();
    _prefs.end();
    
    _prefs.begin(NVS_STATS, false);
    _prefs.clear();
    _prefs.end();
    
    LittleFS.remove(SHOT_HISTORY_FILE);
    
    _settings = Settings();
    _stats = Statistics();
    _shotHistory.clear();
    
    Serial.println("[State] Factory reset complete");
}

// =============================================================================
// STATISTICS PERSISTENCE
// =============================================================================

void StateManager::loadStats() {
    _prefs.begin(NVS_STATS, true);
    
    _stats.totalShots = _prefs.getULong("totalShots", 0);
    _stats.totalSteamCycles = _prefs.getULong("totalSteam", 0);
    _stats.totalKwh = _prefs.getFloat("totalKwh", 0.0f);
    _stats.totalOnTimeMinutes = _prefs.getULong("totalOn", 0);
    
    _stats.shotsSinceDescale = _prefs.getULong("sinceDesc", 0);
    _stats.shotsSinceGroupClean = _prefs.getULong("sinceGrp", 0);
    _stats.shotsSinceBackflush = _prefs.getULong("sinceBack", 0);
    _stats.lastDescaleTimestamp = _prefs.getULong("lastDesc", 0);
    _stats.lastGroupCleanTimestamp = _prefs.getULong("lastGrp", 0);
    _stats.lastBackflushTimestamp = _prefs.getULong("lastBack", 0);
    
    _prefs.end();
}

void StateManager::saveStats() {
    _prefs.begin(NVS_STATS, false);
    
    _prefs.putULong("totalShots", _stats.totalShots);
    _prefs.putULong("totalSteam", _stats.totalSteamCycles);
    _prefs.putFloat("totalKwh", _stats.totalKwh);
    _prefs.putULong("totalOn", _stats.totalOnTimeMinutes);
    
    _prefs.putULong("sinceDesc", _stats.shotsSinceDescale);
    _prefs.putULong("sinceGrp", _stats.shotsSinceGroupClean);
    _prefs.putULong("sinceBack", _stats.shotsSinceBackflush);
    _prefs.putULong("lastDesc", _stats.lastDescaleTimestamp);
    _prefs.putULong("lastGrp", _stats.lastGroupCleanTimestamp);
    _prefs.putULong("lastBack", _stats.lastBackflushTimestamp);
    
    _prefs.end();
    
    Serial.println("[State] Stats saved");
}

void StateManager::recordShot() {
    _stats.totalShots++;
    _stats.shotsToday++;
    _stats.sessionShots++;
    _stats.shotsSinceDescale++;
    _stats.shotsSinceGroupClean++;
    _stats.shotsSinceBackflush++;
    notifyStatsChanged();
}

void StateManager::recordSteamCycle() {
    _stats.totalSteamCycles++;
    notifyStatsChanged();
}

void StateManager::addPowerUsage(float kwh) {
    _stats.totalKwh += kwh;
    _stats.kwhToday += kwh;
    notifyStatsChanged();
}

void StateManager::recordMaintenance(const char* type) {
    _stats.recordMaintenance(type);
    saveStats();
    notifyStatsChanged();
}

// =============================================================================
// SHOT HISTORY PERSISTENCE
// =============================================================================

void StateManager::loadShotHistory() {
    if (!LittleFS.exists(SHOT_HISTORY_FILE)) {
        Serial.println("[State] No shot history file");
        return;
    }
    
    File file = LittleFS.open(SHOT_HISTORY_FILE, "r");
    if (!file) {
        Serial.println("[State] Failed to open shot history");
        return;
    }
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        Serial.printf("[State] Shot history parse error: %s\n", error.c_str());
        return;
    }
    
    JsonArray arr = doc.as<JsonArray>();
    _shotHistory.fromJson(arr);
}

void StateManager::saveShotHistory() {
    File file = LittleFS.open(SHOT_HISTORY_FILE, "w");
    if (!file) {
        Serial.println("[State] Failed to write shot history");
        return;
    }
    
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    _shotHistory.toJson(arr);
    
    serializeJson(doc, file);
    file.close();
    
    Serial.printf("[State] Shot history saved (%d entries)\n", _shotHistory.count);
}

void StateManager::addShotRecord(const ShotRecord& shot) {
    _shotHistory.addShot(shot);
    saveShotHistory();
    recordShot();
    
    if (_onShotCompleted) {
        _onShotCompleted(shot);
    }
}

void StateManager::rateShot(uint8_t index, uint8_t rating) {
    // Not directly modifiable in ring buffer, would need to search
    // For simplicity, skip implementation or use different data structure
}

void StateManager::clearShotHistory() {
    _shotHistory.clear();
    LittleFS.remove(SHOT_HISTORY_FILE);
}

// =============================================================================
// RUNTIME STATE
// =============================================================================

void StateManager::setMachineState(MachineState newState) {
    if (_state.state != newState) {
        _state.state = newState;
        notifyStateChanged();
    }
}

void StateManager::setMachineMode(MachineMode newMode) {
    if (_state.mode != newMode) {
        _state.mode = newMode;
        notifyStateChanged();
    }
}

void StateManager::updateTemperatures(float brew, float steam) {
    _state.brewTemp = brew;
    _state.steamTemp = steam;
    
    // Track during shot
    if (_state.shotActive) {
        _shotTempSum += brew;
        _shotTempCount++;
    }
}

void StateManager::updatePressure(float pressure) {
    _state.pressure = pressure;
    
    // Track peak during shot
    if (_state.shotActive && pressure > _shotPeakPressure) {
        _shotPeakPressure = pressure;
    }
}

void StateManager::updatePower(float watts, float voltage) {
    _state.powerWatts = watts;
    _state.voltage = voltage;
}

void StateManager::updateScale(float weight, float flowRate, bool stable) {
    _state.scaleWeight = weight;
    _state.scaleFlowRate = flowRate;
    _state.scaleStable = stable;
    
    // Update shot weight
    if (_state.shotActive) {
        _state.shotWeight = weight;
    }
}

void StateManager::startShot() {
    _state.shotActive = true;
    _state.shotStartTime = millis();
    _state.shotWeight = 0;
    
    // Reset shot tracking
    _currentShot = ShotRecord();
    _currentShot.timestamp = time(nullptr);
    _currentShot.doseWeight = _state.scaleWeight;  // Capture pre-shot weight
    _shotPeakPressure = 0;
    _shotTempSum = 0;
    _shotTempCount = 0;
    
    setMachineState(MachineState::BREWING);
    notifyStateChanged();
}

void StateManager::endShot() {
    if (!_state.shotActive) return;
    
    _state.shotActive = false;
    uint32_t duration = getShotDuration();
    
    // Complete shot record
    _currentShot.yieldWeight = _state.shotWeight;
    _currentShot.durationMs = duration;
    _currentShot.avgFlowRate = duration > 0 ? (_state.shotWeight / (duration / 1000.0f)) : 0;
    _currentShot.peakPressure = _shotPeakPressure;
    _currentShot.avgTemperature = _shotTempCount > 0 ? (_shotTempSum / _shotTempCount) : 0;
    
    // Save to history
    addShotRecord(_currentShot);
    
    setMachineState(MachineState::READY);
    notifyStateChanged();
}

uint32_t StateManager::getShotDuration() const {
    if (!_state.shotActive) return 0;
    return millis() - _state.shotStartTime;
}

// =============================================================================
// CHANGE NOTIFICATIONS
// =============================================================================

void StateManager::onSettingsChanged(SettingsCallback callback) {
    _onSettingsChanged = callback;
}

void StateManager::onStatsChanged(StatsCallback callback) {
    _onStatsChanged = callback;
}

void StateManager::onStateChanged(StateCallback callback) {
    _onStateChanged = callback;
}

void StateManager::onShotCompleted(ShotCallback callback) {
    _onShotCompleted = callback;
}

void StateManager::notifySettingsChanged() {
    if (_onSettingsChanged) {
        _onSettingsChanged(_settings);
    }
}

void StateManager::notifyStatsChanged() {
    if (_onStatsChanged) {
        _onStatsChanged(_stats);
    }
}

void StateManager::notifyStateChanged() {
    _state.lastUpdate = millis();
    if (_onStateChanged) {
        _onStateChanged(_state);
    }
}

// =============================================================================
// SERIALIZATION
// =============================================================================

void StateManager::getFullStateJson(JsonDocument& doc) const {
    // Settings go at root level via the toJson method
    getSettingsJson(doc);
    
    JsonObject stats = doc["stats"].to<JsonObject>();
    getStatsJson(stats);
    
    JsonObject state = doc["state"].to<JsonObject>();
    getStateJson(state);
    
    JsonArray history = doc["shotHistory"].to<JsonArray>();
    getShotHistoryJson(history);
}

void StateManager::getSettingsJson(JsonDocument& doc) const {
    _settings.toJson(doc);
}

void StateManager::getStatsJson(JsonObject& obj) const {
    _stats.toJson(obj);
}

void StateManager::getStateJson(JsonObject& obj) const {
    _state.toJson(obj);
}

void StateManager::getShotHistoryJson(JsonArray& arr) const {
    _shotHistory.toJson(arr);
}

bool StateManager::applySettings(const JsonDocument& doc) {
    _settings.fromJson(doc);
    saveSettings();
    return true;
}

bool StateManager::applySettings(const char* section, const JsonObject& obj) {
    if (strcmp(section, "temperature") == 0) {
        _settings.temperature.fromJson(obj);
        saveTemperatureSettings();
    } else if (strcmp(section, "brew") == 0) {
        _settings.brew.fromJson(obj);
        saveBrewSettings();
    } else if (strcmp(section, "power") == 0) {
        _settings.power.fromJson(obj);
        savePowerSettings();
    } else if (strcmp(section, "network") == 0) {
        _settings.network.fromJson(obj);
        saveNetworkSettings();
    } else if (strcmp(section, "mqtt") == 0) {
        _settings.mqtt.fromJson(obj);
        saveMQTTSettings();
    } else if (strcmp(section, "cloud") == 0) {
        _settings.cloud.fromJson(obj);
        saveCloudSettings();
    } else if (strcmp(section, "scale") == 0) {
        _settings.scale.fromJson(obj);
        saveScaleSettings();
    } else if (strcmp(section, "display") == 0) {
        _settings.display.fromJson(obj);
        saveDisplaySettings();
    } else {
        return false;
    }
    return true;
}

// =============================================================================
// HELPERS
// =============================================================================

void StateManager::checkDailyReset() {
    // Check if we've passed midnight
    time_t now = time(nullptr);
    struct tm* tm_now = localtime(&now);
    
    // Simple check: if it's a new day (hour < previous hour), reset
    static int lastHour = -1;
    if (lastHour >= 0 && tm_now->tm_hour < lastHour) {
        _stats.resetDaily();
        notifyStatsChanged();
        Serial.println("[State] Daily stats reset");
    }
    lastHour = tm_now->tm_hour;
}

} // namespace BrewOS

