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

// Global instances
WiFiManager wifiManager;
PicoUART picoUart(Serial1);
MQTTClient mqttClient;
PairingManager pairingManager;
WebServer webServer(wifiManager, picoUart, mqttClient, &pairingManager);

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
            case MSG_BOOT:
                LOG_I("Pico booted!");
                webServer.broadcastLog("Pico booted", "info");
                ui.showNotification("Pico Connected", 2000);
                machineState.pico_connected = true;
                break;
                
            case MSG_STATUS:
                // Parse status and update UI
                parsePicoStatus(packet.payload, packet.length);
                machineState.pico_connected = true;
                break;
            
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
            // Set up scale callbacks
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
    
    // Initialize Pairing Manager if cloud is enabled
    auto& cloudSettings = State.settings().cloud;
    if (cloudSettings.enabled && strlen(cloudSettings.serverUrl) > 0) {
        LOG_I("Initializing Pairing Manager...");
        pairingManager.begin(String(cloudSettings.serverUrl));
    }
    
    // Initialize Notification Manager
    LOG_I("Initializing Notification Manager...");
    notificationManager.begin();
    
    // Set up cloud notification callback
    notificationManager.onCloud([&State](const Notification& notif) {
        // Check if cloud integration is enabled
        if (!State.settings().cloud.enabled) {
            return;
        }
        
        String cloudUrl = String(State.settings().cloud.serverUrl);
        String deviceId = String(State.settings().cloud.deviceId);
        
        if (!cloudUrl.isEmpty() && !deviceId.isEmpty()) {
            sendNotificationToCloud(cloudUrl, deviceId, notif);
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
    
    // Update MQTT
    mqttClient.loop();
    
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
    
    // Periodic status broadcast to WebSocket clients
    if (millis() - lastStatusBroadcast > 1000) {
        lastStatusBroadcast = millis();
        
        // Only if we have connected clients
        if (webServer.getClientCount() > 0) {
            // Build status JSON
            String status = "{\"type\":\"esp_status\",";
            status += "\"uptime\":" + String(millis()) + ",";
            status += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
            status += "\"wifi\":\"" + wifiManager.getIP() + "\",";
            status += "\"picoConnected\":" + String(picoUart.isConnected() ? "true" : "false") + ",";
            status += "\"picoPackets\":" + String(picoUart.getPacketsReceived()) + ",";
            status += "\"picoErrors\":" + String(picoUart.getPacketErrors()) + "}";
            
            webServer.broadcastStatus(status);
        }
    }
}

/**
 * Parse status message from Pico and update machine state
 */
void parsePicoStatus(const uint8_t* payload, uint8_t length) {
    if (length < 20) return;  // Minimum status size
    
    // Status payload structure (from protocol):
    // Offset 0: state (uint8)
    // Offset 1: flags (uint8)
    // Offset 2-5: brew_temp (float)
    // Offset 6-9: steam_temp (float)
    // Offset 10-13: pressure (float)
    // Offset 14-17: brew_sp (float)
    // Offset 18-21: steam_sp (float)
    
    machineState.machine_state = payload[0];
    uint8_t flags = payload[1];
    
    machineState.is_brewing = (flags & 0x01) != 0;
    machineState.is_heating = (flags & 0x02) != 0;
    machineState.water_low = (flags & 0x08) != 0;
    machineState.alarm_active = (flags & 0x10) != 0;
    
    memcpy(&machineState.brew_temp, &payload[2], sizeof(float));
    memcpy(&machineState.steam_temp, &payload[6], sizeof(float));
    memcpy(&machineState.pressure, &payload[10], sizeof(float));
    memcpy(&machineState.brew_setpoint, &payload[14], sizeof(float));
    memcpy(&machineState.steam_setpoint, &payload[18], sizeof(float));
    
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
