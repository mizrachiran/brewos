#include "web_server.h"
#include "config.h"
#include "memory_utils.h"
#include "pico_uart.h"
#include "mqtt_client.h"
#include "brew_by_weight.h"
#include "scale/scale_manager.h"
#include "pairing_manager.h"
#include "cloud_connection.h"
#include "state/state_manager.h"
#include "power_meter/power_meter_manager.h"
#include "ui/ui.h"
#include <ArduinoJson.h>
#include <stdarg.h>

// External reference to machine state defined in main.cpp
extern ui_state_t machineState;

/**
 * Helper function to allocate memory for JSON buffers.
 * Tries PSRAM first (preferred for JSON), falls back to malloc.
 * Returns nullptr on failure.
 */
static char* allocateJsonBuffer(size_t size) {
    char* buffer = (char*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buffer) {
        buffer = (char*)malloc(size);
    }
    return buffer;
}

// WebSocket event handler for AsyncWebSocket (ESP32Async library)
void BrewWebServer::handleWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_DISCONNECT:
            LOG_I("WebSocket client %u disconnected", client->id());
            // When last local client disconnects, wait 30s before resuming cloud
            // This prevents rapid cloud reconnect if user is just refreshing the page
            // Note: count() still includes the disconnecting client at this point
            if (server->count() <= 1 && _cloudConnection) {
                // Extend pause by 30s instead of immediate resume
                _cloudConnection->pause();
                LOG_I("Cloud will resume in 30s");
            }
            break;
            
        case WS_EVT_CONNECT:
            {
                // Limit to 1 concurrent client to save RAM (each WS client uses ~4KB)
                if (server->count() > 1) {
                    LOG_W("Too many WebSocket clients (%u), rejecting %u", server->count(), client->id());
                    client->close();
                    return;
                }
                
                LOG_I("WebSocket client %u connected from %s", client->id(), client->remoteIP().toString().c_str());
                
                // Pause cloud connection while serving local clients
                if (_cloudConnection) {
                    _cloudConnection->pause();
                }
                
                // Check if we have enough memory to send device info (needs ~3KB for JSON)
                size_t freeHeap = ESP.getFreeHeap();
                if (freeHeap > 10000) {
                    // Send device info immediately so UI has the saved settings
                    broadcastDeviceInfo();
                } else {
                    LOG_W("Low memory (%zu bytes), deferring device info broadcast", freeHeap);
                    // Client will request full state later when memory is available
                }
            }
            break;
            
        case WS_EVT_DATA:
            {
                AwsFrameInfo* info = (AwsFrameInfo*)arg;
                if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                    // Complete text message
                    handleWsMessage(client->id(), data, len);
                }
            }
            break;
            
        case WS_EVT_PONG:
            // Response to our ping
            break;
            
        case WS_EVT_ERROR:
            LOG_E("WebSocket error on client %u", client->id());
            break;
    }
}

void BrewWebServer::handleWsMessage(uint32_t clientNum, uint8_t* payload, size_t length) {
    // Extend cloud pause on every WebSocket activity from local client
    // This ensures cloud stays disconnected while user is actively using the local UI
    if (_cloudConnection) {
        _cloudConnection->pause();
    }
    
    // Parse JSON command from client - use stack allocation
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    StaticJsonDocument<1024> doc;
    #pragma GCC diagnostic pop
    DeserializationError error = deserializeJson(doc, payload, length);
    
    if (error) {
        LOG_W("Invalid WebSocket message from client %u", clientNum);
        return;
    }
    
    // Use shared command processor
    processCommand(doc);
}

/**
 * Processes a command received in JSON format.
 * 
 * This method is called from both handleWsMessage() (for local WebSocket commands)
 * and from the cloud connection command callback (for cloud-originated commands).
 * It handles commands from either source in a unified way.
 */
void BrewWebServer::processCommand(JsonDocument& doc) {
    String type = doc["type"] | "";
    
    if (type == "ping") {
        LOG_I("Command: ping");
        _picoUart.sendPing();
    }
    else if (type == "request_state") {
        // Cloud client requesting full state - check heap first
        // We use pre-allocated PSRAM buffers, so we mainly need enough for the send queue
        size_t freeHeap = ESP.getFreeHeap();
        const size_t MIN_HEAP_FOR_STATE_BROADCAST = 35000;  // Need 35KB (state uses PSRAM buffers)
        
        if (freeHeap < MIN_HEAP_FOR_STATE_BROADCAST) {
            // Heap critically low - schedule deferred broadcast
            LOG_W("Cloud: Scheduling deferred state broadcast (heap=%zu, need %zu)", 
                  freeHeap, MIN_HEAP_FOR_STATE_BROADCAST);
            _pendingCloudStateBroadcast = true;
            _pendingCloudStateBroadcastTime = millis() + 3000;  // Try in 3 seconds
            return;
        }
        
        LOG_I("Cloud: Sending full state to cloud client (heap=%zu)", freeHeap);
        _pendingCloudStateBroadcast = false;
        broadcastFullStatus(machineState);
        broadcastDeviceInfo();
    }
    else if (type == "getConfig") {
        LOG_I("Command: getConfig");
        _picoUart.requestConfig();
    }
    else if (type == "setLogLevel") {
        // Set log level from WebSocket command
        // Expected payload: { type: "setLogLevel", level: "debug" | "info" | "warn" | "error" }
        String levelStr = doc["level"] | "info";
        LOG_I("Command: setLogLevel=%s", levelStr.c_str());
        BrewOSLogLevel level = stringToLogLevel(levelStr.c_str());
        setLogLevel(level);
        broadcastLog("Log level set to: %s", logLevelToString(level));
    }
    else if (type == "command") {
        // Handle commands from web UI
        String cmd = doc["cmd"] | "";
        LOG_I("Command: %s", cmd.c_str());
        
        if (cmd == "set_eco") {
            // Set eco mode configuration
            // Pico is the source of truth - we just relay the command and cache for UI
            // Payload: enabled (bool), brewTemp (float), timeout (int minutes)
            bool enabled = doc["enabled"] | true;
            float brewTemp = doc["brewTemp"] | 80.0f;
            int timeout = doc["timeout"] | 30;
            
            // Cache in memory for immediate UI feedback (not persisted - Pico handles persistence)
            auto& tempSettings = State.settings().temperature;
            tempSettings.ecoBrewTemp = brewTemp;
            tempSettings.ecoTimeoutMinutes = (uint16_t)timeout;
            // Note: We don't call State.saveTemperatureSettings() - Pico is source of truth
            
            // Convert to Pico format: [enabled:1][eco_brew_temp:2][timeout_minutes:2]
            uint8_t payload[5];
            payload[0] = enabled ? 1 : 0;
            int16_t tempScaled = (int16_t)(brewTemp * 10);  // Convert to Celsius * 10
            payload[1] = (tempScaled >> 8) & 0xFF;
            payload[2] = tempScaled & 0xFF;
            payload[3] = (timeout >> 8) & 0xFF;
            payload[4] = timeout & 0xFF;
            
            if (_picoUart.sendCommand(MSG_CMD_SET_ECO, payload, 5)) {
                broadcastLogLevel("info", "Eco mode config saved: temp=%.1f°C, timeout=%dmin", 
                         brewTemp, timeout);
                // Broadcast updated device info so UI refreshes
                broadcastDeviceInfo();
            } else {
                broadcastLogLevel("error", "Failed to send eco config to device");
            }
        }
        else if (cmd == "enter_eco") {
            // Manually enter eco mode
            uint8_t payload[1] = {1};  // 1 = enter eco
            if (_picoUart.sendCommand(MSG_CMD_SET_ECO, payload, 1)) {
                broadcastLogLevel("info", "Entering eco mode");
            } else {
                broadcastLogLevel("error", "Failed to enter eco mode");
            }
        }
        else if (cmd == "exit_eco") {
            // Manually exit eco mode (wake up)
            uint8_t payload[1] = {0};  // 0 = exit eco
            if (_picoUart.sendCommand(MSG_CMD_SET_ECO, payload, 1)) {
                broadcastLogLevel("info", "Exiting eco mode");
            } else {
                broadcastLogLevel("error", "Failed to exit eco mode");
            }
        }
        else if (cmd == "set_temp") {
            // Set temperature setpoint
            // Pico is the source of truth - we just relay the command
            String boiler = doc["boiler"] | "brew";
            float temp = doc["temp"] | 0.0f;
            
            // Update machineState immediately so status broadcasts show the new value
            // This prevents Pico's old values from overwriting what we just set
            // (Pico will persist and echo back the new value in its next status message)
            if (boiler == "steam") {
                machineState.steam_setpoint = temp;
            } else {
                machineState.brew_setpoint = temp;
            }
            
            // Pico expects: [target:1][temperature:int16] where temperature is Celsius * 10
            // Note: Pico (RP2350) is little-endian, so send LSB first
            uint8_t payload[3];
            payload[0] = (boiler == "steam") ? 0x01 : 0x00;  // 0=brew, 1=steam
            int16_t tempScaled = (int16_t)(temp * 10.0f);  // Convert to 0.1°C units
            payload[1] = tempScaled & 0xFF;         // LSB first (little-endian)
            payload[2] = (tempScaled >> 8) & 0xFF;  // MSB second
            if (_picoUart.sendCommand(MSG_CMD_SET_TEMP, payload, 3)) {
                // Pre-format message to avoid String.c_str() issues
                char tempMsg[64];
                snprintf(tempMsg, sizeof(tempMsg), "%s temp saved: %.1f°C", boiler.c_str(), temp);
                broadcastLog(tempMsg);
                // Broadcast updated device info so UI refreshes
                broadcastDeviceInfo();
            }
        }
        else if (cmd == "set_mode") {
            // Set machine mode
            String mode = doc["mode"] | "";
            uint8_t modeCmd = 0;
            
            // Check for optional heating strategy parameter
            if (!doc["strategy"].isNull() && mode == "on") {
                uint8_t strategy = doc["strategy"].as<uint8_t>();
                if (strategy <= 3) {  // Valid strategy range: 0-3
                    // Set heating strategy first
                    uint8_t strategyPayload[2] = {0x01, strategy};  // CONFIG_HEATING_STRATEGY = 0x01
                    _picoUart.sendCommand(MSG_CMD_CONFIG, strategyPayload, 2);
                    broadcastLog("Heating strategy set to: %d", strategy);
                }
            }
            
            if (mode == "on" || mode == "ready" || mode == "brew") {
                // Validate machine state before allowing turn on
                // Only allow turning on from IDLE, READY, or ECO states
                uint8_t currentState = machineState.machine_state;
                if (currentState != UI_STATE_IDLE && 
                    currentState != UI_STATE_READY && 
                    currentState != UI_STATE_ECO) {
                    const char* stateNames[] = {"INIT", "IDLE", "HEATING", "READY", "BREWING", "FAULT", "SAFE", "ECO"};
                    char errorMsg[128];
                    snprintf(errorMsg, sizeof(errorMsg), 
                        "Cannot turn on machine: current state is %s. Machine must be in IDLE, READY, or ECO state.",
                        (currentState < 8) ? stateNames[currentState] : "UNKNOWN");
                    broadcastLogLevel("error", errorMsg);
                    return;
                }
                modeCmd = 0x01;  // MODE_BREW
            } else if (mode == "steam") {
                modeCmd = 0x02;  // MODE_STEAM
            } else if (mode == "off" || mode == "standby" || mode == "idle") {
                modeCmd = 0x00;  // MODE_IDLE
            } else if (mode == "eco") {
                // Enter eco mode - also validate state
                uint8_t currentState = machineState.machine_state;
                if (currentState != UI_STATE_IDLE && 
                    currentState != UI_STATE_READY && 
                    currentState != UI_STATE_ECO) {
                    const char* stateNames[] = {"INIT", "IDLE", "HEATING", "READY", "BREWING", "FAULT", "SAFE", "ECO"};
                    char errorMsg[128];
                    snprintf(errorMsg, sizeof(errorMsg), 
                        "Cannot enter eco mode: current state is %s. Machine must be in IDLE, READY, or ECO state.",
                        (currentState < 8) ? stateNames[currentState] : "UNKNOWN");
                    broadcastLogLevel("error", errorMsg);
                    return;
                }
                uint8_t ecoPayload[1] = {1};
                _picoUart.sendCommand(MSG_CMD_SET_ECO, ecoPayload, 1);
                return;
            }
            
            if (_picoUart.sendCommand(MSG_CMD_MODE, &modeCmd, 1)) {
                // Pre-format message to avoid String.c_str() issues
                char modeMsg[64];
                snprintf(modeMsg, sizeof(modeMsg), "Mode set to: %s", mode.c_str());
                broadcastLog(modeMsg);
                
                // If setting to standby/idle, immediately force state to IDLE
                // This prevents the machine from staying in cooldown state
                // The state will be updated from Pico status packets, but this ensures
                // immediate response to the user command
                if (modeCmd == 0x00) {  // MODE_IDLE
                    // Force state to IDLE immediately - will be confirmed by next status packet
                    machineState.machine_state = UI_STATE_IDLE;
                    machineState.is_heating = false;
                }
            }
        }
        else if (cmd == "mqtt_test") {
            // Test MQTT connection with temporary config (doesn't modify permanent config)
            MQTTConfig testConfig;
            
            // Start with current config as base
            MQTTConfig currentConfig = _mqttClient.getConfig();
            testConfig = currentConfig;
            
            // Apply test values from command if provided
            if (!doc["broker"].isNull()) strncpy(testConfig.broker, doc["broker"].as<const char*>(), sizeof(testConfig.broker) - 1);
            if (!doc["port"].isNull()) testConfig.port = doc["port"].as<uint16_t>();
            if (!doc["username"].isNull()) strncpy(testConfig.username, doc["username"].as<const char*>(), sizeof(testConfig.username) - 1);
            if (!doc["password"].isNull()) strncpy(testConfig.password, doc["password"].as<const char*>(), sizeof(testConfig.password) - 1);
            
            // Run test (doesn't modify permanent config)
            int result = _mqttClient.testConnectionWithConfig(testConfig);
            
            // Send structured response to UI
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
            StaticJsonDocument<256> response;
            #pragma GCC diagnostic pop
            response["type"] = "mqtt_test_result";
            response["success"] = (result == 0);
            
            switch (result) {
                case 0:
                    response["message"] = "Connection successful";
                    broadcastLogLevel("info", "MQTT test: Connection successful");
                    break;
                case 1:
                    response["message"] = "Broker address is empty";
                    response["error"] = "broker_empty";
                    broadcastLogLevel("error", "MQTT test: Broker address is empty");
                    break;
                case 2:
                    response["message"] = "WiFi not connected";
                    response["error"] = "wifi_disconnected";
                    broadcastLogLevel("error", "MQTT test: WiFi not connected");
                    break;
                case 3:
                    response["message"] = "Connection failed - check broker address and credentials";
                    response["error"] = "connection_failed";
                    broadcastLogLevel("error", "MQTT test: Connection failed");
                    break;
                default:
                    response["message"] = "Unknown error";
                    response["error"] = "unknown";
                    break;
            }
            
            char jsonBuffer[256];
            serializeJson(response, jsonBuffer);
            broadcastRaw(jsonBuffer);
        }
        else if (cmd == "mqtt_config") {
            // Update MQTT config
            MQTTConfig config = _mqttClient.getConfig();
            
            if (!doc["enabled"].isNull()) config.enabled = doc["enabled"].as<bool>();
            if (!doc["broker"].isNull()) strncpy(config.broker, doc["broker"].as<const char*>(), sizeof(config.broker) - 1);
            if (!doc["port"].isNull()) config.port = doc["port"].as<uint16_t>();
            if (!doc["username"].isNull()) strncpy(config.username, doc["username"].as<const char*>(), sizeof(config.username) - 1);
            if (!doc["password"].isNull()) {
                const char* pwd = doc["password"].as<const char*>();
                if (pwd && strlen(pwd) > 0) {
                    strncpy(config.password, pwd, sizeof(config.password) - 1);
                }
            }
            if (!doc["topic"].isNull()) {
                strncpy(config.topic_prefix, doc["topic"].as<const char*>(), sizeof(config.topic_prefix) - 1);
            }
            if (!doc["discovery"].isNull()) config.ha_discovery = doc["discovery"].as<bool>();
            
            if (_mqttClient.setConfig(config)) {
                broadcastLogLevel("info", "MQTT configuration updated");
            }
        }
        else if (cmd == "set_cloud_config") {
            // Update cloud config
            auto& cloudSettings = State.settings().cloud;
            bool wasEnabled = cloudSettings.enabled;
            
            if (!doc["enabled"].isNull()) {
                cloudSettings.enabled = doc["enabled"].as<bool>();
            }
            if (!doc["serverUrl"].isNull()) {
                const char* url = doc["serverUrl"].as<const char*>();
                if (url) {
                    strncpy(cloudSettings.serverUrl, url, sizeof(cloudSettings.serverUrl) - 1);
                    cloudSettings.serverUrl[sizeof(cloudSettings.serverUrl) - 1] = '\0';
                }
            }
            
            // Save settings to NVS
            State.saveCloudSettings();
            
            // Update pairing manager and cloud connection based on enabled state
            if (cloudSettings.enabled && strlen(cloudSettings.serverUrl) > 0) {
                // Initialize pairing manager with new URL
                if (_pairingManager) {
                    _pairingManager->begin(String(cloudSettings.serverUrl));
                    
                    // Get device credentials from pairing manager
                    String deviceId = _pairingManager->getDeviceId();
                    String deviceKey = _pairingManager->getDeviceKey();
                    
                    // Sync device ID to cloud settings
                    if (String(cloudSettings.deviceId) != deviceId) {
                        strncpy(cloudSettings.deviceId, deviceId.c_str(), sizeof(cloudSettings.deviceId) - 1);
                        State.saveCloudSettings();
                    }
                    
                    // Start the cloud WebSocket connection
                    startCloudConnection(String(cloudSettings.serverUrl), deviceId, deviceKey);
                    
                    broadcastLog("Cloud enabled: %s", cloudSettings.serverUrl);
                }
            } else if (!cloudSettings.enabled && wasEnabled) {
                // Cloud was disabled - stop cloud connection
                if (_cloudConnection) {
                    _cloudConnection->end();
                    broadcastLogLevel("info", "Cloud connection stopped");
                }
                if (_pairingManager) {
                    _pairingManager->begin("");  // Clear cloud URL
                }
                broadcastLogLevel("info", "Cloud disabled");
            }
            
            broadcastLog("Cloud configuration updated: %s", cloudSettings.enabled ? "enabled" : "disabled");
        }
        else if (cmd == "add_schedule") {
            // Add a new schedule
            BrewOS::ScheduleEntry entry;
            entry.fromJson(doc.as<JsonObjectConst>());
            
            uint8_t newId = State.addSchedule(entry);
            if (newId > 0) {
                broadcastLog("Schedule added: %s", entry.name);
                broadcastDeviceInfo();  // Notify all clients of schedule changes
            }
        }
        else if (cmd == "update_schedule") {
            // Update existing schedule
            uint8_t id = doc["id"] | 0;
            if (id > 0) {
                BrewOS::ScheduleEntry entry;
                entry.fromJson(doc.as<JsonObjectConst>());
                if (State.updateSchedule(id, entry)) {
                    broadcastLogLevel("info", "Schedule updated");
                    broadcastDeviceInfo();  // Notify all clients of schedule changes
                }
            }
        }
        else if (cmd == "delete_schedule") {
            // Delete schedule
            uint8_t id = doc["id"] | 0;
            if (id > 0 && State.removeSchedule(id)) {
                broadcastLogLevel("info", "Schedule deleted");
                broadcastDeviceInfo();  // Notify all clients of schedule changes
            }
        }
        else if (cmd == "toggle_schedule") {
            // Enable/disable schedule
            uint8_t id = doc["id"] | 0;
            bool enabled = doc["enabled"] | false;
            if (id > 0 && State.enableSchedule(id, enabled)) {
                broadcastDeviceInfo();  // Notify all clients of schedule changes
            }
        }
        else if (cmd == "set_auto_off") {
            // Set auto power-off settings
            bool enabled = doc["enabled"] | false;
            uint16_t minutes = doc["minutes"] | 60;
            State.setAutoPowerOff(enabled, minutes);
            broadcastLogLevel("info", "Auto power-off updated");
            broadcastDeviceInfo();  // Notify all clients of schedule changes
        }
        else if (cmd == "get_schedules") {
            // Return schedules to requesting client
            broadcastDeviceInfo();
        }
        // Scale commands
        else if (cmd == "scale_scan") {
            if (!scaleManager || !scaleManager->isScanning()) {
                if (scaleManager && scaleManager->isConnected()) {
                    scaleManager->disconnect();
                }
                scaleManager->clearDiscovered();
                scaleManager->startScan(15000);
                broadcastLogLevel("info", "BLE scale scan started");
            }
        }
        else if (cmd == "scale_scan_stop") {
            scaleManager->stopScan();
            broadcastLogLevel("info", "BLE scale scan stopped");
        }
        else if (cmd == "scale_connect") {
            String address = doc["address"] | "";
            if (!address.isEmpty()) {
                scaleManager->connect(address.c_str());
                // Pre-format message to avoid String.c_str() issues
                char scaleMsg[64];
                snprintf(scaleMsg, sizeof(scaleMsg), "Connecting to scale: %s", address.c_str());
                broadcastLog(scaleMsg);
            }
        }
        else if (cmd == "scale_disconnect") {
            scaleManager->disconnect();
            broadcastLogLevel("info", "Scale disconnected");
        }
        else if (cmd == "tare" || cmd == "scale_tare") {
            scaleManager->tare();
            broadcastLogLevel("info", "Scale tared");
        }
        else if (cmd == "scale_reset") {
            scaleManager->tare();
            brewByWeight->reset();
            broadcastLogLevel("info", "Scale reset");
        }
        // Brew-by-weight settings
        // Accept both camelCase (from web client) and snake_case field names
        else if (cmd == "set_bbw") {
            // Target weight
            if (!doc["targetWeight"].isNull()) {
                brewByWeight->setTargetWeight(doc["targetWeight"].as<float>());
            } else if (!doc["target_weight"].isNull()) {
                brewByWeight->setTargetWeight(doc["target_weight"].as<float>());
            }
            // Dose weight
            if (!doc["doseWeight"].isNull()) {
                brewByWeight->setDoseWeight(doc["doseWeight"].as<float>());
            } else if (!doc["dose_weight"].isNull()) {
                brewByWeight->setDoseWeight(doc["dose_weight"].as<float>());
            }
            // Stop offset
            if (!doc["stopOffset"].isNull()) {
                brewByWeight->setStopOffset(doc["stopOffset"].as<float>());
            } else if (!doc["stop_offset"].isNull()) {
                brewByWeight->setStopOffset(doc["stop_offset"].as<float>());
            }
            // Enabled / Auto-stop (enabled in UI maps to auto_stop in ESP32)
            if (!doc["enabled"].isNull()) {
                brewByWeight->setAutoStop(doc["enabled"].as<bool>());
            } else if (!doc["auto_stop"].isNull()) {
                brewByWeight->setAutoStop(doc["auto_stop"].as<bool>());
            }
            // Auto-tare
            if (!doc["autoTare"].isNull()) {
                brewByWeight->setAutoTare(doc["autoTare"].as<bool>());
            } else if (!doc["auto_tare"].isNull()) {
                brewByWeight->setAutoTare(doc["auto_tare"].as<bool>());
            }
            
            broadcastLogLevel("info", "Brew-by-weight settings updated");
            
            // Broadcast updated BBW settings to all clients (including cloud)
            broadcastBBWSettings();
        }
        // Pre-infusion settings
        else if (cmd == "set_preinfusion") {
            bool enabled = doc["enabled"] | false;
            uint16_t onTimeMs = doc["onTimeMs"] | 3000;
            uint16_t pauseTimeMs = doc["pauseTimeMs"] | 5000;
            
            // Validate timing parameters
            if (onTimeMs > 10000) {
                broadcastLogLevel("error", "Pre-infusion on_time too long (max 10000ms)");
            } else if (pauseTimeMs > 30000) {
                broadcastLogLevel("error", "Pre-infusion pause_time too long (max 30000ms)");
            } else {
                // Build payload for Pico: [config_type, enabled, on_time_ms(2), pause_time_ms(2)]
                uint8_t payload[6];
                payload[0] = CONFIG_PREINFUSION;  // Config type
                payload[1] = enabled ? 1 : 0;
                payload[2] = onTimeMs & 0xFF;         // Low byte
                payload[3] = (onTimeMs >> 8) & 0xFF;  // High byte
                payload[4] = pauseTimeMs & 0xFF;      // Low byte
                payload[5] = (pauseTimeMs >> 8) & 0xFF; // High byte
                
                if (_picoUart.sendCommand(MSG_CMD_CONFIG, payload, sizeof(payload))) {
                    // Update ESP32 state for persistence
                    auto& settings = State.settings();
                    settings.brew.preinfusionTime = onTimeMs / 1000.0f;  // Store as seconds
                    settings.brew.preinfusionPressure = enabled ? 1.0f : 0.0f;  // Use as enabled flag
                    settings.brew.preinfusionPauseMs = pauseTimeMs;  // Store pause/soak time
                    State.saveBrewSettings();
                    
                    broadcastLogLevel("info", "Pre-infusion settings saved: %s, on=%dms, pause=%dms", 
                             enabled ? "enabled" : "disabled", onTimeMs, pauseTimeMs);
                    // Broadcast updated device info so UI refreshes
                    broadcastDeviceInfo();
                } else {
                    broadcastLogLevel("error", "Failed to send pre-infusion config to Pico");
                }
            }
        }
        // Power settings (accept both "set_power" and "set_power_config" for compatibility)
        else if (cmd == "set_power" || cmd == "set_power_config") {
            // Pico is the source of truth - we just relay the command and cache for UI
            uint16_t voltage = doc["voltage"] | 230;
            uint8_t maxCurrent = doc["maxCurrent"] | 15;
            
            // Cache in memory for immediate UI feedback (not persisted - Pico handles persistence)
            State.settings().power.mainsVoltage = voltage;
            State.settings().power.maxCurrent = (float)maxCurrent;
            // Note: We don't call State.savePowerSettings() - Pico is source of truth
            
            // Send to Pico as environmental config (Pico will persist it)
            uint8_t payload[4];
            payload[0] = CONFIG_ENVIRONMENTAL;  // Config type
            payload[1] = (voltage >> 8) & 0xFF;
            payload[2] = voltage & 0xFF;
            payload[3] = maxCurrent;
            _picoUart.sendCommand(MSG_CMD_CONFIG, payload, 4);
            
            broadcastLog("Power settings saved: %dV, %dA", voltage, maxCurrent);
            // Broadcast updated device info so UI refreshes
            broadcastDeviceInfo();
        }
        // Power meter configuration
        else if (cmd == "configure_power_meter") {
            String source = doc["source"] | "none";
            
            if (source == "none") {
                powerMeterManager->setSource(PowerMeterSource::NONE);
                // Send disable command to Pico
                uint8_t payload[1] = {0};  // 0 = disabled
                _picoUart.sendCommand(MSG_CMD_POWER_METER_CONFIG, payload, 1);
                broadcastLogLevel("info", "Power metering disabled");
            }
            else if (source == "hardware") {
                // Hardware meters are configured on Pico
                powerMeterManager->configureHardware();
                
                // Send enable command to Pico
                uint8_t payload[1] = {1};  // 1 = enabled
                _picoUart.sendCommand(MSG_CMD_POWER_METER_CONFIG, payload, 1);
                broadcastLogLevel("info", "Power meter configured (hardware)");
            }
            else if (source == "mqtt") {
                String topic = doc["topic"] | "";
                String format = doc["format"] | "auto";
                
                if (topic.length() > 0) {
                    if (powerMeterManager && powerMeterManager->configureMqtt(topic.c_str(), format.c_str())) {
                        // Pre-format message to avoid String.c_str() issues
                        char mqttMsg[64];
                        snprintf(mqttMsg, sizeof(mqttMsg), "MQTT power meter configured: %s", topic.c_str());
                        broadcastLog(mqttMsg);
                    } else {
                        broadcastLogLevel("error", "Failed to configure MQTT power meter");
                    }
                } else {
                    broadcastLogLevel("error", "MQTT topic required");
                }
            }
            
            // Broadcast updated status
            broadcastPowerMeterStatus();
        }
        else if (cmd == "start_power_meter_discovery") {
            // Forward discovery command to Pico
            _picoUart.sendCommand(MSG_CMD_POWER_METER_DISCOVER, nullptr, 0);
            powerMeterManager->startAutoDiscovery();
            broadcastLogLevel("info", "Starting power meter auto-discovery...");
        }
        // WiFi commands
        else if (cmd == "wifi_forget") {
            _wifiManager.clearCredentials();
            broadcastLogLevel("warn", "WiFi credentials cleared. Device will restart.");
            delay(1000);
            ESP.restart();
        }
        else if (cmd == "wifi_config") {
            // Static IP configuration
            bool staticIp = doc["staticIp"] | false;
            String ip = doc["ip"] | "";
            String gateway = doc["gateway"] | "";
            String subnet = doc["subnet"] | "255.255.255.0";
            String dns1 = doc["dns1"] | "";
            String dns2 = doc["dns2"] | "";
            
            _wifiManager.setStaticIP(staticIp, ip, gateway, subnet, dns1, dns2);
            
            if (staticIp) {
                // Pre-format message to avoid String.c_str() issues
                char ipMsg[64];
                snprintf(ipMsg, sizeof(ipMsg), "Static IP configured: %s. Reconnecting...", ip.c_str());
                broadcastLog(ipMsg);
            } else {
                broadcastLogLevel("info", "DHCP mode enabled. Reconnecting...");
            }
            
            // Reconnect to apply new settings
            delay(500);
            _wifiManager.connectToWiFi();
        }
        // System commands
        else if (cmd == "restart") {
            broadcastLogLevel("warn", "Device restarting...");
            delay(500);
            ESP.restart();
        }
        else if (cmd == "factory_reset") {
            broadcastLogLevel("warn", "Factory reset initiated...");
            State.factoryReset();
            _wifiManager.clearCredentials();
            delay(1000);
            ESP.restart();
        }
        else if (cmd == "check_update") {
            // Check for OTA updates from GitHub releases
            checkForUpdates();
        }
        else if (cmd == "ota_start") {
            // Update BrewOS firmware (combined: Pico first, then ESP32)
            String version = doc["version"] | "";
            if (version.isEmpty()) {
                broadcastLogLevel("error", "OTA error: No version specified");
            } else {
                startCombinedOTA(version);
            }
        }
        else if (cmd == "esp32_ota_start") {
            // ESP32 only - for recovery/advanced use
            String version = doc["version"] | "";
            if (version.isEmpty()) {
                broadcastLogLevel("error", "OTA error: No version specified");
            } else {
                startGitHubOTA(version);
            }
        }
        else if (cmd == "pico_ota_start") {
            // Pico only - for recovery/advanced use
            String version = doc["version"] | "";
            if (version.isEmpty()) {
                broadcastLogLevel("error", "OTA error: No version specified");
            } else {
                startPicoGitHubOTA(version);
            }
        }
        else if (cmd == "check_version_mismatch") {
            // Check and report firmware version mismatch
            checkVersionMismatch();
        }
        // Machine info
        else if (cmd == "set_machine_info" || cmd == "set_device_info") {
            auto& machineInfo = State.settings().machineInfo;
            auto& networkSettings = State.settings().network;
            
            if (!doc["name"].isNull()) {
                strncpy(machineInfo.deviceName, doc["name"].as<const char*>(), sizeof(machineInfo.deviceName) - 1);
                machineInfo.deviceName[sizeof(machineInfo.deviceName) - 1] = '\0';
                // Also update hostname for mDNS
                strncpy(networkSettings.hostname, doc["name"].as<const char*>(), sizeof(networkSettings.hostname) - 1);
                networkSettings.hostname[sizeof(networkSettings.hostname) - 1] = '\0';
            }
            // Accept both "brand" and "machineBrand" field names for compatibility
            const char* brandValue = nullptr;
            if (!doc["brand"].isNull()) {
                brandValue = doc["brand"].as<const char*>();
            } else if (!doc["machineBrand"].isNull()) {
                brandValue = doc["machineBrand"].as<const char*>();
            }
            if (brandValue) {
                strncpy(machineInfo.machineBrand, brandValue, sizeof(machineInfo.machineBrand) - 1);
                machineInfo.machineBrand[sizeof(machineInfo.machineBrand) - 1] = '\0';
            }
            // Accept both "model" and "machineModel" field names for compatibility
            const char* modelValue = nullptr;
            if (!doc["model"].isNull()) {
                modelValue = doc["model"].as<const char*>();
            } else if (!doc["machineModel"].isNull()) {
                modelValue = doc["machineModel"].as<const char*>();
            }
            if (modelValue) {
                strncpy(machineInfo.machineModel, modelValue, sizeof(machineInfo.machineModel) - 1);
                machineInfo.machineModel[sizeof(machineInfo.machineModel) - 1] = '\0';
            }
            if (!doc["machineType"].isNull()) {
                strncpy(machineInfo.machineType, doc["machineType"].as<const char*>(), sizeof(machineInfo.machineType) - 1);
                machineInfo.machineType[sizeof(machineInfo.machineType) - 1] = '\0';
            }
            
            State.saveMachineInfoSettings();
            State.saveNetworkSettings();
            
            // Broadcast device info update
            broadcastDeviceInfo();
            broadcastLog("Device info updated: %s", machineInfo.deviceName);
        }
        // User preferences (synced across devices)
        else if (cmd == "set_preferences") {
            auto& prefs = State.settings().preferences;
            
            if (!doc["firstDayOfWeek"].isNull()) {
                const char* dow = doc["firstDayOfWeek"];
                prefs.firstDayOfWeek = (strcmp(dow, "monday") == 0) ? 1 : 0;
            }
            if (!doc["use24HourTime"].isNull()) {
                prefs.use24HourTime = doc["use24HourTime"];
            }
            if (!doc["temperatureUnit"].isNull()) {
                const char* unit = doc["temperatureUnit"];
                prefs.temperatureUnit = (strcmp(unit, "fahrenheit") == 0) ? 1 : 0;
            }
            if (!doc["electricityPrice"].isNull()) {
                prefs.electricityPrice = doc["electricityPrice"].as<float>();
            }
            if (!doc["currency"].isNull()) {
                strncpy(prefs.currency, doc["currency"].as<const char*>(), sizeof(prefs.currency) - 1);
                prefs.currency[sizeof(prefs.currency) - 1] = '\0';
            }
            if (!doc["lastHeatingStrategy"].isNull()) {
                prefs.lastHeatingStrategy = doc["lastHeatingStrategy"];
            }
            
            // Mark as initialized once browser sends first preferences
            prefs.initialized = true;
            
            State.saveUserPreferences();
            broadcastDeviceInfo();
            broadcastLogLevel("info", "User preferences updated");
        }
        // Time settings
        else if (cmd == "set_time_config") {
            auto& timeSettings = State.settings().time;
            
            if (!doc["useNTP"].isNull()) timeSettings.useNTP = doc["useNTP"].as<bool>();
            if (!doc["ntpServer"].isNull()) {
                strncpy(
                    timeSettings.ntpServer,
                    doc["ntpServer"].as<const char*>(),
                    sizeof(timeSettings.ntpServer) - 1
                );
            }
            if (!doc["utcOffsetMinutes"].isNull()) timeSettings.utcOffsetMinutes = doc["utcOffsetMinutes"].as<int16_t>();
            if (!doc["dstEnabled"].isNull()) timeSettings.dstEnabled = doc["dstEnabled"].as<bool>();
            if (!doc["dstOffsetMinutes"].isNull()) timeSettings.dstOffsetMinutes = doc["dstOffsetMinutes"].as<int16_t>();
            
            State.saveTimeSettings();
            
            // Apply new NTP settings
            _wifiManager.configureNTP(
                timeSettings.ntpServer,
                timeSettings.utcOffsetMinutes,
                timeSettings.dstEnabled,
                timeSettings.dstOffsetMinutes
            );
            
            broadcastDeviceInfo();
            broadcastLogLevel("info", "Time settings updated");
        }
        // Get time status (for UI polling)
        else if (cmd == "get_time_status") {
            TimeStatus ts = _wifiManager.getTimeStatus();
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
            StaticJsonDocument<512> tsDoc;
            #pragma GCC diagnostic pop
            tsDoc["type"] = "time_status";
            tsDoc["synced"] = ts.ntpSynced;
            tsDoc["currentTime"] = ts.currentTime;
            tsDoc["timezone"] = ts.timezone;
            tsDoc["utcOffset"] = ts.utcOffset;
            
            size_t jsonSize = measureJson(tsDoc) + 1;
            char* jsonBuffer = allocateJsonBuffer(jsonSize);
            if (jsonBuffer) {
                serializeJson(tsDoc, jsonBuffer, jsonSize);
                broadcastRaw(jsonBuffer);
                free(jsonBuffer);
            }
        }
        // Force NTP sync
        else if (cmd == "sync_time") {
            if (_wifiManager.isConnected()) {
                _wifiManager.syncNTP();
                broadcastLogLevel("info", "NTP sync initiated");
            } else {
                broadcastLogLevel("error", "Cannot sync time: WiFi not connected");
            }
        }
        // Maintenance records
        else if (cmd == "record_maintenance") {
            String type = doc["type"] | "";
            if (!type.isEmpty()) {
                State.recordMaintenance(type.c_str());
                // Pre-format message to avoid String.c_str() issues
                char maintMsg[64];
                snprintf(maintMsg, sizeof(maintMsg), "Maintenance recorded: %s", type.c_str());
                broadcastLog(maintMsg);
            }
        }
        // Diagnostics commands
        else if (cmd == "run_diagnostics") {
            // Run all diagnostic tests
            uint8_t payload[1] = { 0x00 };  // DIAG_TEST_ALL
            if (_picoUart.sendCommand(MSG_CMD_DIAGNOSTICS, payload, 1)) {
                broadcastLogLevel("info", "Running hardware diagnostics...");
            } else {
                broadcastLogLevel("error", "Failed to start diagnostics");
            }
        }
        else if (cmd == "run_diagnostic_test") {
            // Run a single diagnostic test
            uint8_t testId = doc["testId"] | 0;
            uint8_t payload[1] = { testId };
            if (_picoUart.sendCommand(MSG_CMD_DIAGNOSTICS, payload, 1)) {
                broadcastLog("Running diagnostic test %d", testId);
            } else {
                broadcastLogLevel("error", "Failed to start diagnostic test");
            }
        }
    }
}
