#include "web_server.h"
#include "config.h"
#include "memory_utils.h"
#include "json_buffer_pool.h"
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
// Runtime state access
#include "runtime_state.h"

// =============================================================================
// Pre-allocated Broadcast Buffers (initialized in initBroadcastBuffers)
// =============================================================================
static SpiRamJsonDocument* g_statusDoc = nullptr;
static uint8_t* g_statusBuffer = nullptr;  // Changed to uint8_t for binary MessagePack
static const size_t STATUS_BUFFER_SIZE = 2048;  // Reduced - MessagePack is smaller
static const size_t STATUS_DOC_SIZE = 8192;  // Increased to 8KB to prevent heap overflow

void initBroadcastBuffers() {
    if (!g_statusDoc) {
        g_statusDoc = new SpiRamJsonDocument(STATUS_DOC_SIZE);  // 8KB to prevent heap overflow
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
                                 const char* level, const char* message, const char* source = "esp32") {
    // Defensive checks: ensure all required pointers are valid
    if (!message || !ws) return;
    
    // CRITICAL FIX: Don't cleanup clients on every log message - it's too expensive
    // Rely on periodic cleanup in sendPingToClients() (every 5s) or loop() (every 1s)
    // This prevents performance issues and race conditions during high-frequency logging
    
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
    doc["source"] = source ? source : "esp32";  // Add source to distinguish Pico vs ESP32 logs
    doc["timestamp"] = millis();
    
    // Use buffer pool to avoid heap fragmentation
    size_t jsonSize = measureJson(doc) + 1;
    char* jsonBuffer = JsonBufferPool::instance().allocate(jsonSize);
    
    if (jsonBuffer) {
        serializeJson(doc, jsonBuffer, jsonSize);
        
        // Only send to WebSocket if clients can receive (prevents queue overflow)
        // Iterate clients individually to avoid blocking on slow clients
        // This prevents one slow client from blocking all clients
        if (ws->count() > 0) {
            // FIX: Iterate over clients list directly instead of using index
            // ws->client(i) expects an ID, not an index!
            // getClients() returns references, so use auto& and access with . not ->
            for (auto& client : ws->getClients()) {
                if (client.status() == WS_CONNECTED && client.canSend()) {
                    client.text(jsonBuffer);
                }
            }
        }
        
        // Always try to send to cloud - it has its own queue management
        if (cloudConnection) {
            cloudConnection->send(jsonBuffer);
        }
        
        // Release buffer back to pool (or free if dynamically allocated)
        JsonBufferPool::instance().release(jsonBuffer);
    }
}

// Direct message broadcast (no formatting) - for platform_log
void BrewWebServer::broadcastLogMessage(const char* level, const char* message) {
    if (!message) return;
    broadcastLogInternal(&_ws, _cloudConnection, level, message, "esp32");
}

// Broadcast log message with explicit source (for Pico logs)
void BrewWebServer::broadcastLogMessageWithSource(const char* level, const char* message, const char* source) {
    if (!message) return;
    broadcastLogInternal(&_ws, _cloudConnection, level, message, source ? source : "esp32");
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
    
    broadcastLogInternal(&_ws, _cloudConnection, "info", message, "esp32");
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
    
    broadcastLogInternal(&_ws, _cloudConnection, level, message, "esp32");
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
    
    // Use buffer pool to avoid heap fragmentation
    size_t jsonSize = measureJson(doc) + 1;
    char* jsonBuffer = JsonBufferPool::instance().allocate(jsonSize);
    
    if (jsonBuffer) {
        serializeJson(doc, jsonBuffer, jsonSize);
        _ws.textAll(jsonBuffer);
        
        // Also send to cloud - use jsonBuffer directly to avoid String allocation
        if (_cloudConnection) {
            _cloudConnection->send(jsonBuffer);
        }
        
        // Release buffer back to pool (or free if dynamically allocated)
        JsonBufferPool::instance().release(jsonBuffer);
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
    static bool isKeepaliveForced = false;  // Track if this send is a keepalive (function scope)
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
    static bool lastHasLocalClients = false;
    static unsigned long lastClientConnectTime = 0;
    if (hasLocalClients) {
        // Reset detector when local client connects (ensures first update is sent)
        if (!lastHasLocalClients && hasLocalClients) {
            localChangeDetector.reset();
            isFirstLocalMessage = true;
            lastClientConnectTime = millis();
            LOG_I("New local client connected, will send initial status");
        }
        lastHasLocalClients = hasLocalClients;
        
        // Always send status for first 2 seconds after client connects
        // This ensures client receives initial state even if canSend() was false initially
        unsigned long now = millis();
        if (now - lastClientConnectTime < 2000) {
            isFirstLocalMessage = true;
        }
        
        localChanged = localChangeDetector.hasChanged(state);
        
        // For local clients: Send status every 2 seconds as keepalive even when idle
        // This ensures client receives regular updates and doesn't mark connection as stale
        static unsigned long lastLocalKeepalive = 0;
        const unsigned long LOCAL_KEEPALIVE_INTERVAL = 2000;  // 2 seconds
        
        // Initialize keepalive timer on first client connection
        if (!lastHasLocalClients && hasLocalClients) {
            lastLocalKeepalive = now;  // Initialize to current time
        }
        
        isKeepaliveForced = false;  // Reset flag (declared at function scope above)
        
        // Only set keepalive if we actually have local clients
        if (hasLocalClients && !localChanged && !isFirstLocalMessage) {
            unsigned long timeSinceLastKeepalive = now - lastLocalKeepalive;
            if (timeSinceLastKeepalive >= LOCAL_KEEPALIVE_INTERVAL) {
                // Force send status as keepalive for local clients
                localChanged = true;
                isKeepaliveForced = true;
                lastLocalKeepalive = now;
            }
        } else if (localChanged) {
            // Reset keepalive timer when we send due to actual changes
            lastLocalKeepalive = now;
        }
        
        if (localChanged && !isKeepaliveForced) {
            localChangedFields = localChangeDetector.getChangedFields(state);
        }
    } else {
        lastHasLocalClients = false;
    }
    
    // Early exit: only build JSON/MessagePack if we have local clients OR need to send to cloud
    // If keepalive was forced but there are no local clients, skip everything (keepalive is only for local clients)
    if (isKeepaliveForced && !hasLocalClients) {
        return;  // Keepalive is only for local clients, skip if none connected
    }
    
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
    // 3. First connection (client just connected) - ALWAYS send full status for new clients
    // 4. Heartbeat (to ensure consistency)
    unsigned long now = millis();
    bool fullStatusSyncDue = (now - lastFullStatusSync >= FULL_STATUS_SYNC_INTERVAL);
    bool majorStateChange = (cloudChanged && cloudChangedFields.machine_state) || 
                           (localChanged && localChangedFields.machine_state);
    // Always send full status for newly connected clients to ensure they get complete state
    // This prevents stale connection errors
    bool sendFullStatus = majorStateChange || fullStatusSyncDue || cloudHeartbeatDue || isFirstLocalMessage;
    
    // For newly connected clients, always send status even if nothing changed
    // This ensures client gets complete initial state
    if (isFirstLocalMessage) {
        localChanged = true;  // Force send even if no changes detected
    }
    
    // For local clients: Always send status updates periodically (every 2s) as keepalive
    // This ensures client receives regular updates and doesn't mark connection as stale
    // Cloud clients use change detection to save bandwidth, but local clients need keepalive
    // The keepalive logic above forces localChanged=true every 2 seconds when idle
    
    // Clear the pre-allocated document for reuse
    g_statusDoc->clear();
    JsonDocument& doc = *g_statusDoc;
    
    // Increment sequence number only when we're actually sending
    statusSequence++;
    
    // Build delta or full status
    // For newly connected clients, always send full status to ensure complete state
    // For local clients with keepalive, send full status (simpler and ensures client gets complete state)
    // Note: isKeepaliveForced is declared as static at function scope above
    
    if (!sendFullStatus && (cloudChanged || localChanged) && !isFirstLocalMessage) {
        // Check if this is a keepalive send - if keepalive was forced, send lightweight keepalive
        // Otherwise, try to build delta status
        if (isKeepaliveForced && hasLocalClients) {
            // This is a keepalive - send lightweight message instead of expensive full status
            // This prevents blocking the main loop with expensive operations when idle
            // Only send if we actually have local clients
            doc.clear();
            doc["type"] = "keepalive";
            doc["seq"] = statusSequence;
            doc["uptime"] = millis();
            
            // Yield before serialization
            yield();
            
            // Serialize lightweight keepalive to MessagePack
            size_t msgpackSize = MessagePackHelper::serialize(doc, g_statusBuffer, STATUS_BUFFER_SIZE);
            
            // Yield after serialization
            yield();
            
            if (msgpackSize > 0 && hasLocalClients) {
                // Send lightweight keepalive to local clients
                size_t clientIndex = 0;
                for (auto& client : _ws.getClients()) {
                    // Yield every 2 clients to prevent blocking
                    if (clientIndex > 0 && (clientIndex % 2 == 0)) {
                        yield();
                    }
                    clientIndex++;
                    
                    if (client.status() == WS_CONNECTED && client.canSend()) {
                        client.binary(g_statusBuffer, msgpackSize);
                        yield(); // Yield after each send
                    }
                }
            }
            // Skip expensive full status building for keepalive
            return;
        } else if (isKeepaliveForced && !hasLocalClients) {
            // Keepalive was forced but no clients - just return early to avoid expensive operations
            return;
        } else {
            // Build delta status - use the changed fields from the appropriate detector
            ChangedFields changed = hasLocalClients ? localChangedFields : cloudChangedFields;
            // Always include stats in delta if explicitly marked as changed
            // (Stats changes are tracked externally when brews are recorded)
            if (buildDeltaStatus(state, changed, statusSequence, doc)) {
                // Delta built successfully - stats included if changed.stats is true
            } else {
                // No changes detected, fall back to full status
                sendFullStatus = true;
            }
        }
    } else {
        // Build full status (for new clients, major changes, or periodic sync)
        sendFullStatus = true;
    }
    
    // If we decided to send full status, build it now
    if (sendFullStatus) {
        doc.clear();
        doc["type"] = "status";
        doc["seq"] = statusSequence;
        
        // Include stats in full status only for:
        // 1. First connection (isFirstLocalMessage)
        // 2. Periodic sync (fullStatusSyncDue)
        // This avoids expensive stats calculation on every full status broadcast
        bool includeStats = isFirstLocalMessage || fullStatusSyncDue;
    
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
    // NOTE: Stats are NOT included in full status broadcasts to avoid expensive
    // calculations every 500ms. Stats are sent:
    // 1. On first connection (via isFirstLocalMessage check below)
    // 2. In delta updates when changed.stats is true
    // 3. In periodic sync (every 5 minutes)
    // 4. Via HTTP endpoint /api/stats
    // =========================================================================
    // Stats are conditionally included below based on sendFullStatus and isFirstLocalMessage
    
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
    // Yield before WiFi status (involves multiple WiFi API calls)
    yield();
    WiFiStatus wifiStatus = _wifiManager.getStatus();
    wifi["staticIp"] = wifiStatus.staticIp;
    
    // Yield after WiFi status (String operations can be slow)
    yield();
    
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
    // Stats Section - Only included on first connection or periodic sync
    // =========================================================================
    if (includeStats) {
        // Yield before expensive stats operations
        yield();
        
        JsonObject stats = doc["stats"].to<JsonObject>();
        
        // Get current statistics
        BrewOS::FullStatistics fullStats;
        Stats.getFullStatistics(fullStats);
        
        // Yield after statistics gathering (can iterate through brew history)
        yield();
        
        // Daily stats
        JsonObject daily = stats["daily"].to<JsonObject>();
        BrewOS::PeriodStats dailyStats;
        Stats.getDailyStats(dailyStats);
        daily["shotCount"] = dailyStats.shotCount;
        daily["avgBrewTimeMs"] = dailyStats.avgBrewTimeMs;
        daily["totalKwh"] = dailyStats.totalKwh;
        
        // Yield after daily stats
        yield();
        
        // Lifetime stats
        JsonObject lifetime = stats["lifetime"].to<JsonObject>();
        lifetime["totalShots"] = fullStats.lifetime.totalShots;
        lifetime["avgBrewTimeMs"] = fullStats.lifetime.avgBrewTimeMs;
        lifetime["totalKwh"] = fullStats.lifetime.totalKwh;
        
        // Session stats
        stats["sessionShots"] = fullStats.sessionShots;
        stats["shotsToday"] = dailyStats.shotCount;
        
        // Yield after all stats are added to JSON
        yield();
    }
    
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
    
    // Yield after building large JSON document to prevent blocking
    yield();
    
    // Check for JSON overflow before serialization
    size_t jsonSize = measureJson(doc);
    size_t memoryUsed = doc.memoryUsage();
    if (memoryUsed > STATUS_DOC_SIZE * 0.9) {  // Warn if using >90% of capacity
        LOG_W("JSON document near capacity: %zu/%zu bytes (%.1f%%)", 
              memoryUsed, (size_t)STATUS_DOC_SIZE, (memoryUsed * 100.0f) / STATUS_DOC_SIZE);
    }
    if (memoryUsed > STATUS_DOC_SIZE) {
        LOG_E("JSON document overflow: %zu > %zu bytes - heap allocation likely!", 
              memoryUsed, (size_t)STATUS_DOC_SIZE);
    }
    
    // Serialize to MessagePack binary format (much smaller than JSON)
    // Only do this expensive operation if we actually need to send
    // Yield before expensive serialization
    yield();
    size_t msgpackSize = MessagePackHelper::serialize(doc, g_statusBuffer, STATUS_BUFFER_SIZE);
    
    // Yield after expensive serialization to prevent blocking main loop
    yield();
    
    if (msgpackSize > 0) {
        // Send to WebSocket clients as binary (MessagePack format)
        // Status updates are only sent when something changes, on first connect,
        // or during periodic sync. Application-level keepalive messages prevent stale connections.
        if (hasLocalClients) {
            // Only cleanup clients periodically, not before every send
            // CRITICAL FIX: Don't cleanup right before sending keepalive
            // Cleanup can remove clients, making them null when we try to send
            // Instead, cleanup AFTER sending messages to avoid race conditions
            static unsigned long lastCleanup = 0;
            const unsigned long CLEANUP_INTERVAL = 5000;  // Clean up every 5 seconds
            
            // Get client count FIRST (before any cleanup)
            // This prevents race conditions where cleanup removes clients while we're iterating
            size_t clientCount = _ws.count();
            // CRITICAL DEBUG: Log if we have clients but loop doesn't execute
            if (clientCount == 0 && isKeepaliveForced) {
                LOG_W("WARNING: No clients found but keepalive was forced! hasLocalClients=%d, _ws.count()=%zu", 
                      hasLocalClients ? 1 : 0, _ws.count());
            }
            
            // CRITICAL FIX: Use getClients() iterator instead of index-based loop
            // _ws.client(i) expects a Client ID (like 3, 4, 5...), NOT an array index (0, 1, 2...)
            // This is why _ws.client(0) returns null even when count() returns 1
            
            // Direct iteration over client list using getClients()
            // getClients() returns references, so use auto& and access with . not ->
            size_t clientIndex = 0;
            for (auto& client : _ws.getClients()) {
                // Yield more frequently during client iteration to prevent blocking main loop
                // This is critical when there are multiple clients or slow network
                // Yield every 2 clients instead of every 5 to prevent 3+ second blocks
                if (clientIndex > 0 && (clientIndex % 2 == 0)) {
                    yield();
                }
                clientIndex++;
                // Check connection status - always try to send if connected
                // This ensures clients receive regular updates and don't mark connection as stale
                if (client.status() == WS_CONNECTED) {
                    // CRITICAL: Always send application-level keepalive messages (not protocol pings)
                    // WebSocket protocol-level pings don't trigger browser's onmessage event,
                    // so the client's lastMessageTime never updates and it marks connection as stale.
                    // We must send actual messages (binary MessagePack or text JSON) that trigger onmessage.
                    // Check queue status before sending to avoid blocking
                    bool queueAvailable = client.canSend();
                    
                    if (isKeepaliveForced) {
                        // Force send keepalive - but only if queue has space to avoid blocking
                        // If queue is full, skip this keepalive rather than blocking for 3+ seconds
                        if (queueAvailable && g_statusBuffer && msgpackSize > 0 && msgpackSize <= STATUS_BUFFER_SIZE) {
                            client.binary(g_statusBuffer, msgpackSize);
                            // Yield after each send to prevent blocking main loop
                            yield();
                        } else if (!queueAvailable) {
                            // Queue full - skip keepalive to avoid blocking (client will get next update)
                            LOG_D("Client %u queue full, skipping keepalive to avoid blocking", client.id());
                        } else {
                            LOG_E("Invalid buffer or size: g_statusBuffer=%p, msgpackSize=%zu, STATUS_BUFFER_SIZE=%d", 
                                  g_statusBuffer, msgpackSize, STATUS_BUFFER_SIZE);
                        }
                    } else if (queueAvailable) {
                        // Normal status update - only send if queue has space
                        client.binary(g_statusBuffer, msgpackSize);
                        // Yield after each send to prevent blocking main loop
                        yield();
                    } else {
                        // Queue full for normal update - skip unless it's initial status
                        if (isFirstLocalMessage) {
                            // For first message, try to send even if queue appears full
                            // But add timeout protection - if it takes too long, skip it
                            unsigned long sendStart = millis();
                            if (g_statusBuffer && msgpackSize > 0 && msgpackSize <= STATUS_BUFFER_SIZE) {
                                client.binary(g_statusBuffer, msgpackSize);
                                // Yield after send to prevent blocking
                                yield();
                                unsigned long sendTime = millis() - sendStart;
                                if (sendTime > 100) {
                                    LOG_W("Client %u initial send took %lu ms (queue may be blocking)", client.id(), sendTime);
                                }
                            }
                        } else {
                            LOG_D("Client %u send queue full, skipping this update", client.id());
                            // Don't send protocol ping - it won't help with client's stale detection
                            // The client will receive keepalive from sendPingToClients() or next status update
                        }
                    }
                }
            }
            
            // CRITICAL FIX: Cleanup AFTER sending messages to avoid race conditions
            // This ensures clients aren't removed while we're trying to send to them
            if (millis() - lastCleanup > CLEANUP_INTERVAL) {
                _ws.cleanupClients();
                lastCleanup = millis();
            }
        } else {
            if (isKeepaliveForced) {
                LOG_W("No local clients but keepalive was forced - this shouldn't happen!");
            }
        }
        
        // Send to cloud if status changed or heartbeat due
        if (shouldSendToCloud) {
            // Yield before cloud send to prevent blocking
            yield();
            _cloudConnection->sendBinary(g_statusBuffer, msgpackSize);
            if (cloudHeartbeatDue) {
                lastCloudHeartbeat = millis();
            }
        }
    } else {
        LOG_E("MessagePack serialization failed or buffer too small (keepalive=%d)", isKeepaliveForced);
        // If keepalive failed to serialize, this is critical - connection will go stale
        if (isKeepaliveForced) {
            LOG_E("CRITICAL: Keepalive serialization failed - client will mark connection as stale!");
        }
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
    // Ensure machineType is never empty (default to "dual_boiler" if empty)
    const char* machineTypeStr = (machineInfo.machineType[0] != '\0') 
        ? machineInfo.machineType 
        : "dual_boiler";
    doc["machineType"] = machineTypeStr;
    doc["firmwareVersion"] = ESP32_VERSION;
    
    // Include power settings (from Pico via MSG_ENV_CONFIG - Pico is source of truth)
    // These are updated when Pico sends MSG_ENV_CONFIG message
    doc["mainsVoltage"] = powerSettings.mainsVoltage;
    doc["maxCurrent"] = powerSettings.maxCurrent;
    
    // Include temperature setpoints (from Pico via machineState - Pico is source of truth)
    // These come from Pico's status messages, which reflect what Pico has persisted
    const ui_state_t& state = runtimeState().get();
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
    
    // Use buffer pool to avoid heap fragmentation
    size_t jsonSize = measureJson(doc) + 1;
    
    // Defensive: Check if buffer pool is available and size is reasonable
    if (jsonSize > 4096) {
        LOG_E("Device info JSON too large (%zu bytes), skipping", jsonSize);
        return;
    }
    
    char* jsonBuffer = JsonBufferPool::instance().allocate(jsonSize);
    
    if (jsonBuffer) {
        serializeJson(doc, jsonBuffer, jsonSize);
        
    // FIX: Iterate over clients list directly instead of using index
    // ws->client(i) expects an ID, not an index!
    if (_ws.count() > 0) {
        _ws.cleanupClients();  // Clean up stale clients first
        
        // getClients() returns references, so use auto& and access with . not ->
        for (auto& client : _ws.getClients()) {
            // Defensive: Check client is connected and ready before sending
            if (client.status() == WS_CONNECTED && client.canSend() && jsonBuffer) {
                client.text(jsonBuffer);
                // Yield after each send to prevent blocking main loop
                yield();
            }
        }
    }
        
        // Also send to cloud - use jsonBuffer directly to avoid String allocation
        if (_cloudConnection) {
            _cloudConnection->send(jsonBuffer);
        }
        
        // Release buffer back to pool (or free if dynamically allocated)
        JsonBufferPool::instance().release(jsonBuffer);
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
    
    // Use buffer pool to avoid heap fragmentation
    size_t jsonSize = measureJson(doc) + 1;
    char* jsonBuffer = JsonBufferPool::instance().allocate(jsonSize);
    
    if (jsonBuffer) {
        serializeJson(doc, jsonBuffer, jsonSize);
        _ws.textAll(jsonBuffer);
        
        // Also send to cloud - use jsonBuffer directly to avoid String allocation
        if (_cloudConnection) {
            _cloudConnection->send(jsonBuffer);
        }
        
        // Release buffer back to pool (or free if dynamically allocated)
        JsonBufferPool::instance().release(jsonBuffer);
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
    
    // Use buffer pool to avoid heap fragmentation
    size_t jsonSize = measureJson(doc) + 1;
    char* jsonBuffer = JsonBufferPool::instance().allocate(jsonSize);
    
    if (jsonBuffer) {
        serializeJson(doc, jsonBuffer, jsonSize);
        _ws.textAll(jsonBuffer);
        
        // Also send to cloud
        if (_cloudConnection) {
            _cloudConnection->send(jsonBuffer);
        }
        
        // Release buffer back to pool (or free if dynamically allocated)
        JsonBufferPool::instance().release(jsonBuffer);
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
    
    // Use buffer pool to avoid heap fragmentation
    size_t jsonSize = measureJson(doc) + 1;
    char* jsonBuffer = JsonBufferPool::instance().allocate(jsonSize);
    
    if (jsonBuffer) {
        serializeJson(doc, jsonBuffer, jsonSize);
        _ws.textAll(jsonBuffer);
        
        // Also send to cloud - use jsonBuffer directly to avoid String allocation
        if (_cloudConnection) {
            _cloudConnection->send(jsonBuffer);
        }
        
        // Release buffer back to pool (or free if dynamically allocated)
        JsonBufferPool::instance().release(jsonBuffer);
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
    
    // Use buffer pool to avoid heap fragmentation
    size_t jsonSize = measureJson(doc) + 1;
    char* jsonBuffer = JsonBufferPool::instance().allocate(jsonSize);
    
    if (jsonBuffer) {
        serializeJson(doc, jsonBuffer, jsonSize);
        _ws.textAll(jsonBuffer);
        
        // Also send to cloud - use jsonBuffer directly to avoid String allocation
        if (_cloudConnection) {
            _cloudConnection->send(jsonBuffer);
        }
        
        // Release buffer back to pool (or free if dynamically allocated)
        JsonBufferPool::instance().release(jsonBuffer);
    }
}
