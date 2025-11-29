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
 * - BLE scale integration (future)
 */

#include <Arduino.h>
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

// Global instances
WiFiManager wifiManager;
PicoUART picoUart(Serial1);
MQTTClient mqttClient;
WebServer webServer(wifiManager, picoUart, mqttClient);

// Machine state from Pico
static ui_state_t machineState = {0};

// Timing
unsigned long lastPing = 0;
unsigned long lastStatusBroadcast = 0;
unsigned long lastUIUpdate = 0;

// Forward declarations
void parsePicoStatus(const uint8_t* payload, uint8_t length);
void handleEncoderEvent(int32_t diff, button_state_t btn);

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
                // Alarm - log prominently
                uint8_t alarmCode = packet.payload[0];
                LOG_W("PICO ALARM: 0x%02X", alarmCode);
                webServer.broadcastLog("Pico ALARM: " + String(alarmCode, HEX), "error");
                machineState.alarm_active = true;
                ui.showError("ALARM", "Machine alarm triggered");
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
        // Get WiFi SSID from status
        WiFiStatus ws = wifiManager.getStatus();
        strncpy(machineState.wifi_ssid, ws.ssid.c_str(), sizeof(machineState.wifi_ssid) - 1);
        machineState.wifi_rssi = WiFi.RSSI();
        ui.showNotification("WiFi Connected", 2000);
        
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
        ui.showNotification("WiFi: BrewOS-Setup", 5000);
    });
    
    wifiManager.begin();
    
    // Start web server
    webServer.begin();
    
    // Set default state values for demo
    machineState.brew_setpoint = 93.0f;
    machineState.steam_setpoint = 145.0f;
    machineState.target_weight = 36.0f;
    
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
    
    // Update Pico connection status
    machineState.pico_connected = picoUart.isConnected();
    machineState.mqtt_connected = mqttClient.isConnected();
    
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
    
    // Switch to brew screen automatically when brewing starts
    if (machineState.is_brewing && ui.getCurrentScreen() != SCREEN_BREW) {
        ui.showScreen(SCREEN_BREW);
    }
    // Return to home when brewing stops
    if (!machineState.is_brewing && ui.getCurrentScreen() == SCREEN_BREW) {
        ui.showScreen(SCREEN_HOME);
        ui.showNotification("Brew Complete!", 3000);
    }
}

/**
 * Handle encoder rotation and button events
 */
void handleEncoderEvent(int32_t diff, button_state_t btn) {
    // Log significant events
    if (diff != 0) {
        LOG_D("Encoder: %+d", diff);
    }
    
    if (btn == BTN_PRESSED) {
        LOG_D("Button: short press");
        ui.handleButtonPress(false);
    } else if (btn == BTN_LONG_PRESSED) {
        LOG_D("Button: long press");
        ui.handleButtonPress(true);
    } else if (btn == BTN_DOUBLE_PRESSED) {
        LOG_D("Button: double press");
        // Double press could be used for special actions
        // e.g., quick tare scale, toggle units, etc.
    }
}
