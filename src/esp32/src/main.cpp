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
#include <LittleFS.h>
#include <nvs_flash.h>  // For NVS initialization
#include <DNSServer.h>
#include <cmath>
#include <esp_heap_caps.h>  // For heap_caps_malloc to avoid PSRAM
#include <new>              // For placement new
#include "config.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "pico_uart.h"

// Display and UI
#include "display/display.h"
#include "display/encoder.h"
#include "display/theme.h"
#include "ui/ui.h"
#include "ui/screen_setup.h"

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

// Global instances - use pointers to defer construction until setup()
// This prevents crashes in constructors before Serial is initialized
WiFiManager* wifiManager = nullptr;
PicoUART* picoUart = nullptr;
MQTTClient* mqttClient = nullptr;
PairingManager* pairingManager = nullptr;
CloudConnection* cloudConnection = nullptr;
WebServer* webServer = nullptr;

// Captive portal DNS server for AP mode
DNSServer dnsServer;
bool dnsServerRunning = false;

// =============================================================================
// LOG LEVEL CONTROL
// =============================================================================
BrewOSLogLevel g_brewos_log_level = BREWOS_LOG_INFO;  // Default to INFO level

void setLogLevel(BrewOSLogLevel level) {
    g_brewos_log_level = level;
    Serial.printf("[Log] Level set to: %s (%d)\n", logLevelToString(level), level);
}

BrewOSLogLevel getLogLevel() {
    return g_brewos_log_level;
}

const char* logLevelToString(BrewOSLogLevel level) {
    switch (level) {
        case BREWOS_LOG_ERROR: return "error";
        case BREWOS_LOG_WARN:  return "warn";
        case BREWOS_LOG_INFO:  return "info";
        case BREWOS_LOG_DEBUG: return "debug";
        default: return "unknown";
    }
}

BrewOSLogLevel stringToLogLevel(const char* str) {
    if (!str) return BREWOS_LOG_INFO;
    if (strcasecmp(str, "error") == 0) return BREWOS_LOG_ERROR;
    if (strcasecmp(str, "warn") == 0) return BREWOS_LOG_WARN;
    if (strcasecmp(str, "warning") == 0) return BREWOS_LOG_WARN;
    if (strcasecmp(str, "info") == 0) return BREWOS_LOG_INFO;
    if (strcasecmp(str, "debug") == 0) return BREWOS_LOG_DEBUG;
    // Try numeric
    int level = atoi(str);
    if (level >= 0 && level <= 3) return (BrewOSLogLevel)level;
    return BREWOS_LOG_INFO;
}

// BLE Scale - disabled by default due to potential WiFi/BLE coexistence issues
// Set to true to enable BLE scale support (may cause instability on some networks)
static bool scaleEnabled = false;

// Machine state from Pico
ui_state_t machineState = {0};  // Made non-static so it can be accessed from other files

// Demo mode - simulates Pico data when Pico not connected
#define DEMO_MODE_TIMEOUT_MS  20000  // Enter demo after 20s without Pico (from server ready)
#define DEMO_MODE_GRACE_PERIOD_MS 5000  // Grace period after server starts before checking
static bool demoMode = false;
static unsigned long demoStartTime = 0;
static unsigned long wifiConnectedTime = 0;
static unsigned long serverReadyTime = 0;  // Track when server is ready to serve requests
static bool mDNSStarted = false;
static bool wifiConnectedLogSent = false;
static bool ntpConfigured = false;

// Timing
unsigned long lastPing = 0;
unsigned long lastStatusBroadcast = 0;
unsigned long lastPowerMeterBroadcast = 0;
unsigned long lastUIUpdate = 0;

// Forward declarations
void parsePicoStatus(const uint8_t* payload, uint8_t length);
void handleEncoderEvent(int32_t diff, button_state_t btn);
void updateDemoMode();
static void onPicoPacket(const PicoPacket& packet);

// =============================================================================
// WiFi Event Callbacks - Static functions to avoid std::function PSRAM issues
// =============================================================================

// Called when WiFi connects to an AP
static void onWiFiConnected() {
    LOG_I("WiFi connected!");
    
    // Stop captive portal DNS server if running
    if (dnsServerRunning) {
        dnsServer.stop();
        dnsServerRunning = false;
        LOG_I("Captive portal DNS server stopped");
    }
    
    // Mark WiFi as connected - web server will delay serving requests
    if (webServer) {
        webServer->setWiFiConnected();
    }
    
    // Update machine state (simple operations only, no String allocations)
    machineState.wifi_connected = true;
    machineState.wifi_ap_mode = false;
    machineState.wifi_rssi = WiFi.RSSI();
    
    // Get WiFi SSID directly from WiFiManager's stored value
    if (wifiManager) {
        const char* ssid = wifiManager->getStoredSSID();
        if (ssid && strlen(ssid) > 0) {
            strncpy(machineState.wifi_ssid, ssid, sizeof(machineState.wifi_ssid) - 1);
            machineState.wifi_ssid[sizeof(machineState.wifi_ssid) - 1] = '\0';
        }
    }
    
    // Get IP directly into buffer (no String allocation)
    IPAddress ip = WiFi.localIP();
    snprintf(machineState.wifi_ip, sizeof(machineState.wifi_ip), "%d.%d.%d.%d", 
             ip[0], ip[1], ip[2], ip[3]);
    
    // Heavy operations (NTP, mDNS, broadcastLog) are done in main loop after delay
}

// Called when WiFi disconnects
static void onWiFiDisconnected() {
    LOG_W("WiFi disconnected");
    machineState.wifi_connected = false;
    // Reset WiFi connected state tracking
    wifiConnectedTime = 0;
    wifiConnectedLogSent = false;
    mDNSStarted = false;
    ntpConfigured = false;
}

// Called when AP mode starts
static void onWiFiAPStarted() {
    LOG_I("AP mode started - connect to: %s", WIFI_AP_SSID);
    LOG_I("Password: %s", WIFI_AP_PASSWORD);
    
    // Get AP IP without String allocation in critical path
    IPAddress apIP = WiFi.softAPIP();
    char apIPStr[16];
    snprintf(apIPStr, sizeof(apIPStr), "%d.%d.%d.%d", apIP[0], apIP[1], apIP[2], apIP[3]);
    LOG_I("Open http://%s to configure", apIPStr);
    
    machineState.wifi_ap_mode = true;
    machineState.wifi_connected = false;
    
    // Stop DNS server first if it was running (clean restart)
    if (dnsServerRunning) {
        dnsServer.stop();
        dnsServerRunning = false;
    }
    
    // Start captive portal DNS server
    dnsServer.start(53, "*", WIFI_AP_IP);
    dnsServerRunning = true;
    LOG_I("Captive portal DNS server started");
    
    // Update setup screen with AP credentials
    screen_setup_set_ap_info(WIFI_AP_SSID, WIFI_AP_PASSWORD, apIPStr);
}

// =============================================================================
// Scale Callbacks - Static functions to avoid std::function PSRAM issues
// =============================================================================

static void onScaleWeight(const scale_state_t& state) {
    machineState.scale_connected = state.connected;
    machineState.brew_weight = state.weight;
    machineState.flow_rate = state.flow_rate;
}

static void onScaleConnection(bool connected) {
    LOG_I("Scale %s", connected ? "connected" : "disconnected");
    machineState.scale_connected = connected;
}

// =============================================================================
// Brew-by-Weight Callbacks - Static functions to avoid std::function PSRAM issues
// =============================================================================

static void onBBWStop() {
    LOG_I("BBW: Sending WEIGHT_STOP signal to Pico");
    if (picoUart && picoUart->isConnected()) {
        picoUart->setWeightStop(true);
        delay(100);
        picoUart->setWeightStop(false);
    } else {
        LOG_W("BBW: Pico not connected, skipping weight stop signal");
    }
}

static void onBBWTare() {
    if (scaleManager && scaleManager->isConnected()) {
        scaleManager->tare();
    }
}

// =============================================================================
// Schedule Callback - Static function to avoid std::function PSRAM issues
// =============================================================================

// =============================================================================
// Cloud Notification Callback - Static function to avoid std::function PSRAM issues
// =============================================================================

// =============================================================================
// Cloud Command Callback - Static function to avoid std::function PSRAM issues
// =============================================================================

static void onCloudCommand(const String& type, JsonDocument& doc) {
    // Commands from cloud users are processed the same as local WebSocket
    if (webServer) {
        webServer->processCommand(doc);
    }
}

static void onCloudNotification(const Notification& notif) {
    // Check if cloud integration is enabled
    auto& cloudSettings = State.settings().cloud;
    if (!cloudSettings.enabled) {
        return;
    }
    
    // Get device key from pairing manager (no String capture needed)
    String deviceKey = pairingManager ? pairingManager->getDeviceKey() : "";
    String cloudUrl = String(cloudSettings.serverUrl);
    String deviceId = String(cloudSettings.deviceId);
    
    if (!cloudUrl.isEmpty() && !deviceId.isEmpty()) {
        sendNotificationToCloud(cloudUrl, deviceId, deviceKey, notif);
    }
}

static void onScheduleTriggered(const BrewOS::ScheduleEntry& schedule) {
    LOG_I("Schedule triggered: %s", schedule.name);
    
    // Only execute if Pico is connected
    if (!picoUart || !picoUart->isConnected()) {
        LOG_W("Schedule: Pico not connected, skipping action");
        return;
    }
    
    if (schedule.action == BrewOS::ACTION_TURN_ON) {
        // Turn on machine with specified heating strategy
        uint8_t modeCmd = 0x01;  // MODE_BREW
        picoUart->sendCommand(MSG_CMD_MODE, &modeCmd, 1);
        
        // Set heating strategy if not default
        if (schedule.strategy != BrewOS::STRATEGY_SEQUENTIAL) {
            uint8_t strategyPayload[2] = {0x01, schedule.strategy};
            picoUart->sendCommand(MSG_CMD_CONFIG, strategyPayload, 2);
        }
    } else {
        // Turn off machine
        uint8_t modeCmd = 0x00;  // MODE_IDLE
        picoUart->sendCommand(MSG_CMD_MODE, &modeCmd, 1);
    }
}

// =============================================================================
// Pico Packet Handler - Static function to avoid std::function PSRAM issues
// =============================================================================

static void onPicoPacket(const PicoPacket& packet) {
    LOG_D("Pico packet: type=0x%02X, len=%d, seq=%d", 
          packet.type, packet.length, packet.seq);
    
    // NOTE: Raw Pico messages are NOT forwarded to WebSocket clients
    // The UI should use processed "status" messages instead, not low-level ESP32-Pico protocol
    // These messages are for ESP32-Pico communication only, not for the web UI
    
    // Handle specific message types
    switch (packet.type) {
        case MSG_BOOT: {
            LOG_I("Pico booted!");
            if (webServer) webServer->broadcastLog("Pico booted");
            machineState.pico_connected = true;
            
            // Parse boot payload (boot_payload_t structure)
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
                
                // Check for version mismatch
                char picoVerStr[16];
                snprintf(picoVerStr, sizeof(picoVerStr), "%d.%d.%d", ver_major, ver_minor, ver_patch);
                if (strcmp(picoVerStr, ESP32_VERSION) != 0) {
                    LOG_W("Internal version mismatch: %s vs %s", ESP32_VERSION, picoVerStr);
                    if (webServer) webServer->broadcastLogLevel("warn", "Firmware update recommended");
                }
            }
            
            // Send environmental config to Pico immediately after boot
            // This is required for Pico to exit STATE_FAULT (0x05)
            // Pico will remain in fault state until environmental config is received
            uint16_t voltage = State.settings().power.mainsVoltage;
            uint8_t maxCurrent = (uint8_t)State.settings().power.maxCurrent;
            
            // Only send if settings are valid (non-zero)
            if (voltage > 0 && maxCurrent > 0) {
                uint8_t envPayload[4];
                envPayload[0] = CONFIG_ENVIRONMENTAL;  // Config type
                envPayload[1] = (voltage >> 8) & 0xFF;
                envPayload[2] = voltage & 0xFF;
                envPayload[3] = maxCurrent;
                
                if (picoUart->sendCommand(MSG_CMD_CONFIG, envPayload, 4)) {
                    LOG_I("Sent environmental config to Pico: %dV, %dA", voltage, maxCurrent);
                } else {
                    LOG_W("Failed to send environmental config to Pico");
                }
            } else {
                LOG_W("Environmental config not set - Pico will remain in STATE_FAULT until power settings are configured");
                if (webServer) {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "Power settings not configured - please set voltage and max current in settings");
                    webServer->broadcastLog(msg, "warn");
                }
            }
            
            break;
        }
            
        case MSG_STATUS:
            parsePicoStatus(packet.payload, packet.length);
            machineState.pico_connected = true;
            break;
        
        case MSG_POWER_METER: {
            if (packet.length >= sizeof(PowerMeterReading) && powerMeterManager) {
                PowerMeterReading reading;
                memcpy(&reading, packet.payload, sizeof(PowerMeterReading));
                powerMeterManager->onPicoPowerData(reading);
            }
            break;
        }
        
        case MSG_ALARM: {
            uint8_t alarmCode = packet.payload[0];
            
            if (alarmCode == ALARM_NONE) {
                // ALARM_NONE (0x00) means no alarm - clear the alarm state
                // Only log if we're actually transitioning from a REAL alarm (non-zero) to cleared
                if (machineState.alarm_active && machineState.alarm_code != ALARM_NONE) {
                    LOG_I("Pico alarm cleared (was: 0x%02X)", machineState.alarm_code);
                    // Defensive: Check webServer pointer and use try-catch equivalent (null check)
                    if (webServer) {
                        // Use a safer approach - format the message first to avoid variadic issues
                        char alarmMsg[64];
                        snprintf(alarmMsg, sizeof(alarmMsg), "Pico alarm cleared");
                        webServer->broadcastLog(alarmMsg, "info");
                    }
                }
                // Always update state, but only log when transitioning from real alarm
                machineState.alarm_active = false;
                machineState.alarm_code = ALARM_NONE;
            } else {
                // Actual alarm - log and set alarm state
                // Only log if this is a new alarm or different from current
                if (!machineState.alarm_active || machineState.alarm_code != alarmCode) {
                    LOG_W("PICO ALARM: 0x%02X", alarmCode);
                    // Defensive: Format message first to avoid variadic argument issues
                    if (webServer) {
                        char alarmMsg[64];
                        snprintf(alarmMsg, sizeof(alarmMsg), "Pico ALARM: 0x%02X", alarmCode);
                        webServer->broadcastLog(alarmMsg, "error");
                    }
                }
                machineState.alarm_active = true;
                machineState.alarm_code = alarmCode;
            }
            break;
        }
        
        case MSG_CONFIG:
            LOG_I("Received config from Pico");
            if (webServer) webServer->broadcastLog("Config received from Pico");
            break;
            
        case MSG_ENV_CONFIG: {
            if (packet.length >= 18) {
                uint16_t voltage = packet.payload[0] | (packet.payload[1] << 8);
                float max_current = 0;
                memcpy(&max_current, &packet.payload[2], sizeof(float));
                LOG_I("Env config: %dV, %.1fA max", voltage, max_current);
                if (webServer) webServer->broadcastLogLevel("info", "Env config: %dV, %.1fA max", voltage, max_current);
            }
            break;
        }
            
        case MSG_DEBUG_RESP:
            LOG_D("Debug response from Pico");
            break;
            
        case MSG_DIAGNOSTICS: {
            if (packet.length == 8) {
                // Diagnostic header
                LOG_I("Diag header: tests=%d, pass=%d, fail=%d, warn=%d, skip=%d, complete=%d",
                      packet.payload[0], packet.payload[1], packet.payload[2],
                      packet.payload[3], packet.payload[4], packet.payload[5]);
                
                #pragma GCC diagnostic push
                #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
                StaticJsonDocument<256> doc;
                #pragma GCC diagnostic pop
                doc["type"] = "diagnostics_header";
                doc["testCount"] = packet.payload[0];
                doc["passCount"] = packet.payload[1];
                doc["failCount"] = packet.payload[2];
                doc["warnCount"] = packet.payload[3];
                doc["skipCount"] = packet.payload[4];
                doc["isComplete"] = packet.payload[5] != 0;
                doc["durationMs"] = packet.payload[6] | (packet.payload[7] << 8);
                
                size_t jsonSize = measureJson(doc) + 1;
                char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
                if (!jsonBuffer) jsonBuffer = (char*)malloc(jsonSize);
                if (jsonBuffer && webServer) {
                    serializeJson(doc, jsonBuffer, jsonSize);
                    webServer->broadcastRaw(jsonBuffer);
                    free(jsonBuffer);
                }
                
                if (packet.payload[5] && webServer) {
                    webServer->broadcastLog("Diagnostics complete");
                }
            } else if (packet.length >= 32) {
                // Diagnostic result
                LOG_I("Diag result: test=%d, status=%d", packet.payload[0], packet.payload[1]);
                
                #pragma GCC diagnostic push
                #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
                StaticJsonDocument<384> doc;
                #pragma GCC diagnostic pop
                doc["type"] = "diagnostics_result";
                doc["testId"] = packet.payload[0];
                doc["status"] = packet.payload[1];
                doc["rawValue"] = (int16_t)(packet.payload[2] | (packet.payload[3] << 8));
                doc["expectedMin"] = (int16_t)(packet.payload[4] | (packet.payload[5] << 8));
                doc["expectedMax"] = (int16_t)(packet.payload[6] | (packet.payload[7] << 8));
                
                char msg[25];
                memcpy(msg, &packet.payload[8], 24);
                msg[24] = '\0';
                doc["message"] = msg;
                
                size_t jsonSize = measureJson(doc) + 1;
                char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
                if (!jsonBuffer) jsonBuffer = (char*)malloc(jsonSize);
                if (jsonBuffer && webServer) {
                    serializeJson(doc, jsonBuffer, jsonSize);
                    webServer->broadcastRaw(jsonBuffer);
                    free(jsonBuffer);
                }
            }
            break;
        }
            
        default:
            LOG_D("Unknown packet type: 0x%02X", packet.type);
    }
}

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
    
    // Simulate periodic brewing - DISABLED (too noisy, not needed)
    // Users can test brewing manually via the web UI if needed
    // static unsigned long brewStart = 0;
    // if (demoCycle > 0.5f && demoCycle < 0.7f) {
    //     if (!machineState.is_brewing) {
    //         brewStart = millis();
    //         machineState.is_brewing = true;
    //         machineState.machine_state = STATE_BREWING;
    //         LOG_I("[DEMO] Simulated brew started");
    //     }
    //     machineState.brew_time_ms = millis() - brewStart;
    //     machineState.brew_weight = machineState.brew_time_ms / 1000.0f * 1.5f;  // ~1.5g/s
    //     machineState.flow_rate = 1.5f + sin(millis() / 500.0f) * 0.3f;
    // } else {
    //     if (machineState.is_brewing) {
    //         machineState.is_brewing = false;
    //         LOG_I("[DEMO] Simulated brew ended: %.1fs, %.1fg", 
    //               machineState.brew_time_ms / 1000.0f, machineState.brew_weight);
    //     }
    // }
    
    // Keep machine in ready state (not brewing) in demo mode
    machineState.is_brewing = false;
    machineState.brew_time_ms = 0;
    machineState.brew_weight = 0.0f;
    machineState.flow_rate = 0.0f;
    
    // Always connected in demo mode
    machineState.pico_connected = true;  // Pretend Pico is connected
}

void setup() {
    // CRITICAL: Initialize serial FIRST
    // With ARDUINO_USB_CDC_ON_BOOT=1, Serial goes to USB CDC (appears as /dev/cu.usbmodem* or /dev/cu.usbserial*)
    // Serial0 is hardware UART (GPIO43/44 by default, or can be configured to GPIO36/37)
    // Serial1 is used for Pico communication (GPIO43/44)
    Serial.begin(115200);
    delay(500);  // Give USB CDC time to enumerate
    
    // Note: Watchdog is kept enabled - it helps catch hangs and crashes
    // Attempting to disable it causes errors on ESP32-S3
    
    // Print immediately - minimal code
    Serial.println();
    Serial.println("SETUP START");
    Serial.flush();
    delay(100);
    
    Serial.print("Heap: ");
    Serial.println(ESP.getFreeHeap());
    Serial.print("PSRAM size: ");
    Serial.println(ESP.getPsramSize());
    Serial.flush();
    
    // Verify PSRAM is disabled for heap allocations
    // ESP32-S3 PSRAM address range
    constexpr uintptr_t ESP32S3_PSRAM_START = 0x3C000000;
    constexpr uintptr_t ESP32S3_PSRAM_END   = 0x3E000000;
    
    void* testAlloc = malloc(64);
    uintptr_t addr = (uintptr_t)testAlloc;
    bool inPSRAM = (addr >= ESP32S3_PSRAM_START && addr < ESP32S3_PSRAM_END);
    Serial.printf("Malloc test: 0x%08X (%s)\n", addr, inPSRAM ? "PSRAM - ERROR!" : "Internal RAM - OK");
    if (inPSRAM) {
        Serial.println("ERROR: PSRAM should be disabled! Check platformio.ini");
    }
    free(testAlloc);
    Serial.flush();
    
    // Initialize NVS (Non-Volatile Storage) FIRST
    // This ensures Preferences library works correctly after fresh flash
    Serial.println("[1/8] Initializing NVS...");
    Serial.flush();
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        Serial.print("NVS needs erase (err=");
        Serial.print(nvs_err);
        Serial.println(") - erasing...");
        Serial.flush();
        nvs_flash_erase();
        nvs_err = nvs_flash_init();
    }
    if (nvs_err != ESP_OK) {
        Serial.print("NVS init FAILED: ");
        Serial.println(nvs_err);
        Serial.flush();
        // Continue anyway - Preferences will handle missing NVS gracefully
    } else {
        Serial.println("NVS initialized OK");
        Serial.flush();
    }
    
    // Initialize LittleFS (needed by State, WebServer, etc.)
    Serial.println("[2/8] Initializing LittleFS...");
    Serial.flush();
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed - formatting...");
        Serial.flush();
        LittleFS.format();
        if (!LittleFS.begin(true)) {
            Serial.println("ERROR: LittleFS format failed!");
            Serial.flush();
            // Continue anyway - web server will handle missing files gracefully
        } else {
            Serial.println("LittleFS formatted and mounted OK");
            Serial.flush();
        }
    } else {
        Serial.println("LittleFS mounted OK");
        Serial.flush();
    }
    
    // Turn on backlight so user knows device is running
    Serial.println("[3/8] Turning on backlight...");
    Serial.flush();
    pinMode(7, OUTPUT);
    digitalWrite(7, HIGH);
    Serial.println("Backlight ON");
    Serial.flush();
    
    // Construct global objects NOW (after Serial is initialized)
    // CRITICAL: Allocate in internal RAM (not PSRAM) to avoid InstructionFetchError
    // when callbacks are called. PSRAM pointers cause CPU crashes.
    Serial.println("[3.5/8] Creating global objects in internal RAM...");
    Serial.flush();
    
    // All objects use regular new - PSRAM is disabled so malloc uses internal RAM
    // Placement new with heap_caps_malloc was causing memory corruption
    wifiManager = new WiFiManager();
    Serial.println("WiFiManager created");
    Serial.flush();
    
    picoUart = new PicoUART(Serial1);
    Serial.println("PicoUART created");
    Serial.flush();
    
    mqttClient = new MQTTClient();
    Serial.println("MQTTClient created");
    Serial.flush();
    
    pairingManager = new PairingManager();
    Serial.println("PairingManager created");
    Serial.flush();
    
    cloudConnection = new CloudConnection();
    Serial.println("CloudConnection created");
    Serial.flush();
    
    webServer = new WebServer(*wifiManager, *picoUart, *mqttClient, pairingManager);
    Serial.println("WebServer created");
    Serial.flush();
    
    scaleManager = new ScaleManager();
    Serial.println("ScaleManager created");
    Serial.flush();
    
    brewByWeight = new BrewByWeight();
    Serial.println("BrewByWeight created");
    Serial.flush();
    
    powerMeterManager = new PowerMeterManager();
    Serial.println("PowerMeterManager created");
    Serial.flush();
    
    notificationManager = new NotificationManager();
    Serial.println("NotificationManager created");
    Serial.flush();
    
    Serial.println("All global objects created OK");
    Serial.flush();
    
    /*
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
    */
    
    // UI callbacks - DISABLED (no display)
    /*
    // Set up UI callbacks
    ui.onTurnOn([]() {
        LOG_I("UI: Turn on requested");
        uint8_t cmd = 0x01;
        picoUart->sendCommand(MSG_CMD_MODE, &cmd, 1);
    });
    
    ui.onTurnOff([]() {
        LOG_I("UI: Turn off requested");
        uint8_t cmd = 0x00;
        picoUart->sendCommand(MSG_CMD_MODE, &cmd, 1);
    });
    
    ui.onSetTemp([](bool is_steam, float temp) {
        LOG_I("UI: Set %s temp to %.1f°C", is_steam ? "steam" : "brew", temp);
        uint8_t payload[5];
        payload[0] = is_steam ? 0x02 : 0x01;
        memcpy(&payload[1], &temp, sizeof(float));
        picoUart->sendCommand(MSG_CMD_SET_TEMP, payload, 5);
    });
    
    ui.onTareScale([]() {
        LOG_I("UI: Tare scale requested");
        scaleManager->tare();
    });
    
    ui.onSetTargetWeight([](float weight) {
        LOG_I("UI: Set target weight to %.1fg", weight);
        brewByWeight->setTargetWeight(weight);
        machineState.target_weight = weight;
    });
    
    ui.onWifiSetup([]() {
        LOG_I("UI: WiFi setup requested");
        wifiManager->setStaticIP(false);
        wifiManager->startAP();
    });
    */
    
    // Initialize Pico UART
    Serial.println("[4/8] Initializing Pico UART...");
    Serial.flush();
    picoUart->begin();
    Serial.println("Pico UART initialized OK");
    Serial.flush();
    
    // Reset Pico to ensure fresh boot
    Serial.println("[4.4/8] Resetting Pico...");
    Serial.flush();
    picoUart->resetPico();
    delay(1000);  // Give Pico time to reset and start booting (Core 1 needs time to init)
    
    // Wait for Pico to connect (sends boot message)
    // Pico Core 1 needs time to initialize and send boot message
    // Increased to 10 seconds to allow for simultaneous power-on initialization
    Serial.println("[4.5/8] Waiting for Pico connection (10 seconds)...");
    Serial.flush();
    unsigned long picoWaitStart = millis();
    bool picoConnected = false;
    uint32_t initialPackets = picoUart->getPacketsReceived();
    bool sawAnyData = false;
    
    while (millis() - picoWaitStart < 10000) {
        picoUart->loop();  // Process any incoming packets
        
        // Check if we're receiving any raw data at all
        if (picoUart->bytesAvailable() > 0) {
            sawAnyData = true;
        }
        
        // Check if we received any packets (even if not "connected" yet)
        if (picoUart->getPacketsReceived() > initialPackets) {
            Serial.printf("Received %d packet(s) from Pico\n", 
                         picoUart->getPacketsReceived() - initialPackets);
            Serial.flush();
        }
        
        if (picoUart->isConnected()) {
            picoConnected = true;
            Serial.println("Pico connected!");
            Serial.flush();
            break;
        }
        delay(10);
    }
    
    if (!picoConnected) {
        Serial.printf("Pico not connected after %d ms\n", millis() - picoWaitStart);
        Serial.printf("Packets received: %d, Errors: %d\n", 
                     picoUart->getPacketsReceived(), 
                     picoUart->getPacketErrors());
        if (sawAnyData) {
            Serial.println("WARNING: Received raw data but no valid packets - check baud rate/protocol");
        } else {
            Serial.println("WARNING: No data received - check wiring (TX/RX pins)");
            Serial.println("  ESP32 TX (GPIO43) -> Pico RX (GPIO1)");
            Serial.println("  ESP32 RX (GPIO44) <- Pico TX (GPIO0)");
        }
        
        // Try sending a ping to see if Pico responds
        Serial.println("Attempting to ping Pico...");
        Serial.flush();
        if (picoUart->sendPing()) {
            Serial.println("Ping sent, waiting 500ms for response...");
            Serial.flush();
            delay(500);
            picoUart->loop();
            if (picoUart->isConnected()) {
                picoConnected = true;
                Serial.println("Pico responded to ping - connected!");
                Serial.flush();
            } else {
                Serial.println("Ping sent but no response received");
                Serial.flush();
            }
        } else {
            Serial.println("Failed to send ping");
            Serial.flush();
        }
        
        if (!picoConnected) {
            Serial.println("Continuing without Pico (demo mode)");
            Serial.flush();
        }
    }
    
    // Set up packet handler using static function to avoid PSRAM issues
    Serial.println("[4.6/8] Setting up Pico packet handler...");
    Serial.flush();
    picoUart->onPacket(onPicoPacket);
    
    // Initialize WiFi callbacks using static function pointers
    // This avoids std::function which allocates in PSRAM and causes InstructionFetchError
    wifiManager->onConnected(onWiFiConnected);
    wifiManager->onDisconnected(onWiFiDisconnected);
    wifiManager->onAPStarted(onWiFiAPStarted);
    
    Serial.println("[5/8] Initializing WiFi Manager...");
    Serial.flush();
    wifiManager->begin();
    Serial.println("WiFi Manager initialized OK");
    Serial.flush();
    
    // Start web server
    Serial.println("[6/8] Starting web server...");
    Serial.flush();
    webServer->begin();
    Serial.println("Web server started OK");
    Serial.flush();
    
    // Record server ready time for demo mode timeout calculation
    // This ensures we give Pico time to initialize even if both devices boot simultaneously
    serverReadyTime = millis();
    
    // Initialize MQTT
    Serial.println("[7/8] Initializing MQTT...");
    Serial.flush();
    mqttClient->begin();
    Serial.println("MQTT initialized OK");
    Serial.flush();
    
    // Initialize Power Meter Manager
    Serial.println("[7.5/8] Initializing Power Meter...");
    Serial.flush();
    powerMeterManager->begin();
    Serial.println("Power Meter initialized OK");
    Serial.flush();
    
    // Set up MQTT command handler
    mqttClient->onCommand([](const char* cmd, const JsonDocument& doc) {
        String cmdStr = cmd;
        
        if (cmdStr == "set_temp") {
            String boiler = doc["boiler"] | "brew";
            float temp = doc["temp"] | 0.0f;
            
            uint8_t payload[5];
            payload[0] = (boiler == "steam") ? 0x02 : 0x01;
            memcpy(&payload[1], &temp, sizeof(float));
            picoUart->sendCommand(MSG_CMD_SET_TEMP, payload, 5);
        }
        else if (cmdStr == "set_mode") {
            String mode = doc["mode"] | "";
            uint8_t modeCmd = 0;
            
            if (mode == "on" || mode == "ready") {
                modeCmd = 0x01;
            } else if (mode == "off" || mode == "standby") {
                modeCmd = 0x00;
            }
            picoUart->sendCommand(MSG_CMD_MODE, &modeCmd, 1);
        }
        else if (cmdStr == "tare") {
            scaleManager->tare();
        }
        else if (cmdStr == "set_target_weight") {
            float weight = doc["weight"] | 0.0f;
            if (weight > 0) {
                brewByWeight->setTargetWeight(weight);
                machineState.target_weight = weight;
            }
        }
        else if (cmdStr == "set_eco") {
            if (!picoUart->isConnected()) {
                LOG_W("MQTT command set_eco: Pico not connected");
                return;
            }
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
            
            picoUart->sendCommand(MSG_CMD_SET_ECO, payload, 5);
        }
        else if (cmdStr == "enter_eco") {
            if (!picoUart->isConnected()) {
                LOG_W("MQTT command enter_eco: Pico not connected");
                return;
            }
            uint8_t payload[1] = {1};  // 1 = enter eco
            picoUart->sendCommand(MSG_CMD_SET_ECO, payload, 1);
        }
        else if (cmdStr == "exit_eco") {
            if (!picoUart->isConnected()) {
                LOG_W("MQTT command exit_eco: Pico not connected");
                return;
            }
            uint8_t payload[1] = {0};  // 0 = exit eco
            picoUart->sendCommand(MSG_CMD_SET_ECO, payload, 1);
        }
    });
    
    // Initialize BLE Scale Manager
    if (scaleEnabled) {
        LOG_I("Initializing BLE Scale Manager...");
        if (scaleManager->begin()) {
            // Set up scale callbacks using static functions to avoid PSRAM issues
            scaleManager->onWeight(onScaleWeight);
            scaleManager->onConnection(onScaleConnection);
            LOG_I("Scale Manager ready");
        } else {
            LOG_E("Scale Manager initialization failed!");
        }
    }
    
    // Initialize Brew-by-Weight controller
    LOG_I("Initializing Brew-by-Weight...");
    brewByWeight->begin();
    
    // Connect brew-by-weight callbacks using static functions to avoid PSRAM issues
    brewByWeight->onStop(onBBWStop);
    brewByWeight->onTare(onBBWTare);
    
    // Set default state values from BBW settings
    LOG_I("Setting default machine state values...");
    Serial.flush();
    machineState.brew_setpoint = 93.0f;
    machineState.steam_setpoint = 145.0f;
    machineState.target_weight = brewByWeight->getTargetWeight();
    machineState.dose_weight = brewByWeight->getDoseWeight();
    machineState.brew_max_temp = 105.0f;
    machineState.steam_max_temp = 160.0f;
    machineState.dose_weight = 18.0f;
    LOG_I("Default values set");
    Serial.flush();
    
    // Initialize State Manager (schedules, settings persistence)
    Serial.println("[8/8] Initializing State Manager...");
    Serial.print("Free heap before State: ");
    Serial.println(ESP.getFreeHeap());
    Serial.flush();
    State.begin();
    Serial.print("Free heap after State: ");
    Serial.println(ESP.getFreeHeap());
    Serial.println("State Manager initialized OK");
    Serial.flush();
    
    // Initialize Pairing Manager and Cloud Connection if cloud is enabled
    auto& cloudSettings = State.settings().cloud;
    if (cloudSettings.enabled && strlen(cloudSettings.serverUrl) > 0) {
        LOG_I("Initializing Pairing Manager...");
        pairingManager->begin(String(cloudSettings.serverUrl));
        
        // Get device ID and key from pairing manager (it manages these securely)
        String deviceId = pairingManager->getDeviceId();
        String deviceKey = pairingManager->getDeviceKey();
        
        // Sync device ID to cloud settings if different (for display purposes)
        if (String(cloudSettings.deviceId) != deviceId) {
            strncpy(cloudSettings.deviceId, deviceId.c_str(), sizeof(cloudSettings.deviceId) - 1);
            State.saveCloudSettings();
        }
        
        // Register device key with cloud server BEFORE trying to connect
        // This ensures the cloud knows our device key for WebSocket authentication
        LOG_I("Registering device key with cloud...");
        if (pairingManager->registerTokenWithCloud()) {
            LOG_I("Device key registered with cloud");
        } else {
            LOG_W("Failed to register device key with cloud - will retry on connect");
        }
        
        // Initialize Cloud Connection for real-time state relay
        // Uses pairing manager's device key for secure authentication
        LOG_I("Initializing Cloud Connection...");
        cloudConnection->begin(
            String(cloudSettings.serverUrl),
            deviceId,
            deviceKey
        );
        
        // Set up command handler using static function to avoid PSRAM issues
        cloudConnection->onCommand(onCloudCommand);
        
        // Connect cloud to WebServer for state broadcasting
        webServer->setCloudConnection(cloudConnection);
    }
    
    // Initialize Notification Manager
    Serial.println("[8.5/8] Initializing Notification Manager...");
    Serial.flush();
    notificationManager->begin();
    Serial.println("Notification Manager initialized OK");
    Serial.flush();
    
    // Set up cloud notification callback using static function to avoid PSRAM issues
    notificationManager->onCloud(onCloudNotification);
    
    // Set up schedule callback using static function to avoid PSRAM issues
    State.onScheduleTriggered(onScheduleTriggered);
    
    Serial.println("========================================");
    Serial.println("SETUP COMPLETE!");
    Serial.print("Free heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");
    Serial.println("Entering main loop...");
    Serial.println("========================================");
    Serial.flush();
    Serial.flush();
}

// =============================================================================
// MAIN LOOP - Robust state management with error handling
// =============================================================================

void loop() {
    // Feed watchdog at start of every loop iteration
    // This prevents watchdog reset if any single component takes too long
    yield();
    
    
    // =========================================================================
    // PHASE 1: Critical object validation
    // Skip iteration if core objects failed to initialize
    // =========================================================================
    if (!wifiManager || !picoUart || !webServer) {
        static unsigned long lastWarning = 0;
        if (millis() - lastWarning > 5000) {
            Serial.println("[FATAL] Core objects not initialized - check heap allocation");
            lastWarning = millis();
        }
        delay(100);
        return;
    }
    
    // =========================================================================
    // PHASE 2: Core system updates (always run)
    // These are essential for basic operation
    // =========================================================================
    
    // WiFi management - handles connection state machine
    wifiManager->loop();
    yield();  // Feed watchdog
    
    // Captive portal DNS (only in AP mode)
    if (dnsServerRunning) {
        dnsServer.processNextRequest();
    }
    
    // Pico UART communication
    picoUart->loop();
    yield();  // Feed watchdog
    
    // Web server request handling
    webServer->loop();
    yield();  // Feed watchdog after web server (handles HTTP requests)
    
    // =========================================================================
    // PHASE 3: Optional service updates
    // =========================================================================
    
    // Cloud connection (handles WebSocket to cloud server)
    if (cloudConnection) {
        cloudConnection->loop();
    }
    yield();  // Feed watchdog
    
    // MQTT client (for Home Assistant integration)
    if (mqttClient) {
        mqttClient->loop();
    }
    
    // Power meter (Shelly/Tasmota integration)
    if (powerMeterManager) {
        powerMeterManager->loop();
    }
    yield();  // Feed watchdog
    
    // BLE Scale - disabled by default (scaleEnabled = false at top of file)
    // Known issue: BLE scanning may conflict with WiFi causing watchdog resets on some networks
    if (scaleEnabled && scaleManager) {
        scaleManager->loop();
        yield();
    }
    
    // =========================================================================
    // PHASE 4: State synchronization
    // Update machine state from various sources
    // =========================================================================
    
    // Connection states (defensive - default to false if object missing)
    bool picoConnected = picoUart->isConnected();
    machineState.mqtt_connected = mqttClient ? mqttClient->isConnected() : false;
    machineState.scale_connected = (scaleEnabled && scaleManager) ? scaleManager->isConnected() : false;
    machineState.cloud_connected = cloudConnection ? cloudConnection->isConnected() : false;
    
    // =========================================================================
    // PHASE 5: Demo mode management
    // Provides simulated data when Pico is not connected
    // =========================================================================
    
    // Only check for demo mode after server is ready and grace period has passed
    // This allows both ESP32 and Pico to initialize when powering on simultaneously
    bool canEnterDemoMode = (serverReadyTime > 0) && 
                           (millis() - serverReadyTime > DEMO_MODE_GRACE_PERIOD_MS);
    
    if (!picoConnected && picoUart->getPacketsReceived() == 0) {
        // No Pico detected - enter demo mode after timeout (from server ready time)
        if (!demoMode && canEnterDemoMode && 
            (millis() - serverReadyTime) > DEMO_MODE_TIMEOUT_MS) {
            demoMode = true;
            demoStartTime = millis();
            LOG_I("=== DEMO MODE ENABLED (no Pico detected after %lu ms) ===", 
                  millis() - serverReadyTime);
        }
    } else {
        // Pico connected - exit demo mode
        if (demoMode) {
            demoMode = false;
            LOG_I("=== DEMO MODE DISABLED (Pico connected) ===");
        }
        machineState.pico_connected = picoConnected;
    }
    
    // Update demo simulation (sets is_brewing = false, prevents false triggers)
    if (demoMode) {
        updateDemoMode();
    }
    
    // =========================================================================
    // PHASE 6: Brew-by-Weight (only when actively brewing with scale)
    // This is DISABLED when:
    // - Scale not enabled or not connected
    // - Not brewing
    // - In demo mode (is_brewing is always false)
    // =========================================================================
    
    // Only process BBW if we're actually brewing with a connected scale
    // This prevents any callbacks from firing when not needed
    bool shouldUpdateBBW = brewByWeight && 
                           scaleEnabled && 
                           scaleManager && 
                           scaleManager->isConnected() &&
                           machineState.is_brewing;
    
    // Update BBW with current brewing state and scale weight
    if (shouldUpdateBBW) {
        brewByWeight->update(
            machineState.is_brewing,
            scaleManager->getState().weight,
            true
        );
    }
    
    // State Manager - handles schedules, idle timeout, etc.
    State.loop();
    
    // Reset idle timer on user activity (brewing, encoder, etc.)
    // Encoder disabled - just check brewing
    if (machineState.is_brewing) {
        State.resetIdleTimer();
    }
    /*
    static bool lastPressed = false;
    bool currentPressed = encoder.isPressed();
    if (machineState.is_brewing || (currentPressed && !lastPressed) || encoder.getPosition() != 0) {
        State.resetIdleTimer();
        encoder.resetPosition();
    }
    lastPressed = currentPressed;
    */
    
    // Sync BBW state to machine state (with null check)
    if (brewByWeight && brewByWeight->isActive()) {
        machineState.brew_weight = brewByWeight->getState().current_weight;
        machineState.target_weight = brewByWeight->getTargetWeight();
        machineState.dose_weight = brewByWeight->getDoseWeight();
    }
    
    // Publish MQTT status periodically (1 second interval)
    static unsigned long lastMQTTPublish = 0;
    if (millis() - lastMQTTPublish > 1000) {
        lastMQTTPublish = millis();
        if (mqttClient && mqttClient->isConnected()) {
            mqttClient->publishStatus(machineState);
        }
    }
    
    // Periodic ping to Pico for connection monitoring
    if (millis() - lastPing > 5000) {
        lastPing = millis();
        if (picoUart->isConnected() || picoUart->getPacketsReceived() == 0) {
            picoUart->sendPing();
        }
    }
    
    // Periodic unified status broadcast to WebSocket clients (500ms for responsive UI)
    if (millis() - lastStatusBroadcast > 500) {
        lastStatusBroadcast = millis();
        
        // Broadcast if we have local clients OR cloud connection
        if (webServer->getClientCount() > 0 || cloudConnection->isConnected()) {
            // Update connection status in machineState
            machineState.pico_connected = picoUart->isConnected();
            machineState.wifi_connected = wifiManager->isConnected();
            machineState.mqtt_connected = mqttClient->isConnected();
            machineState.cloud_connected = cloudConnection->isConnected();
            
            // Broadcast unified status (goes to both local and cloud clients)
            webServer->broadcastFullStatus(machineState);
        }
    }
    
    // Periodic power meter status broadcast (5 seconds)
    if (millis() - lastPowerMeterBroadcast > 5000) {
        lastPowerMeterBroadcast = millis();
        
        if (powerMeterManager->getSource() != PowerMeterSource::NONE) {
            // Broadcast to WebSocket clients
            if (webServer->getClientCount() > 0 || cloudConnection->isConnected()) {
                webServer->broadcastPowerMeterStatus();
            }
            
            // Publish to MQTT if connected
            PowerMeterReading reading;
            if (mqttClient->isConnected() && powerMeterManager->getReading(reading)) {
                mqttClient->publishPowerMeter(reading);
            }
        }
    }
    
    // Handle WiFi connected tasks
    if (machineState.wifi_connected && wifiConnectedTime == 0) {
        wifiConnectedTime = millis();
    }
    if (wifiConnectedTime > 0 && millis() - wifiConnectedTime > 2000 && !ntpConfigured) {
        auto& timeSettings = State.settings().time;
        wifiManager->configureNTP(
            timeSettings.ntpServer,
            timeSettings.utcOffsetMinutes,
            timeSettings.dstEnabled,
            timeSettings.dstOffsetMinutes
        );
        ntpConfigured = true;
    }
    if (wifiConnectedTime > 0 && millis() - wifiConnectedTime > 3000 && !wifiConnectedLogSent) {
        // Send log message after WiFi is stable (3 seconds)
        webServer->broadcastLog("WiFi connected");
        wifiConnectedLogSent = true;
    }
    if (wifiConnectedTime > 0 && millis() - wifiConnectedTime > 3000 && !mDNSStarted) {
        // Start mDNS after WiFi is stable (3 seconds)
        if (MDNS.begin("brewos")) {
            LOG_I("mDNS started: http://brewos.local");
            MDNS.addService("http", "tcp", 80);
            mDNSStarted = true;
        } else {
            LOG_W("mDNS failed to start");
            mDNSStarted = true;  // Don't retry
        }
    }
    if (!machineState.wifi_connected) {
        // Reset when WiFi disconnects
        wifiConnectedTime = 0;
        wifiConnectedLogSent = false;
        mDNSStarted = false;
        ntpConfigured = false;
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
    // Only update alarm_active from status if we have a valid alarm code
    // MSG_ALARM messages are the source of truth for alarm state
    // Status packet flag is just a hint - don't set alarm_active without alarm_code
    bool statusAlarmFlag = (flags & 0x10) != 0;
    if (machineState.alarm_code != ALARM_NONE) {
        // We have a real alarm code - status flag should match
        machineState.alarm_active = statusAlarmFlag;
    } else {
        // No alarm code - status flag is unreliable, keep alarm_active false
        machineState.alarm_active = false;
    }
    
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
 * Handle encoder rotation and button events - DISABLED
 */
void handleEncoderEvent(int32_t diff, button_state_t btn) {
    // Encoder/UI disabled
    (void)diff;
    (void)btn;
    /*
    if (diff != 0) {
        LOG_D("Encoder: %+d", diff);
        ui.handleEncoder(diff);
    }
    
    if (btn == BTN_PRESSED) {
        ui.handleButtonPress();
    } else if (btn == BTN_LONG_PRESSED) {
        ui.handleLongPress();
    } else if (btn == BTN_DOUBLE_PRESSED) {
        ui.handleDoublePress();
    }
    */
}
