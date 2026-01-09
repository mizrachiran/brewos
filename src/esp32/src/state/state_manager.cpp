#include "state/state_manager.h"
#include "config.h"  // For LOG_I macro
#include "memory_utils.h"
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
    
    // Load persisted data with error handling - step by step with logging
    Serial.println("[State] Step 1: Loading settings...");
    loadSettings();
    Serial.println("[State] Step 1: OK");
    
    Serial.println("[State] Step 2: Loading stats...");
    loadStats();
    Serial.println("[State] Step 2: OK");
    
    Serial.println("[State] Step 3: Loading shot history...");
    loadShotHistory();
    Serial.println("[State] Step 3: OK");
    
    Serial.println("[State] Step 4: Loading schedules...");
    loadScheduleSettings();
    Serial.println("[State] Step 4: OK");
    
    // Initialize session
    // Safe time() call - if NTP not synced, use 0 (will be updated later)
    time_t now = time(nullptr);
    _stats.sessionStartTimestamp = (now > 1000000) ? now : 0;  // Only use if valid (after 2001)
    _stats.sessionShots = 0;
    _state.uptime = millis();
    _lastActivityTime = millis();
    _lastScheduleCheck = millis();
    _lastScheduleMinute = 255;
    
    // Simplified logging to avoid stack issues
    Serial.print("[State] Loaded settings, shots: ");
    Serial.print(_stats.totalShots);
    Serial.print(", history: ");
    Serial.print(_shotHistory.count);
    Serial.print(", schedules: ");
    Serial.println(_settings.schedule.count);
    
    Serial.println("[State] Initialization complete");
}

void StateManager::loop() {
    // Update uptime
    _state.uptime = millis();
    
    // Periodic stats save
    if (millis() - _lastStatsSave > STATS_SAVE_INTERVAL) {
        _lastStatsSave = millis();
        saveStats();
    }
    
    // Deferred shot history save (non-blocking, on-demand only)
    // Only save if there are actual shots in history AND something changed (avoid unnecessary saves)
    // Save 5 seconds after shot completes to avoid blocking main loop during UI operations
    if (_shotHistoryDirty && (millis() - _lastShotHistorySave >= SHOT_HISTORY_SAVE_DELAY)) {
        // Only save if there are shots to save AND count changed since last save
        if (_shotHistory.count > 0 && _shotHistory.count != _lastSavedShotCount) {
            // Only save if we have enough heap (indicates system is not under heavy load)
            // This prevents blocking during critical operations like screen switching
            size_t freeHeap = ESP.getFreeHeap();
            if (freeHeap > 60000) {  // Only save if we have plenty of heap (system is idle)
                // Clear dirty flag only AFTER successful save
                if (saveShotHistory()) {
                    _shotHistoryDirty = false;
                    _lastSavedShotCount = _shotHistory.count;
                    _lastShotHistorySave = millis();
                } else {
                    // Save failed - keep dirty flag set and retry in 2 seconds
                    _lastShotHistorySave = millis() - (SHOT_HISTORY_SAVE_DELAY - 2000);
                }
            } else {
                // Defer further if heap is low (system is busy with UI/network operations)
                // Reset timer to retry in 2 seconds
                _lastShotHistorySave = millis() - (SHOT_HISTORY_SAVE_DELAY - 2000);
            }
        } else {
            // No shots to save OR count hasn't changed - clear dirty flag
            _shotHistoryDirty = false;
            if (_shotHistory.count == 0) {
                _lastSavedShotCount = 0;
            }
        }
    }
    
    // Daily reset check
    checkDailyReset();
    
    // Check schedules
    checkSchedules();
}

// =============================================================================
// SETTINGS PERSISTENCE
// =============================================================================

void StateManager::loadSettings() {
    // Use a fresh Preferences object for safety
    Preferences prefs;
    
    // Try read-write first to create namespace if it doesn't exist
    // This is normal after a fresh flash - will use defaults
    if (!prefs.begin(NVS_SETTINGS, false)) {
        Serial.println("[State] No saved settings (fresh flash) - using defaults");
        return;  // Use default values
    }
    // Now we can read
    
    // Temperature
    _settings.temperature.brewSetpoint = prefs.getFloat("brewSP", 93.5f);
    _settings.temperature.steamSetpoint = prefs.getFloat("steamSP", 145.0f);
    _settings.temperature.brewOffset = prefs.getFloat("brewOff", 0.0f);
    _settings.temperature.steamOffset = prefs.getFloat("steamOff", 0.0f);
    _settings.temperature.ecoBrewTemp = prefs.getFloat("ecoTemp", 80.0f);
    _settings.temperature.ecoTimeoutMinutes = prefs.getUShort("ecoTimeout", 30);
    
    // Brew
    _settings.brew.bbwEnabled = prefs.getBool("bbwEnabled", false);
    _settings.brew.doseWeight = prefs.getFloat("doseWt", 18.0f);
    _settings.brew.targetWeight = prefs.getFloat("targetWt", 36.0f);
    _settings.brew.stopOffset = prefs.getFloat("stopOff", 2.0f);
    _settings.brew.autoTare = prefs.getBool("autoTare", true);
    _settings.brew.preinfusionTime = prefs.getFloat("preinf", 0.0f);
    _settings.brew.preinfusionPressure = prefs.getFloat("preinfP", 2.0f);
    _settings.brew.preinfusionPauseMs = prefs.getUShort("preinfPause", 5000);
    
    // Power
    _settings.power.mainsVoltage = prefs.getUShort("voltage", 220);
    _settings.power.maxCurrent = prefs.getFloat("maxCurr", 13.0f);
    _settings.power.powerOnBoot = prefs.getBool("pwrBoot", false);
    
    // Network
    prefs.getString("wifiSsid", _settings.network.wifiSsid, sizeof(_settings.network.wifiSsid));
    prefs.getString("wifiPass", _settings.network.wifiPassword, sizeof(_settings.network.wifiPassword));
    _settings.network.wifiConfigured = prefs.getBool("wifiCfg", false);
    prefs.getString("hostname", _settings.network.hostname, sizeof(_settings.network.hostname));
    if (strlen(_settings.network.hostname) == 0) {
        strcpy(_settings.network.hostname, "brewos");
    }
    
    // Time/NTP
    _settings.time.useNTP = prefs.getBool("useNTP", true);
    prefs.getString("ntpSrv", _settings.time.ntpServer, sizeof(_settings.time.ntpServer));
    if (strlen(_settings.time.ntpServer) == 0) {
        strcpy(_settings.time.ntpServer, "pool.ntp.org");
    }
    _settings.time.utcOffsetMinutes = prefs.getShort("utcOff", 0);
    _settings.time.dstEnabled = prefs.getBool("dstEn", false);
    _settings.time.dstOffsetMinutes = prefs.getShort("dstOff", 60);
    
    // MQTT
    _settings.mqtt.enabled = prefs.getBool("mqttEn", false);
    prefs.getString("mqttBrk", _settings.mqtt.broker, sizeof(_settings.mqtt.broker));
    _settings.mqtt.port = prefs.getUShort("mqttPort", 1883);
    prefs.getString("mqttUser", _settings.mqtt.username, sizeof(_settings.mqtt.username));
    prefs.getString("mqttPass", _settings.mqtt.password, sizeof(_settings.mqtt.password));
    prefs.getString("mqttTopic", _settings.mqtt.baseTopic, sizeof(_settings.mqtt.baseTopic));
    if (strlen(_settings.mqtt.baseTopic) == 0) {
        strcpy(_settings.mqtt.baseTopic, "brewos");
    }
    _settings.mqtt.discovery = prefs.getBool("mqttDisc", true);
    
    // Cloud
    _settings.cloud.enabled = prefs.getBool("cloudEn", false);
    prefs.getString("cloudUrl", _settings.cloud.serverUrl, sizeof(_settings.cloud.serverUrl));
    prefs.getString("devId", _settings.cloud.deviceId, sizeof(_settings.cloud.deviceId));
    prefs.getString("devKey", _settings.cloud.deviceKey, sizeof(_settings.cloud.deviceKey));
    
    // Scale
    _settings.scale.enabled = prefs.getBool("scaleEn", true);
    prefs.getString("scaleAddr", _settings.scale.pairedAddress, sizeof(_settings.scale.pairedAddress));
    prefs.getString("scaleName", _settings.scale.pairedName, sizeof(_settings.scale.pairedName));
    _settings.scale.scaleType = prefs.getUChar("scaleType", 0);
    
    // Display
    _settings.display.brightness = prefs.getUChar("dispBri", 200);
    _settings.display.screenTimeout = prefs.getUChar("dispTO", 30);
    _settings.display.showShotTimer = prefs.getBool("showTimer", true);
    _settings.display.showWeight = prefs.getBool("showWt", true);
    _settings.display.showPressure = prefs.getBool("showPres", true);
    
    // Machine Info
    prefs.getString("devName", _settings.machineInfo.deviceName, sizeof(_settings.machineInfo.deviceName));
    _settings.machineInfo.deviceName[sizeof(_settings.machineInfo.deviceName) - 1] = '\0';
    if (strlen(_settings.machineInfo.deviceName) == 0) {
        strcpy(_settings.machineInfo.deviceName, "BrewOS");
    }
    prefs.getString("mcBrand", _settings.machineInfo.machineBrand, sizeof(_settings.machineInfo.machineBrand));
    _settings.machineInfo.machineBrand[sizeof(_settings.machineInfo.machineBrand) - 1] = '\0';
    prefs.getString("mcModel", _settings.machineInfo.machineModel, sizeof(_settings.machineInfo.machineModel));
    _settings.machineInfo.machineModel[sizeof(_settings.machineInfo.machineModel) - 1] = '\0';
    prefs.getString("mcType", _settings.machineInfo.machineType, sizeof(_settings.machineInfo.machineType));
    _settings.machineInfo.machineType[sizeof(_settings.machineInfo.machineType) - 1] = '\0';
    if (strlen(_settings.machineInfo.machineType) == 0) {
        strcpy(_settings.machineInfo.machineType, "dual_boiler");
    }
    // Set numeric machine type from string
    if (strcmp(_settings.machineInfo.machineType, "dual_boiler") == 0) {
        _state.machineType = 1;
    } else if (strcmp(_settings.machineInfo.machineType, "single_boiler") == 0) {
        _state.machineType = 2;
    } else if (strcmp(_settings.machineInfo.machineType, "heat_exchanger") == 0) {
        _state.machineType = 3;
    } else if (strcmp(_settings.machineInfo.machineType, "thermoblock") == 0) {
        _state.machineType = 4;
    } else {
        _state.machineType = 0;  // unknown
    }
    
    // Notification Preferences
    _settings.notifications.machineReady = prefs.getBool("notifReady", true);
    _settings.notifications.waterEmpty = prefs.getBool("notifWater", true);
    _settings.notifications.descaleDue = prefs.getBool("notifDescale", true);
    _settings.notifications.serviceDue = prefs.getBool("notifService", true);
    _settings.notifications.backflushDue = prefs.getBool("notifBackflush", true);
    _settings.notifications.machineError = prefs.getBool("notifError", true);
    _settings.notifications.picoOffline = prefs.getBool("notifPico", true);
    _settings.notifications.scheduleTriggered = prefs.getBool("notifSched", true);
    _settings.notifications.brewComplete = prefs.getBool("notifBrew", false);
    
    // System
    _settings.system.setupComplete = prefs.getBool("setupDone", false);
    _settings.system.logBufferEnabled = prefs.getBool("logBufEn", false);
    _settings.system.debugLogsEnabled = prefs.getBool("debugLogs", false);
    _settings.system.picoLogForwardingEnabled = prefs.getBool("picoLogFwd", false);
    
    // User Preferences
    _settings.preferences.firstDayOfWeek = prefs.getUChar("prefDOW", 0);
    _settings.preferences.use24HourTime = prefs.getBool("pref24h", false);
    _settings.preferences.temperatureUnit = prefs.getUChar("prefTempU", 0);
    _settings.preferences.electricityPrice = prefs.getFloat("prefElecP", 0.15f);
    prefs.getString("prefCurr", _settings.preferences.currency, sizeof(_settings.preferences.currency));
    if (strlen(_settings.preferences.currency) == 0) {
        strncpy(_settings.preferences.currency, "USD", sizeof(_settings.preferences.currency) - 1);
        _settings.preferences.currency[sizeof(_settings.preferences.currency) - 1] = '\0';
    }
    _settings.preferences.lastHeatingStrategy = prefs.getUChar("prefHeatS", 1);
    _settings.preferences.initialized = prefs.getBool("prefInit", false);
    
    Serial.println("[State] Finished reading settings, closing NVS...");
    // Serial.flush(); // Removed - can block on USB CDC
    
    prefs.end();
    
    Serial.println("[State] loadSettings() complete");
}

void StateManager::saveSettings() {
    LOG_I("Saving all settings to NVS");
    saveTemperatureSettings();
    saveBrewSettings();
    savePowerSettings();
    saveNetworkSettings();
    saveMQTTSettings();
    saveCloudSettings();
    saveScaleSettings();
    saveDisplaySettings();
    saveMachineInfoSettings();
    saveNotificationSettings();
    saveSystemSettings();
    saveUserPreferences();
    LOG_I("All settings saved successfully");
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
    _prefs.putUShort("preinfPause", _settings.brew.preinfusionPauseMs);
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

void StateManager::saveTimeSettings() {
    _prefs.begin(NVS_SETTINGS, false);
    _prefs.putBool("useNTP", _settings.time.useNTP);
    _prefs.putString("ntpSrv", _settings.time.ntpServer);
    _prefs.putShort("utcOff", _settings.time.utcOffsetMinutes);
    _prefs.putBool("dstEn", _settings.time.dstEnabled);
    _prefs.putShort("dstOff", _settings.time.dstOffsetMinutes);
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

void StateManager::saveMachineInfoSettings() {
    _prefs.begin(NVS_SETTINGS, false);
    _prefs.putString("devName", _settings.machineInfo.deviceName);
    _prefs.putString("mcBrand", _settings.machineInfo.machineBrand);
    _prefs.putString("mcModel", _settings.machineInfo.machineModel);
    _prefs.putString("mcType", _settings.machineInfo.machineType);
    _prefs.end();
    notifySettingsChanged();
}

void StateManager::saveNotificationSettings() {
    _prefs.begin(NVS_SETTINGS, false);
    _prefs.putBool("notifReady", _settings.notifications.machineReady);
    _prefs.putBool("notifWater", _settings.notifications.waterEmpty);
    _prefs.putBool("notifDescale", _settings.notifications.descaleDue);
    _prefs.putBool("notifService", _settings.notifications.serviceDue);
    _prefs.putBool("notifBackflush", _settings.notifications.backflushDue);
    _prefs.putBool("notifError", _settings.notifications.machineError);
    _prefs.putBool("notifPico", _settings.notifications.picoOffline);
    _prefs.putBool("notifSched", _settings.notifications.scheduleTriggered);
    _prefs.putBool("notifBrew", _settings.notifications.brewComplete);
    _prefs.end();
    notifySettingsChanged();
}

void StateManager::saveSystemSettings() {
    if (!_prefs.begin(NVS_SETTINGS, false)) {
        Serial.println("[State] ERROR: Failed to open NVS for system settings save");
        return;
    }
    _prefs.putBool("setupDone", _settings.system.setupComplete);
    _prefs.putBool("logBufEn", _settings.system.logBufferEnabled);
    _prefs.putBool("debugLogs", _settings.system.debugLogsEnabled);
    _prefs.putBool("picoLogFwd", _settings.system.picoLogForwardingEnabled);
    _prefs.end();  // This commits the changes to flash
    LOG_I("System settings saved (setupComplete=%s, logBuffer=%s, debugLogs=%s, picoForwarding=%s)",
          _settings.system.setupComplete ? "true" : "false",
          _settings.system.logBufferEnabled ? "true" : "false",
          _settings.system.debugLogsEnabled ? "true" : "false",
          _settings.system.picoLogForwardingEnabled ? "true" : "false");
    notifySettingsChanged();
}

void StateManager::saveUserPreferences() {
    _prefs.begin(NVS_SETTINGS, false);
    _prefs.putUChar("prefDOW", _settings.preferences.firstDayOfWeek);
    _prefs.putBool("pref24h", _settings.preferences.use24HourTime);
    _prefs.putUChar("prefTempU", _settings.preferences.temperatureUnit);
    _prefs.putFloat("prefElecP", _settings.preferences.electricityPrice);
    _prefs.putString("prefCurr", _settings.preferences.currency);
    _prefs.putUChar("prefHeatS", _settings.preferences.lastHeatingStrategy);
    _prefs.putBool("prefInit", _settings.preferences.initialized);
    _prefs.end();
    notifySettingsChanged();
}

void StateManager::resetSettings() {
    _settings = Settings();  // Reset to defaults
    saveSettings();
}

void StateManager::factoryReset() {
    // Clear all settings
    _prefs.begin(NVS_SETTINGS, false);
    _prefs.clear();
    _prefs.end();
    
    // Clear statistics
    _prefs.begin(NVS_STATS, false);
    _prefs.clear();
    _prefs.end();
    
    // Clear MQTT settings (separate namespace)
    _prefs.begin("mqtt", false);
    _prefs.clear();
    _prefs.end();
    
    // Note: WiFi credentials are cleared separately by WiFiManager::clearCredentials()
    // which is called by the factory_reset command handler
    
    // Remove shot history file
    LittleFS.remove(SHOT_HISTORY_FILE);
    
    // Reset in-memory state to defaults
    _settings = Settings();
    _stats = Statistics();
    _shotHistory.clear();
    
    Serial.println("[State] Factory reset complete - all settings and stats cleared");
}

// =============================================================================
// STATISTICS PERSISTENCE
// =============================================================================

void StateManager::loadStats() {
    // Use a fresh Preferences object for safety
    Preferences prefs;
    // Try read-write first to create namespace if it doesn't exist
    // This is normal after a fresh flash - will use defaults
    if (!prefs.begin(NVS_STATS, false)) {
        Serial.println("[State] No saved stats (fresh flash) - using defaults");
        return;  // Use default values
    }
    
    _stats.totalShots = prefs.getULong("totalShots", 0);
    _stats.totalSteamCycles = prefs.getULong("totalSteam", 0);
    _stats.totalKwh = prefs.getFloat("totalKwh", 0.0f);
    _stats.totalOnTimeMinutes = prefs.getULong("totalOn", 0);
    
    _stats.shotsSinceDescale = prefs.getULong("sinceDesc", 0);
    _stats.shotsSinceGroupClean = prefs.getULong("sinceGrp", 0);
    _stats.shotsSinceBackflush = prefs.getULong("sinceBack", 0);
    _stats.lastDescaleTimestamp = prefs.getULong("lastDesc", 0);
    _stats.lastGroupCleanTimestamp = prefs.getULong("lastGrp", 0);
    _stats.lastBackflushTimestamp = prefs.getULong("lastBack", 0);
    
    prefs.end();
}

void StateManager::saveStats() {
    // Yield before NVS operations to prevent blocking
    yield();
    
    _prefs.begin(NVS_STATS, false);
    
    _prefs.putULong("totalShots", _stats.totalShots);
    _prefs.putULong("totalSteam", _stats.totalSteamCycles);
    _prefs.putFloat("totalKwh", _stats.totalKwh);
    _prefs.putULong("totalOn", _stats.totalOnTimeMinutes);
    
    yield(); // Yield after first batch of writes
    
    _prefs.putULong("sinceDesc", _stats.shotsSinceDescale);
    _prefs.putULong("sinceGrp", _stats.shotsSinceGroupClean);
    _prefs.putULong("sinceBack", _stats.shotsSinceBackflush);
    _prefs.putULong("lastDesc", _stats.lastDescaleTimestamp);
    _prefs.putULong("lastGrp", _stats.lastGroupCleanTimestamp);
    _prefs.putULong("lastBack", _stats.lastBackflushTimestamp);
    
    _prefs.end();
    
    yield(); // Yield after NVS operations complete
    
    LOG_I("Stats saved");
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
    // Missing file is normal after fresh flash
    if (!LittleFS.exists(SHOT_HISTORY_FILE)) {
        Serial.println("[State] No shot history (fresh flash) - starting empty");
        _shotHistory.clear();  // Ensure clean state
        _lastSavedShotCount = 0;
        return;
    }
    
    File file = LittleFS.open(SHOT_HISTORY_FILE, "r");
    if (!file) {
        Serial.println("[State] Failed to open shot history - using defaults");
        _shotHistory.clear();
        _lastSavedShotCount = 0;
        return;
    }
    
    size_t fileSize = file.size();
    
    // Binary format only - check minimum size
    if (fileSize < 6) {
        Serial.println("[State] Shot history file too small - using defaults");
        file.close();
        _shotHistory.clear();
        _lastSavedShotCount = 0;
        return;
    }
    
    // Allocate buffer (binary format is small, ~2KB max)
    uint8_t* buffer = (uint8_t*)heap_caps_malloc(fileSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buffer) {
        buffer = (uint8_t*)malloc(fileSize);
    }
    
    if (!buffer) {
        Serial.println("[State] Failed to allocate buffer for shot history");
        file.close();
        _shotHistory.clear();
        _lastSavedShotCount = 0;
        return;
    }
    
    // Read entire file
    file.readBytes((char*)buffer, fileSize);
    file.close();
    
    // Parse binary format
    if (_shotHistory.fromBinary(buffer, fileSize)) {
        Serial.printf("[State] Loaded shot history: %d entries (%zu bytes)\n", _shotHistory.count, fileSize);
        _lastSavedShotCount = _shotHistory.count;
    } else {
        Serial.println("[State] Invalid shot history format - using defaults");
        _shotHistory.clear();
        _lastSavedShotCount = 0;
    }
    
    free(buffer);
}

bool StateManager::saveShotHistory() {
    // Binary format is much faster than JSON (50-100ms vs 2-4 seconds)
    // Format: [magic:4][version:1][count:1][records:count*40]
    const size_t headerSize = 6;
    const size_t recordSize = sizeof(ShotRecord);
    const size_t maxSize = headerSize + (MAX_SHOT_HISTORY * recordSize);  // ~2006 bytes max
    
    // Allocate buffer in PSRAM if available, otherwise heap
    uint8_t* buffer = (uint8_t*)heap_caps_malloc(maxSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buffer) {
        buffer = (uint8_t*)malloc(maxSize);
        if (!buffer) {
            Serial.println("[State] Failed to allocate buffer for shot history");
            return false;
        }
    }
    
    // Serialize to binary format
    size_t written = _shotHistory.toBinary(buffer, maxSize);
    if (written == 0) {
        free(buffer);
        Serial.println("[State] Failed to serialize shot history");
        return false;
    }
    
    // Yield before file operations
    yield();
    
    // Write to file
    File file = LittleFS.open(SHOT_HISTORY_FILE, "w");
    if (!file) {
        free(buffer);
        Serial.println("[State] Failed to open shot history file for writing");
        return false;
    }
    
    size_t bytesWritten = file.write(buffer, written);
    free(buffer);
    
    // Yield during file close
    yield();
    
    file.close();
    
    if (bytesWritten != written) {
        Serial.printf("[State] Shot history write incomplete: %zu/%zu bytes\n", bytesWritten, written);
        return false;
    }
    
    Serial.printf("[State] Shot history saved (%d entries, %zu bytes)\n", _shotHistory.count, written);
    return true;
}

void StateManager::addShotRecord(const ShotRecord& shot) {
    _shotHistory.addShot(shot);
    // Defer save to avoid blocking main loop (2 second delay)
    // This prevents slow loops that block network operations
    _shotHistoryDirty = true;
    _lastShotHistorySave = millis();
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
    _shotHistoryDirty = false;
    _lastSavedShotCount = 0;
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

void StateManager::setPicoVersion(uint8_t major, uint8_t minor, uint8_t patch) {
    snprintf(_state.picoVersion, sizeof(_state.picoVersion), "%u.%u.%u", major, minor, patch);
    Serial.printf("[State] Pico version: %s\n", _state.picoVersion);
}

void StateManager::setPicoBuildDate(const char* buildDate, const char* buildTime) {
    if (buildDate && buildDate[0] && buildTime && buildTime[0]) {
        snprintf(_state.picoBuildDate, sizeof(_state.picoBuildDate), "%s %s", buildDate, buildTime);
    } else if (buildDate && buildDate[0]) {
        strncpy(_state.picoBuildDate, buildDate, sizeof(_state.picoBuildDate) - 1);
    } else {
        _state.picoBuildDate[0] = '\0';
    }
    if (_state.picoBuildDate[0]) {
        Serial.printf("[State] Pico build: %s\n", _state.picoBuildDate);
    }
}

void StateManager::setPicoResetReason(uint8_t reason) {
    _state.picoResetReason = reason;
    const char* reasonStr = "unknown";
    switch (reason) {
        case 0: reasonStr = "power-on"; break;
        case 1: reasonStr = "watchdog"; break;
        case 2: reasonStr = "software"; break;
        case 3: reasonStr = "debug"; break;
    }
    Serial.printf("[State] Pico reset reason: %s (%d)\n", reasonStr, reason);
}

void StateManager::setMachineType(uint8_t type, bool force) {
    // If not forcing and machine type is already set (not unknown), preserve it
    // This prevents Pico's MSG_BOOT from overwriting user-set machine type
    if (!force && _state.machineType != 0 && type != 0) {
        // Machine type already set - preserve user setting
        Serial.printf("[State] Preserving existing machine type: %s (%d) (ignoring: %d)\n", 
                     _settings.machineInfo.machineType, _state.machineType, type);
        return;
    }
    
    _state.machineType = type;
    const char* typeStr = "unknown";
    switch (type) {
        case 1: typeStr = "dual_boiler"; break;
        case 2: typeStr = "single_boiler"; break;
        case 3: typeStr = "heat_exchanger"; break;
        case 4: typeStr = "thermoblock"; break;
    }
    // Also update the persisted machine type string
    strncpy(_settings.machineInfo.machineType, typeStr, sizeof(_settings.machineInfo.machineType) - 1);
    _settings.machineInfo.machineType[sizeof(_settings.machineInfo.machineType) - 1] = '\0';
    // Persist the change
    saveMachineInfoSettings();
    Serial.printf("[State] Machine type: %s (%d)\n", typeStr, type);
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
    } else if (strcmp(section, "time") == 0) {
        _settings.time.fromJson(obj);
        saveTimeSettings();
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
    } else if (strcmp(section, "machineInfo") == 0) {
        _settings.machineInfo.fromJson(obj);
        saveMachineInfoSettings();
    } else if (strcmp(section, "notifications") == 0) {
        _settings.notifications.fromJson(obj);
        saveNotificationSettings();
    } else if (strcmp(section, "system") == 0) {
        _settings.system.fromJson(obj);
        saveSystemSettings();
    } else if (strcmp(section, "preferences") == 0) {
        _settings.preferences.fromJson(obj);
        saveUserPreferences();
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

// =============================================================================
// SCHEDULE MANAGEMENT
// =============================================================================

static const char* NVS_SCHEDULES = "schedules";
static const char* SCHEDULE_FILE = "/schedules.json";

void StateManager::saveScheduleSettings() {
    // Save to LittleFS as JSON (NVS has limited size for complex data)
    File file = LittleFS.open(SCHEDULE_FILE, "w");
    if (!file) {
        Serial.println("[State] Failed to write schedules");
        return;
    }
    
    // Use PSRAM for schedule serialization
    SpiRamJsonDocument doc(4096);
    JsonObject obj = doc.to<JsonObject>();
    _settings.schedule.toJson(obj);
    
    serializeJson(doc, file);
    file.close();
    
    Serial.printf("[State] Schedules saved (%d active)\n", _settings.schedule.count);
    notifySettingsChanged();
}

void StateManager::loadScheduleSettings() {
    // Missing file is normal after fresh flash
    if (!LittleFS.exists(SCHEDULE_FILE)) {
        Serial.println("[State] No schedules (fresh flash) - using defaults");
        // ScheduleSettings will use defaults from constructor
        return;
    }
    
    File file = LittleFS.open(SCHEDULE_FILE, "r");
    if (!file) {
        Serial.println("[State] Failed to open schedules - using defaults");
        return;
    }
    
    // Limit file size to prevent memory issues
    size_t fileSize = file.size();
    if (fileSize > 10000) {  // 10KB max for schedules
        Serial.print("[State] Schedule file too large: ");
        Serial.print(fileSize);
        Serial.println(" bytes - using defaults");
        file.close();
        return;
    }
    
    // Use PSRAM for schedule parsing (up to 10KB)
    SpiRamJsonDocument doc(fileSize + 256);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        Serial.print("[State] Schedule parse error: ");
        Serial.println(error.c_str());
        // Use defaults on parse error (already initialized)
        return;
    }
    
    // Safely parse JSON object with error handling
    if (doc.is<JsonObject>()) {
        JsonObject obj = doc.as<JsonObject>();
        // Check if object is valid before parsing
        if (!obj.isNull()) {
            // Call fromJson with error checking
            bool success = _settings.schedule.fromJson(obj);
            if (success) {
                Serial.print("[State] Loaded ");
                Serial.print(_settings.schedule.count);
                Serial.println(" schedules");
            } else {
                Serial.println("[State] Schedule fromJson failed - using defaults");
            }
        } else {
            Serial.println("[State] Schedule JSON object is null - using defaults");
        }
    } else {
        Serial.println("[State] Schedule file is not an object - using defaults");
        // Use defaults (already initialized)
    }
}

uint8_t StateManager::addSchedule(const ScheduleEntry& entry) {
    uint8_t id = _settings.schedule.addSchedule(entry);
    if (id > 0) {
        saveScheduleSettings();
    }
    return id;
}

bool StateManager::updateSchedule(uint8_t id, const ScheduleEntry& entry) {
    ScheduleEntry* existing = _settings.schedule.findById(id);
    if (!existing) {
        return false;
    }
    
    *existing = entry;
    existing->id = id;  // Preserve ID
    saveScheduleSettings();
    return true;
}

bool StateManager::removeSchedule(uint8_t id) {
    bool success = _settings.schedule.removeSchedule(id);
    if (success) {
        saveScheduleSettings();
    }
    return success;
}

bool StateManager::enableSchedule(uint8_t id, bool enabled) {
    ScheduleEntry* entry = _settings.schedule.findById(id);
    if (!entry) {
        return false;
    }
    
    entry->enabled = enabled;
    saveScheduleSettings();
    return true;
}

void StateManager::setAutoPowerOff(bool enabled, uint16_t minutes) {
    _settings.schedule.autoPowerOffEnabled = enabled;
    _settings.schedule.autoPowerOffMinutes = minutes;
    saveScheduleSettings();
}

void StateManager::onScheduleTriggered(ScheduleCallback callback) {
    _onScheduleTriggered = callback;
}

void StateManager::checkSchedules() {
    uint32_t now = millis();
    
    // Only check every SCHEDULE_CHECK_INTERVAL
    if (now - _lastScheduleCheck < SCHEDULE_CHECK_INTERVAL) {
        return;
    }
    _lastScheduleCheck = now;
    
    // Get current time
    time_t currentTime = time(nullptr);
    if (currentTime < 1000000) {
        // Time not set yet (NTP not synced)
        return;
    }
    
    struct tm* tm_now = localtime(&currentTime);
    uint8_t currentHour = tm_now->tm_hour;
    uint8_t currentMinute = tm_now->tm_min;
    uint8_t currentDay = tm_now->tm_wday;  // 0 = Sunday
    
    // Avoid duplicate triggers within the same minute
    uint8_t currentMinuteId = (currentHour * 60) + currentMinute;
    if (_lastScheduleMinute == currentMinuteId) {
        return;
    }
    
    // Collect all matching schedules first to resolve conflicts
    const ScheduleEntry* matchingSchedules[MAX_SCHEDULES];
    size_t matchCount = 0;
    
    for (size_t i = 0; i < MAX_SCHEDULES; i++) {
        const ScheduleEntry& sched = _settings.schedule.schedules[i];
        
        if (sched.id == 0 || !sched.enabled) {
            continue;
        }
        
        // Check if schedule matches current time and day
        if (sched.matchesTime(currentHour, currentMinute) && 
            sched.isValidForDay(currentDay)) {
            matchingSchedules[matchCount++] = &sched;
        }
    }
    
    // Resolve conflicts if multiple schedules match
    if (matchCount > 0) {
        const ScheduleEntry* scheduleToTrigger = nullptr;
        
        if (matchCount == 1) {
            // Single schedule - no conflict
            scheduleToTrigger = matchingSchedules[0];
        } else {
            // Multiple schedules at same time - resolve conflict
            // Priority: OFF actions take precedence over ON (safety first)
            // If multiple ON or multiple OFF, use the one with highest ID (last in list)
            bool hasOff = false;
            bool hasOn = false;
            const ScheduleEntry* lastOff = nullptr;
            const ScheduleEntry* lastOn = nullptr;
            
            for (size_t i = 0; i < matchCount; i++) {
                const ScheduleEntry* sched = matchingSchedules[i];
                if (sched->action == ACTION_TURN_OFF) {
                    hasOff = true;
                    if (!lastOff || sched->id > lastOff->id) {
                        lastOff = sched;
                    }
                } else {
                    hasOn = true;
                    if (!lastOn || sched->id > lastOn->id) {
                        lastOn = sched;
                    }
                }
            }
            
            if (hasOff && hasOn) {
                // Conflict: both ON and OFF scheduled - prioritize OFF for safety
                scheduleToTrigger = lastOff;
                Serial.printf("[Schedule] Conflict detected: %zu schedules at %02d:%02d, prioritizing OFF (safety first)\n",
                    matchCount, currentHour, currentMinute);
            } else if (hasOff) {
                // Multiple OFF schedules - use highest ID
                scheduleToTrigger = lastOff;
            } else {
                // Multiple ON schedules - use highest ID
                scheduleToTrigger = lastOn;
            }
        }
        
        if (scheduleToTrigger) {
            Serial.printf("[Schedule] Triggered: %s (action=%s, strategy=%d)\n",
                scheduleToTrigger->name, 
                scheduleToTrigger->action == ACTION_TURN_ON ? "on" : "off", 
                scheduleToTrigger->strategy);
            
            if (_onScheduleTriggered) {
                _onScheduleTriggered(*scheduleToTrigger);
            }
        }
    }
    
    _lastScheduleMinute = currentMinuteId;
    
    // Check auto power-off
    if (_settings.schedule.autoPowerOffEnabled && 
        _settings.schedule.autoPowerOffMinutes > 0 &&
        isIdleTimeout()) {
        
        // Create a virtual "off" schedule entry for the callback
        ScheduleEntry autoPowerOff;
        autoPowerOff.action = ACTION_TURN_OFF;
        strncpy(autoPowerOff.name, "Auto Power-Off", sizeof(autoPowerOff.name) - 1);
        
        Serial.println("[Schedule] Auto power-off triggered");
        
        if (_onScheduleTriggered) {
            _onScheduleTriggered(autoPowerOff);
        }
        
        // Reset idle timer to prevent repeated triggers
        resetIdleTimer();
    }
}

void StateManager::resetIdleTimer() {
    _lastActivityTime = millis();
}

bool StateManager::isIdleTimeout() const {
    if (!_settings.schedule.autoPowerOffEnabled || 
        _settings.schedule.autoPowerOffMinutes == 0) {
        return false;
    }
    
    // Only trigger auto power-off if machine is on and not brewing
    if (_state.mode == MachineMode::STANDBY || _state.shotActive) {
        return false;
    }
    
    uint32_t idleMs = millis() - _lastActivityTime;
    uint32_t timeoutMs = (uint32_t)_settings.schedule.autoPowerOffMinutes * 60 * 1000;
    
    return idleMs >= timeoutMs;
}

} // namespace BrewOS

