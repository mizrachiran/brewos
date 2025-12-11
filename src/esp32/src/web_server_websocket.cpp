#include "web_server.h"
#include "config.h"
#include "pico_uart.h"
#include "mqtt_client.h"
#include "brew_by_weight.h"
#include "scale/scale_manager.h"
#include "pairing_manager.h"
#include "state/state_manager.h"
#include "power_meter/power_meter_manager.h"
#include "ui/ui.h"
#include <ArduinoJson.h>
#include <stdarg.h>

// WebSocket event handler for AsyncWebSocket (ESP32Async library)
void WebServer::handleWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_DISCONNECT:
            LOG_I("WebSocket client %u disconnected", client->id());
            break;
            
        case WS_EVT_CONNECT:
            LOG_I("WebSocket client %u connected from %s", client->id(), client->remoteIP().toString().c_str());
            // Send device info immediately so UI has the saved settings
            broadcastDeviceInfo();
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

void WebServer::handleWsMessage(uint32_t clientNum, uint8_t* payload, size_t length) {
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
void WebServer::processCommand(JsonDocument& doc) {
    String type = doc["type"] | "";
    
    if (type == "ping") {
        LOG_I("Command: ping");
        _picoUart.sendPing();
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
            // Payload: enabled (bool), brewTemp (float), timeout (int minutes)
            bool enabled = doc["enabled"] | true;
            float brewTemp = doc["brewTemp"] | 80.0f;
            int timeout = doc["timeout"] | 30;
            
            // Save to State and NVS first
            auto& tempSettings = State.settings().temperature;
            tempSettings.ecoBrewTemp = brewTemp;
            tempSettings.ecoTimeoutMinutes = (uint16_t)timeout;
            State.saveTemperatureSettings();
            
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
            String boiler = doc["boiler"] | "brew";
            float temp = doc["temp"] | 0.0f;
            
            // Save to State and NVS first
            auto& tempSettings = State.settings().temperature;
            if (boiler == "steam") {
                tempSettings.steamSetpoint = temp;
            } else {
                tempSettings.brewSetpoint = temp;
            }
            State.saveTemperatureSettings();
            
            uint8_t payload[5];
            payload[0] = (boiler == "steam") ? 0x02 : 0x01;
            memcpy(&payload[1], &temp, sizeof(float));
            if (_picoUart.sendCommand(MSG_CMD_SET_TEMP, payload, 5)) {
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
                modeCmd = 0x01;  // MODE_BREW
            } else if (mode == "steam") {
                modeCmd = 0x02;  // MODE_STEAM
            } else if (mode == "off" || mode == "standby" || mode == "idle") {
                modeCmd = 0x00;  // MODE_IDLE
            } else if (mode == "eco") {
                // Enter eco mode
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
                    extern ui_state_t machineState;
                    machineState.machine_state = UI_STATE_IDLE;
                    machineState.is_heating = false;
                }
            }
        }
        else if (cmd == "mqtt_test") {
            // Test MQTT connection
            MQTTConfig config = _mqttClient.getConfig();
            
            // Apply temporary config from command if provided
            if (!doc["broker"].isNull()) strncpy(config.broker, doc["broker"].as<const char*>(), sizeof(config.broker) - 1);
            if (!doc["port"].isNull()) config.port = doc["port"].as<uint16_t>();
            if (!doc["username"].isNull()) strncpy(config.username, doc["username"].as<const char*>(), sizeof(config.username) - 1);
            if (!doc["password"].isNull()) strncpy(config.password, doc["password"].as<const char*>(), sizeof(config.password) - 1);
            
            // Temporarily apply and test
            config.enabled = true;
            _mqttClient.setConfig(config);
            
            if (_mqttClient.testConnection()) {
                broadcastLogLevel("info", "MQTT connection test successful");
            } else {
                broadcastLogLevel("error", "MQTT connection test failed");
            }
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
            
            // Update pairing manager based on enabled state
            if (_pairingManager) {
                if (cloudSettings.enabled && strlen(cloudSettings.serverUrl) > 0) {
                    // Initialize or update with new URL
                    _pairingManager->begin(String(cloudSettings.serverUrl));
                    
                    // Register device key with cloud server
                    if (_pairingManager->registerTokenWithCloud()) {
                        broadcastLog("Cloud enabled and device registered: %s", cloudSettings.serverUrl);
                    } else {
                        broadcastLog("Cloud enabled: %s (registration pending)", cloudSettings.serverUrl);
                    }
                } else if (!cloudSettings.enabled && wasEnabled) {
                    // Cloud was disabled - clear pairing manager
                    _pairingManager->begin("");  // Clear cloud URL
                    broadcastLogLevel("info", "Cloud disabled");
                }
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
                }
            }
        }
        else if (cmd == "delete_schedule") {
            // Delete schedule
            uint8_t id = doc["id"] | 0;
            if (id > 0 && State.removeSchedule(id)) {
                broadcastLogLevel("info", "Schedule deleted");
            }
        }
        else if (cmd == "toggle_schedule") {
            // Enable/disable schedule
            uint8_t id = doc["id"] | 0;
            bool enabled = doc["enabled"] | false;
            if (id > 0) {
                State.enableSchedule(id, enabled);
            }
        }
        else if (cmd == "set_auto_off") {
            // Set auto power-off settings
            bool enabled = doc["enabled"] | false;
            uint16_t minutes = doc["minutes"] | 60;
            State.setAutoPowerOff(enabled, minutes);
            broadcastLogLevel("info", "Auto power-off updated");
        }
        else if (cmd == "get_schedules") {
            // Return schedules to requesting client (if needed)
            // Usually schedules are sent via full state broadcast
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
        else if (cmd == "set_bbw") {
            if (!doc["target_weight"].isNull()) {
                brewByWeight->setTargetWeight(doc["target_weight"].as<float>());
            }
            if (!doc["dose_weight"].isNull()) {
                brewByWeight->setDoseWeight(doc["dose_weight"].as<float>());
            }
            if (!doc["stop_offset"].isNull()) {
                brewByWeight->setStopOffset(doc["stop_offset"].as<float>());
            }
            if (!doc["auto_stop"].isNull()) {
                brewByWeight->setAutoStop(doc["auto_stop"].as<bool>());
            }
            if (!doc["auto_tare"].isNull()) {
                brewByWeight->setAutoTare(doc["auto_tare"].as<bool>());
            }
            broadcastLogLevel("info", "Brew-by-weight settings updated");
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
            uint16_t voltage = doc["voltage"] | 230;
            uint8_t maxCurrent = doc["maxCurrent"] | 15;
            
            // Send to Pico as environmental config
            uint8_t payload[4];
            payload[0] = CONFIG_ENVIRONMENTAL;  // Config type
            payload[1] = (voltage >> 8) & 0xFF;
            payload[2] = voltage & 0xFF;
            payload[3] = maxCurrent;
            _picoUart.sendCommand(MSG_CMD_CONFIG, payload, 4);
            
            // Also save to local settings
            State.settings().power.mainsVoltage = voltage;
            State.settings().power.maxCurrent = (float)maxCurrent;
            State.savePowerSettings();
            
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
                prefs.electricityPrice = doc["electricityPrice"];
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
