#include "web_server.h"
#include "config.h"
#include "cloud_connection.h"
#include "state/state_manager.h"
#include "statistics/statistics_manager.h"
#include "power_meter/power_meter_manager.h"
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <stdarg.h>
#include <stdio.h>

// Internal helper to broadcast a formatted log message
static void broadcastLogInternal(AsyncWebSocket* ws, CloudConnection* cloudConnection, 
                                 const char* level, const char* message) {
    // Defensive checks: ensure all required pointers are valid
    if (!message || !ws) return;
    
    // Additional safety: check if WebSocket is in a valid state
    // Accessing invalid WebSocket can cause LoadProhibited crashes
    
    // Use stack allocation to avoid PSRAM crashes
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    StaticJsonDocument<512> doc;
    #pragma GCC diagnostic pop
    doc["type"] = "log";
    doc["level"] = level ? level : "info";
    doc["message"] = message;
    doc["timestamp"] = millis();
    
    // Allocate JSON buffer in internal RAM (not PSRAM) to avoid crashes
    size_t jsonSize = measureJson(doc) + 1;
    char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!jsonBuffer) {
        // Fallback to regular malloc if heap_caps_malloc fails
        jsonBuffer = (char*)malloc(jsonSize);
    }
    
    if (jsonBuffer) {
        serializeJson(doc, jsonBuffer, jsonSize);
        ws->textAll(jsonBuffer);
        
        // Also send to cloud - use jsonBuffer directly to avoid String allocation
        if (cloudConnection) {
            cloudConnection->send(jsonBuffer);
        }
        
        free(jsonBuffer);
    }
}

// Variadic version - format string with arguments (like printf)
// Variadic version - format string with arguments (like printf), defaults to "info"
void WebServer::broadcastLog(const char* format, ...) {
    if (!format) return;
    
    // Format message into stack-allocated buffer (internal RAM, not PSRAM)
    char message[256];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    broadcastLogInternal(&_ws, _cloudConnection, "info", message);
}

// Variadic version with explicit level (level, format, ...args)
void WebServer::broadcastLogLevel(const char* level, const char* format, ...) {
    if (!format || !level) return;
    
    // Format message into stack-allocated buffer (internal RAM, not PSRAM)
    char message[256];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    broadcastLogInternal(&_ws, _cloudConnection, level, message);
}


void WebServer::broadcastPicoMessage(uint8_t type, const uint8_t* payload, size_t len) {
    // Use stack allocation to avoid PSRAM crashes
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    StaticJsonDocument<512> doc;
    #pragma GCC diagnostic pop
    doc["type"] = "pico";
    doc["msgType"] = type;
    doc["timestamp"] = millis();
    
    // Convert payload to hex string for debugging - use stack buffer
    char hexPayload[128] = {0};  // Max 56 bytes * 2 = 112 chars + null
    size_t hexLen = 0;
    for (size_t i = 0; i < len && i < 56 && hexLen < sizeof(hexPayload) - 2; i++) {
        hexLen += snprintf(hexPayload + hexLen, sizeof(hexPayload) - hexLen, "%02X", payload[i]);
    }
    doc["payload"] = hexPayload;
    doc["length"] = len;
    
    // Allocate JSON buffer in internal RAM (not PSRAM) to avoid crashes
    size_t jsonSize = measureJson(doc) + 1;
    char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!jsonBuffer) {
        jsonBuffer = (char*)malloc(jsonSize);
    }
    
    if (jsonBuffer) {
        serializeJson(doc, jsonBuffer, jsonSize);
        _ws.textAll(jsonBuffer);
        
        // Also send to cloud - use jsonBuffer directly to avoid String allocation
        if (_cloudConnection) {
            _cloudConnection->send(jsonBuffer);
        }
        
        free(jsonBuffer);
    }
}

void WebServer::broadcastRaw(const String& json) {
    _ws.textAll(json.c_str(), json.length());
    
    // Also send to cloud
    if (_cloudConnection) {
        _cloudConnection->send(json);
    }
}

void WebServer::broadcastRaw(const char* json) {
    if (!json) return;
    _ws.textAll(json);
    
    // Also send to cloud
    if (_cloudConnection) {
        _cloudConnection->send(json);
    }
}

// =============================================================================
// Unified Status Broadcast - Single comprehensive message
// =============================================================================
void WebServer::broadcastFullStatus(const ui_state_t& state) {
    // Skip status broadcasts during OTA to prevent WebSocket queue overflow
    if (_otaInProgress) {
        return;
    }
    
    // Safety check - only broadcast if we have clients
    if (_ws.count() == 0 && (!_cloudConnection || !_cloudConnection->isConnected())) {
        return;  // No one to send to
    }
    
    // Use heap allocation to avoid stack overflow (4KB is too large for stack)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    DynamicJsonDocument* docPtr = new DynamicJsonDocument(4096);
    if (!docPtr) {
        LOG_E("Failed to allocate JSON document for status broadcast");
        return;
    }
    DynamicJsonDocument& doc = *docPtr;
    #pragma GCC diagnostic pop
    doc["type"] = "status";
    
    // Timestamps - track machine on time and last shot
    // Use Unix timestamps (milliseconds) for client compatibility
    static uint64_t machineOnTimestamp = 0;
    static uint64_t lastShotTimestamp = 0;
    static bool wasOn = false;
    
    // Machine is "on" when in active states (heating, ready, brewing)
    bool isOn = state.machine_state >= UI_STATE_HEATING && state.machine_state <= UI_STATE_BREWING;
    
    // Track when machine turns on (Unix timestamp in milliseconds)
    // Bounds: year 2020 (1577836800) to year 2100 (4102444800) for overflow protection
    constexpr time_t MIN_VALID_TIME = 1577836800;  // Jan 1, 2020
    constexpr time_t MAX_VALID_TIME = 4102444800;  // Jan 1, 2100
    
    if (isOn && !wasOn) {
        time_t now = time(nullptr);
        // Only set timestamp if NTP is synced and within reasonable bounds
        if (now > MIN_VALID_TIME && now < MAX_VALID_TIME) {
            machineOnTimestamp = (uint64_t)now * 1000ULL;
        } else {
            machineOnTimestamp = 0;  // Time not synced or out of bounds
        }
    } else if (!isOn) {
        machineOnTimestamp = 0;
    }
    wasOn = isOn;
    
    // Track last shot timestamp (Unix timestamp in milliseconds)
    static bool wasBrewing = false;
    if (wasBrewing && !state.is_brewing) {
        time_t now = time(nullptr);
        // Only set timestamp if NTP is synced and within reasonable bounds
        if (now > MIN_VALID_TIME && now < MAX_VALID_TIME) {
            lastShotTimestamp = (uint64_t)now * 1000ULL;
        }
    }
    wasBrewing = state.is_brewing;
    
    // =========================================================================
    // Machine Section
    // =========================================================================
    JsonObject machine = doc["machine"].to<JsonObject>();
    
    // Machine state - convert to string for web client
    // MUST match Pico state values exactly!
    const char* stateStr = "unknown";
    switch (state.machine_state) {
        case UI_STATE_INIT: stateStr = "init"; break;
        case UI_STATE_IDLE: stateStr = "idle"; break;
        case UI_STATE_HEATING: stateStr = "heating"; break;
        case UI_STATE_READY: stateStr = "ready"; break;
        case UI_STATE_BREWING: stateStr = "brewing"; break;
        case UI_STATE_FAULT: stateStr = "fault"; break;
        case UI_STATE_SAFE: stateStr = "safe"; break;
        case UI_STATE_ECO: stateStr = "eco"; break;
    }
    machine["state"] = stateStr;
    
    // Machine mode - derive from state
    const char* modeStr = "standby";
    if (state.machine_state >= UI_STATE_HEATING && state.machine_state <= UI_STATE_BREWING) {
        modeStr = "on";
    } else if (state.machine_state == UI_STATE_ECO) {
        modeStr = "eco";
    }
    machine["mode"] = modeStr;
    machine["isHeating"] = state.is_heating;
    machine["isBrewing"] = state.is_brewing;
    machine["heatingStrategy"] = state.heating_strategy;
    
    // Timestamps (Unix milliseconds for JavaScript compatibility)
    if (machineOnTimestamp > 0) {
        machine["machineOnTimestamp"] = (double)machineOnTimestamp;  // Cast to double for JSON
    } else {
        machine["machineOnTimestamp"] = (char*)nullptr;
    }
    if (lastShotTimestamp > 0) {
        machine["lastShotTimestamp"] = (double)lastShotTimestamp;  // Cast to double for JSON
    } else {
        machine["lastShotTimestamp"] = (char*)nullptr;
    }
    
    // =========================================================================
    // Temperatures Section
    // =========================================================================
    JsonObject temps = doc["temps"].to<JsonObject>();
    JsonObject brew = temps["brew"].to<JsonObject>();
    brew["current"] = state.brew_temp;
    brew["setpoint"] = state.brew_setpoint;
    
    JsonObject steam = temps["steam"].to<JsonObject>();
    steam["current"] = state.steam_temp;
    steam["setpoint"] = state.steam_setpoint;
    
    temps["group"] = state.group_temp;
    
    // =========================================================================
    // Pressure
    // =========================================================================
    doc["pressure"] = state.pressure;
    
    // =========================================================================
    // Power Section
    // =========================================================================
    JsonObject power = doc["power"].to<JsonObject>();
    power["current"] = state.power_watts;
    
    // Get power meter reading if available
    PowerMeterReading meterReading;
    if (powerMeterManager && powerMeterManager->getReading(meterReading)) {
        power["voltage"] = meterReading.voltage;
        power["todayKwh"] = powerMeterManager->getTodayKwh();  // Daily energy (since midnight)
        power["totalKwh"] = powerMeterManager->getTotalKwh();  // Total energy from meter
        
        // Add meter info
        JsonObject meter = power["meter"].to<JsonObject>();
        meter["source"] = powerMeterManager ? powerMeterSourceToString(powerMeterManager->getSource()) : "none";
        meter["connected"] = powerMeterManager ? powerMeterManager->isConnected() : false;
        meter["meterType"] = powerMeterManager ? powerMeterManager->getMeterName() : "";
        meter["lastUpdate"] = meterReading.timestamp;
        
        JsonObject reading = meter["reading"].to<JsonObject>();
        reading["voltage"] = meterReading.voltage;
        reading["current"] = meterReading.current;
        reading["power"] = meterReading.power;
        reading["energy"] = meterReading.energy_import;
        reading["frequency"] = meterReading.frequency;
        reading["powerFactor"] = meterReading.power_factor;
    } else {
        // Fallback to configured voltage
        power["voltage"] = State.settings().power.mainsVoltage;
        power["todayKwh"] = 0;
        power["totalKwh"] = 0;
    }
    
    power["maxCurrent"] = State.settings().power.maxCurrent;
    
    // =========================================================================
    // Stats Section - Key metrics for dashboard
    // =========================================================================
    JsonObject stats = doc["stats"].to<JsonObject>();
    
    // Get current statistics
    BrewOS::FullStatistics fullStats;
    Stats.getFullStatistics(fullStats);
    
    // Daily stats
    JsonObject daily = stats["daily"].to<JsonObject>();
    BrewOS::PeriodStats dailyStats;
    Stats.getDailyStats(dailyStats);
    daily["shotCount"] = dailyStats.shotCount;
    daily["avgBrewTimeMs"] = dailyStats.avgBrewTimeMs;
    daily["totalKwh"] = dailyStats.totalKwh;
    
    // Lifetime stats
    JsonObject lifetime = stats["lifetime"].to<JsonObject>();
    lifetime["totalShots"] = fullStats.lifetime.totalShots;
    lifetime["avgBrewTimeMs"] = fullStats.lifetime.avgBrewTimeMs;
    lifetime["totalKwh"] = fullStats.lifetime.totalKwh;
    
    // Session stats
    stats["sessionShots"] = fullStats.sessionShots;
    stats["shotsToday"] = dailyStats.shotCount;
    
    // =========================================================================
    // Cleaning Section
    // =========================================================================
    JsonObject cleaning = doc["cleaning"].to<JsonObject>();
    cleaning["brewCount"] = state.brew_count;
    cleaning["reminderDue"] = state.cleaning_reminder;
    
    // =========================================================================
    // Water Section
    // =========================================================================
    JsonObject water = doc["water"].to<JsonObject>();
    water["tankLevel"] = state.water_low ? "low" : "ok";
    
    // =========================================================================
    // Scale Section
    // =========================================================================
    JsonObject scale = doc["scale"].to<JsonObject>();
    scale["connected"] = state.scale_connected;
    scale["weight"] = state.brew_weight;
    scale["flowRate"] = state.flow_rate;
    // Scale name and type come from scaleManager (accessed in main.cpp)
    
    // =========================================================================
    // Connections Section
    // =========================================================================
    JsonObject connections = doc["connections"].to<JsonObject>();
    connections["pico"] = state.pico_connected;
    connections["wifi"] = state.wifi_connected;
    connections["mqtt"] = state.mqtt_connected;
    connections["scale"] = state.scale_connected;
    connections["cloud"] = state.cloud_connected;
    
    // =========================================================================
    // Pico Info Section
    // =========================================================================
    JsonObject pico = doc["pico"].to<JsonObject>();
    pico["connected"] = state.pico_connected;
    
    // Get Pico version from StateManager (set when MSG_BOOT is received)
    const char* picoVersion = State.getPicoVersion();
    if (picoVersion && picoVersion[0] != '\0') {
        // Copy to stack buffer to avoid PSRAM issues
        char picoVerBuf[16];
        strncpy(picoVerBuf, picoVersion, sizeof(picoVerBuf) - 1);
        picoVerBuf[sizeof(picoVerBuf) - 1] = '\0';
        pico["version"] = picoVerBuf;
    } else {
        pico["version"] = (char*)nullptr;  // Will be omitted from JSON
    }
    
    // =========================================================================
    // WiFi Details Section
    // =========================================================================
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["connected"] = state.wifi_connected;
    wifi["apMode"] = state.wifi_ap_mode;
    wifi["ssid"] = state.wifi_ssid;
    wifi["ip"] = state.wifi_ip;
    wifi["rssi"] = state.wifi_rssi;
    
    // Get WiFi configuration from WiFiManager (gateway, subnet, DNS, static IP)
    WiFiStatus wifiStatus = _wifiManager.getStatus();
    wifi["staticIp"] = wifiStatus.staticIp;
    
    // Copy strings to stack buffers to avoid PSRAM issues
    char gatewayBuf[16];
    char subnetBuf[16];
    char dns1Buf[16];
    char dns2Buf[16];
    
    strncpy(gatewayBuf, wifiStatus.gateway.c_str(), sizeof(gatewayBuf) - 1);
    gatewayBuf[sizeof(gatewayBuf) - 1] = '\0';
    strncpy(subnetBuf, wifiStatus.subnet.c_str(), sizeof(subnetBuf) - 1);
    subnetBuf[sizeof(subnetBuf) - 1] = '\0';
    strncpy(dns1Buf, wifiStatus.dns1.c_str(), sizeof(dns1Buf) - 1);
    dns1Buf[sizeof(dns1Buf) - 1] = '\0';
    strncpy(dns2Buf, wifiStatus.dns2.c_str(), sizeof(dns2Buf) - 1);
    dns2Buf[sizeof(dns2Buf) - 1] = '\0';
    
    wifi["gateway"] = gatewayBuf;
    wifi["subnet"] = subnetBuf;
    wifi["dns1"] = dns1Buf;
    wifi["dns2"] = dns2Buf;
    
    // =========================================================================
    // ESP32 Info
    // =========================================================================
    JsonObject esp32 = doc["esp32"].to<JsonObject>();
    esp32["version"] = ESP32_VERSION;
    esp32["freeHeap"] = ESP.getFreeHeap();
    esp32["uptime"] = millis();
    
    // Allocate JSON buffer in internal RAM (not PSRAM) to avoid crashes
    size_t jsonSize = measureJson(doc) + 1;
    char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!jsonBuffer) {
        // Fallback to regular malloc if heap_caps_malloc fails
        jsonBuffer = (char*)malloc(jsonSize);
    }
    
    if (jsonBuffer) {
        serializeJson(doc, jsonBuffer, jsonSize);
        
        // Send to WebSocket clients (check count again to be safe)
        if (_ws.count() > 0) {
            _ws.textAll(jsonBuffer);
        }
        
        // Also send to cloud - use jsonBuffer directly to avoid String allocation
        if (_cloudConnection && _cloudConnection->isConnected()) {
            _cloudConnection->send(jsonBuffer);
        }
        
        free(jsonBuffer);
    }
    
    // Free the JSON document
    delete docPtr;
}

void WebServer::broadcastDeviceInfo() {
    // Use stack allocation to avoid PSRAM crashes
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    StaticJsonDocument<2048> doc;
    #pragma GCC diagnostic pop
    doc["type"] = "device_info";
    
    // Get device info from state manager
    const auto& machineInfo = State.settings().machineInfo;
    const auto& cloudSettings = State.settings().cloud;
    const auto& powerSettings = State.settings().power;
    const auto& tempSettings = State.settings().temperature;
    const auto& prefs = State.settings().preferences;
    
    doc["deviceId"] = cloudSettings.deviceId;
    doc["deviceName"] = machineInfo.deviceName;
    doc["machineBrand"] = machineInfo.machineBrand;
    doc["machineModel"] = machineInfo.machineModel;
    doc["machineType"] = machineInfo.machineType;
    doc["firmwareVersion"] = ESP32_VERSION;
    
    // Include power settings
    doc["mainsVoltage"] = powerSettings.mainsVoltage;
    doc["maxCurrent"] = powerSettings.maxCurrent;
    
    // Include eco mode settings
    doc["ecoBrewTemp"] = tempSettings.ecoBrewTemp;
    doc["ecoTimeoutMinutes"] = tempSettings.ecoTimeoutMinutes;
    
    // Include pre-infusion settings
    const auto& brewSettings = State.settings().brew;
    // preinfusionPressure > 0 is used as the enabled flag (legacy compatibility)
    doc["preinfusionEnabled"] = brewSettings.preinfusionPressure > 0;
    doc["preinfusionOnMs"] = (uint16_t)(brewSettings.preinfusionTime * 1000);  // Convert seconds to ms
    doc["preinfusionPauseMs"] = (uint16_t)(brewSettings.preinfusionPressure > 0 ? 5000 : 0);  // Default pause
    
    // Include user preferences (synced across devices)
    JsonObject preferences = doc["preferences"].to<JsonObject>();
    prefs.toJson(preferences);
    
    // Allocate JSON buffer in internal RAM (not PSRAM) to avoid crashes
    size_t jsonSize = measureJson(doc) + 1;
    char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!jsonBuffer) {
        jsonBuffer = (char*)malloc(jsonSize);
    }
    
    if (jsonBuffer) {
        serializeJson(doc, jsonBuffer, jsonSize);
        _ws.textAll(jsonBuffer);
        
        // Also send to cloud - use jsonBuffer directly to avoid String allocation
        if (_cloudConnection) {
            _cloudConnection->send(jsonBuffer);
        }
        
        free(jsonBuffer);
    }
}

void WebServer::broadcastPowerMeterStatus() {
    // Skip during OTA to prevent WebSocket queue overflow
    if (_otaInProgress) {
        return;
    }
    
    // Use stack allocation to avoid PSRAM crashes
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    StaticJsonDocument<1024> doc;
    #pragma GCC diagnostic pop
    doc["type"] = "power_meter_status";
    
    // Get status from power meter manager
    if (powerMeterManager) powerMeterManager->getStatus(doc);
    
    // Allocate JSON buffer in internal RAM (not PSRAM) to avoid crashes
    size_t jsonSize = measureJson(doc) + 1;
    char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!jsonBuffer) {
        jsonBuffer = (char*)malloc(jsonSize);
    }
    
    if (jsonBuffer) {
        serializeJson(doc, jsonBuffer, jsonSize);
        _ws.textAll(jsonBuffer);
        
        // Also send to cloud - use jsonBuffer directly to avoid String allocation
        if (_cloudConnection) {
            _cloudConnection->send(jsonBuffer);
        }
        
        free(jsonBuffer);
    }
}

void WebServer::broadcastEvent(const String& event, const JsonDocument* data) {
    // Use stack allocation to avoid PSRAM crashes
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    StaticJsonDocument<1024> doc;
    #pragma GCC diagnostic pop
    doc["type"] = "event";
    // Copy event string to avoid PSRAM pointer issues
    char eventBuf[64];
    strncpy(eventBuf, event.c_str(), sizeof(eventBuf) - 1);
    eventBuf[sizeof(eventBuf) - 1] = '\0';
    doc["event"] = eventBuf;
    doc["timestamp"] = millis();
    
    if (data) {
        doc["data"] = *data;
    }
    
    // Allocate JSON buffer in internal RAM (not PSRAM) to avoid crashes
    size_t jsonSize = measureJson(doc) + 1;
    char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!jsonBuffer) {
        jsonBuffer = (char*)malloc(jsonSize);
    }
    
    if (jsonBuffer) {
        serializeJson(doc, jsonBuffer, jsonSize);
        _ws.textAll(jsonBuffer);
        
        // Also send to cloud - use jsonBuffer directly to avoid String allocation
        if (_cloudConnection) {
            _cloudConnection->send(jsonBuffer);
        }
        
        free(jsonBuffer);
    }
}
