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

bool TemperatureSettings::fromJson(const JsonObject& obj) {
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

bool BrewSettings::fromJson(const JsonObject& obj) {
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

bool PowerSettings::fromJson(const JsonObject& obj) {
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

bool NetworkSettings::fromJson(const JsonObject& obj) {
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

bool MQTTSettings::fromJson(const JsonObject& obj) {
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

bool CloudSettings::fromJson(const JsonObject& obj) {
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

bool ScaleSettings::fromJson(const JsonObject& obj) {
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

bool DisplaySettings::fromJson(const JsonObject& obj) {
    if (obj["brightness"].is<uint8_t>()) brightness = obj["brightness"];
    if (obj["screenTimeout"].is<uint16_t>()) screenTimeout = obj["screenTimeout"];
    if (obj["showShotTimer"].is<bool>()) showShotTimer = obj["showShotTimer"];
    if (obj["showWeight"].is<bool>()) showWeight = obj["showWeight"];
    if (obj["showPressure"].is<bool>()) showPressure = obj["showPressure"];
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
    
    JsonObject mqttObj = doc["mqtt"].to<JsonObject>();
    mqtt.toJson(mqttObj);
    
    JsonObject cloudObj = doc["cloud"].to<JsonObject>();
    cloud.toJson(cloudObj);
    
    JsonObject scaleObj = doc["scale"].to<JsonObject>();
    scale.toJson(scaleObj);
    
    JsonObject displayObj = doc["display"].to<JsonObject>();
    display.toJson(displayObj);
}

bool Settings::fromJson(const JsonDocument& doc) {
    if (doc["temperature"].is<JsonObject>()) temperature.fromJson(doc["temperature"].as<JsonObject>());
    if (doc["brew"].is<JsonObject>()) brew.fromJson(doc["brew"].as<JsonObject>());
    if (doc["power"].is<JsonObject>()) power.fromJson(doc["power"].as<JsonObject>());
    if (doc["network"].is<JsonObject>()) network.fromJson(doc["network"].as<JsonObject>());
    if (doc["mqtt"].is<JsonObject>()) mqtt.fromJson(doc["mqtt"].as<JsonObject>());
    if (doc["cloud"].is<JsonObject>()) cloud.fromJson(doc["cloud"].as<JsonObject>());
    if (doc["scale"].is<JsonObject>()) scale.fromJson(doc["scale"].as<JsonObject>());
    if (doc["display"].is<JsonObject>()) display.fromJson(doc["display"].as<JsonObject>());
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

bool Statistics::fromJson(const JsonObject& obj) {
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
    } else if (strcmp(type, "groupclean") == 0) {
        shotsSinceGroupClean = 0;
        lastGroupCleanTimestamp = now;
    } else if (strcmp(type, "backflush") == 0) {
        shotsSinceBackflush = 0;
        lastBackflushTimestamp = now;
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

bool ShotRecord::fromJson(const JsonObject& obj) {
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

bool ShotHistory::fromJson(const JsonArray& arr) {
    clear();
    for (JsonObject obj : arr) {
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
    obj["dripTrayFull"] = dripTrayFull;
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

} // namespace BrewOS
