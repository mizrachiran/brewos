/**
 * BrewOS MQTT Client Implementation
 */

#include "mqtt_client.h"
#include "config.h"
#include "memory_utils.h"
#include "ui/ui.h"  // For ui_state_t definition
#include "power_meter/power_meter.h"
#include <ArduinoJson.h>
#include <initializer_list>
#include <esp_mac.h>  // ESP-IDF 5.x: esp_read_mac moved here

// Static instance for callback
MQTTClient* MQTTClient::_instance = nullptr;

// MQTT buffer size - must be large enough for HA discovery messages
// Current largest discovery payload (Heating Strategy select) is ~600 bytes
// Increased to 2048 bytes to provide headroom for future entity additions
// and larger option lists in select entities
static const uint16_t MQTT_BUFFER_SIZE = 2048;

// Total number of entities published to Home Assistant
// Sensors: 5 temps + 7 power + 5 shot/scale + 3 stats = 20
// Binary: 7 status sensors
// Controls: 1 switch + 3 buttons + 3 numbers + 1 select = 8
static const uint8_t HA_TOTAL_ENTITY_COUNT = 35;

// =============================================================================
// MQTT Client Implementation
// =============================================================================

MQTTClient::MQTTClient()
    : _client(_wifiClient)
    , _connected(false)
    , _lastReconnectAttempt(0)
    , _lastStatusPublish(0)
    , _reconnectDelay(1000)
    , _wasConnected(false)
    , _taskHandle(nullptr)
    , _mutex(nullptr)
    , _taskRunning(false)
    , _commandQueue(nullptr) {
    
    _instance = this;
    _client.setCallback(messageCallback);
    _client.setBufferSize(MQTT_BUFFER_SIZE);
    
    // Set socket timeout for PubSubClient (default is 15s, but WiFiClient timeout might be shorter)
    // This ensures MQTT connection attempts wait long enough
    _client.setSocketTimeout(15);  // 15 second socket timeout
    
    // Create mutex for thread-safe access
    _mutex = xSemaphoreCreateMutex();
    
    // Create command queue for thread-safe command passing from Core 0 to Core 1
    _commandQueue = xQueueCreate(COMMAND_QUEUE_SIZE, sizeof(MQTTCommand));
    if (_commandQueue == nullptr) {
        LOG_E("Failed to create MQTT command queue");
    }
}

bool MQTTClient::begin() {
    LOG_I("Initializing MQTT client...");
    
    // Load configuration from NVS
    loadConfig();
    
    if (!_config.enabled) {
        LOG_I("MQTT is disabled");
        return true;
    }
    
    // Set broker
    _client.setServer(_config.broker, _config.port);
    
    // Generate device ID if not set
    if (strlen(_config.ha_device_id) == 0) {
        generateDeviceID();
    }
    
    // Generate client ID if not set
    if (strlen(_config.client_id) == 0) {
        snprintf(_config.client_id, sizeof(_config.client_id), "brewos_%s", _config.ha_device_id);
    }
    
    LOG_I("MQTT configured: broker=%s:%d, client_id=%s", 
          _config.broker, _config.port, _config.client_id);
    
    // Start background task on Core 0
    if (_taskHandle == nullptr && _config.enabled) {
        _taskRunning = true;
        xTaskCreatePinnedToCore(
            taskCode,
            "MQTTTask",
            MQTT_TASK_STACK_SIZE,
            this,
            MQTT_TASK_PRIORITY,
            &_taskHandle,
            MQTT_TASK_CORE
        );
        LOG_I("MQTT task started on Core %d", MQTT_TASK_CORE);
    }
    
    return true;
}

// =============================================================================
// FreeRTOS Task Implementation
// =============================================================================

void MQTTClient::taskCode(void* parameter) {
    MQTTClient* self = static_cast<MQTTClient*>(parameter);
    self->taskLoop();
}

void MQTTClient::taskLoop() {
    while (_taskRunning) {
        if (!_config.enabled) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Check for disconnect event - sync _connected with actual connection state
            bool clientConnected = _client.connected();
            if (_wasConnected && !clientConnected) {
                _connected = false;
                _wasConnected = false;
                LOG_W("MQTT connection lost");
                xSemaphoreGive(_mutex);
                if (_onDisconnected) {
                    _onDisconnected();
                }
                continue;
            }
            
            // Sync _connected flag with actual connection state
            if (clientConnected && !_connected) {
                _connected = true;
            } else if (!clientConnected && _connected) {
                _connected = false;
            }
            
            if (!clientConnected) {
                unsigned long now = millis();
                if (now - _lastReconnectAttempt > (unsigned long)_reconnectDelay) {
                    _lastReconnectAttempt = now;
                    xSemaphoreGive(_mutex);
                    if (connect()) {
                        _reconnectDelay = 1000;  // Reset delay on success
                    } else {
                        // Exponential backoff, max 60 seconds
                        _reconnectDelay = min(_reconnectDelay * 2, 60000);
                    }
                    continue;
                }
            } else {
                _client.loop();
                _wasConnected = true;
            }
            xSemaphoreGive(_mutex);
        }
        
        // Small delay to prevent tight looping
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    // Task is ending
    vTaskDelete(nullptr);
}

void MQTTClient::loop() {
    // The main loop() now provides API compatibility only
    // All MQTT work happens in the background task (taskLoop() on Core 0)
    // This prevents MQTT operations from blocking the main loop on Core 1
    // 
    // NON-BLOCKING GUARANTEE:
    // - MQTT connection attempts run in FreeRTOS task (taskLoop)
    // - Reconnection logic uses vTaskDelay() instead of delay()
    // - PubSubClient::loop() is called from task, not main loop
    // - Main loop() never blocks waiting for MQTT operations
    
    // Process queued commands (called from main loop on Core 1)
    processCommands();
}

void MQTTClient::processCommands() {
    // Process all queued commands (non-blocking)
    // This runs on Core 1 (main loop) for thread safety
    if (_commandQueue == nullptr || _commandCallback == nullptr) {
        return;
    }
    
    MQTTCommand mqttCmd;
    while (xQueueReceive(_commandQueue, &mqttCmd, 0) == pdTRUE) {
        // Parse JSON payload
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, mqttCmd.payload);
        if (error) {
            LOG_W("Failed to parse queued MQTT command JSON: %s", error.c_str());
            continue;
        }
        
        // Execute callback on Core 1 (main loop context)
        // This ensures thread safety: scaleManager, brewByWeight, etc. are accessed
        // only from Core 1 where they were created
        _commandCallback(mqttCmd.cmd, doc);
    }
}

bool MQTTClient::setConfig(const MQTTConfig& config) {
    _config = config;
    
    // Validate - only require broker if MQTT is enabled
    if (_config.enabled) {
        if (strlen(_config.broker) == 0) {
            LOG_E("MQTT broker cannot be empty when MQTT is enabled");
            return false;
        }
        
        if (_config.port == 0 || _config.port > 65535) {
            LOG_E("Invalid MQTT port: %d", _config.port);
            return false;
        }
    }
    
    // Generate device ID if not set
    if (strlen(_config.ha_device_id) == 0) {
        generateDeviceID();
    }
    
    // Generate client ID if not set
    if (strlen(_config.client_id) == 0) {
        snprintf(_config.client_id, sizeof(_config.client_id), "brewos_%s", _config.ha_device_id);
    }
    
    // Save to NVS
    saveConfig();
    
    // Reconnect if enabled
    if (_config.enabled) {
        disconnect();
        _client.setServer(_config.broker, _config.port);
        if (WiFi.status() == WL_CONNECTED) {
            connect();
        }
    } else {
        disconnect();
    }
    
    LOG_I("MQTT configuration updated");
    return true;
}

bool MQTTClient::testConnection() {
    if (!_config.enabled) {
        return false;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        LOG_W("Cannot test MQTT: WiFi not connected");
        return false;
    }
    
    return connect();
}

int MQTTClient::testConnectionWithConfig(const MQTTConfig& testConfig) {
    // Validate broker
    if (strlen(testConfig.broker) == 0) {
        LOG_E("MQTT broker cannot be empty");
        return 1;  // Broker empty
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        LOG_W("Cannot test MQTT: WiFi not connected");
        return 2;  // WiFi not connected
    }
    
    LOG_I("Testing MQTT connection to %s:%d...", testConfig.broker, testConfig.port);
    
    // Create temporary client for testing (don't disturb active connection)
    WiFiClient testWifiClient;
    PubSubClient testClient(testWifiClient);
    testClient.setServer(testConfig.broker, testConfig.port);
    testClient.setBufferSize(512);  // Small buffer for test
    
    // Set shorter timeout for test connection to avoid watchdog timeout
    // Use 5 seconds instead of default 15s to prevent blocking CloudTask too long
    // This ensures the test completes before the task watchdog (10s) triggers
    testWifiClient.setTimeout(5000);  // 5 second timeout for test
    testClient.setSocketTimeout(5);   // 5 second socket timeout
    
    // Generate temporary client ID
    char testClientId[48];
    snprintf(testClientId, sizeof(testClientId), "brewos_test_%lu", millis());
    
    // Feed watchdog before blocking operation
    yield();
    
    // Attempt connection (blocking, but with shorter timeout)
    bool connected = false;
    if (strlen(testConfig.username) > 0) {
        connected = testClient.connect(testClientId, testConfig.username, testConfig.password);
    } else {
        connected = testClient.connect(testClientId);
    }
    
    // Feed watchdog after blocking operation
    yield();
    
    if (connected) {
        LOG_I("MQTT test connection successful!");
        testClient.disconnect();
        return 0;  // Success
    } else {
        int state = testClient.state();
        LOG_W("MQTT test connection failed (state=%d)", state);
        return 3;  // Connection failed
    }
}

bool MQTTClient::connect() {
    if (WiFi.status() != WL_CONNECTED) {
        LOG_W("MQTT: WiFi not connected");
        return false;
    }
    
    // Log network diagnostics
    int32_t rssi = WiFi.RSSI();
    IPAddress localIP = WiFi.localIP();
    IPAddress gatewayIP = WiFi.gatewayIP();
    LOG_I("MQTT: Network: IP=%s, RSSI=%d dBm, Gateway=%s", 
          localIP.toString().c_str(), rssi, gatewayIP.toString().c_str());
    
    // Configure WiFi client timeout - MUST be set before connection attempt
    // ESP32 NetworkClient has a default 3s timeout that needs to be overridden
    // Set both socket timeout and overall client timeout
    _wifiClient.setTimeout(15000);  // 15 second timeout for MQTT operations
    
    // Also set socket timeout explicitly (PubSubClient uses this)
    _client.setSocketTimeout(15);  // Ensure PubSubClient uses 15s timeout
    
    LOG_I("Connecting to MQTT broker %s:%d...", _config.broker, _config.port);
    
    // Test broker connectivity first (non-blocking check)
    // This helps diagnose if broker is unreachable vs authentication failure
    IPAddress brokerIP;
    if (WiFi.hostByName(_config.broker, brokerIP)) {
        LOG_I("MQTT: Broker resolved to %s", brokerIP.toString().c_str());
    } else {
        LOG_W("MQTT: DNS resolution failed for %s", _config.broker);
        // Continue anyway - PubSubClient will try DNS itself
    }
    
    // Additional diagnostic: Check if broker IP is reachable
    // This helps identify network/firewall issues
    if (brokerIP != INADDR_NONE) {
        LOG_D("MQTT: Broker IP resolved, attempting TCP connection...");
    }
    
    // Build will topic for LWT
    char willTopic[64];
    getTopic("availability", willTopic, sizeof(willTopic));
    
    bool connected = false;
    if (strlen(_config.username) > 0) {
        connected = _client.connect(
            _config.client_id,
            _config.username,
            _config.password,
            willTopic,
            1,  // QoS 1
            true,  // Retain
            "offline"
        );
    } else {
        connected = _client.connect(
            _config.client_id,
            willTopic,
            1,
            true,
            "offline"
        );
    }
    
    if (connected) {
        LOG_I("MQTT connected!");
        _connected = true;
        _reconnectDelay = 1000;
        
        // Publish online status
        publishAvailability(true);
        
        // Subscribe to command topic
        char cmdTopic[64];
        getTopic("command", cmdTopic, sizeof(cmdTopic));
        _client.subscribe(cmdTopic, 1);
        LOG_I("Subscribed to: %s", cmdTopic);
        
        // Publish Home Assistant discovery if enabled
        if (_config.ha_discovery) {
            publishHomeAssistantDiscovery();
        }
        
        if (_onConnected) {
            _onConnected();
        }
    } else {
        int state = _client.state();
        const char* errorMsg = "";
        switch (state) {
            case -4: errorMsg = "Connection timeout"; break;
            case -3: errorMsg = "Connection lost"; break;
            case -2: errorMsg = "Connect failed"; break;
            case -1: errorMsg = "Disconnected"; break;
            case 1: errorMsg = "Bad protocol"; break;
            case 2: errorMsg = "Bad client ID"; break;
            case 3: errorMsg = "Unavailable"; break;
            case 4: errorMsg = "Bad credentials"; break;
            case 5: errorMsg = "Unauthorized"; break;
            default: errorMsg = "Unknown error"; break;
        }
        LOG_W("MQTT connection failed: %d (%s)", state, errorMsg);
        _connected = false;
    }
    
    return _connected;
}

void MQTTClient::disconnect() {
    if (_client.connected()) {
        publishAvailability(false);
        _client.disconnect();
    }
    _connected = false;
}

void MQTTClient::setEnabled(bool enabled) {
    if (enabled && !_config.enabled) {
        // Re-enable MQTT
        _config.enabled = true;
        _reconnectDelay = 1000;
        _lastReconnectAttempt = 0;  // Allow immediate reconnect
        LOG_I("MQTT enabled");
    } else if (!enabled && _config.enabled) {
        // Disable MQTT - disconnect first
        LOG_I("MQTT disabling...");
        disconnect();
        _config.enabled = false;
        LOG_I("MQTT disabled");
    }
}

void MQTTClient::publishStatus(const ui_state_t& state) {
    // Thread-safe publish with connection check
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;  // Couldn't acquire mutex, skip this publish
    }
    
    // Check actual connection state (not just flag)
    if (!_client.connected()) {
        _connected = false;
        xSemaphoreGive(_mutex);
        return;
    }
    
    // Build comprehensive status JSON - use StaticJsonDocument on stack
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    StaticJsonDocument<1024> doc;
    #pragma GCC diagnostic pop
    
    // Sequence number for tracking updates
    static uint32_t mqttStatusSequence = 0;
    mqttStatusSequence++;
    doc["seq"] = mqttStatusSequence;
    
    // Machine state - convert to string for HA templates
    const char* stateStr = "unknown";
    const char* modeStr = "standby";
    switch (state.machine_state) {
        case UI_STATE_INIT: stateStr = "init"; modeStr = "standby"; break;
        case UI_STATE_IDLE: stateStr = "standby"; modeStr = "standby"; break;
        case UI_STATE_HEATING: stateStr = "heating"; modeStr = "on"; break;
        case UI_STATE_READY: stateStr = "ready"; modeStr = "on"; break;
        case UI_STATE_BREWING: stateStr = "brewing"; modeStr = "on"; break;
        case UI_STATE_FAULT: stateStr = "fault"; modeStr = "standby"; break;
        case UI_STATE_SAFE: stateStr = "safe"; modeStr = "standby"; break;
        case UI_STATE_ECO: stateStr = "eco"; modeStr = "eco"; break;
    }
    doc["state"] = stateStr;
    doc["mode"] = modeStr;
    doc["heating_strategy"] = state.heating_strategy;  // 0-3 for HA template
    
    // Temperature readings - use stack buffers instead of String
    char tempBuf[16];
    snprintf(tempBuf, sizeof(tempBuf), "%.1f", state.brew_temp);
    doc["brew_temp"] = serialized(tempBuf);
    snprintf(tempBuf, sizeof(tempBuf), "%.1f", state.brew_setpoint);
    doc["brew_setpoint"] = serialized(tempBuf);
    snprintf(tempBuf, sizeof(tempBuf), "%.1f", state.steam_temp);
    doc["steam_temp"] = serialized(tempBuf);
    snprintf(tempBuf, sizeof(tempBuf), "%.1f", state.steam_setpoint);
    doc["steam_setpoint"] = serialized(tempBuf);
    
    // Pressure
    snprintf(tempBuf, sizeof(tempBuf), "%.2f", state.pressure);
    doc["pressure"] = serialized(tempBuf);
    
    // Scale data - use brew_weight as scale weight when brewing
    snprintf(tempBuf, sizeof(tempBuf), "%.1f", state.brew_weight);
    doc["scale_weight"] = serialized(tempBuf);
    snprintf(tempBuf, sizeof(tempBuf), "%.1f", state.flow_rate);
    doc["flow_rate"] = serialized(tempBuf);
    doc["scale_connected"] = state.scale_connected;
    
    // Active shot data
    doc["shot_duration"] = state.brew_time_ms / 1000.0f;
    snprintf(tempBuf, sizeof(tempBuf), "%.1f", state.brew_weight);
    doc["shot_weight"] = serialized(tempBuf);
    doc["is_brewing"] = state.is_brewing;
    
    // Target weight (for BBW)
    snprintf(tempBuf, sizeof(tempBuf), "%.1f", state.target_weight);
    doc["target_weight"] = serialized(tempBuf);
    
    // Status flags
    doc["is_heating"] = state.is_heating;
    doc["water_low"] = state.water_low;
    doc["alarm_active"] = state.alarm_active;
    doc["pico_connected"] = state.pico_connected;
    doc["wifi_connected"] = state.wifi_connected;
    
    // Serialize to stack buffer (avoid String allocation)
    char statusBuffer[1024];
    size_t len = serializeJson(doc, statusBuffer, sizeof(statusBuffer));
    
    // Publish to status topic (retained) - use getTopic() helper to respect topic_prefix
    char statusTopic[64];
    getTopic("status", statusTopic, sizeof(statusTopic));
    if (!_client.publish(statusTopic, (const uint8_t*)statusBuffer, len, true)) {
        LOG_W("Failed to publish status");
        // Check if connection was lost during publish
        if (!_client.connected()) {
            _connected = false;
        }
    } else {
        LOG_D("Published status to %s (%d bytes)", statusTopic, len);
    }
    
    xSemaphoreGive(_mutex);
}

void MQTTClient::publishStatusDelta(const ui_state_t& state, const ChangedFields& changed) {
    // Thread-safe publish with connection check
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }
    
    // Check actual connection state
    if (!_client.connected()) {
        _connected = false;
        xSemaphoreGive(_mutex);
        return;
    }
    
    // Build delta status JSON - only changed fields
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    StaticJsonDocument<512> doc;  // Smaller buffer for delta
    #pragma GCC diagnostic pop
    
    // Sequence number (shared with full status)
    static uint32_t mqttStatusSequence = 0;
    mqttStatusSequence++;
    doc["seq"] = mqttStatusSequence;
    doc["type"] = "status_delta";
    
    // Machine state
    if (changed.machine_state || changed.machine_mode) {
        const char* stateStr = "unknown";
        const char* modeStr = "standby";
        switch (state.machine_state) {
            case UI_STATE_INIT: stateStr = "init"; modeStr = "standby"; break;
            case UI_STATE_IDLE: stateStr = "standby"; modeStr = "standby"; break;
            case UI_STATE_HEATING: stateStr = "heating"; modeStr = "on"; break;
            case UI_STATE_READY: stateStr = "ready"; modeStr = "on"; break;
            case UI_STATE_BREWING: stateStr = "brewing"; modeStr = "on"; break;
            case UI_STATE_FAULT: stateStr = "fault"; modeStr = "standby"; break;
            case UI_STATE_SAFE: stateStr = "safe"; modeStr = "standby"; break;
            case UI_STATE_ECO: stateStr = "eco"; modeStr = "eco"; break;
        }
        doc["state"] = stateStr;
        doc["mode"] = modeStr;
    }
    
    if (changed.heating_strategy) {
        doc["heating_strategy"] = state.heating_strategy;
    }
    
    // Temperatures
    if (changed.temps) {
        char tempBuf[16];
        snprintf(tempBuf, sizeof(tempBuf), "%.1f", state.brew_temp);
        doc["brew_temp"] = serialized(tempBuf);
        snprintf(tempBuf, sizeof(tempBuf), "%.1f", state.brew_setpoint);
        doc["brew_setpoint"] = serialized(tempBuf);
        snprintf(tempBuf, sizeof(tempBuf), "%.1f", state.steam_temp);
        doc["steam_temp"] = serialized(tempBuf);
        snprintf(tempBuf, sizeof(tempBuf), "%.1f", state.steam_setpoint);
        doc["steam_setpoint"] = serialized(tempBuf);
    }
    
    // Pressure
    if (changed.pressure) {
        char tempBuf[16];
        snprintf(tempBuf, sizeof(tempBuf), "%.2f", state.pressure);
        doc["pressure"] = serialized(tempBuf);
    }
    
    // Scale
    if (changed.scale_weight || changed.scale_flow_rate) {
        char tempBuf[16];
        if (changed.scale_weight) {
            snprintf(tempBuf, sizeof(tempBuf), "%.1f", state.brew_weight);
            doc["scale_weight"] = serialized(tempBuf);
        }
        if (changed.scale_flow_rate) {
            snprintf(tempBuf, sizeof(tempBuf), "%.1f", state.flow_rate);
            doc["flow_rate"] = serialized(tempBuf);
        }
    }
    
    if (changed.scale_connected) {
        doc["scale_connected"] = state.scale_connected;
    }
    
    // Shot data
    if (changed.is_brewing || changed.brew_time) {
        doc["is_brewing"] = state.is_brewing;
        if (state.is_brewing) {
            doc["shot_duration"] = state.brew_time_ms / 1000.0f;
            char tempBuf[16];
            snprintf(tempBuf, sizeof(tempBuf), "%.1f", state.brew_weight);
            doc["shot_weight"] = serialized(tempBuf);
        }
    }
    
    if (changed.target_weight) {
        char tempBuf[16];
        snprintf(tempBuf, sizeof(tempBuf), "%.1f", state.target_weight);
        doc["target_weight"] = serialized(tempBuf);
    }
    
    // Status flags
    if (changed.is_heating) doc["is_heating"] = state.is_heating;
    if (changed.water_low) doc["water_low"] = state.water_low;
    if (changed.alarm) doc["alarm_active"] = state.alarm_active;
    if (changed.connections) {
        doc["pico_connected"] = state.pico_connected;
        doc["wifi_connected"] = state.wifi_connected;
    }
    
    // Serialize to stack buffer
    char statusBuffer[512];
    size_t len = serializeJson(doc, statusBuffer, sizeof(statusBuffer));
    
    // Publish to delta topic (non-retained)
    char deltaTopic[64];
    getTopic("status/delta", deltaTopic, sizeof(deltaTopic));
    if (!_client.publish(deltaTopic, (const uint8_t*)statusBuffer, len, false)) {
        LOG_W("Failed to publish status delta");
        if (!_client.connected()) {
            _connected = false;
        }
    } else {
        LOG_D("Published status delta to %s (%d bytes)", deltaTopic, len);
    }
    
    xSemaphoreGive(_mutex);
}

void MQTTClient::publishShot(const char* shot_json) {
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }
    
    if (!_client.connected()) {
        _connected = false;
        xSemaphoreGive(_mutex);
        return;
    }
    
    char shotTopic[64];
    getTopic("shot", shotTopic, sizeof(shotTopic));
    if (!_client.publish(shotTopic, (const uint8_t*)shot_json, strlen(shot_json), false)) {
        LOG_W("Failed to publish shot data");
        if (!_client.connected()) {
            _connected = false;
        }
    }
    
    xSemaphoreGive(_mutex);
}

void MQTTClient::publishStatisticsJson(const char* stats_json) {
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }
    
    if (!_client.connected()) {
        _connected = false;
        xSemaphoreGive(_mutex);
        return;
    }
    
    char statsTopic[64];
    getTopic("statistics", statsTopic, sizeof(statsTopic));
    if (!_client.publish(statsTopic, (const uint8_t*)stats_json, strlen(stats_json), true)) {
        LOG_W("Failed to publish statistics");
        if (!_client.connected()) {
            _connected = false;
        }
    }
    
    xSemaphoreGive(_mutex);
}

void MQTTClient::publishStatistics(uint16_t shotsToday, uint32_t totalShots, float kwhToday) {
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }
    
    if (!_client.connected()) {
        _connected = false;
        xSemaphoreGive(_mutex);
        return;
    }
    
    // Build stats JSON - use stack allocation
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    StaticJsonDocument<128> doc;
    #pragma GCC diagnostic pop
    doc["shots_today"] = shotsToday;
    doc["total_shots"] = totalShots;
    char kwhBuf[16];
    snprintf(kwhBuf, sizeof(kwhBuf), "%.3f", kwhToday);
    doc["kwh_today"] = serialized(kwhBuf);
    
    // Serialize to stack buffer
    char statsBuffer[128];
    size_t len = serializeJson(doc, statsBuffer, sizeof(statsBuffer));
    
    // Publish to statistics topic (retained) - use getTopic() helper to respect topic_prefix
    char statsTopic[64];
    getTopic("statistics", statsTopic, sizeof(statsTopic));
    if (!_client.publish(statsTopic, (const uint8_t*)statsBuffer, len, true)) {
        LOG_W("Failed to publish statistics");
        if (!_client.connected()) {
            _connected = false;
        }
    } else {
        LOG_D("Published statistics to %s (%d bytes)", statsTopic, len);
    }
    
    xSemaphoreGive(_mutex);
}

void MQTTClient::publishPowerMeter(const PowerMeterReading& reading) {
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }
    
    if (!_client.connected()) {
        _connected = false;
        xSemaphoreGive(_mutex);
        return;
    }
    
    // Build power JSON - use stack allocation
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    StaticJsonDocument<256> doc;
    #pragma GCC diagnostic pop
    
    char valBuf[16];
    snprintf(valBuf, sizeof(valBuf), "%.1f", reading.voltage);
    doc["voltage"] = serialized(valBuf);
    snprintf(valBuf, sizeof(valBuf), "%.2f", reading.current);
    doc["current"] = serialized(valBuf);
    snprintf(valBuf, sizeof(valBuf), "%.0f", reading.power);
    doc["power"] = serialized(valBuf);
    snprintf(valBuf, sizeof(valBuf), "%.3f", reading.energy_import);
    doc["energy_import"] = serialized(valBuf);
    snprintf(valBuf, sizeof(valBuf), "%.3f", reading.energy_export);
    doc["energy_export"] = serialized(valBuf);
    snprintf(valBuf, sizeof(valBuf), "%.1f", reading.frequency);
    doc["frequency"] = serialized(valBuf);
    snprintf(valBuf, sizeof(valBuf), "%.2f", reading.power_factor);
    doc["power_factor"] = serialized(valBuf);
    
    // Serialize to stack buffer
    char powerBuffer[256];
    size_t len = serializeJson(doc, powerBuffer, sizeof(powerBuffer));
    
    // Publish to power topic (retained)
    char powerTopic[64];
    snprintf(powerTopic, sizeof(powerTopic), "brewos/%s/power", _config.ha_device_id);
    if (!_client.publish(powerTopic, (const uint8_t*)powerBuffer, len, true)) {
        LOG_W("Failed to publish power meter data");
        if (!_client.connected()) {
            _connected = false;
        }
    }
    
    xSemaphoreGive(_mutex);
}

void MQTTClient::publishAvailability(bool online) {
    // Allow publishing "offline" even if not connected (for graceful disconnect)
    if (!_client.connected() && online) return;
    
    // Use mutex for thread safety
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }
    
    char availTopic[64];
    getTopic("availability", availTopic, sizeof(availTopic));
    const char* msg = online ? "online" : "offline";
    if (!_client.publish(availTopic, (const uint8_t*)msg, strlen(msg), true)) {
        LOG_W("Failed to publish availability: %s", msg);
        if (!_client.connected()) {
            _connected = false;
        }
    } else {
        LOG_D("Published availability: %s", msg);
    }
    
    xSemaphoreGive(_mutex);
}

void MQTTClient::publishHomeAssistantDiscovery() {
    // NOTE: We do NOT hold the mutex for the entire discovery process.
    // Instead, we acquire/release it for each individual publish.
    // This allows publishStatus() calls to succeed during discovery.
    
    // Check actual connection state (acquire mutex briefly)
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        LOG_W("Failed to acquire mutex for HA discovery check");
        return;
    }
    
    if (!_client.connected()) {
        _connected = false;
        xSemaphoreGive(_mutex);
        return;
    }
    xSemaphoreGive(_mutex);  // Release mutex immediately
    
    LOG_I("Publishing Home Assistant discovery...");
    
    const char* deviceId = _config.ha_device_id;
    char statusTopic[64];
    getTopic("status", statusTopic, sizeof(statusTopic));
    char availTopic[64];
    getTopic("availability", availTopic, sizeof(availTopic));
    char commandTopic[64];
    getTopic("command", commandTopic, sizeof(commandTopic));
    char powerTopic[64];
    getTopic("power", powerTopic, sizeof(powerTopic));
    char shotTopic[64];
    getTopic("shot", shotTopic, sizeof(shotTopic));
    
    // Counter for pacing publishes (prevents network stack saturation)
    int publishCount = 0;
    bool connectionLost = false;
    
    // Device info helper - returns a consistent device object
    auto addDeviceInfo = [&](JsonDocument& doc) {
        JsonObject device = doc["device"].to<JsonObject>();
        char identifier[64];
        snprintf(identifier, sizeof(identifier), "brewos_%s", deviceId);
        device["identifiers"].to<JsonArray>().add(identifier);
        device["name"] = "BrewOS Coffee Machine";
        device["model"] = "ECM Controller";
        device["manufacturer"] = "BrewOS";
        device["sw_version"] = ESP32_VERSION;
        char configUrl[64];
        snprintf(configUrl, sizeof(configUrl), "http://%s", WiFi.localIP().toString().c_str());
        device["configuration_url"] = configUrl;
    };
    
    // Helper to publish a sensor discovery message
    // Use StaticJsonDocument on stack to avoid heap fragmentation (called 35+ times)
    // NOTE: Acquires/releases mutex per publish to allow status updates during discovery
    auto publishSensor = [&](const char* name, const char* sensorId, const char* valueTemplate, 
                             const char* unit, const char* deviceClass, const char* stateClass,
                             const char* stateTopic = nullptr, const char* icon = nullptr) {
        // Check if already lost connection
        if (connectionLost) return;
        
        // Acquire mutex for this publish only
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            return;  // Skip this entity if mutex busy (status update in progress)
        }
        
        // Check connection before each publish
        if (!_client.connected()) {
            connectionLost = true;
            _connected = false;
            xSemaphoreGive(_mutex);
            return;
        }
        
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        StaticJsonDocument<512> doc;
        #pragma GCC diagnostic pop
        addDeviceInfo(doc);
        
        doc["name"] = name;
        // Use snprintf instead of String concatenation to reduce heap fragmentation
        char uniqueId[64];
        snprintf(uniqueId, sizeof(uniqueId), "brewos_%s_%s", deviceId, sensorId);
        doc["unique_id"] = uniqueId;
        char objectId[48];
        snprintf(objectId, sizeof(objectId), "brewos_%s", sensorId);
        doc["object_id"] = objectId;
        doc["state_topic"] = stateTopic ? stateTopic : statusTopic;
        doc["value_template"] = valueTemplate;
        if (unit && strlen(unit) > 0) doc["unit_of_measurement"] = unit;
        if (deviceClass && strlen(deviceClass) > 0) doc["device_class"] = deviceClass;
        if (stateClass && strlen(stateClass) > 0) doc["state_class"] = stateClass;
        if (icon) doc["icon"] = icon;
        doc["availability_topic"] = availTopic;
        doc["payload_available"] = "online";
        doc["payload_not_available"] = "offline";
        
        // Serialize to stack buffer (avoid String heap allocation)
        char payloadBuffer[512];
        size_t payloadLen = serializeJson(doc, payloadBuffer, sizeof(payloadBuffer));
        
        // Build topic on stack (avoid String heap allocation)
        char configTopic[128];
        snprintf(configTopic, sizeof(configTopic), "homeassistant/sensor/brewos_%s/%s/config", 
                 deviceId, sensorId);
        
        if (!_client.publish(configTopic, (const uint8_t*)payloadBuffer, payloadLen, true)) {
            LOG_W("Failed to publish HA discovery for %s", sensorId);
            // Check if connection was lost
            if (!_client.connected()) {
                connectionLost = true;
                _connected = false;
            }
        }
        
        xSemaphoreGive(_mutex);  // Release mutex after publish
        
        yield();  // Yield to prevent blocking network stack
        // Delay after each publish to prevent broker overwhelm
        // Some brokers (Mosquitto, etc) disconnect clients that publish too rapidly
        delay(50);  // 50ms delay per publish (~2 seconds total for 35 entities)
        publishCount++;
    };
    
    // Publish binary sensors for status
    auto publishBinarySensor = [&](const char* name, const char* sensorId, const char* valueTemplate, 
                                   const char* deviceClass, const char* icon = nullptr) {
        // Check if already lost connection
        if (connectionLost) return;
        
        // Acquire mutex for this publish only
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            return;
        }
        
        // Check connection before each publish
        if (!_client.connected()) {
            connectionLost = true;
            _connected = false;
            xSemaphoreGive(_mutex);
            return;
        }
        
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        StaticJsonDocument<512> doc;
        #pragma GCC diagnostic pop
        addDeviceInfo(doc);
        
        doc["name"] = name;
        char uniqueId[64];
        snprintf(uniqueId, sizeof(uniqueId), "brewos_%s_%s", deviceId, sensorId);
        doc["unique_id"] = uniqueId;
        char objectId[48];
        snprintf(objectId, sizeof(objectId), "brewos_%s", sensorId);
        doc["object_id"] = objectId;
        doc["state_topic"] = statusTopic;
        doc["value_template"] = valueTemplate;
        if (deviceClass && strlen(deviceClass) > 0) {
            doc["device_class"] = deviceClass;
        }
        if (icon) doc["icon"] = icon;
        doc["payload_on"] = "True";
        doc["payload_off"] = "False";
        doc["availability_topic"] = availTopic;
        
        // Serialize to stack buffer
        char payloadBuffer[512];
        size_t payloadLen = serializeJson(doc, payloadBuffer, sizeof(payloadBuffer));
        
        // Build topic on stack
        char configTopic[128];
        snprintf(configTopic, sizeof(configTopic), "homeassistant/binary_sensor/brewos_%s/%s/config",
                 deviceId, sensorId);
        if (!_client.publish(configTopic, (const uint8_t*)payloadBuffer, payloadLen, true)) {
            LOG_W("Failed to publish HA discovery for %s", sensorId);
            if (!_client.connected()) {
                connectionLost = true;
                _connected = false;
            }
        }
        
        xSemaphoreGive(_mutex);
        
        yield();
        delay(50);  // Prevent broker overwhelm
        publishCount++;
    };
    
    // Helper to publish a switch
    auto publishSwitch = [&](const char* name, const char* switchId, const char* icon,
                             const char* payloadOn, const char* payloadOff, 
                             const char* stateTemplate) {
        if (connectionLost) return;
        
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            return;
        }
        
        if (!_client.connected()) {
            connectionLost = true;
            _connected = false;
            xSemaphoreGive(_mutex);
            return;
        }
        
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        StaticJsonDocument<512> doc;
        #pragma GCC diagnostic pop
        addDeviceInfo(doc);
        
        doc["name"] = name;
        char uniqueId[64];
        snprintf(uniqueId, sizeof(uniqueId), "brewos_%s_%s", deviceId, switchId);
        doc["unique_id"] = uniqueId;
        char objectId[48];
        snprintf(objectId, sizeof(objectId), "brewos_%s", switchId);
        doc["object_id"] = objectId;
        doc["state_topic"] = statusTopic;
        doc["command_topic"] = commandTopic;
        doc["value_template"] = stateTemplate;
        doc["payload_on"] = payloadOn;
        doc["payload_off"] = payloadOff;
        doc["state_on"] = "ON";
        doc["state_off"] = "OFF";
        doc["icon"] = icon;
        doc["availability_topic"] = availTopic;
        
        // Serialize to stack buffer
        char payloadBuffer[512];
        size_t payloadLen = serializeJson(doc, payloadBuffer, sizeof(payloadBuffer));
        
        // Build topic on stack
        char configTopic[128];
        snprintf(configTopic, sizeof(configTopic), "homeassistant/switch/brewos_%s/%s/config",
                 deviceId, switchId);
        if (!_client.publish(configTopic, (const uint8_t*)payloadBuffer, payloadLen, true)) {
            LOG_W("Failed to publish HA discovery for %s", switchId);
            if (!_client.connected()) {
                connectionLost = true;
                _connected = false;
            }
        }
        
        xSemaphoreGive(_mutex);
        
        yield();
        delay(50);  // Prevent broker overwhelm
        publishCount++;
    };
    
    // Helper to publish a button
    auto publishButton = [&](const char* name, const char* buttonId, const char* icon,
                             const char* payload) {
        if (connectionLost) return;
        
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            return;
        }
        
        if (!_client.connected()) {
            connectionLost = true;
            _connected = false;
            xSemaphoreGive(_mutex);
            return;
        }
        
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        StaticJsonDocument<512> doc;
        #pragma GCC diagnostic pop
        addDeviceInfo(doc);
        
        doc["name"] = name;
        char uniqueId[64];
        snprintf(uniqueId, sizeof(uniqueId), "brewos_%s_%s", deviceId, buttonId);
        doc["unique_id"] = uniqueId;
        char objectId[48];
        snprintf(objectId, sizeof(objectId), "brewos_%s", buttonId);
        doc["object_id"] = objectId;
        doc["command_topic"] = commandTopic;
        doc["payload_press"] = payload;
        doc["icon"] = icon;
        doc["availability_topic"] = availTopic;
        
        // Serialize to stack buffer
        char payloadBuffer[512];
        size_t payloadLen = serializeJson(doc, payloadBuffer, sizeof(payloadBuffer));
        
        // Build topic on stack
        char configTopic[128];
        snprintf(configTopic, sizeof(configTopic), "homeassistant/button/brewos_%s/%s/config",
                 deviceId, buttonId);
        if (!_client.publish(configTopic, (const uint8_t*)payloadBuffer, payloadLen, true)) {
            LOG_W("Failed to publish HA discovery for %s", buttonId);
            if (!_client.connected()) {
                connectionLost = true;
                _connected = false;
            }
        }
        
        xSemaphoreGive(_mutex);
        
        yield();
        delay(50);  // Prevent broker overwhelm
        publishCount++;
    };
    
    // Helper to publish a number entity
    auto publishNumber = [&](const char* name, const char* numberId, const char* icon,
                             float min, float max, float step, const char* unit,
                             const char* valueTemplate, const char* commandTemplate) {
        if (connectionLost) return;
        
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            return;
        }
        
        if (!_client.connected()) {
            connectionLost = true;
            _connected = false;
            xSemaphoreGive(_mutex);
            return;
        }
        
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        StaticJsonDocument<512> doc;
        #pragma GCC diagnostic pop
        addDeviceInfo(doc);
        
        doc["name"] = name;
        char uniqueId[64];
        snprintf(uniqueId, sizeof(uniqueId), "brewos_%s_%s", deviceId, numberId);
        doc["unique_id"] = uniqueId;
        char objectId[48];
        snprintf(objectId, sizeof(objectId), "brewos_%s", numberId);
        doc["object_id"] = objectId;
        doc["state_topic"] = statusTopic;
        doc["command_topic"] = commandTopic;
        doc["value_template"] = valueTemplate;
        doc["command_template"] = commandTemplate;
        doc["min"] = min;
        doc["max"] = max;
        doc["step"] = step;
        doc["unit_of_measurement"] = unit;
        doc["icon"] = icon;
        doc["mode"] = "slider";
        doc["availability_topic"] = availTopic;
        
        // Serialize to stack buffer
        char payloadBuffer[512];
        size_t payloadLen = serializeJson(doc, payloadBuffer, sizeof(payloadBuffer));
        
        // Build topic on stack
        char configTopic[128];
        snprintf(configTopic, sizeof(configTopic), "homeassistant/number/brewos_%s/%s/config",
                 deviceId, numberId);
        if (!_client.publish(configTopic, (const uint8_t*)payloadBuffer, payloadLen, true)) {
            LOG_W("Failed to publish HA discovery for %s", numberId);
            if (!_client.connected()) {
                connectionLost = true;
                _connected = false;
            }
        }
        
        xSemaphoreGive(_mutex);
        
        yield();
        delay(50);  // Prevent broker overwhelm
        publishCount++;
    };
    
    // Helper to publish a select entity
    auto publishSelect = [&](const char* name, const char* selectId, const char* icon,
                             std::initializer_list<const char*> options, const char* valueTemplate,
                             const char* commandTemplate) {
        if (connectionLost) return;
        
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            return;
        }
        
        if (!_client.connected()) {
            connectionLost = true;
            _connected = false;
            xSemaphoreGive(_mutex);
            return;
        }
        
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        StaticJsonDocument<512> doc;
        #pragma GCC diagnostic pop
        addDeviceInfo(doc);
        
        doc["name"] = name;
        char uniqueId[64];
        snprintf(uniqueId, sizeof(uniqueId), "brewos_%s_%s", deviceId, selectId);
        doc["unique_id"] = uniqueId;
        char objectId[48];
        snprintf(objectId, sizeof(objectId), "brewos_%s", selectId);
        doc["object_id"] = objectId;
        doc["state_topic"] = statusTopic;
        doc["command_topic"] = commandTopic;
        doc["value_template"] = valueTemplate;
        doc["command_template"] = commandTemplate;
        doc["icon"] = icon;
        doc["availability_topic"] = availTopic;
        
        JsonArray optionsArr = doc["options"].to<JsonArray>();
        for (const char* opt : options) {
            optionsArr.add(opt);
        }
        
        // Serialize to stack buffer
        char payloadBuffer[512];
        size_t payloadLen = serializeJson(doc, payloadBuffer, sizeof(payloadBuffer));
        
        // Build topic on stack
        char configTopic[128];
        snprintf(configTopic, sizeof(configTopic), "homeassistant/select/brewos_%s/%s/config",
                 deviceId, selectId);
        if (!_client.publish(configTopic, (const uint8_t*)payloadBuffer, payloadLen, true)) {
            LOG_W("Failed to publish HA discovery for %s", selectId);
            if (!_client.connected()) {
                connectionLost = true;
                _connected = false;
            }
        }
        
        xSemaphoreGive(_mutex);
        
        yield();
        delay(50);  // Prevent broker overwhelm
        publishCount++;
    };
    
    // ==========================================================================
    // TEMPERATURE SENSORS
    // ==========================================================================
    publishSensor("Brew Temperature", "brew_temp", "{{ value_json.brew_temp }}", "°C", "temperature", "measurement");
    publishSensor("Steam Temperature", "steam_temp", "{{ value_json.steam_temp }}", "°C", "temperature", "measurement");
    publishSensor("Brew Setpoint", "brew_setpoint", "{{ value_json.brew_setpoint }}", "°C", "temperature", "measurement");
    publishSensor("Steam Setpoint", "steam_setpoint", "{{ value_json.steam_setpoint }}", "°C", "temperature", "measurement");
    publishSensor("Brew Pressure", "pressure", "{{ value_json.pressure }}", "bar", "pressure", "measurement");
    
    // ==========================================================================
    // SCALE & SHOT SENSORS
    // ==========================================================================
    publishSensor("Scale Weight", "scale_weight", "{{ value_json.scale_weight | default(0) }}", "g", "weight", "measurement", nullptr, "mdi:scale");
    publishSensor("Flow Rate", "flow_rate", "{{ value_json.flow_rate | default(0) }}", "g/s", nullptr, "measurement", nullptr, "mdi:water-outline");
    publishSensor("Shot Duration", "shot_duration", "{{ value_json.shot_duration | default(0) }}", "s", "duration", "measurement", nullptr, "mdi:timer");
    publishSensor("Shot Weight", "shot_weight", "{{ value_json.shot_weight | default(0) }}", "g", "weight", "measurement", nullptr, "mdi:coffee");
    publishSensor("Target Weight", "target_weight", "{{ value_json.target_weight | default(36) }}", "g", "weight", nullptr, nullptr, "mdi:target");
    
    // ==========================================================================
    // STATISTICS SENSORS (use statistics topic)
    // ==========================================================================
    char statisticsTopic[64];
    getTopic("statistics", statisticsTopic, sizeof(statisticsTopic));
    publishSensor("Shots Today", "shots_today", "{{ value_json.shots_today | default(0) }}", "shots", nullptr, "total_increasing", statisticsTopic, "mdi:counter");
    publishSensor("Total Shots", "total_shots", "{{ value_json.total_shots | default(0) }}", "shots", nullptr, "total_increasing", statisticsTopic, "mdi:coffee-maker");
    publishSensor("Energy Today", "energy_today", "{{ value_json.kwh_today | default(0) }}", "kWh", "energy", "total_increasing", statisticsTopic, nullptr);
    
    // ==========================================================================
    // BINARY SENSORS
    // ==========================================================================
    publishBinarySensor("Brewing", "is_brewing", "{{ value_json.is_brewing }}", "running", "mdi:coffee");
    publishBinarySensor("Heating", "is_heating", "{{ value_json.is_heating }}", "heat", nullptr);
    publishBinarySensor("Machine Ready", "ready", "{{ 'True' if value_json.state == 'ready' else 'False' }}", nullptr, "mdi:check-circle");
    publishBinarySensor("Water Low", "water_low", "{{ value_json.water_low }}", "problem", nullptr);
    publishBinarySensor("Alarm", "alarm_active", "{{ value_json.alarm_active }}", "problem", nullptr);
    publishBinarySensor("Pico Connected", "pico_connected", "{{ value_json.pico_connected }}", "connectivity", nullptr);
    publishBinarySensor("Scale Connected", "scale_connected", "{{ value_json.scale_connected }}", "connectivity", "mdi:bluetooth");
    
    // ==========================================================================
    // POWER METER SENSORS
    // ==========================================================================
    publishSensor("Voltage", "voltage", "{{ value_json.voltage }}", "V", "voltage", "measurement", powerTopic);
    publishSensor("Current", "current", "{{ value_json.current }}", "A", "current", "measurement", powerTopic);
    publishSensor("Power", "power", "{{ value_json.power }}", "W", "power", "measurement", powerTopic);
    publishSensor("Energy Import", "energy_import", "{{ value_json.energy_import }}", "kWh", "energy", "total_increasing", powerTopic);
    publishSensor("Energy Export", "energy_export", "{{ value_json.energy_export }}", "kWh", "energy", "total_increasing", powerTopic);
    publishSensor("Frequency", "frequency", "{{ value_json.frequency }}", "Hz", "frequency", "measurement", powerTopic);
    publishSensor("Power Factor", "power_factor", "{{ value_json.power_factor }}", "", "power_factor", "measurement", powerTopic);
    
    // ==========================================================================
    // SWITCH - Machine Power
    // ==========================================================================
    publishSwitch("Power", "power_switch", "mdi:power",
                  "{\"cmd\":\"set_mode\",\"mode\":\"on\"}",
                  "{\"cmd\":\"set_mode\",\"mode\":\"standby\"}",
                  "{{ 'ON' if value_json.state != 'standby' else 'OFF' }}");
    
    // ==========================================================================
    // BUTTONS - Actions
    // ==========================================================================
    publishButton("Start Brew", "start_brew", "mdi:coffee", "{\"cmd\":\"brew_start\"}");
    publishButton("Stop Brew", "stop_brew", "mdi:stop", "{\"cmd\":\"brew_stop\"}");
    publishButton("Tare Scale", "tare_scale", "mdi:scale-balance", "{\"cmd\":\"tare\"}");
    publishButton("Enter Eco Mode", "enter_eco", "mdi:leaf", "{\"cmd\":\"enter_eco\"}");
    publishButton("Exit Eco Mode", "exit_eco", "mdi:lightning-bolt", "{\"cmd\":\"exit_eco\"}");
    
    // ==========================================================================
    // NUMBER - Configurable values
    // ==========================================================================
    publishNumber("Brew Temperature Target", "brew_temp_target", "mdi:thermometer",
                  85.0, 100.0, 0.5, "°C",
                  "{{ value_json.brew_setpoint }}",
                  "{\"cmd\":\"set_temp\",\"boiler\":\"brew\",\"temp\":{{ value }}}");
    
    publishNumber("Steam Temperature Target", "steam_temp_target", "mdi:thermometer-high",
                  120.0, 160.0, 1.0, "°C",
                  "{{ value_json.steam_setpoint }}",
                  "{\"cmd\":\"set_temp\",\"boiler\":\"steam\",\"temp\":{{ value }}}");
    
    publishNumber("Target Weight", "bbw_target", "mdi:target",
                  18.0, 100.0, 0.5, "g",
                  "{{ value_json.target_weight | default(36) }}",
                  "{\"cmd\":\"set_target_weight\",\"weight\":{{ value }}}");
    
    // ==========================================================================
    // SELECT - Machine Mode
    // ==========================================================================
    publishSelect("Machine Mode", "mode_select", "mdi:coffee-maker-outline",
                  {"standby", "on", "eco"},
                  "{{ value_json.mode | default('standby') }}",
                  "{\"cmd\":\"set_mode\",\"mode\":\"{{ value }}\"}");
    
    // ==========================================================================
    // SELECT - Heating Strategy
    // ==========================================================================
    publishSelect("Heating Strategy", "heating_strategy", "mdi:fire",
                  {"brew_only", "sequential", "parallel", "smart_stagger"},
                  "{% set strategies = ['brew_only', 'sequential', 'parallel', 'smart_stagger'] %}{{ strategies[value_json.heating_strategy | int] | default('sequential') }}",
                  "{% set strategies = {'brew_only': 0, 'sequential': 1, 'parallel': 2, 'smart_stagger': 3} %}{\"cmd\":\"set_heating_strategy\",\"strategy\":{{ strategies[value] | default(1) }}}");
    
    // Log result (mutex is released per-publish, not held at end)
    if (connectionLost) {
        LOG_W("Home Assistant discovery incomplete - connection lost (%d/%d entities published)", 
              publishCount, HA_TOTAL_ENTITY_COUNT);
        _connected = false;
    } else {
        LOG_I("Home Assistant discovery published (%d entities)", HA_TOTAL_ENTITY_COUNT);
    }
}

void MQTTClient::onMessage(char* topicName, byte* payload, unsigned int length) {
    // Null-terminate payload
    char message[256];
    size_t len = min(length, sizeof(message) - 1);
    memcpy(message, payload, len);
    message[len] = '\0';
    
    LOG_D("MQTT message: topic=%s, payload=%s", topicName, message);
    
    // Parse command
    char cmdTopicBuf[64];
    getTopic("command", cmdTopicBuf, sizeof(cmdTopicBuf));
    
    if (strcmp(topicName, cmdTopicBuf) == 0) {
        LOG_I("Received MQTT command: %s", message);
        
        // Parse JSON command to extract cmd name
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, message);
        if (error) {
            LOG_W("Failed to parse MQTT command JSON: %s", error.c_str());
            return;
        }
        
        const char* cmd = doc["cmd"] | "";
        
        // Enqueue command for processing on Core 1 (main loop)
        // This ensures thread safety: MQTT task (Core 0) doesn't directly modify
        // state that main loop (Core 1) accesses
        if (_commandQueue != nullptr) {
            MQTTCommand mqttCmd;
            strncpy(mqttCmd.cmd, cmd, sizeof(mqttCmd.cmd) - 1);
            mqttCmd.cmd[sizeof(mqttCmd.cmd) - 1] = '\0';
            strncpy(mqttCmd.payload, message, sizeof(mqttCmd.payload) - 1);
            mqttCmd.payload[sizeof(mqttCmd.payload) - 1] = '\0';
            
            // Try to enqueue (non-blocking, drop if queue full)
            if (xQueueSend(_commandQueue, &mqttCmd, 0) != pdTRUE) {
                LOG_W("MQTT command queue full, dropping command: %s", cmd);
            }
        } else {
            // Fallback: call callback directly if queue not initialized
            // This should not happen in normal operation
            LOG_W("MQTT command queue not initialized, calling callback directly");
            if (_commandCallback) {
                _commandCallback(cmd, doc);
            }
        }
    }
}

void MQTTClient::messageCallback(char* topic, byte* payload, unsigned int length) {
    if (_instance) {
        _instance->onMessage(topic, payload, length);
    }
}

void MQTTClient::getTopic(const char* suffix, char* buffer, size_t len) const {
    // Build topic string using stack buffer to avoid heap allocations
    // Format: {topic_prefix}/{ha_device_id}/{suffix}
    snprintf(buffer, len, "%s/%s/%s", _config.topic_prefix, _config.ha_device_id, suffix);
}

String MQTTClient::topic(const char* suffix) const {
    // Deprecated: use getTopic() instead to avoid heap allocations
    // Kept for backward compatibility
    String t = String(_config.topic_prefix);
    t += "/";
    t += _config.ha_device_id;
    t += "/";
    t += suffix;
    return t;
}

String MQTTClient::getStatusString() const {
    if (!_config.enabled) {
        return "Disabled";
    }
    
    if (!_connected) {
        // Get state from non-const client (temporary)
        MQTTClient* nonConst = const_cast<MQTTClient*>(this);
        int state = nonConst->_client.state();
        // Use stack buffer to avoid String concatenation in PSRAM
        char buf[32];
        snprintf(buf, sizeof(buf), "Disconnected (%d)", state);
        return String(buf);
    }
    
    return "Connected";
}

void MQTTClient::loadConfig() {
    // After fresh flash, NVS namespace won't exist - this is expected
    if (!_prefs.begin("mqtt", true)) {
        LOG_I("No saved MQTT config (fresh flash) - using defaults");
        _config.enabled = false;
        _config.broker[0] = '\0';
        _config.port = 1883;
        _config.username[0] = '\0';
        _config.password[0] = '\0';
        _config.client_id[0] = '\0';
        strcpy(_config.topic_prefix, "brewos");
        _config.use_tls = false;
        return;
    }
    
    _config.enabled = _prefs.getBool("enabled", false);
    _prefs.getString("broker", _config.broker, sizeof(_config.broker));
    _config.port = _prefs.getUShort("port", 1883);
    _prefs.getString("username", _config.username, sizeof(_config.username));
    _prefs.getString("password", _config.password, sizeof(_config.password));
    _prefs.getString("client_id", _config.client_id, sizeof(_config.client_id));
    
    // Topic prefix with fallback to default
    size_t prefixLen = _prefs.getString("topic_prefix", _config.topic_prefix, sizeof(_config.topic_prefix));
    if (prefixLen == 0 || strlen(_config.topic_prefix) == 0) {
        strcpy(_config.topic_prefix, "brewos");
    }
    
    _config.use_tls = _prefs.getBool("use_tls", false);
    _config.ha_discovery = _prefs.getBool("ha_discovery", true);
    _prefs.getString("ha_device_id", _config.ha_device_id, sizeof(_config.ha_device_id));
    
    _prefs.end();
    
    LOG_D("MQTT config loaded: enabled=%d, broker=%s, port=%d", 
          _config.enabled, _config.broker, _config.port);
}

void MQTTClient::saveConfig() {
    _prefs.begin("mqtt", false);
    
    _prefs.putBool("enabled", _config.enabled);
    _prefs.putString("broker", _config.broker);
    _prefs.putUShort("port", _config.port);
    _prefs.putString("username", _config.username);
    _prefs.putString("password", _config.password);
    _prefs.putString("client_id", _config.client_id);
    _prefs.putString("topic_prefix", _config.topic_prefix);
    _prefs.putBool("use_tls", _config.use_tls);
    _prefs.putBool("ha_discovery", _config.ha_discovery);
    _prefs.putString("ha_device_id", _config.ha_device_id);
    
    _prefs.end();
}

void MQTTClient::generateDeviceID() {
    // Generate device ID from MAC address
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(_config.ha_device_id, sizeof(_config.ha_device_id), 
             "%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

