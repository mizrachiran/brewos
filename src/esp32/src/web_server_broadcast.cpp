#include "web_server.h"
#include "config.h"
#include "memory_utils.h"
#include "cloud_connection.h"
#include "mqtt_client.h"
#include "state/state_manager.h"
#include "statistics/statistics_manager.h"
#include "power_meter/power_meter_manager.h"
#include "brew_by_weight.h"
#include "msgpack_helper.h"
#include "utils/status_change_detector.h"
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <stdarg.h>
#include <stdio.h>

// External references for status broadcast
extern BrewByWeight* brewByWeight;
// Use getter function for thread-safe access to machine state
const ui_state_t& getMachineState();  // Defined in main.cpp

// =============================================================================
// Pre-allocated Broadcast Buffers (initialized in initBroadcastBuffers)
// =============================================================================
static SpiRamJsonDocument* g_statusDoc = nullptr;
static uint8_t* g_statusBuffer = nullptr;  // Changed to uint8_t for binary MessagePack
static const size_t STATUS_BUFFER_SIZE = 2048;  // Reduced - MessagePack is smaller

void initBroadcastBuffers() {
    if (!g_statusDoc) {
        g_statusDoc = new SpiRamJsonDocument(4096);  // JSON doc still needs 4KB
    }
    if (!g_statusBuffer) {
        g_statusBuffer = (uint8_t*)psram_malloc(STATUS_BUFFER_SIZE);
    }
}

// Internal helper to broadcast a formatted log message
// CRITICAL: During OTA, the WebSocket queue can fill up quickly.
// Check availableForWriteAll() before sending to prevent queue overflow
// which causes "Too many messages queued" and disconnects the client.
static void broadcastLogInternal(AsyncWebSocket* ws, CloudConnection* cloudConnection, 
                                 const char* level, const char* message) {
    // Defensive checks: ensure all required pointers are valid
    if (!message || !ws) return;
    
    // Clean up disconnected clients first
    ws->cleanupClients();
    
    // If no clients connected, nothing to do (but still send to cloud)
    if (ws->count() == 0 && !cloudConnection) return;
    
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
    char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!jsonBuffer) {
        // Fallback to regular malloc if heap_caps_malloc fails
        jsonBuffer = (char*)malloc(jsonSize);
    }
    
    if (jsonBuffer) {
        serializeJson(doc, jsonBuffer, jsonSize);
        
        // Only send to WebSocket if clients can receive (prevents queue overflow)
        // Log messages are less critical than OTA progress, so we just skip if queue is full
        // NOTE: availableForWriteAll() may not exist in all ESPAsyncWebServer versions
        // If compilation fails, replace with: iterate clients and check client->canSend()
        if (ws->count() > 0 && ws->availableForWriteAll()) {
            ws->textAll(jsonBuffer);
        }
        
        // Always try to send to cloud - it has its own queue management
        if (cloudConnection) {
            cloudConnection->send(jsonBuffer);
        }
        
        free(jsonBuffer);
    }
}

// Direct message broadcast (no formatting) - for platform_log
void BrewWebServer::broadcastLogMessage(const char* level, const char* message) {
    if (!message) return;
    broadcastLogInternal(&_ws, _cloudConnection, level, message);
}

// C wrapper for platform_log to broadcast via WebSocket
// This allows platform_log (in arduino_impl.h) to broadcast logs without including web_server.h
extern "C" void platform_broadcast_log(const char* level, const char* message) {
    // Access global webServer pointer (defined in main.cpp)
    extern BrewWebServer* webServer;
    
    if (!webServer || !message) {
        return;
    }
    
    // Use the direct message broadcast method (no formatting needed)
    webServer->broadcastLogMessage(level ? level : "info", message);
}

// Variadic version - format string with arguments (like printf)
// Variadic version - format string with arguments (like printf), defaults to "info"
void BrewWebServer::broadcastLog(const char* format, ...) {
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
void BrewWebServer::broadcastLogLevel(const char* level, const char* format, ...) {
    if (!format || !level) return;
    
    // Format message into stack-allocated buffer (internal RAM, not PSRAM)
    char message[256];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    broadcastLogInternal(&_ws, _cloudConnection, level, message);
}


void BrewWebServer::broadcastPicoMessage(uint8_t type, const uint8_t* payload, size_t len) {
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
    char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
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

void BrewWebServer::broadcastRaw(const String& json) {
    _ws.textAll(json.c_str(), json.length());
    
    // Also send to cloud
    if (_cloudConnection) {
        _cloudConnection->send(json);
    }
}

void BrewWebServer::broadcastRaw(const char* json) {
    if (!json) return;
    _ws.textAll(json);
    
    // Also send to cloud
    if (_cloudConnection) {
        _cloudConnection->send(json);
    }
}

// =============================================================================
// Build Delta Status - Only changed fields
// Returns true if delta was built, false if no changes
// =============================================================================
bool BrewWebServer::buildDeltaStatus(const ui_state_t& state, const ChangedFields& changed, 
                                      uint32_t sequence, JsonDocument& doc) {
    // Clear document and set type
    doc.clear();
    doc["type"] = "status_delta";
    doc["seq"] = sequence;
    
    // Track timestamps (same logic as full status)
    static uint64_t machineOnTimestamp = 0;
    static uint64_t lastShotTimestamp = 0;
    static bool wasOn = false;
    static bool wasBrewing = false;
    
    bool isOn = state.machine_state >= UI_STATE_HEATING && state.machine_state <= UI_STATE_BREWING;
    constexpr time_t MIN_VALID_TIME = 1577836800;
    constexpr time_t MAX_VALID_TIME = 4102444800;
    
    if (isOn && !wasOn) {
        time_t now = time(nullptr);
        if (now > MIN_VALID_TIME && now < MAX_VALID_TIME) {
            machineOnTimestamp = (uint64_t)now * 1000ULL;
        } else {
            machineOnTimestamp = 0;
        }
    } else if (!isOn) {
        machineOnTimestamp = 0;
    }
    wasOn = isOn;
    
    if (wasBrewing && !state.is_brewing) {
        time_t now = time(nullptr);
        if (now > MIN_VALID_TIME && now < MAX_VALID_TIME) {
            lastShotTimestamp = (uint64_t)now * 1000ULL;
        }
    }
    wasBrewing = state.is_brewing;
    
    // Machine section
    if (changed.machine_state || changed.machine_mode || changed.heating_strategy || 
        changed.is_heating || changed.is_brewing) {
        JsonObject machine = doc["machine"].to<JsonObject>();
        
        if (changed.machine_state || changed.machine_mode) {
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
            
            const char* modeStr = "standby";
            if (state.machine_state >= UI_STATE_HEATING && state.machine_state <= UI_STATE_BREWING) {
                modeStr = "on";
            } else if (state.machine_state == UI_STATE_ECO) {
                modeStr = "eco";
            }
            machine["mode"] = modeStr;
        }
        
        if (changed.is_heating) machine["isHeating"] = state.is_heating;
        if (changed.is_brewing) machine["isBrewing"] = state.is_brewing;
        if (changed.heating_strategy) machine["heatingStrategy"] = state.heating_strategy;
        
        // Timestamps (include if machine state changed)
        if (changed.machine_state) {
            if (machineOnTimestamp > 0) {
                machine["machineOnTimestamp"] = (double)machineOnTimestamp;
            }
            if (lastShotTimestamp > 0) {
                machine["lastShotTimestamp"] = (double)lastShotTimestamp;
            }
        }
    }
    
    // Temperatures
    if (changed.temps) {
        JsonObject temps = doc["temps"].to<JsonObject>();
        JsonObject brew = temps["brew"].to<JsonObject>();
        brew["current"] = state.brew_temp;
        brew["setpoint"] = state.brew_setpoint;
        
        JsonObject steam = temps["steam"].to<JsonObject>();
        steam["current"] = state.steam_temp;
        steam["setpoint"] = state.steam_setpoint;
        
        temps["group"] = state.group_temp;
    }
    
    // Pressure
    if (changed.pressure) {
        doc["pressure"] = state.pressure;
    }
    
    // Power
    if (changed.power) {
        JsonObject power = doc["power"].to<JsonObject>();
        power["current"] = state.power_watts;
        
        // Include power meter if available (simplified for delta)
        PowerMeterReading meterReading;
        if (powerMeterManager && powerMeterManager->getReading(meterReading)) {
            power["voltage"] = meterReading.voltage;
        } else {
            power["voltage"] = State.settings().power.mainsVoltage;
        }
    }
    
    // Scale
    if (changed.scale_weight || changed.scale_flow_rate || changed.scale_connected || 
        changed.target_weight) {
        JsonObject scale = doc["scale"].to<JsonObject>();
        if (changed.scale_connected) scale["connected"] = state.scale_connected;
        if (changed.scale_weight) scale["weight"] = state.brew_weight;
        if (changed.scale_flow_rate) scale["flowRate"] = state.flow_rate;
        
        if (changed.target_weight && brewByWeight) {
            JsonObject bbw = scale["bbw"].to<JsonObject>();
            bbw["targetWeight"] = brewByWeight->getTargetWeight();
        }
    }
    
    // Brew time (when brewing)
    if (changed.brew_time && state.is_brewing) {
        // Include in machine section for delta
        if (!doc["machine"].is<JsonObject>()) {
            doc["machine"].to<JsonObject>();
        }
        // Note: brew_time_ms is typically tracked separately, but for simplicity
        // we'll include it in the delta when brewing
    }
    
    // Connections
    if (changed.connections) {
        JsonObject connections = doc["connections"].to<JsonObject>();
        connections["pico"] = state.pico_connected;
        connections["wifi"] = state.wifi_connected;
        connections["mqtt"] = state.mqtt_connected;
        connections["scale"] = state.scale_connected;
        connections["cloud"] = state.cloud_connected;
    }
    
    // Water
    if (changed.water_low) {
        JsonObject water = doc["water"].to<JsonObject>();
        water["tankLevel"] = state.water_low ? "low" : "ok";
    }
    
    // Alarm
    if (changed.alarm) {
        if (!doc["machine"].is<JsonObject>()) {
            doc["machine"].to<JsonObject>();
        }
        // Note: alarm fields would go in machine section
    }
    
    // Cleaning
    if (changed.cleaning) {
        JsonObject cleaning = doc["cleaning"].to<JsonObject>();
        cleaning["brewCount"] = state.brew_count;
        cleaning["reminderDue"] = state.cleaning_reminder;
    }
    
    // WiFi (only if changed)
    if (changed.wifi) {
        JsonObject wifi = doc["wifi"].to<JsonObject>();
        wifi["connected"] = state.wifi_connected;
        wifi["apMode"] = state.wifi_ap_mode;
        wifi["ip"] = state.wifi_ip;
        wifi["rssi"] = state.wifi_rssi;
        
        WiFiStatus wifiStatus = _wifiManager.getStatus();
        wifi["staticIp"] = wifiStatus.staticIp;
        
        char gatewayBuf[16], subnetBuf[16], dns1Buf[16], dns2Buf[16];
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
    }
    
    // MQTT (only if changed)
    if (changed.mqtt) {
        JsonObject mqtt = doc["mqtt"].to<JsonObject>();
        MQTTConfig mqttConfig = _mqttClient.getConfig();
        mqtt["enabled"] = mqttConfig.enabled;
        mqtt["connected"] = _mqttClient.isConnected();
        
        char brokerBuf[64], topicBuf[32];
        strncpy(brokerBuf, mqttConfig.broker, sizeof(brokerBuf) - 1);
        brokerBuf[sizeof(brokerBuf) - 1] = '\0';
        strncpy(topicBuf, mqttConfig.topic_prefix, sizeof(topicBuf) - 1);
        topicBuf[sizeof(topicBuf) - 1] = '\0';
        
        mqtt["broker"] = brokerBuf;
        mqtt["topic"] = topicBuf;
    }
    
    // Stats (only if changed - expensive to compute)
    if (changed.stats) {
        JsonObject stats = doc["stats"].to<JsonObject>();
        BrewOS::FullStatistics fullStats;
        Stats.getFullStatistics(fullStats);
        
        JsonObject daily = stats["daily"].to<JsonObject>();
        BrewOS::PeriodStats dailyStats;
        Stats.getDailyStats(dailyStats);
        daily["shotCount"] = dailyStats.shotCount;
        daily["avgBrewTimeMs"] = dailyStats.avgBrewTimeMs;
        daily["totalKwh"] = dailyStats.totalKwh;
        
        JsonObject lifetime = stats["lifetime"].to<JsonObject>();
        lifetime["totalShots"] = fullStats.lifetime.totalShots;
        lifetime["avgBrewTimeMs"] = fullStats.lifetime.avgBrewTimeMs;
        lifetime["totalKwh"] = fullStats.lifetime.totalKwh;
        
        stats["sessionShots"] = fullStats.sessionShots;
        stats["shotsToday"] = dailyStats.shotCount;
    }
    
    // ESP32 (only if changed)
    if (changed.esp32) {
        JsonObject esp32 = doc["esp32"].to<JsonObject>();
        esp32["version"] = ESP32_VERSION;
        esp32["freeHeap"] = ESP.getFreeHeap();
        esp32["uptime"] = millis();
    }
    
    return changed.anyChanged();
}

// =============================================================================
// Unified Status Broadcast - Single comprehensive message
// Uses pre-allocated PSRAM buffers to avoid repeated allocation every 500ms
// Optimized to only build JSON/MessagePack when actually needed
// =============================================================================
void BrewWebServer::broadcastFullStatus(const ui_state_t& state) {
    // Skip status broadcasts during OTA to prevent WebSocket queue overflow
    if (_otaInProgress) {
        return;
    }
    
    // Initialize pre-allocated buffers if not already done
    if (!g_statusDoc || !g_statusBuffer) {
        initBroadcastBuffers();
        if (!g_statusDoc || !g_statusBuffer) {
            LOG_E("Failed to allocate broadcast buffers");
            return;
        }
    }
    
    // Check if we have local WebSocket clients (always send to them every 500ms)
    bool hasLocalClients = (_ws.count() > 0);
    
    // Check cloud connection and change detection BEFORE building expensive JSON
    static StatusChangeDetector cloudChangeDetector;
    static StatusChangeDetector localChangeDetector;  // Separate detector for local clients
    static unsigned long lastCloudHeartbeat = 0;
    static unsigned long lastFullStatusSync = 0;  // Periodic full status sync
    static bool lastCloudConnected = false;
    const unsigned long CLOUD_HEARTBEAT_INTERVAL = 30000;  // 30 seconds minimum heartbeat
    const unsigned long FULL_STATUS_SYNC_INTERVAL = 300000;  // 5 minutes for full status sync
    
    bool cloudConnected = _cloudConnection && _cloudConnection->isConnected();
    bool cloudChanged = false;
    bool cloudHeartbeatDue = false;
    ChangedFields cloudChangedFields;
    
    if (cloudConnected) {
        // Reset detector when cloud connection is established (ensures first update is sent)
        if (!lastCloudConnected) {
            cloudChangeDetector.reset();
        }
        lastCloudConnected = true;
        
        cloudChanged = cloudChangeDetector.hasChanged(state, &cloudChangedFields);
        unsigned long now = millis();
        cloudHeartbeatDue = (now - lastCloudHeartbeat >= CLOUD_HEARTBEAT_INTERVAL);
    } else {
        lastCloudConnected = false;
    }
    
    bool shouldSendToCloud = cloudConnected && (cloudChanged || cloudHeartbeatDue);
    
    // Check local client changes
    bool localChanged = false;
    ChangedFields localChangedFields;
    bool isFirstLocalMessage = false;
    if (hasLocalClients) {
        // Reset detector when local client connects (ensures first update is sent)
        static bool lastHasLocalClients = false;
        if (!lastHasLocalClients && hasLocalClients) {
            localChangeDetector.reset();
            isFirstLocalMessage = true;
        }
        lastHasLocalClients = hasLocalClients;
        
        localChanged = localChangeDetector.hasChanged(state);
        if (localChanged) {
            localChangedFields = localChangeDetector.getChangedFields(state);
        }
    }
    
    // Early exit: only build JSON/MessagePack if we have local clients OR need to send to cloud
    if (!hasLocalClients && !shouldSendToCloud) {
        return;  // No one to send to, skip expensive JSON building
    }
    
    // Sequence number for tracking updates and detecting missed messages
    // Only increment when we're actually going to send a message
    static uint32_t statusSequence = 0;
    
    // Decide: delta or full status?
    // Send full status on:
    // 1. Major state changes (machine_state changed)
    // 2. Periodic sync (every 5 minutes)
    // 3. First connection (client just connected)
    // 4. Heartbeat (to ensure consistency)
    unsigned long now = millis();
    bool fullStatusSyncDue = (now - lastFullStatusSync >= FULL_STATUS_SYNC_INTERVAL);
    bool majorStateChange = (cloudChanged && cloudChangedFields.machine_state) || 
                           (localChanged && localChangedFields.machine_state);
    bool sendFullStatus = majorStateChange || fullStatusSyncDue || cloudHeartbeatDue || isFirstLocalMessage;
    
    // Clear the pre-allocated document for reuse
    g_statusDoc->clear();
    JsonDocument& doc = *g_statusDoc;
    
    // Increment sequence number only when we're actually sending
    statusSequence++;
    
    // Build delta or full status
    if (!sendFullStatus && (cloudChanged || localChanged)) {
        // Build delta status - use the changed fields from the appropriate detector
        ChangedFields changed = hasLocalClients ? localChangedFields : cloudChangedFields;
        if (buildDeltaStatus(state, changed, statusSequence, doc)) {
            // Delta built successfully
        } else {
            // No changes detected, fall back to full status
            sendFullStatus = true;
        }
    } else {
        // Build full status
        sendFullStatus = true;
    }
    
    // If we decided to send full status, build it now
    if (sendFullStatus) {
        doc.clear();
        doc["type"] = "status";
        doc["seq"] = statusSequence;
    
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
    
    // Brew-by-weight settings (needed for cloud clients)
    if (brewByWeight) {
        JsonObject bbw = scale["bbw"].to<JsonObject>();
        bbw["targetWeight"] = brewByWeight->getTargetWeight();
        bbw["doseWeight"] = brewByWeight->getDoseWeight();
        bbw["stopOffset"] = brewByWeight->getStopOffset();
        bbw["autoStop"] = brewByWeight->isAutoStopEnabled();
        bbw["autoTare"] = brewByWeight->isAutoTareEnabled();
    }
    
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
    // MQTT Status Section
    // =========================================================================
    JsonObject mqtt = doc["mqtt"].to<JsonObject>();
    MQTTConfig mqttConfig = _mqttClient.getConfig();
    mqtt["enabled"] = mqttConfig.enabled;
    mqtt["connected"] = _mqttClient.isConnected();
    
    // Copy MQTT strings to stack buffers to avoid PSRAM issues
    char brokerBuf[64];
    char topicBuf[32];
    strncpy(brokerBuf, mqttConfig.broker, sizeof(brokerBuf) - 1);
    brokerBuf[sizeof(brokerBuf) - 1] = '\0';
    strncpy(topicBuf, mqttConfig.topic_prefix, sizeof(topicBuf) - 1);
    topicBuf[sizeof(topicBuf) - 1] = '\0';
    
    mqtt["broker"] = brokerBuf;
    mqtt["topic"] = topicBuf;
    
        // =========================================================================
        // ESP32 Info
        // =========================================================================
        JsonObject esp32 = doc["esp32"].to<JsonObject>();
        esp32["version"] = ESP32_VERSION;
        esp32["freeHeap"] = ESP.getFreeHeap();
        esp32["uptime"] = millis();
        
        // Update full status sync timer
        if (fullStatusSyncDue) {
            lastFullStatusSync = now;
        }
    }
    
    // Serialize to MessagePack binary format (much smaller than JSON)
    // Only do this expensive operation if we actually need to send
    size_t msgpackSize = MessagePackHelper::serialize(doc, g_statusBuffer, STATUS_BUFFER_SIZE);
    if (msgpackSize > 0) {
        // Send to WebSocket clients as binary (MessagePack format)
        // Local clients always get updates every 500ms for responsive UI
        if (hasLocalClients) {
            // AsyncWebSocket doesn't have binaryAll, so send to each client
            _ws.cleanupClients();
            for (size_t i = 0; i < _ws.count(); i++) {
                AsyncWebSocketClient* client = _ws.client(i);
                if (client && client->status() == WS_CONNECTED) {
                    client->binary(g_statusBuffer, msgpackSize);
                }
            }
        }
        
        // Send to cloud if status changed or heartbeat due
        if (shouldSendToCloud) {
            _cloudConnection->sendBinary(g_statusBuffer, msgpackSize);
            if (cloudHeartbeatDue) {
                lastCloudHeartbeat = millis();
            }
        }
    } else {
        LOG_W("MessagePack serialization failed or buffer too small");
    }
    
    // Note: No cleanup needed - we're using pre-allocated reusable buffers
}

void BrewWebServer::broadcastDeviceInfo() {
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
    
    // Include power settings (from Pico via MSG_ENV_CONFIG - Pico is source of truth)
    // These are updated when Pico sends MSG_ENV_CONFIG message
    doc["mainsVoltage"] = powerSettings.mainsVoltage;
    doc["maxCurrent"] = powerSettings.maxCurrent;
    
    // Include temperature setpoints (from Pico via machineState - Pico is source of truth)
    // These come from Pico's status messages, which reflect what Pico has persisted
    const ui_state_t& state = getMachineState();
    doc["brewSetpoint"] = state.brew_setpoint;
    doc["steamSetpoint"] = state.steam_setpoint;
    
    // Include eco mode settings (from Pico - Pico is source of truth)
    // Note: These are cached from last set_eco command until Pico reports them back
    // Pico persists these in its flash, but doesn't currently send them in status/config
    // For now, we use cached values (Pico will use its persisted values on boot)
    doc["ecoBrewTemp"] = tempSettings.ecoBrewTemp;
    doc["ecoTimeoutMinutes"] = tempSettings.ecoTimeoutMinutes;
    
    // Include pre-infusion settings
    const auto& brewSettings = State.settings().brew;
    // preinfusionPressure > 0 is used as the enabled flag (legacy compatibility)
    doc["preinfusionEnabled"] = brewSettings.preinfusionPressure > 0;
    doc["preinfusionOnMs"] = (uint16_t)(brewSettings.preinfusionTime * 1000);  // Convert seconds to ms
    doc["preinfusionPauseMs"] = brewSettings.preinfusionPauseMs;  // Use saved value
    
    // Include user preferences (synced across devices)
    JsonObject preferences = doc["preferences"].to<JsonObject>();
    prefs.toJson(preferences);
    
    // Include time settings (for cloud sync)
    const auto& timeSettings = State.settings().time;
    JsonObject timeObj = doc["time"].to<JsonObject>();
    timeSettings.toJson(timeObj);
    
    // Include schedules for cloud clients (they can't access REST APIs)
    const auto& scheduleSettings = State.settings().schedule;
    JsonObject scheduleObj = doc["schedule"].to<JsonObject>();
    scheduleSettings.toJson(scheduleObj);
    
    // Allocate JSON buffer in internal RAM (not PSRAM) to avoid crashes
    size_t jsonSize = measureJson(doc) + 1;
    char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
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

void BrewWebServer::broadcastBBWSettings() {
    // Skip during OTA to prevent WebSocket queue overflow
    if (_otaInProgress) {
        return;
    }
    
    // Use stack allocation to avoid PSRAM crashes
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    StaticJsonDocument<512> doc;
    #pragma GCC diagnostic pop
    doc["type"] = "bbw_settings";
    
    // Get BBW settings from brewByWeight manager
    if (brewByWeight) {
        // enabled maps to auto_stop (when BBW is enabled, it auto-stops at target)
        doc["enabled"] = brewByWeight->isAutoStopEnabled();
        doc["targetWeight"] = brewByWeight->getTargetWeight();
        doc["doseWeight"] = brewByWeight->getDoseWeight();
        doc["stopOffset"] = brewByWeight->getStopOffset();
        doc["autoTare"] = brewByWeight->isAutoTareEnabled();
    }
    
    // Allocate JSON buffer in internal RAM (not PSRAM) to avoid crashes
    size_t jsonSize = measureJson(doc) + 1;
    char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
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

void BrewWebServer::broadcastMqttStatus() {
    // Skip during OTA to prevent WebSocket queue overflow
    if (_otaInProgress) {
        return;
    }
    
    // Use stack allocation to avoid PSRAM crashes
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    StaticJsonDocument<256> doc;
    #pragma GCC diagnostic pop
    doc["type"] = "mqtt_status";
    
    // Get MQTT config and status
    MQTTConfig config = _mqttClient.getConfig();
    
    JsonObject mqtt = doc["mqtt"].to<JsonObject>();
    mqtt["enabled"] = config.enabled;
    mqtt["connected"] = _mqttClient.isConnected();
    
    // Copy strings to stack buffers to avoid PSRAM issues
    char brokerBuf[64];
    char topicBuf[32];
    strncpy(brokerBuf, config.broker, sizeof(brokerBuf) - 1);
    brokerBuf[sizeof(brokerBuf) - 1] = '\0';
    strncpy(topicBuf, config.topic_prefix, sizeof(topicBuf) - 1);
    topicBuf[sizeof(topicBuf) - 1] = '\0';
    
    mqtt["broker"] = brokerBuf;
    mqtt["topic"] = topicBuf;
    
    // Allocate JSON buffer
    size_t jsonSize = measureJson(doc) + 1;
    char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!jsonBuffer) {
        jsonBuffer = (char*)malloc(jsonSize);
    }
    
    if (jsonBuffer) {
        serializeJson(doc, jsonBuffer, jsonSize);
        _ws.textAll(jsonBuffer);
        
        // Also send to cloud
        if (_cloudConnection) {
            _cloudConnection->send(jsonBuffer);
        }
        
        free(jsonBuffer);
    }
}

void BrewWebServer::broadcastPowerMeterStatus() {
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
    char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
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

void BrewWebServer::broadcastEvent(const String& event, const JsonDocument* data) {
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
    char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
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
