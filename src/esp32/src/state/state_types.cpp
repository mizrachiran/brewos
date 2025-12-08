#include "state/state_types.h"

namespace BrewOS {

// =============================================================================
// TemperatureSettings
// =============================================================================

void TemperatureSettings::toJson(JsonObject& obj) const {
    obj["brewSetpoint"] = brewSetpoint;
    obj["steamSetpoint"] = steamSetpoint;
    obj["brewOffset"] = brewOffset;
    obj["steamOffset"] = steamOffset;
    obj["ecoBrewTemp"] = ecoBrewTemp;
    obj["ecoTimeoutMinutes"] = ecoTimeoutMinutes;
}

bool TemperatureSettings::fromJson(JsonObjectConst obj) {
    if (obj["brewSetpoint"].is<float>()) brewSetpoint = obj["brewSetpoint"];
    if (obj["steamSetpoint"].is<float>()) steamSetpoint = obj["steamSetpoint"];
    if (obj["brewOffset"].is<float>()) brewOffset = obj["brewOffset"];
    if (obj["steamOffset"].is<float>()) steamOffset = obj["steamOffset"];
    if (obj["ecoBrewTemp"].is<float>()) ecoBrewTemp = obj["ecoBrewTemp"];
    if (obj["ecoTimeoutMinutes"].is<uint16_t>()) ecoTimeoutMinutes = obj["ecoTimeoutMinutes"];
    return true;
}

// =============================================================================
// BrewSettings
// =============================================================================

void BrewSettings::toJson(JsonObject& obj) const {
    obj["bbwEnabled"] = bbwEnabled;
    obj["doseWeight"] = doseWeight;
    obj["targetWeight"] = targetWeight;
    obj["stopOffset"] = stopOffset;
    obj["autoTare"] = autoTare;
    obj["preinfusionTime"] = preinfusionTime;
    obj["preinfusionPressure"] = preinfusionPressure;
}

bool BrewSettings::fromJson(JsonObjectConst obj) {
    if (obj["bbwEnabled"].is<bool>()) bbwEnabled = obj["bbwEnabled"];
    if (obj["doseWeight"].is<float>()) doseWeight = obj["doseWeight"];
    if (obj["targetWeight"].is<float>()) targetWeight = obj["targetWeight"];
    if (obj["stopOffset"].is<float>()) stopOffset = obj["stopOffset"];
    if (obj["autoTare"].is<bool>()) autoTare = obj["autoTare"];
    if (obj["preinfusionTime"].is<uint16_t>()) preinfusionTime = obj["preinfusionTime"];
    if (obj["preinfusionPressure"].is<float>()) preinfusionPressure = obj["preinfusionPressure"];
    return true;
}

// =============================================================================
// PowerSettings
// =============================================================================

void PowerSettings::toJson(JsonObject& obj) const {
    obj["mainsVoltage"] = mainsVoltage;
    obj["maxCurrent"] = maxCurrent;
    obj["powerOnBoot"] = powerOnBoot;
}

bool PowerSettings::fromJson(JsonObjectConst obj) {
    if (obj["mainsVoltage"].is<uint16_t>()) mainsVoltage = obj["mainsVoltage"];
    if (obj["maxCurrent"].is<float>()) maxCurrent = obj["maxCurrent"];
    if (obj["powerOnBoot"].is<bool>()) powerOnBoot = obj["powerOnBoot"];
    return true;
}

// =============================================================================
// NetworkSettings
// =============================================================================

void NetworkSettings::toJson(JsonObject& obj) const {
    obj["wifiSsid"] = wifiSsid;
    // Don't expose password
    obj["wifiConfigured"] = wifiConfigured;
    obj["hostname"] = hostname;
}

bool NetworkSettings::fromJson(JsonObjectConst obj) {
    if (obj["wifiSsid"].is<const char*>()) {
        strncpy(wifiSsid, obj["wifiSsid"] | "", sizeof(wifiSsid) - 1);
    }
    if (obj["wifiPassword"].is<const char*>()) {
        strncpy(wifiPassword, obj["wifiPassword"] | "", sizeof(wifiPassword) - 1);
        wifiConfigured = strlen(wifiSsid) > 0;
    }
    if (obj["hostname"].is<const char*>()) {
        strncpy(hostname, obj["hostname"] | "brewos", sizeof(hostname) - 1);
    }
    return true;
}

// =============================================================================
// TimeSettings
// =============================================================================

void TimeSettings::toJson(JsonObject& obj) const {
    obj["useNTP"] = useNTP;
    obj["ntpServer"] = ntpServer;
    obj["utcOffsetMinutes"] = utcOffsetMinutes;
    obj["dstEnabled"] = dstEnabled;
    obj["dstOffsetMinutes"] = dstOffsetMinutes;
}

bool TimeSettings::fromJson(JsonObjectConst obj) {
    if (obj["useNTP"].is<bool>()) useNTP = obj["useNTP"];
    if (obj["ntpServer"].is<const char*>()) strncpy(ntpServer, obj["ntpServer"] | "pool.ntp.org", sizeof(ntpServer) - 1);
    if (obj["utcOffsetMinutes"].is<int16_t>()) utcOffsetMinutes = obj["utcOffsetMinutes"];
    if (obj["dstEnabled"].is<bool>()) dstEnabled = obj["dstEnabled"];
    if (obj["dstOffsetMinutes"].is<int16_t>()) dstOffsetMinutes = obj["dstOffsetMinutes"];
    return true;
}

// =============================================================================
// MQTTSettings
// =============================================================================

void MQTTSettings::toJson(JsonObject& obj) const {
    obj["enabled"] = enabled;
    obj["broker"] = broker;
    obj["port"] = port;
    obj["username"] = username;
    // Don't expose password
    obj["baseTopic"] = baseTopic;
    obj["discovery"] = discovery;
}

bool MQTTSettings::fromJson(JsonObjectConst obj) {
    if (obj["enabled"].is<bool>()) enabled = obj["enabled"];
    if (obj["broker"].is<const char*>()) strncpy(broker, obj["broker"] | "", sizeof(broker) - 1);
    if (obj["port"].is<uint16_t>()) port = obj["port"];
    if (obj["username"].is<const char*>()) strncpy(username, obj["username"] | "", sizeof(username) - 1);
    if (obj["password"].is<const char*>()) strncpy(password, obj["password"] | "", sizeof(password) - 1);
    if (obj["baseTopic"].is<const char*>()) strncpy(baseTopic, obj["baseTopic"] | "brewos", sizeof(baseTopic) - 1);
    if (obj["discovery"].is<bool>()) discovery = obj["discovery"];
    return true;
}

// =============================================================================
// CloudSettings
// =============================================================================

void CloudSettings::toJson(JsonObject& obj) const {
    obj["enabled"] = enabled;
    obj["serverUrl"] = serverUrl;
    obj["deviceId"] = deviceId;
    // Don't expose deviceKey
}

bool CloudSettings::fromJson(JsonObjectConst obj) {
    if (obj["enabled"].is<bool>()) enabled = obj["enabled"];
    if (obj["serverUrl"].is<const char*>()) strncpy(serverUrl, obj["serverUrl"] | "", sizeof(serverUrl) - 1);
    if (obj["deviceId"].is<const char*>()) strncpy(deviceId, obj["deviceId"] | "", sizeof(deviceId) - 1);
    if (obj["deviceKey"].is<const char*>()) strncpy(deviceKey, obj["deviceKey"] | "", sizeof(deviceKey) - 1);
    return true;
}

// =============================================================================
// ScaleSettings
// =============================================================================

void ScaleSettings::toJson(JsonObject& obj) const {
    obj["enabled"] = enabled;
    obj["pairedAddress"] = pairedAddress;
    obj["pairedName"] = pairedName;
    obj["scaleType"] = scaleType;
}

bool ScaleSettings::fromJson(JsonObjectConst obj) {
    if (obj["enabled"].is<bool>()) enabled = obj["enabled"];
    if (obj["pairedAddress"].is<const char*>()) strncpy(pairedAddress, obj["pairedAddress"] | "", sizeof(pairedAddress) - 1);
    if (obj["pairedName"].is<const char*>()) strncpy(pairedName, obj["pairedName"] | "", sizeof(pairedName) - 1);
    if (obj["scaleType"].is<uint8_t>()) scaleType = obj["scaleType"];
    return true;
}

// =============================================================================
// DisplaySettings
// =============================================================================

void DisplaySettings::toJson(JsonObject& obj) const {
    obj["brightness"] = brightness;
    obj["screenTimeout"] = screenTimeout;
    obj["showShotTimer"] = showShotTimer;
    obj["showWeight"] = showWeight;
    obj["showPressure"] = showPressure;
}

bool DisplaySettings::fromJson(JsonObjectConst obj) {
    if (obj["brightness"].is<uint8_t>()) brightness = obj["brightness"];
    if (obj["screenTimeout"].is<uint16_t>()) screenTimeout = obj["screenTimeout"];
    if (obj["showShotTimer"].is<bool>()) showShotTimer = obj["showShotTimer"];
    if (obj["showWeight"].is<bool>()) showWeight = obj["showWeight"];
    if (obj["showPressure"].is<bool>()) showPressure = obj["showPressure"];
    return true;
}

// =============================================================================
// UserPreferences
// =============================================================================

void UserPreferences::toJson(JsonObject& obj) const {
    obj["firstDayOfWeek"] = firstDayOfWeek == 0 ? "sunday" : "monday";
    obj["use24HourTime"] = use24HourTime;
    obj["temperatureUnit"] = temperatureUnit == 0 ? "celsius" : "fahrenheit";
    obj["electricityPrice"] = electricityPrice;
    obj["currency"] = currency;
    obj["lastHeatingStrategy"] = lastHeatingStrategy;
    obj["initialized"] = initialized;
}

bool UserPreferences::fromJson(JsonObjectConst obj) {
    if (obj["firstDayOfWeek"].is<const char*>()) {
        const char* dow = obj["firstDayOfWeek"];
        firstDayOfWeek = (strcmp(dow, "monday") == 0) ? 1 : 0;
    }
    if (obj["use24HourTime"].is<bool>()) use24HourTime = obj["use24HourTime"];
    if (obj["temperatureUnit"].is<const char*>()) {
        const char* unit = obj["temperatureUnit"];
        temperatureUnit = (strcmp(unit, "fahrenheit") == 0) ? 1 : 0;
    }
    if (obj["electricityPrice"].is<float>()) electricityPrice = obj["electricityPrice"];
    if (obj["currency"].is<const char*>()) {
        strncpy(currency, obj["currency"], sizeof(currency) - 1);
        currency[sizeof(currency) - 1] = '\0';
    }
    if (obj["lastHeatingStrategy"].is<uint8_t>()) lastHeatingStrategy = obj["lastHeatingStrategy"];
    if (obj["initialized"].is<bool>()) initialized = obj["initialized"];
    return true;
}

// =============================================================================
// SystemSettings
// =============================================================================

void SystemSettings::toJson(JsonObject& obj) const {
    obj["setupComplete"] = setupComplete;
}

bool SystemSettings::fromJson(JsonObjectConst obj) {
    if (obj["setupComplete"].is<bool>()) setupComplete = obj["setupComplete"];
    return true;
}

// =============================================================================
// MachineInfoSettings
// =============================================================================

void MachineInfoSettings::toJson(JsonObject& obj) const {
    obj["deviceName"] = deviceName;
    obj["machineBrand"] = machineBrand;
    obj["machineModel"] = machineModel;
    obj["machineType"] = machineType;
}

bool MachineInfoSettings::fromJson(JsonObjectConst obj) {
    if (obj["deviceName"].is<const char*>()) {
        strncpy(deviceName, obj["deviceName"] | "BrewOS", sizeof(deviceName) - 1);
        deviceName[sizeof(deviceName) - 1] = '\0';
    }
    if (obj["machineBrand"].is<const char*>()) {
        strncpy(machineBrand, obj["machineBrand"] | "", sizeof(machineBrand) - 1);
        machineBrand[sizeof(machineBrand) - 1] = '\0';
    }
    if (obj["machineModel"].is<const char*>()) {
        strncpy(machineModel, obj["machineModel"] | "", sizeof(machineModel) - 1);
        machineModel[sizeof(machineModel) - 1] = '\0';
    }
    if (obj["machineType"].is<const char*>()) {
        strncpy(machineType, obj["machineType"] | "dual_boiler", sizeof(machineType) - 1);
        machineType[sizeof(machineType) - 1] = '\0';
    }
    return true;
}

// =============================================================================
// NotificationSettings
// =============================================================================

void NotificationSettings::toJson(JsonObject& obj) const {
    obj["machineReady"] = machineReady;
    obj["waterEmpty"] = waterEmpty;
    obj["descaleDue"] = descaleDue;
    obj["serviceDue"] = serviceDue;
    obj["backflushDue"] = backflushDue;
    obj["machineError"] = machineError;
    obj["picoOffline"] = picoOffline;
    obj["scheduleTriggered"] = scheduleTriggered;
    obj["brewComplete"] = brewComplete;
}

bool NotificationSettings::fromJson(JsonObjectConst obj) {
    if (obj["machineReady"].is<bool>()) machineReady = obj["machineReady"];
    if (obj["waterEmpty"].is<bool>()) waterEmpty = obj["waterEmpty"];
    if (obj["descaleDue"].is<bool>()) descaleDue = obj["descaleDue"];
    if (obj["serviceDue"].is<bool>()) serviceDue = obj["serviceDue"];
    if (obj["backflushDue"].is<bool>()) backflushDue = obj["backflushDue"];
    if (obj["machineError"].is<bool>()) machineError = obj["machineError"];
    if (obj["picoOffline"].is<bool>()) picoOffline = obj["picoOffline"];
    if (obj["scheduleTriggered"].is<bool>()) scheduleTriggered = obj["scheduleTriggered"];
    if (obj["brewComplete"].is<bool>()) brewComplete = obj["brewComplete"];
    return true;
}

// =============================================================================
// Settings (combined)
// =============================================================================

void Settings::toJson(JsonDocument& doc) const {
    JsonObject tempObj = doc["temperature"].to<JsonObject>();
    temperature.toJson(tempObj);
    
    JsonObject brewObj = doc["brew"].to<JsonObject>();
    brew.toJson(brewObj);
    
    JsonObject powerObj = doc["power"].to<JsonObject>();
    power.toJson(powerObj);
    
    JsonObject networkObj = doc["network"].to<JsonObject>();
    network.toJson(networkObj);
    
    JsonObject timeObj = doc["time"].to<JsonObject>();
    time.toJson(timeObj);
    
    JsonObject mqttObj = doc["mqtt"].to<JsonObject>();
    mqtt.toJson(mqttObj);
    
    JsonObject cloudObj = doc["cloud"].to<JsonObject>();
    cloud.toJson(cloudObj);
    
    JsonObject scaleObj = doc["scale"].to<JsonObject>();
    scale.toJson(scaleObj);
    
    JsonObject displayObj = doc["display"].to<JsonObject>();
    display.toJson(displayObj);
    
    JsonObject machineInfoObj = doc["machineInfo"].to<JsonObject>();
    machineInfo.toJson(machineInfoObj);
    
    JsonObject notificationsObj = doc["notifications"].to<JsonObject>();
    notifications.toJson(notificationsObj);
    
    JsonObject systemObj = doc["system"].to<JsonObject>();
    system.toJson(systemObj);
}

bool Settings::fromJson(const JsonDocument& doc) {
    // In ArduinoJson 7.x, we access nested objects directly through the variant
    JsonVariantConst tempVar = doc["temperature"];
    if (tempVar.is<JsonObjectConst>()) {
        JsonObjectConst obj = tempVar.as<JsonObjectConst>();
        temperature.fromJson(obj);
    }
    
    JsonVariantConst brewVar = doc["brew"];
    if (brewVar.is<JsonObjectConst>()) {
        JsonObjectConst obj = brewVar.as<JsonObjectConst>();
        brew.fromJson(obj);
    }
    
    JsonVariantConst powerVar = doc["power"];
    if (powerVar.is<JsonObjectConst>()) {
        JsonObjectConst obj = powerVar.as<JsonObjectConst>();
        power.fromJson(obj);
    }
    
    JsonVariantConst networkVar = doc["network"];
    if (networkVar.is<JsonObjectConst>()) {
        JsonObjectConst obj = networkVar.as<JsonObjectConst>();
        network.fromJson(obj);
    }
    
    JsonVariantConst timeVar = doc["time"];
    if (timeVar.is<JsonObjectConst>()) {
        JsonObjectConst obj = timeVar.as<JsonObjectConst>();
        time.fromJson(obj);
    }
    
    JsonVariantConst mqttVar = doc["mqtt"];
    if (mqttVar.is<JsonObjectConst>()) {
        JsonObjectConst obj = mqttVar.as<JsonObjectConst>();
        mqtt.fromJson(obj);
    }
    
    JsonVariantConst cloudVar = doc["cloud"];
    if (cloudVar.is<JsonObjectConst>()) {
        JsonObjectConst obj = cloudVar.as<JsonObjectConst>();
        cloud.fromJson(obj);
    }
    
    JsonVariantConst scaleVar = doc["scale"];
    if (scaleVar.is<JsonObjectConst>()) {
        JsonObjectConst obj = scaleVar.as<JsonObjectConst>();
        scale.fromJson(obj);
    }
    
    JsonVariantConst displayVar = doc["display"];
    if (displayVar.is<JsonObjectConst>()) {
        JsonObjectConst obj = displayVar.as<JsonObjectConst>();
        display.fromJson(obj);
    }
    
    JsonVariantConst machineInfoVar = doc["machineInfo"];
    if (machineInfoVar.is<JsonObjectConst>()) {
        JsonObjectConst obj = machineInfoVar.as<JsonObjectConst>();
        machineInfo.fromJson(obj);
    }
    
    JsonVariantConst notificationsVar = doc["notifications"];
    if (notificationsVar.is<JsonObjectConst>()) {
        JsonObjectConst obj = notificationsVar.as<JsonObjectConst>();
        notifications.fromJson(obj);
    }
    
    JsonVariantConst systemVar = doc["system"];
    if (systemVar.is<JsonObjectConst>()) {
        JsonObjectConst obj = systemVar.as<JsonObjectConst>();
        system.fromJson(obj);
    }
    
    return true;
}

// =============================================================================
// Statistics
// =============================================================================

void Statistics::toJson(JsonObject& obj) const {
    // Lifetime
    obj["totalShots"] = totalShots;
    obj["totalSteamCycles"] = totalSteamCycles;
    obj["totalKwh"] = totalKwh;
    obj["totalOnTimeMinutes"] = totalOnTimeMinutes;
    
    // Daily
    obj["shotsToday"] = shotsToday;
    obj["kwhToday"] = kwhToday;
    obj["onTimeToday"] = onTimeToday;
    
    // Maintenance
    obj["shotsSinceDescale"] = shotsSinceDescale;
    obj["shotsSinceGroupClean"] = shotsSinceGroupClean;
    obj["shotsSinceBackflush"] = shotsSinceBackflush;
    obj["lastDescaleTimestamp"] = lastDescaleTimestamp;
    obj["lastGroupCleanTimestamp"] = lastGroupCleanTimestamp;
    obj["lastBackflushTimestamp"] = lastBackflushTimestamp;
    
    // Session
    obj["sessionStartTimestamp"] = sessionStartTimestamp;
    obj["sessionShots"] = sessionShots;
}

bool Statistics::fromJson(JsonObjectConst obj) {
    if (obj["totalShots"].is<uint32_t>()) totalShots = obj["totalShots"];
    if (obj["totalSteamCycles"].is<uint32_t>()) totalSteamCycles = obj["totalSteamCycles"];
    if (obj["totalKwh"].is<float>()) totalKwh = obj["totalKwh"];
    if (obj["totalOnTimeMinutes"].is<uint32_t>()) totalOnTimeMinutes = obj["totalOnTimeMinutes"];
    if (obj["shotsSinceDescale"].is<uint16_t>()) shotsSinceDescale = obj["shotsSinceDescale"];
    if (obj["shotsSinceGroupClean"].is<uint16_t>()) shotsSinceGroupClean = obj["shotsSinceGroupClean"];
    if (obj["shotsSinceBackflush"].is<uint16_t>()) shotsSinceBackflush = obj["shotsSinceBackflush"];
    if (obj["lastDescaleTimestamp"].is<uint32_t>()) lastDescaleTimestamp = obj["lastDescaleTimestamp"];
    if (obj["lastGroupCleanTimestamp"].is<uint32_t>()) lastGroupCleanTimestamp = obj["lastGroupCleanTimestamp"];
    if (obj["lastBackflushTimestamp"].is<uint32_t>()) lastBackflushTimestamp = obj["lastBackflushTimestamp"];
    return true;
}

void Statistics::resetDaily() {
    shotsToday = 0;
    kwhToday = 0;
    onTimeToday = 0;
}

void Statistics::recordMaintenance(const char* type) {
    uint32_t now = time(nullptr);
    if (strcmp(type, "descale") == 0) {
        shotsSinceDescale = 0;
        lastDescaleTimestamp = now;
    } else if (strcmp(type, "backflush") == 0) {
        // Backflush includes group clean - reset both counters
        shotsSinceBackflush = 0;
        lastBackflushTimestamp = now;
        shotsSinceGroupClean = 0;
        lastGroupCleanTimestamp = now;
    }
}

// =============================================================================
// ShotRecord
// =============================================================================

void ShotRecord::toJson(JsonObject& obj) const {
    obj["timestamp"] = timestamp;
    obj["doseWeight"] = doseWeight;
    obj["yieldWeight"] = yieldWeight;
    obj["durationMs"] = durationMs;
    obj["preinfusionMs"] = preinfusionMs;
    obj["avgFlowRate"] = avgFlowRate;
    obj["peakPressure"] = peakPressure;
    obj["avgTemperature"] = avgTemperature;
    obj["rating"] = rating;
    obj["ratio"] = ratio();
}

bool ShotRecord::fromJson(JsonObjectConst obj) {
    if (obj["timestamp"].is<uint32_t>()) timestamp = obj["timestamp"];
    if (obj["doseWeight"].is<float>()) doseWeight = obj["doseWeight"];
    if (obj["yieldWeight"].is<float>()) yieldWeight = obj["yieldWeight"];
    if (obj["durationMs"].is<uint32_t>()) durationMs = obj["durationMs"];
    if (obj["preinfusionMs"].is<uint32_t>()) preinfusionMs = obj["preinfusionMs"];
    if (obj["avgFlowRate"].is<float>()) avgFlowRate = obj["avgFlowRate"];
    if (obj["peakPressure"].is<float>()) peakPressure = obj["peakPressure"];
    if (obj["avgTemperature"].is<float>()) avgTemperature = obj["avgTemperature"];
    if (obj["rating"].is<uint8_t>()) rating = obj["rating"];
    return true;
}

// =============================================================================
// ShotHistory
// =============================================================================

void ShotHistory::addShot(const ShotRecord& shot) {
    shots[head] = shot;
    head = (head + 1) % MAX_SHOT_HISTORY;
    if (count < MAX_SHOT_HISTORY) count++;
}

const ShotRecord* ShotHistory::getShot(uint8_t index) const {
    if (index >= count) return nullptr;
    // Most recent is at (head - 1), going backwards
    int actualIndex = (head - 1 - index + MAX_SHOT_HISTORY) % MAX_SHOT_HISTORY;
    return &shots[actualIndex];
}

void ShotHistory::toJson(JsonArray& arr) const {
    for (uint8_t i = 0; i < count; i++) {
        const ShotRecord* shot = getShot(i);
        if (shot) {
            JsonObject obj = arr.add<JsonObject>();
            shot->toJson(obj);
        }
    }
}

bool ShotHistory::fromJson(JsonArrayConst arr) {
    clear();
    for (JsonObjectConst obj : arr) {
        ShotRecord shot;
        shot.fromJson(obj);
        addShot(shot);
    }
    return true;
}

void ShotHistory::clear() {
    head = 0;
    count = 0;
}

// =============================================================================
// RuntimeState
// =============================================================================

void RuntimeState::toJson(JsonObject& obj) const {
    obj["state"] = machineStateToString(state);
    obj["mode"] = machineModeToString(mode);
    obj["brewTemp"] = brewTemp;
    obj["steamTemp"] = steamTemp;
    obj["brewHeating"] = brewHeating;
    obj["steamHeating"] = steamHeating;
    obj["pressure"] = pressure;
    obj["flowRate"] = flowRate;
    obj["powerWatts"] = powerWatts;
    obj["voltage"] = voltage;
    obj["waterLevel"] = waterLevel;
    obj["scaleConnected"] = scaleConnected;
    obj["scaleWeight"] = scaleWeight;
    obj["scaleFlowRate"] = scaleFlowRate;
    obj["scaleStable"] = scaleStable;
    obj["shotActive"] = shotActive;
    obj["shotStartTime"] = shotStartTime;
    obj["shotWeight"] = shotWeight;
    obj["wifiConnected"] = wifiConnected;
    obj["mqttConnected"] = mqttConnected;
    obj["cloudConnected"] = cloudConnected;
    obj["picoConnected"] = picoConnected;
    obj["uptime"] = uptime;
}

// =============================================================================
// Enum Helpers
// =============================================================================

const char* machineStateToString(MachineState state) {
    switch (state) {
        case MachineState::INIT: return "init";
        case MachineState::IDLE: return "idle";
        case MachineState::HEATING: return "heating";
        case MachineState::READY: return "ready";
        case MachineState::BREWING: return "brewing";
        case MachineState::STEAMING: return "steaming";
        case MachineState::COOLDOWN: return "cooldown";
        case MachineState::ECO: return "eco";
        case MachineState::FAULT: return "fault";
        default: return "unknown";
    }
}

const char* machineModeToString(MachineMode mode) {
    switch (mode) {
        case MachineMode::STANDBY: return "standby";
        case MachineMode::ON: return "on";
        case MachineMode::ECO: return "eco";
        default: return "unknown";
    }
}

MachineState stringToMachineState(const char* str) {
    if (strcmp(str, "init") == 0) return MachineState::INIT;
    if (strcmp(str, "idle") == 0) return MachineState::IDLE;
    if (strcmp(str, "heating") == 0) return MachineState::HEATING;
    if (strcmp(str, "ready") == 0) return MachineState::READY;
    if (strcmp(str, "brewing") == 0) return MachineState::BREWING;
    if (strcmp(str, "steaming") == 0) return MachineState::STEAMING;
    if (strcmp(str, "cooldown") == 0) return MachineState::COOLDOWN;
    if (strcmp(str, "eco") == 0) return MachineState::ECO;
    if (strcmp(str, "fault") == 0) return MachineState::FAULT;
    return MachineState::INIT;
}

MachineMode stringToMachineMode(const char* str) {
    if (strcmp(str, "standby") == 0) return MachineMode::STANDBY;
    if (strcmp(str, "on") == 0) return MachineMode::ON;
    if (strcmp(str, "eco") == 0) return MachineMode::ECO;
    return MachineMode::STANDBY;
}

// =============================================================================
// ScheduleEntry
// =============================================================================

void ScheduleEntry::toJson(JsonObject& obj) const {
    obj["id"] = id;
    obj["enabled"] = enabled;
    obj["days"] = days;
    obj["hour"] = hour;
    obj["minute"] = minute;
    obj["action"] = (action == ACTION_TURN_ON) ? "on" : "off";
    obj["strategy"] = strategy;
    obj["name"] = name;
}

bool ScheduleEntry::fromJson(JsonObjectConst obj) {
    if (obj["id"].is<uint8_t>()) id = obj["id"];
    if (obj["enabled"].is<bool>()) enabled = obj["enabled"];
    if (obj["days"].is<uint8_t>()) days = obj["days"];
    if (obj["hour"].is<uint8_t>()) hour = obj["hour"];
    if (obj["minute"].is<uint8_t>()) minute = obj["minute"];
    
    if (obj["action"].is<const char*>()) {
        const char* actionStr = obj["action"];
        action = (strcmp(actionStr, "on") == 0) ? ACTION_TURN_ON : ACTION_TURN_OFF;
    } else if (obj["action"].is<uint8_t>()) {
        action = (ScheduleAction)obj["action"].as<uint8_t>();
    }
    
    if (obj["strategy"].is<uint8_t>()) strategy = (HeatingStrategy)obj["strategy"].as<uint8_t>();
    if (obj["name"].is<const char*>()) strncpy(name, obj["name"] | "", sizeof(name) - 1);
    
    return true;
}

bool ScheduleEntry::isValidForDay(uint8_t dayOfWeek) const {
    // dayOfWeek: 0=Sun, 1=Mon, etc.
    uint8_t dayMask = 1 << dayOfWeek;
    return (days & dayMask) != 0;
}

bool ScheduleEntry::matchesTime(uint8_t h, uint8_t m) const {
    return hour == h && minute == m;
}

// =============================================================================
// ScheduleSettings
// =============================================================================

void ScheduleSettings::toJson(JsonObject& obj) const {
    JsonArray arr = obj["schedules"].to<JsonArray>();
    for (size_t i = 0; i < MAX_SCHEDULES; i++) {
        if (schedules[i].id > 0) {
            JsonObject schedObj = arr.add<JsonObject>();
            schedules[i].toJson(schedObj);
        }
    }
    
    obj["autoPowerOffEnabled"] = autoPowerOffEnabled;
    obj["autoPowerOffMinutes"] = autoPowerOffMinutes;
}

bool ScheduleSettings::fromJson(JsonObjectConst obj) {
    // Clear existing schedules
    for (size_t i = 0; i < MAX_SCHEDULES; i++) {
        schedules[i] = ScheduleEntry();
    }
    count = 0;
    
    // Parse schedules array
    JsonArrayConst arr = obj["schedules"];
    if (!arr.isNull()) {
        size_t idx = 0;
        for (JsonObjectConst schedObj : arr) {
            if (idx >= MAX_SCHEDULES) break;
            schedules[idx].fromJson(schedObj);
            if (schedules[idx].id > 0) {
                count++;
            }
            idx++;
        }
    }
    
    if (obj["autoPowerOffEnabled"].is<bool>()) autoPowerOffEnabled = obj["autoPowerOffEnabled"];
    if (obj["autoPowerOffMinutes"].is<uint16_t>()) autoPowerOffMinutes = obj["autoPowerOffMinutes"];
    
    return true;
}

ScheduleEntry* ScheduleSettings::findById(uint8_t id) {
    for (size_t i = 0; i < MAX_SCHEDULES; i++) {
        if (schedules[i].id == id) {
            return &schedules[i];
        }
    }
    return nullptr;
}

const ScheduleEntry* ScheduleSettings::findById(uint8_t id) const {
    for (size_t i = 0; i < MAX_SCHEDULES; i++) {
        if (schedules[i].id == id) {
            return &schedules[i];
        }
    }
    return nullptr;
}

uint8_t ScheduleSettings::getNextId() const {
    uint8_t maxId = 0;
    for (size_t i = 0; i < MAX_SCHEDULES; i++) {
        if (schedules[i].id > maxId) {
            maxId = schedules[i].id;
        }
    }
    return maxId + 1;
}

uint8_t ScheduleSettings::addSchedule(const ScheduleEntry& entry) {
    // Find empty slot
    for (size_t i = 0; i < MAX_SCHEDULES; i++) {
        if (schedules[i].id == 0) {
            schedules[i] = entry;
            schedules[i].id = getNextId();
            count++;
            return schedules[i].id;
        }
    }
    return 0;  // No space
}

bool ScheduleSettings::removeSchedule(uint8_t id) {
    for (size_t i = 0; i < MAX_SCHEDULES; i++) {
        if (schedules[i].id == id) {
            schedules[i] = ScheduleEntry();  // Clear
            count--;
            return true;
        }
    }
    return false;
}

} // namespace BrewOS
