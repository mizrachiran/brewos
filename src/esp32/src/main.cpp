/**
 * BrewOS ESP32-S3 Display Module
 * 
 * Target: UEDX48480021-MD80E (2.1" Round Knob Display)
 * 
 * Main firmware for the ESP32-S3 that provides:
 * - 480x480 round LVGL display with coffee-themed UI
 * - Rotary encoder + button navigation
 * - WiFi connectivity (AP setup mode + STA mode)
 * - Web interface for monitoring and configuration
 * - UART bridge to Pico control board
 * - OTA firmware updates for Pico
 * - BLE scale integration
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <cmath>
#include "config.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "pico_uart.h"

// Display and UI
#include "display/display.h"
#include "display/encoder.h"
#include "display/theme.h"
#include "ui/ui.h"

// MQTT
#include "mqtt_client.h"

// BLE Scale
#include "scale/scale_manager.h"

// Brew-by-Weight
#include "brew_by_weight.h"

// State Manager
#include "state/state_manager.h"

// Notifications
#include "notifications/notification_manager.h"
#include "notifications/cloud_notifier.h"

// Pairing
#include "pairing_manager.h"

// Cloud Connection
#include "cloud_connection.h"

// Power Metering
#include "power_meter/power_meter_manager.h"

// Global instances
WiFiManager wifiManager;
PicoUART picoUart(Serial1);
MQTTClient mqttClient;
PairingManager pairingManager;
CloudConnection cloudConnection;
WebServer webServer(wifiManager, picoUart, mqttClient, &pairingManager);

// =============================================================================
// LOG LEVEL CONTROL
// =============================================================================
LogLevel g_log_level = LOG_LEVEL_INFO;  // Default to INFO level

void setLogLevel(LogLevel level) {
    g_log_level = level;
    Serial.printf("[Log] Level set to: %s (%d)\n", logLevelToString(level), level);
}

LogLevel getLogLevel() {
    return g_log_level;
}

const char* logLevelToString(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_ERROR: return "error";
        case LOG_LEVEL_WARN:  return "warn";
        case LOG_LEVEL_INFO:  return "info";
        case LOG_LEVEL_DEBUG: return "debug";
        default: return "unknown";
    }
}

LogLevel stringToLogLevel(const char* str) {
    if (!str) return LOG_LEVEL_INFO;
    if (strcasecmp(str, "error") == 0) return LOG_LEVEL_ERROR;
    if (strcasecmp(str, "warn") == 0) return LOG_LEVEL_WARN;
    if (strcasecmp(str, "warning") == 0) return LOG_LEVEL_WARN;
    if (strcasecmp(str, "info") == 0) return LOG_LEVEL_INFO;
    if (strcasecmp(str, "debug") == 0) return LOG_LEVEL_DEBUG;
    // Try numeric
    int level = atoi(str);
    if (level >= 0 && level <= 3) return (LogLevel)level;
    return LOG_LEVEL_INFO;
}

// Scale state
static bool scaleEnabled = true;  // Can be disabled to save power

// Machine state from Pico
static ui_state_t machineState = {0};

// Demo mode - simulates Pico data when Pico not connected
#define DEMO_MODE_TIMEOUT_MS  10000  // Enter demo after 10s without Pico
static bool demoMode = false;
static unsigned long demoStartTime = 0;

// Timing
unsigned long lastPing = 0;
unsigned long lastStatusBroadcast = 0;
unsigned long lastPowerMeterBroadcast = 0;
unsigned long lastUIUpdate = 0;

// Forward declarations
void parsePicoStatus(const uint8_t* payload, uint8_t length);
void handleEncoderEvent(int32_t diff, button_state_t btn);
void updateDemoMode();

/**
 * Demo mode - simulates machine behavior when Pico not connected
 * Useful for testing the UI without any hardware
 */
void updateDemoMode() {
    static unsigned long lastDemoUpdate = 0;
    static float demoCycle = 0;
    
    // Only update every 100ms
    if (millis() - lastDemoUpdate < 100) return;
    lastDemoUpdate = millis();
    
    // Cycle through demo states
    demoCycle += 0.01f;
    if (demoCycle > 1.0f) demoCycle = 0;
    
    // Simulate heating up
    float heatProgress = demoCycle;
    
    // Simulate temperatures approaching setpoints
    machineState.brew_temp = 25.0f + (machineState.brew_setpoint - 25.0f) * heatProgress;
    machineState.steam_temp = 25.0f + (machineState.steam_setpoint - 25.0f) * heatProgress;
    
    // Simulate pressure (varies slightly)
    machineState.pressure = 9.0f + sin(millis() / 1000.0f) * 0.3f;
    
    // Update state based on temperature
    if (heatProgress < 0.95f) {
        machineState.machine_state = STATE_HEATING;
        machineState.is_heating = true;
    } else {
        machineState.machine_state = STATE_READY;
        machineState.is_heating = false;
    }
    
    // Simulate periodic brewing
    static unsigned long brewStart = 0;
    if (demoCycle > 0.5f && demoCycle < 0.7f) {
        if (!machineState.is_brewing) {
            brewStart = millis();
            machineState.is_brewing = true;
            machineState.machine_state = STATE_BREWING;
            LOG_I("[DEMO] Simulated brew started");
        }
        machineState.brew_time_ms = millis() - brewStart;
        machineState.brew_weight = machineState.brew_time_ms / 1000.0f * 1.5f;  // ~1.5g/s
        machineState.flow_rate = 1.5f + sin(millis() / 500.0f) * 0.3f;
    } else {
        if (machineState.is_brewing) {
            machineState.is_brewing = false;
            LOG_I("[DEMO] Simulated brew ended: %.1fs, %.1fg", 
                  machineState.brew_time_ms / 1000.0f, machineState.brew_weight);
        }
    }
    
    // Always connected in demo mode
    machineState.pico_connected = true;  // Pretend Pico is connected
}

void setup() {
    // Initialize debug serial (USB)
    Serial.begin(DEBUG_BAUD);
    delay(1000);  // Wait for USB serial
    
    LOG_I("========================================");
    LOG_I("BrewOS ESP32-S3 v%s", ESP32_VERSION);
    LOG_I("UEDX48480021-MD80E Knob Display");
    LOG_I("========================================");
    LOG_I("Free heap: %d bytes", ESP.getFreeHeap());
    LOG_I("PSRAM size: %d bytes", ESP.getPsramSize());
    
    // Initialize display
    LOG_I("Initializing display...");
    if (!display.begin()) {
        LOG_E("Display initialization failed!");
    }
    
    // Initialize encoder
    LOG_I("Initializing encoder...");
    if (!encoder.begin()) {
        LOG_E("Encoder initialization failed!");
    }
    encoder.setCallback(handleEncoderEvent);
    
    // Initialize UI
    LOG_I("Initializing UI...");
    if (!ui.begin()) {
        LOG_E("UI initialization failed!");
    }
    
    // Set up UI callbacks
    ui.onTurnOn([]() {
        LOG_I("UI: Turn on requested");
        // Send turn-on command to Pico
        uint8_t cmd = 0x01;  // Turn on
        picoUart.sendCommand(MSG_CMD_MODE, &cmd, 1);
    });
    
    ui.onTurnOff([]() {
        LOG_I("UI: Turn off requested");
        // Send turn-off command to Pico
        uint8_t cmd = 0x00;  // Turn off
        picoUart.sendCommand(MSG_CMD_MODE, &cmd, 1);
    });
    
    ui.onSetTemp([](bool is_steam, float temp) {
        LOG_I("UI: Set %s temp to %.1f°C", is_steam ? "steam" : "brew", temp);
        // Send temperature set command to Pico
        uint8_t payload[5];
        payload[0] = is_steam ? 0x02 : 0x01;  // Boiler ID
        memcpy(&payload[1], &temp, sizeof(float));
        picoUart.sendCommand(MSG_CMD_SET_TEMP, payload, 5);
    });
    
    ui.onTareScale([]() {
        LOG_I("UI: Tare scale requested");
        scaleManager.tare();
    });
    
    ui.onSetTargetWeight([](float weight) {
        LOG_I("UI: Set target weight to %.1fg", weight);
        brewByWeight.setTargetWeight(weight);
        machineState.target_weight = weight;
    });
    
    ui.onWifiSetup([]() {
        LOG_I("UI: WiFi setup requested - resetting to DHCP and starting AP mode");
        // Reset static IP to DHCP before starting AP mode
        wifiManager.setStaticIP(false);
        // Start AP mode for network configuration
        wifiManager.startAP();
    });
    
    // Initialize Pico UART
    picoUart.begin();
    
    // Set up packet handler
    picoUart.onPacket([](const PicoPacket& packet) {
        LOG_D("Pico packet: type=0x%02X, len=%d, seq=%d", 
              packet.type, packet.length, packet.seq);
        
        // Forward to WebSocket clients
        webServer.broadcastPicoMessage(packet.type, packet.payload, packet.length);
        
        // Handle specific message types
        switch (packet.type) {
            case MSG_BOOT: {
                LOG_I("Pico booted!");
                webServer.broadcastLog("Pico booted", "info");
                ui.showNotification("Pico Connected", 2000);
                machineState.pico_connected = true;
                
                // Parse boot payload (boot_payload_t structure)
                // version_major(1), version_minor(1), version_patch(1), machine_type(1), 
                // pcb_type(1), pcb_version_major(1), pcb_version_minor(1), reset_reason(1)
                if (packet.length >= 4) {
                    uint8_t ver_major = packet.payload[0];
                    uint8_t ver_minor = packet.payload[1];
                    uint8_t ver_patch = packet.payload[2];
                    machineState.machine_type = packet.payload[3];
                    
                    // Store in StateManager
                    State.setPicoVersion(ver_major, ver_minor, ver_patch);
                    State.setMachineType(machineState.machine_type);
                    
                    LOG_I("Pico version: %d.%d.%d, Machine type: %d", 
                          ver_major, ver_minor, ver_patch, machineState.machine_type);
                    
                    // Parse reset reason if available (offset 7, uint8_t)
                    if (packet.length >= 8) {
                        uint8_t reset_reason = packet.payload[7];
                        State.setPicoResetReason(reset_reason);
                    }
                    
                    // Check for version mismatch (internal - don't expose to user)
                    char picoVerStr[16];
                    snprintf(picoVerStr, sizeof(picoVerStr), "%d.%d.%d", ver_major, ver_minor, ver_patch);
                    if (strcmp(picoVerStr, ESP32_VERSION) != 0) {
                        LOG_W("Internal version mismatch: %s vs %s", ESP32_VERSION, picoVerStr);
                        // Silently suggest update - user doesn't need to know about internal modules
                        webServer.broadcastLog("Firmware update recommended", "warning");
                    }
                }
                break;
            }
                
            case MSG_STATUS:
                // Parse status and update UI
                parsePicoStatus(packet.payload, packet.length);
                machineState.pico_connected = true;
                // Note: Full status broadcast is done periodically in loop()
                break;
            
            case MSG_POWER_METER: {
                // Power meter reading from Pico
                if (packet.length >= sizeof(PowerMeterReading)) {
                    PowerMeterReading reading;
                    memcpy(&reading, packet.payload, sizeof(PowerMeterReading));
                    powerMeterManager.onPicoPowerData(reading);
                }
                break;
            }
            
            case MSG_ALARM: {
                // Alarm - show alarm screen
                uint8_t alarmCode = packet.payload[0];
                LOG_W("PICO ALARM: 0x%02X", alarmCode);
                webServer.broadcastLog("Pico ALARM: " + String(alarmCode, HEX), "error");
                machineState.alarm_active = true;
                machineState.alarm_code = alarmCode;
                ui.showAlarm(alarmCode, nullptr);
                break;
            }
            
            case MSG_CONFIG:
                LOG_I("Received config from Pico");
                webServer.broadcastLog("Config received from Pico", "info");
                break;
                
            case MSG_ENV_CONFIG: {
                // Environmental configuration (voltage, current limits)
                if (packet.length >= 18) {
                    uint16_t voltage = packet.payload[0] | (packet.payload[1] << 8);
                    float max_current = 0;
                    memcpy(&max_current, &packet.payload[2], sizeof(float));
                    LOG_I("Env config: %dV, %.1fA max", voltage, max_current);
                    webServer.broadcastLog("Env config: " + String(voltage) + "V, " + String(max_current, 1) + "A max", "info");
                }
                break;
            }
                
            case MSG_DEBUG_RESP:
                LOG_D("Debug response from Pico");
                break;
                
            case MSG_DIAGNOSTICS: {
                // Diagnostic results from Pico
                // First check if this is a header (8 bytes) or result (32 bytes)
                if (packet.length == 8) {
                    // Diagnostic header
                    LOG_I("Diag header: tests=%d, pass=%d, fail=%d, warn=%d, skip=%d, complete=%d",
                          packet.payload[0], packet.payload[1], packet.payload[2],
                          packet.payload[3], packet.payload[4], packet.payload[5]);
                    
                    // Broadcast to WebSocket clients
                    JsonDocument doc;
                    doc["type"] = "diagnostics_header";
                    doc["testCount"] = packet.payload[0];
                    doc["passCount"] = packet.payload[1];
                    doc["failCount"] = packet.payload[2];
                    doc["warnCount"] = packet.payload[3];
                    doc["skipCount"] = packet.payload[4];
                    doc["isComplete"] = packet.payload[5] != 0;
                    doc["durationMs"] = packet.payload[6] | (packet.payload[7] << 8);
                    String json;
                    serializeJson(doc, json);
                    webServer.broadcastRaw(json);
                    
                    if (packet.payload[5]) {  // is_complete
                        webServer.broadcastLog("Diagnostics complete", "info");
                    }
                } else if (packet.length >= 32) {
                    // Diagnostic result
                    LOG_I("Diag result: test=%d, status=%d", packet.payload[0], packet.payload[1]);
                    
                    // Broadcast to WebSocket clients
                    JsonDocument doc;
                    doc["type"] = "diagnostics_result";
                    doc["testId"] = packet.payload[0];
                    doc["status"] = packet.payload[1];
                    doc["rawValue"] = (int16_t)(packet.payload[2] | (packet.payload[3] << 8));
                    doc["expectedMin"] = (int16_t)(packet.payload[4] | (packet.payload[5] << 8));
                    doc["expectedMax"] = (int16_t)(packet.payload[6] | (packet.payload[7] << 8));
                    
                    // Extract message (remaining bytes, null-terminated)
                    char msg[25];
                    memcpy(msg, &packet.payload[8], 24);
                    msg[24] = '\0';
                    doc["message"] = msg;
                    
                    String json;
                    serializeJson(doc, json);
                    webServer.broadcastRaw(json);
                }
                break;
            }
                
            default:
                LOG_D("Unknown packet type: 0x%02X", packet.type);
        }
    });
    
    // Initialize WiFi
    wifiManager.onConnected([]() {
        LOG_I("WiFi connected!");
        webServer.broadcastLog("WiFi connected: " + wifiManager.getIP(), "info");
        machineState.wifi_connected = true;
        machineState.wifi_ap_mode = false;
        // Get WiFi SSID from status
        WiFiStatus ws = wifiManager.getStatus();
        strncpy(machineState.wifi_ssid, ws.ssid.c_str(), sizeof(machineState.wifi_ssid) - 1);
        strncpy(machineState.wifi_ip, ws.ip.c_str(), sizeof(machineState.wifi_ip) - 1);
        machineState.wifi_rssi = WiFi.RSSI();
        ui.showNotification("WiFi Connected", 2000);
        
        // Configure and sync NTP
        auto& timeSettings = State.settings().time;
        wifiManager.configureNTP(
            timeSettings.ntpServer,
            timeSettings.utcOffsetMinutes,
            timeSettings.dstEnabled,
            timeSettings.dstOffsetMinutes
        );
        
        // Start mDNS responder for brewos.local
        if (MDNS.begin("brewos")) {
            LOG_I("mDNS started: http://brewos.local");
            MDNS.addService("http", "tcp", 80);
            webServer.broadcastLog("Access via: http://brewos.local", "info");
        } else {
            LOG_W("mDNS failed to start");
        }
        
        // Try MQTT connection if enabled
        if (mqttClient.getConfig().enabled) {
            mqttClient.testConnection();
        }
    });
    
    wifiManager.onDisconnected([]() {
        LOG_W("WiFi disconnected");
        machineState.wifi_connected = false;
    });
    
    wifiManager.onAPStarted([]() {
        LOG_I("AP mode started - connect to: %s", WIFI_AP_SSID);
        LOG_I("Open http://%s to configure", WIFI_AP_IP.toString().c_str());
        machineState.wifi_ap_mode = true;
        machineState.wifi_connected = false;
        ui.showScreen(SCREEN_SETUP);
    });
    
    wifiManager.begin();
    
    // Start web server
    webServer.begin();
    
    // Initialize MQTT
    mqttClient.begin();
    
    // Initialize Power Meter Manager
    powerMeterManager.begin();
    
    // Set up MQTT command handler
    mqttClient.onCommand([](const char* cmd, const JsonDocument& doc) {
        String cmdStr = cmd;
        
        if (cmdStr == "set_temp") {
            String boiler = doc["boiler"] | "brew";
            float temp = doc["temp"] | 0.0f;
            
            uint8_t payload[5];
            payload[0] = (boiler == "steam") ? 0x02 : 0x01;
            memcpy(&payload[1], &temp, sizeof(float));
            picoUart.sendCommand(MSG_CMD_SET_TEMP, payload, 5);
        }
        else if (cmdStr == "set_mode") {
            String mode = doc["mode"] | "";
            uint8_t modeCmd = 0;
            
            if (mode == "on" || mode == "ready") {
                modeCmd = 0x01;
            } else if (mode == "off" || mode == "standby") {
                modeCmd = 0x00;
            }
            picoUart.sendCommand(MSG_CMD_MODE, &modeCmd, 1);
        }
        else if (cmdStr == "tare") {
            scaleManager.tare();
        }
        else if (cmdStr == "set_target_weight") {
            float weight = doc["weight"] | 0.0f;
            if (weight > 0) {
                brewByWeight.setTargetWeight(weight);
                machineState.target_weight = weight;
            }
        }
        else if (cmdStr == "set_eco") {
            // Set eco mode config
            bool enabled = doc["enabled"] | true;
            float brewTemp = doc["brewTemp"] | 80.0f;
            int timeout = doc["timeout"] | 30;
            
            // Convert to Pico format: [enabled:1][eco_brew_temp:2][timeout_minutes:2]
            uint8_t payload[5];
            payload[0] = enabled ? 1 : 0;
            int16_t tempScaled = (int16_t)(brewTemp * 10);  // Convert to Celsius * 10
            payload[1] = (tempScaled >> 8) & 0xFF;
            payload[2] = tempScaled & 0xFF;
            payload[3] = (timeout >> 8) & 0xFF;
            payload[4] = timeout & 0xFF;
            
            picoUart.sendCommand(MSG_CMD_SET_ECO, payload, 5);
        }
        else if (cmdStr == "enter_eco") {
            uint8_t payload[1] = {1};  // 1 = enter eco
            picoUart.sendCommand(MSG_CMD_SET_ECO, payload, 1);
        }
        else if (cmdStr == "exit_eco") {
            uint8_t payload[1] = {0};  // 0 = exit eco
            picoUart.sendCommand(MSG_CMD_SET_ECO, payload, 1);
        }
    });
    
    // Initialize BLE Scale Manager
    if (scaleEnabled) {
        LOG_I("Initializing BLE Scale Manager...");
        if (scaleManager.begin()) {
            // Set up scale callbacks - just update state, unified broadcast handles web clients
            scaleManager.onWeight([](const scale_state_t& state) {
                machineState.scale_connected = state.connected;
                machineState.brew_weight = state.weight;
                machineState.flow_rate = state.flow_rate;
            });
            
            scaleManager.onConnection([](bool connected) {
                LOG_I("Scale %s", connected ? "connected" : "disconnected");
                machineState.scale_connected = connected;
                if (connected) {
                    ui.showNotification("Scale Connected", 2000);
                }
            });
            
            LOG_I("Scale Manager ready");
        } else {
            LOG_E("Scale Manager initialization failed!");
        }
    }
    
    // Initialize Brew-by-Weight controller
    LOG_I("Initializing Brew-by-Weight...");
    brewByWeight.begin();
    
    // Connect brew-by-weight callbacks
    brewByWeight.onStop([]() {
        LOG_I("BBW: Sending WEIGHT_STOP signal to Pico");
        picoUart.setWeightStop(true);
        ui.showNotification("Target Reached!", 2000);
        
        // Clear stop signal after a short delay
        // (Pico will latch the stop, we just pulse the signal)
        delay(100);
        picoUart.setWeightStop(false);
    });
    
    brewByWeight.onTare([]() {
        scaleManager.tare();
    });
    
    // Set default state values from BBW settings
    machineState.brew_setpoint = 93.0f;
    machineState.steam_setpoint = 145.0f;
    machineState.target_weight = brewByWeight.getTargetWeight();
    machineState.dose_weight = brewByWeight.getDoseWeight();
    machineState.brew_max_temp = 105.0f;
    machineState.steam_max_temp = 160.0f;
    machineState.dose_weight = 18.0f;
    
    // Initialize State Manager (schedules, settings persistence)
    LOG_I("Initializing State Manager...");
    State.begin();
    
    // Initialize Pairing Manager and Cloud Connection if cloud is enabled
    auto& cloudSettings = State.settings().cloud;
    if (cloudSettings.enabled && strlen(cloudSettings.serverUrl) > 0) {
        LOG_I("Initializing Pairing Manager...");
        pairingManager.begin(String(cloudSettings.serverUrl));
        
        // Get device ID and key from pairing manager (it manages these securely)
        String deviceId = pairingManager.getDeviceId();
        String deviceKey = pairingManager.getDeviceKey();
        
        // Sync device ID to cloud settings if different (for display purposes)
        if (String(cloudSettings.deviceId) != deviceId) {
            strncpy(cloudSettings.deviceId, deviceId.c_str(), sizeof(cloudSettings.deviceId) - 1);
            State.saveCloudSettings();
        }
        
        // Initialize Cloud Connection for real-time state relay
        // Uses pairing manager's device key for secure authentication
        LOG_I("Initializing Cloud Connection...");
        cloudConnection.begin(
            String(cloudSettings.serverUrl),
            deviceId,
            deviceKey
        );
        
        // Set up command handler - forward cloud commands to WebServer
        cloudConnection.onCommand([&webServer](const String& type, JsonDocument& doc) {
            // Commands from cloud users are processed the same as local WebSocket
            webServer.processCommand(doc);
        });
        
        // Connect cloud to WebServer for state broadcasting
        webServer.setCloudConnection(&cloudConnection);
    }
    
    // Initialize Notification Manager
    LOG_I("Initializing Notification Manager...");
    notificationManager.begin();
    
    // Set up cloud notification callback
    // Capture device key from pairing manager for authenticated notifications
    String cloudDeviceKey = cloudSettings.enabled ? pairingManager.getDeviceKey() : "";
    notificationManager.onCloud([cloudDeviceKey](const Notification& notif) {
        // Check if cloud integration is enabled
        auto& cloudSettings = State.settings().cloud;
        if (!cloudSettings.enabled) {
            return;
        }
        
        String cloudUrl = String(cloudSettings.serverUrl);
        String deviceId = String(cloudSettings.deviceId);
        
        if (!cloudUrl.isEmpty() && !deviceId.isEmpty()) {
            sendNotificationToCloud(cloudUrl, deviceId, cloudDeviceKey, notif);
        }
    });
    
    // Set up schedule callback
    State.onScheduleTriggered([](const BrewOS::ScheduleEntry& schedule) {
        LOG_I("Schedule triggered: %s", schedule.name);
        
        if (schedule.action == BrewOS::ACTION_TURN_ON) {
            // Turn on machine with specified heating strategy
            uint8_t modeCmd = 0x01;  // MODE_BREW
            picoUart.sendCommand(MSG_CMD_MODE, &modeCmd, 1);
            
            // Set heating strategy if not default
            if (schedule.strategy != BrewOS::STRATEGY_SEQUENTIAL) {
                uint8_t strategyPayload[2] = {0x01, schedule.strategy};  // CONFIG_HEATING_STRATEGY
                picoUart.sendCommand(MSG_CMD_CONFIG, strategyPayload, 2);
            }
            
            ui.showNotification("Schedule: ON", 3000);
        } else {
            // Turn off machine
            uint8_t modeCmd = 0x00;  // MODE_IDLE
            picoUart.sendCommand(MSG_CMD_MODE, &modeCmd, 1);
            
            ui.showNotification("Schedule: OFF", 3000);
        }
    });
    
    LOG_I("Setup complete. Free heap: %d bytes", ESP.getFreeHeap());
}

void loop() {
    // Update display (calls LVGL timer handler)
    display.update();
    
    // Update encoder
    encoder.update();
    
    // Update WiFi manager
    wifiManager.loop();
    
    // Process Pico UART
    picoUart.loop();
    
    // Update web server
    webServer.loop();
    
    // Update cloud connection (WebSocket to cloud relay)
    cloudConnection.loop();
    
    // Update MQTT
    mqttClient.loop();
    
    // Update Power Meter
    powerMeterManager.loop();
    
    // Update BLE Scale
    if (scaleEnabled) {
        scaleManager.loop();
    }
    
    // Update Brew-by-Weight controller
    brewByWeight.update(
        machineState.is_brewing,
        scaleManager.getState().weight,
        scaleManager.isConnected()
    );
    
    // Update State Manager (schedules, auto power-off)
    State.loop();
    
    // Reset idle timer on user activity (brewing, encoder, etc.)
    static bool lastPressed = false;
    bool currentPressed = encoder.isPressed();
    if (machineState.is_brewing || (currentPressed && !lastPressed) || encoder.getPosition() != 0) {
        State.resetIdleTimer();
        encoder.resetPosition();  // Reset so we detect the next movement
    }
    lastPressed = currentPressed;
    
    // Sync BBW state to machine state
    if (brewByWeight.isActive()) {
        machineState.brew_weight = brewByWeight.getState().current_weight;
        machineState.target_weight = brewByWeight.getTargetWeight();
        machineState.dose_weight = brewByWeight.getDoseWeight();
    }
    
    // Update Pico connection status
    bool picoConnected = picoUart.isConnected();
    machineState.mqtt_connected = mqttClient.isConnected();
    machineState.scale_connected = scaleManager.isConnected();
    machineState.cloud_connected = cloudConnection.isConnected();
    
    // Demo mode: If Pico not connected for DEMO_MODE_TIMEOUT_MS, simulate data
    if (!picoConnected && picoUart.getPacketsReceived() == 0) {
        if (!demoMode && millis() > DEMO_MODE_TIMEOUT_MS) {
            demoMode = true;
            demoStartTime = millis();
            LOG_I("=== DEMO MODE ENABLED (no Pico detected) ===");
            ui.showNotification("Demo Mode", 3000);
        }
    } else {
        if (demoMode) {
            demoMode = false;
            LOG_I("=== DEMO MODE DISABLED (Pico connected) ===");
        }
        machineState.pico_connected = picoConnected;
    }
    
    // Update demo simulation if active
    if (demoMode) {
        updateDemoMode();
    }
    
    // Update UI state periodically
    if (millis() - lastUIUpdate > 100) {  // 10Hz UI update
        lastUIUpdate = millis();
        ui.update(machineState);
    }
    
    // Publish MQTT status periodically (every 1 second)
    static unsigned long lastMQTTPublish = 0;
    if (millis() - lastMQTTPublish > 1000) {
        lastMQTTPublish = millis();
        if (mqttClient.isConnected()) {
            mqttClient.publishStatus(machineState);
        }
    }
    
    // Periodic ping to Pico
    if (millis() - lastPing > 5000) {
        lastPing = millis();
        if (picoUart.isConnected() || picoUart.getPacketsReceived() == 0) {
            picoUart.sendPing();
        }
    }
    
    // Periodic unified status broadcast to WebSocket clients (500ms for responsive UI)
    if (millis() - lastStatusBroadcast > 500) {
        lastStatusBroadcast = millis();
        
        // Broadcast if we have local clients OR cloud connection
        if (webServer.getClientCount() > 0 || cloudConnection.isConnected()) {
            // Update connection status in machineState
            machineState.pico_connected = picoUart.isConnected();
            machineState.wifi_connected = wifiManager.isConnected();
            machineState.mqtt_connected = mqttClient.isConnected();
            machineState.cloud_connected = cloudConnection.isConnected();
            
            // Broadcast unified status (goes to both local and cloud clients)
            webServer.broadcastFullStatus(machineState);
        }
    }
    
    // Periodic power meter status broadcast (5 seconds)
    if (millis() - lastPowerMeterBroadcast > 5000) {
        lastPowerMeterBroadcast = millis();
        
        if (powerMeterManager.getSource() != PowerMeterSource::NONE) {
            // Broadcast to WebSocket clients
            if (webServer.getClientCount() > 0 || cloudConnection.isConnected()) {
                webServer.broadcastPowerMeterStatus();
            }
            
            // Publish to MQTT if connected
            PowerMeterReading reading;
            if (mqttClient.isConnected() && powerMeterManager.getReading(reading)) {
                mqttClient.publishPowerMeter(reading);
            }
        }
    }
}

/**
 * Parse status message from Pico and update machine state
 * 
 * Status payload structure (from protocol.h status_payload_t):
 * Offset  0-1:  brew_temp (int16, °C * 10)
 * Offset  2-3:  steam_temp (int16, °C * 10)
 * Offset  4-5:  group_temp (int16, °C * 10)
 * Offset  6-7:  pressure (uint16, bar * 100)
 * Offset  8-9:  brew_setpoint (int16, °C * 10)
 * Offset 10-11: steam_setpoint (int16, °C * 10)
 * Offset 12:    brew_output (uint8, 0-100%)
 * Offset 13:    steam_output (uint8, 0-100%)
 * Offset 14:    pump_output (uint8, 0-100%)
 * Offset 15:    state (uint8)
 * Offset 16:    flags (uint8)
 * Offset 17:    water_level (uint8, 0-100%)
 * Offset 18-19: power_watts (uint16)
 * Offset 20-23: uptime_ms (uint32)
 * Offset 24-27: shot_start_timestamp_ms (uint32)
 * Offset 28:    heating_strategy (uint8)
 * Offset 29:    cleaning_reminder (uint8, 0 or 1)
 * Offset 30-31: brew_count (uint16)
 */
void parsePicoStatus(const uint8_t* payload, uint8_t length) {
    if (length < 18) return;  // Minimum status size (up to water_level)
    
    // Parse temperatures (int16 scaled by 10 -> float)
    int16_t brew_temp_raw, steam_temp_raw, group_temp_raw;
    memcpy(&brew_temp_raw, &payload[0], sizeof(int16_t));
    memcpy(&steam_temp_raw, &payload[2], sizeof(int16_t));
    memcpy(&group_temp_raw, &payload[4], sizeof(int16_t));
    
    machineState.brew_temp = brew_temp_raw / 10.0f;
    machineState.steam_temp = steam_temp_raw / 10.0f;
    machineState.group_temp = group_temp_raw / 10.0f;
    
    // Parse pressure (uint16 scaled by 100 -> float)
    uint16_t pressure_raw;
    memcpy(&pressure_raw, &payload[6], sizeof(uint16_t));
    machineState.pressure = pressure_raw / 100.0f;
    
    // Parse setpoints (int16 scaled by 10 -> float)
    int16_t brew_sp_raw, steam_sp_raw;
    memcpy(&brew_sp_raw, &payload[8], sizeof(int16_t));
    memcpy(&steam_sp_raw, &payload[10], sizeof(int16_t));
    machineState.brew_setpoint = brew_sp_raw / 10.0f;
    machineState.steam_setpoint = steam_sp_raw / 10.0f;
    
    // Parse state and flags
    machineState.machine_state = payload[15];
    uint8_t flags = payload[16];
    
    machineState.is_brewing = (flags & 0x01) != 0;
    machineState.is_heating = (flags & 0x02) != 0;
    machineState.water_low = (flags & 0x08) != 0;
    machineState.alarm_active = (flags & 0x10) != 0;
    
    // Parse power watts (offset 18-19, if available)
    if (length >= 20) {
        uint16_t power_raw;
        memcpy(&power_raw, &payload[18], sizeof(uint16_t));
        machineState.power_watts = power_raw;
    }
    
    // Parse heating strategy (offset 28, if available)
    if (length >= 30) {
        machineState.heating_strategy = payload[28];
    }
    
    // Parse cleaning status (offsets 29-31, if available)
    if (length >= 32) {
        machineState.cleaning_reminder = payload[29] != 0;
        uint16_t brew_count_raw;
        memcpy(&brew_count_raw, &payload[30], sizeof(uint16_t));
        machineState.brew_count = brew_count_raw;
    }
    
    // Auto-switch screens is now handled by UI::checkAutoScreenSwitch()
}

/**
 * Handle encoder rotation and button events
 */
void handleEncoderEvent(int32_t diff, button_state_t btn) {
    // Log significant events
    if (diff != 0) {
        LOG_D("Encoder: %+d", diff);
        ui.handleEncoder(diff);
    }
    
    if (btn == BTN_PRESSED) {
        LOG_D("Button: short press");
        ui.handleButtonPress();
    } else if (btn == BTN_LONG_PRESSED) {
        LOG_D("Button: long press");
        ui.handleLongPress();
    } else if (btn == BTN_DOUBLE_PRESSED) {
        LOG_D("Button: double press");
        ui.handleDoublePress();
    }
}
