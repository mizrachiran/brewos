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
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>  // For vTaskDelay
#include <freertos/semphr.h>  // For xSemaphoreCreateMutex
#include <esp_wifi.h>       // For esp_wifi_set_ps, esp_wifi_get_ps
#include "config.h"
#include "memory_utils.h"   // For heap fragmentation monitoring
#include "wifi_manager.h"
#include "web_server.h"
#include "pico_uart.h"
#include "log_manager.h"

// Display and UI
#include "display/display.h"
#include "display/encoder.h"
#include "display/theme.h"
#include "ui/ui.h"
#include "ui/screen_setup.h"
#include "ui/screen_ota.h"
#include "ui/screen_cloud.h"

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

// Status Change Detection
#include "utils/status_change_detector.h"

// Pico Protocol Handler
#include "pico_protocol_handler.h"

// Runtime State Management
#include "runtime_state.h"

// Panic Handler - catches crashes and writes to log buffer
#include "panic_handler.h"

// Global instances - use pointers to defer construction until setup()
// This prevents crashes in constructors before Serial is initialized
WiFiManager* wifiManager = nullptr;
PicoUART* picoUart = nullptr;
MQTTClient* mqttClient = nullptr;
PairingManager* pairingManager = nullptr;
CloudConnection* cloudConnection = nullptr;
BrewWebServer* webServer = nullptr;

// Pico Protocol Handler - encapsulates protocol V1.1 logic
PicoProtocolHandler protocolHandler;

// Captive portal DNS server for AP mode
DNSServer dnsServer;
bool dnsServerRunning = false;

// Pre-infusion default pause time (ms) when enabled but no specific pause time is saved
static constexpr uint16_t DEFAULT_PREINFUSION_PAUSE_MS = 5000;

// =============================================================================
// LOG LEVEL CONTROL
// =============================================================================
BrewOSLogLevel g_brewos_log_level = BREWOS_LOG_INFO;  // Default to INFO level

void setLogLevel(BrewOSLogLevel level) {
    g_brewos_log_level = level;
    LOG_I("Level set to: %s (%d)", logLevelToString(level), level);
}

BrewOSLogLevel getLogLevel() {
    return g_brewos_log_level;
}

// Helper function to check if debug logs should be broadcast via WebSocket
// Used by LOG_D macro in config.h
bool should_broadcast_debug_logs(void) {
    return State.settings().system.debugLogsEnabled;
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

// Alarm debouncing to prevent rapid toggling
static uint8_t lastProcessedAlarmCode = 0x00;  // ALARM_NONE - tracks what we actually processed
static uint8_t lastReceivedAlarmCode = 0x00;   // Tracks last received code for debouncing
static uint32_t lastAlarmChangeTime = 0;
static constexpr uint32_t ALARM_DEBOUNCE_MS = 2000;  // 2 seconds debounce - require stable period

// Machine state from Pico - Now managed by RuntimeState class
// All state access should go through runtimeState() singleton

// Note: Demo mode is handled by the web UI only (via URL parameters)
// ESP32 does not simulate data when Pico is not connected
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

// Flag to trigger immediate UI refresh on encoder activity
static volatile bool encoderActivityFlag = false;

// Pre-allocated JSON buffer for diagnostics messages (reused to avoid heap fragmentation)
// Size 512 bytes is sufficient for both diagnostic header and result messages
static char diagnosticJsonBuffer[512] = {0};
static SemaphoreHandle_t diagnosticBufferMutex = nullptr;  // Thread-safe access to diagnostic buffer

// Forward declarations
void parsePicoStatus(const uint8_t* payload, uint8_t length);
void handleEncoderEvent(int32_t diff, button_state_t btn);
static void onPicoPacket(const PicoPacket& packet);

// Setup helper functions
static void setupEarlyInitialization();
static void setupCheckPendingOTA();
static void setupInitializeFilesystem();
static void setupCreateGlobalObjects();
static void setupInitializeDisplayAndEncoder();
static void setupInitializeUI();
static void setupInitializePicoUART();
static void setupWaitForPicoConnection();
static void setupRequestPicoBootInfo(bool picoConnected);
static void setupInitializeWiFi();
static void setupInitializeWebServer();
static void setupInitializeMQTT();
static void setupInitializeScaleAndBBW();
static void setupInitializeStateManager();
static void setupInitializeLogManager();  // Early initialization (after filesystem)
static void setupInitializeLogManagerSettings();  // Settings configuration (after State loaded)
static void setupInitializeCloudConnection();
static void setupInitializeNotificationManager();

// Loop helper functions
static void loopCoreSystemUpdates();
static void loopOptionalServiceUpdates();
static void loopUpdateConnectionStates();
static void loopHandlePicoBootInfo();
static void loopUpdateBrewByWeight();
static void loopUpdateUI();
static void loopUpdateMQTTStatus();
static void loopPeriodicTasks();
static void loopHandleWiFiTasks();
static void loopMonitorMemoryAndTiming();

// Pico packet handler functions
static void handlePicoACK(const PicoPacket& packet);
static void handlePicoAlarm(const PicoPacket& packet);
static void handlePicoEnvConfig(const PicoPacket& packet);
static void handlePicoDiagnostics(const PicoPacket& packet);

// State management functions now provided by RuntimeState class
// Use runtimeState() singleton for all state access

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
    
    // Update machine state using thread-safe function that updates both buffers
    // This prevents lost updates when parsePicoStatus swaps buffers
        runtimeState().updateWiFi(true, false, WiFi.RSSI());
    
    // Get IP address for logging (before mutex)
    IPAddress ip = WiFi.localIP();
    
    // Update SSID and IP in state (use RuntimeState to prevent race conditions)
    ui_state_t& state = runtimeState().beginUpdate();
    
    // Get WiFi SSID directly from WiFiManager's stored value
    if (wifiManager) {
        const char* ssid = wifiManager->getStoredSSID();
        if (ssid && strlen(ssid) > 0) {
            strncpy(state.wifi_ssid, ssid, sizeof(state.wifi_ssid) - 1);
            state.wifi_ssid[sizeof(state.wifi_ssid) - 1] = '\0';
        }
    }
    
    // Get IP directly into buffer (no String allocation)
    snprintf(state.wifi_ip, sizeof(state.wifi_ip), "%d.%d.%d.%d", 
             ip[0], ip[1], ip[2], ip[3]);
    
    runtimeState().endUpdate();
    
    // Log immediately - web server is already running and ready
    LOG_I("Web server ready: http://%d.%d.%d.%d/ or http://brewos.local/", 
          ip[0], ip[1], ip[2], ip[3]);
    
    // Note: Cloudflare DNS is applied in WiFiManager::loop() after connection
    // Don't apply here - using INADDR_NONE would reset the DHCP configuration
    
    // mDNS will be started immediately in main loop (no delay)
}

// Called when WiFi disconnects
static void onWiFiDisconnected() {
    LOG_W("WiFi disconnected");
    // Use thread-safe update that modifies both buffers to prevent lost updates
        runtimeState().updateWiFi(false, false, 0);
    // Reset WiFi connected state tracking
    wifiConnectedTime = 0;
    wifiConnectedLogSent = false;
    
    // Stop mDNS cleanly so it can be restarted
    if (mDNSStarted) {
        MDNS.end();
        mDNSStarted = false;
    }
    
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
    
    // AP is active - check if we also have STA connection (AP+STA mode)
    // Only set wifi_ap_mode if we're truly in AP-only mode (no STA connection)
    // If WiFi is still connected, we're in AP+STA mode - don't trigger auto-setup screen
    bool wifiStillConnected = (WiFi.status() == WL_CONNECTED);
    ui_state_t& state = runtimeState().beginUpdate();
    state.wifi_ap_mode = !wifiStillConnected;  // Only true if AP-only (no WiFi connection)
    state.wifi_connected = wifiStillConnected;
    runtimeState().endUpdate();
    
    if (wifiStillConnected) {
        LOG_I("AP+STA mode: WiFi still connected, setup screen will not auto-show");
    }
    
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

static void onScaleWeight(const scale_state_t& scaleState) {
    ui_state_t& state = runtimeState().beginUpdate();
    state.scale_connected = scaleState.connected;
    state.brew_weight = scaleState.weight;
    state.flow_rate = scaleState.flow_rate;
    runtimeState().endUpdate();
}

static void onScaleConnection(bool connected) {
    LOG_I("Scale %s", connected ? "connected" : "disconnected");
        runtimeState().updateScaleConnection(connected);
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

static void onCloudCommand(const char* type, JsonDocument& doc) {
    // Commands from cloud users are processed the same as local WebSocket
    // Using const char* instead of String to avoid heap fragmentation
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
        // Validate machine state before allowing turn on
        // Only allow turning on from IDLE, READY, or ECO states
        uint8_t currentState = runtimeState().get().machine_state;
        if (currentState != UI_STATE_IDLE && 
            currentState != UI_STATE_READY && 
            currentState != UI_STATE_ECO) {
            const char* stateNames[] = {"INIT", "IDLE", "HEATING", "READY", "BREWING", "FAULT", "SAFE", "ECO"};
            LOG_W("Schedule: Cannot turn on machine: current state is %s. Machine must be in IDLE, READY, or ECO state.",
                  (currentState < 8) ? stateNames[currentState] : "UNKNOWN");
            return;
        }
        
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
    // NOTE: Raw Pico messages are NOT forwarded to WebSocket clients
    // The UI should use processed "status" messages instead, not low-level ESP32-Pico protocol
    // These messages are for ESP32-Pico communication only, not for the web UI
    
    // Delegate protocol-level messages to handler (boot, handshake, NACK, status, power meter)
    // These are handled by PicoProtocolHandler for better maintainability
    if (packet.type == MSG_BOOT || packet.type == MSG_HANDSHAKE || packet.type == MSG_NACK || 
        packet.type == MSG_STATUS || packet.type == MSG_POWER_METER) {
        protocolHandler.handlePacket(packet);
        // MSG_STATUS also needs to update connection state
        if (packet.type == MSG_STATUS) {
            runtimeState().updatePicoConnection(true);
        }
        return;
    }
    
    // Handle message types that are tightly coupled to main.cpp state
    // These remain in main.cpp for now but could be moved to handler in future refactoring
    switch (packet.type) {
        case MSG_ACK:
            handlePicoACK(packet);
            break;
        case MSG_ALARM:
            handlePicoAlarm(packet);
            break;
        case MSG_CONFIG:
            LOG_I("Received config from Pico");
            if (webServer) webServer->broadcastLog("Config received from Pico");
            break;
        case MSG_ENV_CONFIG:
            handlePicoEnvConfig(packet);
            break;
        case MSG_DEBUG_RESP:
            LOG_D("Debug response from Pico");
            break;
        case MSG_DIAGNOSTICS:
            handlePicoDiagnostics(packet);
            break;
        case MSG_LOG: {
            // Log message from Pico - forward to log manager
            if (g_logManager && packet.length > 0) {
                g_logManager->handlePicoLog(packet.payload, packet.length);
            }
            break;
        }
        default:
            // Only log unknown packet types (not every packet)
            LOG_W("Unknown packet type: 0x%02X, len=%d, seq=%d", 
                  packet.type, packet.length, packet.seq);
    }
}

static void handlePicoACK(const PicoPacket& packet) {
    // Command acknowledgment from Pico
    // ACK payload structure: [cmd_type(1)] [cmd_seq(1)] [result(1)] [reserved(1)]
    if (packet.length >= 3) {
        uint8_t cmdType = packet.payload[0];  // Original command type
        uint8_t cmdSeq = packet.payload[1];   // Sequence number
        uint8_t resultCode = packet.payload[2]; // Actual result code
        // Only log non-success acknowledgments to reduce noise
        if (resultCode != ACK_SUCCESS) {
            const char* errorMsg = "Unknown error";
            switch (resultCode) {
                case ACK_ERROR_INVALID:    errorMsg = "Invalid command"; break;
                case ACK_ERROR_REJECTED:   errorMsg = "Command rejected"; break;
                case ACK_ERROR_FAILED:    errorMsg = "Command failed"; break;
                case ACK_ERROR_TIMEOUT:   errorMsg = "Timeout"; break;
                case ACK_ERROR_BUSY:      errorMsg = "System busy"; break;
                case ACK_ERROR_NOT_READY: errorMsg = "Not ready"; break;
            }
            LOG_W("Pico ACK error: %s (cmd=0x%02X, result=0x%02X)", errorMsg, cmdType, resultCode);
        }
    }
}

static void handlePicoAlarm(const PicoPacket& packet) {
    uint8_t alarmCode = packet.payload[0];
    uint32_t now = millis();
    
    // Debounce: Require stable alarm state before processing changes
    // This prevents rapid toggling between alarm codes (e.g., 0x05 <-> 0x00)
    // Strategy: Track the last received code and only process when it's been stable
    bool shouldProcess = false;
    
    if (alarmCode != lastReceivedAlarmCode) {
        // New alarm code received - reset debounce timer
        lastReceivedAlarmCode = alarmCode;
        lastAlarmChangeTime = now;
        // Don't process yet - wait for it to be stable
    } else if ((now - lastAlarmChangeTime) >= ALARM_DEBOUNCE_MS) {
        // Same code received for debounce period - it's stable, process it
        shouldProcess = true;
    }
    // Otherwise: same code but not stable yet - continue waiting
    
    if (!shouldProcess) {
        // Ignore this alarm message - waiting for stable state
        return;
    }
    
    // Process the stable alarm code
    lastProcessedAlarmCode = alarmCode;
    
    // Check current state before updating (need to read first)
    const ui_state_t& currentState = runtimeState().get();
    bool wasAlarmActive = currentState.alarm_active;
    uint8_t currentAlarmCode = currentState.alarm_code;
    
    if (alarmCode == ALARM_NONE) {
        // ALARM_NONE (0x00) means no alarm - clear the alarm state
        // Only log if we're actually transitioning from a REAL alarm (non-zero) to cleared
        if (wasAlarmActive && currentAlarmCode != ALARM_NONE) {
            LOG_I("Pico alarm cleared (was: 0x%02X)", currentAlarmCode);
            // Defensive: Check webServer pointer and use try-catch equivalent (null check)
            if (webServer) {
                // Use a safer approach - format the message first to avoid variadic issues
                char alarmMsg[64];
                snprintf(alarmMsg, sizeof(alarmMsg), "Pico alarm cleared");
                webServer->broadcastLog(alarmMsg, "info");
            }
        }
        // Always update state, but only log when transitioning from real alarm
        ui_state_t& state = runtimeState().beginUpdate();
        state.alarm_active = false;
        state.alarm_code = ALARM_NONE;
        runtimeState().endUpdate();
    } else {
        // Actual alarm - log and set alarm state
        // Only log if this is a new alarm or different from current
        if (!wasAlarmActive || currentAlarmCode != alarmCode) {
            LOG_W("PICO ALARM: 0x%02X", alarmCode);
            // Defensive: Format message first to avoid variadic argument issues
            if (webServer) {
                char alarmMsg[64];
                snprintf(alarmMsg, sizeof(alarmMsg), "Pico ALARM: 0x%02X", alarmCode);
                webServer->broadcastLog(alarmMsg, "error");
            }
        }
        ui_state_t& state = runtimeState().beginUpdate();
        state.alarm_active = true;
        state.alarm_code = alarmCode;
        runtimeState().endUpdate();
    }
}

static void handlePicoEnvConfig(const PicoPacket& packet) {
    // Pico is the source of truth for power settings
    if (packet.length >= 18) {
        uint16_t voltage = packet.payload[0] | (packet.payload[1] << 8);
        float max_current = 0;
        memcpy(&max_current, &packet.payload[2], sizeof(float));
        
        // Store in machineState for use in broadcasts (Pico is source of truth)
        // Note: We don't persist these on ESP32 - Pico handles persistence
        State.settings().power.mainsVoltage = voltage;
        State.settings().power.maxCurrent = max_current;
        
        LOG_I("Env config from Pico: %dV, %.1fA max", voltage, max_current);
        if (webServer) {
            webServer->broadcastLogLevel("info", "Env config: %dV, %.1fA max", voltage, max_current);
            // Broadcast updated device info so UI refreshes
            webServer->broadcastDeviceInfo();
        }
    }
}

static void handlePicoDiagnostics(const PicoPacket& packet) {
    // Use pre-allocated buffer to avoid heap fragmentation
    // Thread-safe mutex protection for concurrent access
    if (diagnosticBufferMutex == nullptr) {
        // Mutex not initialized yet, skip (shouldn't happen in normal operation)
        LOG_W("Diagnostic buffer mutex not initialized, skipping message");
        return;
    }
    
    if (xSemaphoreTake(diagnosticBufferMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        LOG_W("Diagnostic buffer in use, skipping message");
        return;
    }
    
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
        if (jsonSize <= sizeof(diagnosticJsonBuffer) && webServer) {
            serializeJson(doc, diagnosticJsonBuffer, sizeof(diagnosticJsonBuffer));
            webServer->broadcastRaw(diagnosticJsonBuffer);
        } else {
            LOG_W("Diagnostic JSON too large for buffer: %zu bytes", jsonSize);
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
        if (jsonSize <= sizeof(diagnosticJsonBuffer) && webServer) {
            serializeJson(doc, diagnosticJsonBuffer, sizeof(diagnosticJsonBuffer));
            webServer->broadcastRaw(diagnosticJsonBuffer);
        } else {
            LOG_W("Diagnostic JSON too large for buffer: %zu bytes", jsonSize);
        }
    }
    
    xSemaphoreGive(diagnosticBufferMutex);
}

static void setupEarlyInitialization() {
    // Turn on backlight immediately so user knows device is running
    // Backlight is GPIO7, active LOW (LOW = ON)
    pinMode(7, OUTPUT);
    digitalWrite(7, LOW);
    
    // Initialize Serial (USB CDC if enabled, hardware UART if disabled)
    // When USB CDC is disabled (ARDUINO_USB_MODE=0), Serial uses hardware UART
    // and doesn't have setTxTimeoutMs() method
    Serial.begin(115200);
    #if ARDUINO_USB_MODE == 1
    // USB CDC mode - set timeout to prevent blocking
    Serial.setTxTimeoutMs(10);  // Short timeout - won't block long if no USB host
    #endif
    
    // Note: Watchdog is kept enabled - it helps catch hangs and crashes
    // Attempting to disable it causes errors on ESP32-S3
    // 
    // Watchdog Configuration:
    // - Default timeout: 3-5 seconds (hardware watchdog)
    // - If loop() takes >1 second, watchdog may reset the chip
    // - All blocking operations must use yield() or vTaskDelay() to feed watchdog
    // - Slow loop detection (Phase 8) is diagnostic only - doesn't prevent resets
    
    // Initialize runtime state (creates mutex for state buffer protection)
    runtimeState().begin();
    
    // Initialize diagnostic buffer mutex for thread-safe access
    diagnosticBufferMutex = xSemaphoreCreateMutex();
    if (diagnosticBufferMutex == nullptr) {
        Serial.println("ERROR: Failed to create diagnostic buffer mutex");
    }
    
    // Register panic handler early to catch crashes
    registerPanicHandler();
    
    // Print startup info (will be lost if no USB host connected)
    Serial.println();
    Serial.println("SETUP START");
    
    Serial.print("Internal heap: ");
    Serial.println(ESP.getFreeHeap());
    Serial.print("PSRAM size: ");
    Serial.println(ESP.getPsramSize());
    Serial.print("PSRAM free: ");
    Serial.println(ESP.getFreePsram());
    
    // Check memory allocation strategy
    // Small allocations (<4KB) should use internal RAM for speed
    // Large allocations (>4KB) can use PSRAM
    void* smallAlloc = malloc(64);
    void* largeAlloc = ps_malloc(65536);  // 64KB - use PSRAM explicitly
    
    constexpr uintptr_t ESP32S3_PSRAM_START = 0x3C000000;
    constexpr uintptr_t ESP32S3_PSRAM_END   = 0x3E000000;
    
    uintptr_t smallAddr = (uintptr_t)smallAlloc;
    bool smallInPSRAM = (smallAddr >= ESP32S3_PSRAM_START && smallAddr < ESP32S3_PSRAM_END);
    Serial.printf("Small alloc (64B): 0x%08X (%s)\n", smallAddr, smallInPSRAM ? "PSRAM" : "Internal RAM");
    
    if (largeAlloc) {
        uintptr_t largeAddr = (uintptr_t)largeAlloc;
        bool largeInPSRAM = (largeAddr >= ESP32S3_PSRAM_START && largeAddr < ESP32S3_PSRAM_END);
        Serial.printf("Large alloc (64KB): 0x%08X (%s)\n", largeAddr, largeInPSRAM ? "PSRAM - OK" : "Internal RAM - PSRAM not working!");
        free(largeAlloc);
    } else {
        Serial.println("WARNING: PSRAM allocation failed - PSRAM may not be available");
    }
    free(smallAlloc);
    
    // Initialize NVS (Non-Volatile Storage) FIRST
    // This ensures Preferences library works correctly after fresh flash
    Serial.println("[1/8] Initializing NVS...");
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        Serial.print("NVS needs erase (err=");
        Serial.print(nvs_err);
        Serial.println(") - erasing...");
        nvs_flash_erase();
        nvs_err = nvs_flash_init();
    }
    if (nvs_err != ESP_OK) {
        Serial.print("NVS init FAILED: ");
        Serial.println(nvs_err);
        // Continue anyway - Preferences will handle missing NVS gracefully
    } else {
        Serial.println("NVS initialized OK");
    }
}

static void setupCheckPendingOTA() {
    // =========================================================================
    // EARLY PENDING OTA CHECK - Before heavy initialization
    // If there's a pending OTA, do minimal boot to preserve memory
    // =========================================================================
    String pendingOtaVersion;
    if (hasPendingOTA(pendingOtaVersion)) {
        // Check retry counter to prevent crash loops
        uint8_t retries = incrementPendingOTARetries();
        const uint8_t MAX_OTA_RETRIES = 2;
        
        Serial.println("========================================");
        Serial.println("PENDING OTA DETECTED - MINIMAL BOOT MODE");
        Serial.printf("Version: %s (attempt %d/%d)\n", pendingOtaVersion.c_str(), retries, MAX_OTA_RETRIES);
        Serial.println("========================================");
        Serial.flush();
        
        // If we've exceeded max retries, give up and boot normally
        if (retries > MAX_OTA_RETRIES) {
            Serial.println("ERROR: OTA failed too many times - clearing pending OTA");
            Serial.println("Booting normally...");
            Serial.flush();
            clearPendingOTA();
            // Fall through to normal boot (don't return, let setup() continue)
        } else {
            // Proceed with OTA attempt
            size_t heapBefore = ESP.getFreeHeap();
            size_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            Serial.printf("Memory before init: heap=%zu, largest block=%zu\n", heapBefore, largestBlock);
            Serial.flush();
        
            // Initialize LittleFS (needed for OTA to write files)
            Serial.println("Initializing LittleFS for OTA...");
            Serial.flush();
            if (!LittleFS.begin(true, "/littlefs", 5)) {  // Reduced max files
                LittleFS.format();
                LittleFS.begin(true, "/littlefs", 5);
            }
            
            // Create minimal objects needed for OTA
            Serial.println("Creating minimal objects for OTA...");
            Serial.flush();
            wifiManager = new WiFiManager();
            picoUart = new PicoUART(Serial1);
            mqttClient = new MQTTClient();
            pairingManager = new PairingManager();
            webServer = new BrewWebServer(*wifiManager, *picoUart, *mqttClient, pairingManager);
            
            // Initialize Pico UART (needed for Pico OTA)
            Serial.println("Initializing Pico UART...");
            Serial.flush();
            picoUart->begin();
            picoUart->onPacket(onPicoPacket);  // Need callback to process Pico responses
            
            // Wait for Pico to send machine type (needed for OTA firmware selection)
            Serial.println("Waiting for Pico machine type...");
            Serial.flush();
            unsigned long picoWaitStart = millis();
            while (State.getMachineType() == 0 && millis() - picoWaitStart < 5000) {
                picoUart->loop();
                delay(50);
            }
            if (State.getMachineType() != 0) {
                Serial.printf("Pico machine type: %d\n", State.getMachineType());
            } else {
                Serial.println("Warning: Pico not responding, OTA may fail");
            }
            Serial.flush();
            
            // Connect to WiFi
            Serial.println("Connecting to WiFi...");
            Serial.flush();
            wifiManager->begin();
            
            // NOTE: Web server NOT started in minimal boot mode to save memory
            // OTA progress will not be visible to users, but OTA will complete silently
            
            // Wait for WiFi to connect (up to 30 seconds)
            Serial.println("Waiting for WiFi connection...");
            Serial.flush();
            unsigned long wifiWaitStart = millis();
            while (!wifiManager->isConnected() && millis() - wifiWaitStart < 30000) {
                wifiManager->loop();
                picoUart->loop();  // Keep Pico UART responsive
                delay(100);
            }
            
            if (!wifiManager->isConnected()) {
                Serial.println("ERROR: WiFi connection failed - will retry on next boot");
                Serial.flush();
                delay(1000);
                ESP.restart();
                return;
            }
            
            Serial.printf("WiFi connected: %s\n", WiFi.localIP().toString().c_str());
            Serial.flush();
            
            // Check memory after minimal init
            size_t heapAfter = ESP.getFreeHeap();
            largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            Serial.printf("Memory after minimal init: heap=%zu, largest block=%zu\n", heapAfter, largestBlock);
            Serial.flush();
            
            // Start OTA with maximum available memory
            Serial.println("Starting OTA update...");
            Serial.flush();
            webServer->startCombinedOTA(pendingOtaVersion, true);
            
            // If we get here, OTA failed - restart to retry (counter already incremented)
            Serial.println("OTA returned unexpectedly - restarting to retry...");
            Serial.flush();
            delay(1000);
            ESP.restart();
            return;  // Won't reach
        }
    }
    // =========================================================================
    // END EARLY PENDING OTA CHECK - Continue normal boot
    // =========================================================================
}

static void setupInitializeFilesystem() {
    // Initialize LittleFS (needed by State, WebServer, etc.)
    Serial.println("[2/8] Initializing LittleFS...");
    // Use 10 max open files (reduced from 15 to save heap)
    if (!LittleFS.begin(true, "/littlefs", 10)) {
        Serial.println("LittleFS mount failed - formatting...");
        LittleFS.format();
        if (!LittleFS.begin(true, "/littlefs", 10)) {
            Serial.println("ERROR: LittleFS format failed!");
            // Continue anyway - web server will handle missing files gracefully
        } else {
            Serial.println("LittleFS formatted and mounted OK");
        }
    } else {
        Serial.println("LittleFS mounted OK");
    }
    
    // Log Manager is initialized but NOT enabled by default
    // Buffer is only allocated when enabled via settings (dev mode feature)
    // This is done later after State is loaded, to check the setting
}

static void setupCreateGlobalObjects() {
    // Turn on backlight so user knows device is running
    // IMPORTANT: Backlight is ACTIVE LOW on VIEWESMART UEDX48480021-MD80E!
    Serial.println("[3/8] Turning on backlight...");
    pinMode(7, OUTPUT);
    digitalWrite(7, LOW);  // Active LOW - LOW = ON, HIGH = OFF
    Serial.println("Backlight ON (active low)");
    
    // Construct global objects NOW (after Serial is initialized)
    // CRITICAL: Allocate in internal RAM (not PSRAM) to avoid InstructionFetchError
    // when callbacks are called. PSRAM pointers cause CPU crashes.
    Serial.println("[3.5/8] Creating global objects in internal RAM...");
    // Serial.flush(); // Removed - can block on USB CDC
    
    // All objects use regular new - PSRAM is disabled so malloc uses internal RAM
    // Placement new with heap_caps_malloc was causing memory corruption
    wifiManager = new WiFiManager();
    Serial.println("WiFiManager created");
    // Serial.flush(); // Removed - can block on USB CDC
    
    picoUart = new PicoUART(Serial1);
    Serial.println("PicoUART created");
    // Serial.flush(); // Removed - can block on USB CDC
    
    mqttClient = new MQTTClient();
    Serial.println("MQTTClient created");
    // Serial.flush(); // Removed - can block on USB CDC
    
    pairingManager = new PairingManager();
    Serial.println("PairingManager created");
    // Serial.flush(); // Removed - can block on USB CDC
    
    cloudConnection = new CloudConnection();
    Serial.println("CloudConnection created");
    // Serial.flush(); // Removed - can block on USB CDC
    
    webServer = new BrewWebServer(*wifiManager, *picoUart, *mqttClient, pairingManager);
    Serial.println("WebServer created");
    // Serial.flush(); // Removed - can block on USB CDC
    
    scaleManager = new ScaleManager();
    Serial.println("ScaleManager created");
    // Serial.flush(); // Removed - can block on USB CDC
    
    brewByWeight = new BrewByWeight();
    Serial.println("BrewByWeight created");
    // Serial.flush(); // Removed - can block on USB CDC
    
    powerMeterManager = new PowerMeterManager();
    Serial.println("PowerMeterManager created");
    // Serial.flush(); // Removed - can block on USB CDC
    
    notificationManager = new NotificationManager();
    Serial.println("NotificationManager created");
    // Serial.flush(); // Removed - can block on USB CDC
    
    Serial.println("All global objects created OK");
    // Serial.flush(); // Removed - can block on USB CDC
}

static void setupInitializeDisplayAndEncoder() {
    // Initialize display (PSRAM enabled for RGB frame buffer)
    // Now using lower PCLK (8 MHz) and bounce buffer for WiFi compatibility
    Serial.println("[4/8] Initializing display...");
    if (!display.begin()) {
        Serial.println("ERROR: Display initialization failed!");
    } else {
        Serial.println("Display initialized OK");
    }
    
    // Initialize encoder
    Serial.println("[4.5/8] Initializing encoder...");
    if (!encoder.begin()) {
        Serial.println("ERROR: Encoder initialization failed!");
    } else {
        Serial.println("Encoder initialized OK");
    }
    encoder.setCallback(handleEncoderEvent);
}

static void setupInitializeUI() {
    // Check if WiFi setup is needed BEFORE initializing UI
    // This ensures the setup screen shows immediately if no credentials exist
    Serial.println("[4.7/8] Checking WiFi credentials...");
    // Serial.flush(); // Removed - can block on USB CDC
    bool needsWifiSetup = !wifiManager->checkCredentials();
    ui_state_t& state = runtimeState().beginUpdate();
    if (needsWifiSetup) {
        Serial.println("No WiFi credentials found - setup screen will be shown");
        state.wifi_ap_mode = true;
        state.wifi_connected = false;
    } else {
        Serial.println("WiFi credentials found");
        state.wifi_ap_mode = false;
        state.wifi_connected = false;  // Will be updated when WiFi connects
    }
    runtimeState().endUpdate();
    // Serial.flush(); // Removed - can block on USB CDC
    
    // Initialize UI
    Serial.println("[4.8/8] Initializing UI...");
    if (!ui.begin()) {
        Serial.println("ERROR: UI initialization failed!");
    } else {
        Serial.println("UI initialized OK");
        ui.update(runtimeState().get());
        if (needsWifiSetup) {
            Serial.println("Showing WiFi setup screen...");
        }
        display.update();
    }
}

static void setupUICallbacks() {
    // UI callbacks
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
        uint8_t payload[3];
        payload[0] = is_steam ? 0x01 : 0x00;
        int16_t tempScaled = (int16_t)(temp * 10.0f);
        payload[1] = tempScaled & 0xFF;
        payload[2] = (tempScaled >> 8) & 0xFF;
        picoUart->sendCommand(MSG_CMD_SET_TEMP, payload, 3);
    });
    
    ui.onTareScale([]() {
        LOG_I("UI: Tare scale requested");
        scaleManager->tare();
    });
    
    ui.onSetTargetWeight([](float weight) {
        LOG_I("UI: Set target weight to %.1fg", weight);
        brewByWeight->setTargetWeight(weight);
        ui_state_t& state = runtimeState().beginUpdate();
        state.target_weight = weight;
        runtimeState().endUpdate();
    });
    
    ui.onWifiSetup([]() {
        LOG_I("UI: WiFi setup requested");
        wifiManager->setStaticIP(false);
        wifiManager->startAP();
    });
}

static void setupInitializePicoUART() {
    // Initialize Pico UART
    Serial.println("[4/8] Initializing Pico UART...");
    // Serial.flush(); // Removed - can block on USB CDC
    picoUart->begin();
    Serial.println("Pico UART initialized OK");
    // Serial.flush(); // Removed - can block on USB CDC
    
    // Set up packet handler BEFORE waiting for Pico
    // This ensures we capture the MSG_BOOT packet with machine type
    Serial.println("[4.4/8] Setting up Pico packet handler...");
    // Serial.flush(); // Removed - can block on USB CDC
    picoUart->onPacket(onPicoPacket);
    
    // Initialize protocol handler with dependencies
    // State is a macro for BrewOS::StateManager::getInstance(), which returns a reference
    protocolHandler.begin(picoUart, webServer, &BrewOS::StateManager::getInstance(), powerMeterManager);
    Serial.println("Protocol handler initialized");
    
    // Pico reset is now available via GPIO20 (repurposed from USB D-)
    // No longer conflicts with display reset pin (GPIO8)
    // Note: Pico reset can be performed via picoUart->resetPico() if needed
}

static void setupWaitForPicoConnection() {
    // Wait for Pico to connect (sends boot message)
    // Pico Core 1 needs time to initialize and send boot message
    // Increased to 10 seconds to allow for simultaneous power-on initialization
    Serial.println("[4.6/8] Waiting for Pico connection (10 seconds)...");
    // Serial.flush(); // Removed - can block on USB CDC
    unsigned long picoWaitStart = millis();
    bool picoConnected = false;
    uint32_t initialPackets = picoUart->getPacketsReceived();
    bool sawAnyData = false;
    
    while (millis() - picoWaitStart < 10000) {
        picoUart->loop();  // Process any incoming packets
        
        // Skip display.update() during this wait to minimize PSRAM bandwidth contention
        // The hardware LCD controller will keep the last frame displayed automatically.
        // Running LVGL here can cause display noise due to memory bus contention.
        
        // Check if we're receiving any raw data at all
        if (picoUart->bytesAvailable() > 0) {
            sawAnyData = true;
        }
        
        // Check if we received any packets (even if not "connected" yet)
        if (picoUart->getPacketsReceived() > initialPackets) {
            Serial.printf("Received %d packet(s) from Pico\n", 
                         picoUart->getPacketsReceived() - initialPackets);
            // Serial.flush(); // Removed - can block on USB CDC
        }
        
        if (picoUart->isConnected()) {
            picoConnected = true;
            Serial.println("Pico connected!");
            // Serial.flush(); // Removed - can block on USB CDC
            break;
        }
        
        // Increase delay to reduce CPU/memory load
        delay(100);
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
        // Serial.flush(); // Removed - can block on USB CDC
        if (picoUart->sendPing()) {
            Serial.println("Ping sent, waiting 500ms for response...");
            // Serial.flush(); // Removed - can block on USB CDC
            delay(500);
            picoUart->loop();
            if (picoUart->isConnected()) {
                picoConnected = true;
                Serial.println("Pico responded to ping - connected!");
                // Serial.flush(); // Removed - can block on USB CDC
            } else {
                Serial.println("Ping sent but no response received");
                // Serial.flush(); // Removed - can block on USB CDC
            }
        } else {
            Serial.println("Failed to send ping");
            // Serial.flush(); // Removed - can block on USB CDC
        }
        
        if (!picoConnected) {
            Serial.println("Continuing without Pico");
            // Serial.flush(); // Removed - can block on USB CDC
        }
    } else {
        // Pico connected - initiate protocol v1.1 handshake
        Serial.println("Initiating protocol v1.1 handshake...");
        if (picoUart->sendHandshake()) {
            Serial.println("Protocol handshake sent - waiting for response...");
            // Process incoming packets for handshake response
            for (int i = 0; i < 50; i++) {  // Wait up to 500ms
                delay(10);
                picoUart->loop();
            }
            Serial.println("Protocol handshake complete");
        } else {
            Serial.println("WARNING: Failed to send protocol handshake");
        }
    }
    
    setupRequestPicoBootInfo(picoConnected);
}

static void setupRequestPicoBootInfo(bool picoConnected) {
    // If machine type or pico version is still unknown, request boot info from Pico
    // This handles the case where MSG_BOOT was missed (Pico was already running before ESP32)
    const char* picoVersion = State.getPicoVersion();
    bool picoVersionUnknown = !picoVersion || picoVersion[0] == '\0';
    if (picoConnected && (State.getMachineType() == 0 || picoVersionUnknown)) {
        Serial.println("Machine type or pico version unknown - requesting boot info from Pico...");
        // Serial.flush(); // Removed - can block on USB CDC
        
        // Try multiple times since Pico might be busy
        for (int attempt = 0; attempt < 5 && (State.getMachineType() == 0 || picoVersionUnknown); attempt++) {
            if (picoUart->requestBootInfo()) {
                // Wait up to 500ms for response, processing packets
                for (int i = 0; i < 50 && (State.getMachineType() == 0 || picoVersionUnknown); i++) {
                    delay(10);
                    picoUart->loop();
                    // Re-check version after processing packets
                    picoVersion = State.getPicoVersion();
                    picoVersionUnknown = !picoVersion || picoVersion[0] == '\0';
                }
            }
            if (State.getMachineType() == 0 || picoVersionUnknown) {
                Serial.printf("Attempt %d: No boot info received\n", attempt + 1);
                // Serial.flush(); // Removed - can block on USB CDC
            }
        }
        
        if (State.getMachineType() != 0 && !picoVersionUnknown) {
            Serial.printf("Machine type received: %d, Pico version: %s\n", State.getMachineType(), State.getPicoVersion());
        } else {
            Serial.println("WARNING: Could not get complete boot info from Pico");
            if (State.getMachineType() == 0) {
                Serial.println("OTA updates will wait for Pico to report its type");
            }
            if (picoVersionUnknown) {
                Serial.println("Pico version will remain unknown");
            }
        }
        // Serial.flush(); // Removed - can block on USB CDC
    }
}

static void setupInitializeWiFi() {
    // Initialize WiFi callbacks using static function pointers
    // This avoids std::function which allocates in PSRAM and causes InstructionFetchError
    wifiManager->onConnected(onWiFiConnected);
    wifiManager->onDisconnected(onWiFiDisconnected);
    wifiManager->onAPStarted(onWiFiAPStarted);
    
    Serial.println("[5/8] Initializing WiFi Manager...");
    // Serial.flush(); // Removed - can block on USB CDC
    wifiManager->begin();
    Serial.println("WiFi Manager initialized OK");
    // Serial.flush(); // Removed - can block on USB CDC
    
    // Stagger initialization to reduce power supply load and EMI spikes
    delay(500);
}

static void setupInitializeWebServer() {
    // Start web server
    Serial.println("[6/8] Starting web server...");
    // Serial.flush(); // Removed - can block on USB CDC
    webServer->begin();
    Serial.println("Web server started OK");
    
    // Initialize pre-allocated broadcast buffers in PSRAM
    // This avoids repeated allocations in broadcastFullStatus (called every 500ms)
    initBroadcastBuffers();
    Serial.println("Broadcast buffers initialized");
    // Serial.flush(); // Removed - can block on USB CDC
    
    // Stagger initialization
    delay(200);
    
    // Record server ready time
    serverReadyTime = millis();
    
    // Initialize Power Meter Manager
    Serial.println("[7.5/8] Initializing Power Meter...");
    // Serial.flush(); // Removed - can block on USB CDC
    powerMeterManager->begin();
    Serial.println("Power Meter initialized OK");
    // Serial.flush(); // Removed - can block on USB CDC
}

void setup() {
    setupEarlyInitialization();
    setupCheckPendingOTA();
    setupInitializeFilesystem();
    setupInitializeLogManager();  // Enable early to capture all boot logs
    setupCreateGlobalObjects();
    setupInitializeDisplayAndEncoder();
    setupInitializeUI();
    setupUICallbacks();
    setupInitializePicoUART();
    setupWaitForPicoConnection();
    setupInitializeWiFi();
    setupInitializeWebServer();
    setupInitializeMQTT();
    setupInitializeScaleAndBBW();
    setupInitializeStateManager();
    setupInitializeLogManagerSettings();  // Check settings and configure Pico forwarding
    setupInitializeCloudConnection();
    setupInitializeNotificationManager();
    
    Serial.println("========================================");
    Serial.println("SETUP COMPLETE!");
    Serial.print("Free heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");
    Serial.println("Entering main loop...");
    Serial.println("========================================");
    // Serial.flush(); // Removed - can block on USB CDC
    
    // Final display update before entering main loop to ensure screen is visible
    display.update();
    ui.update(runtimeState().get());
    display.update();
}

static void setupInitializeMQTT() {
    // Set up MQTT command handler (before MQTT initialization)
    mqttClient->onCommand([](const char* cmd, JsonDocument& doc) {
        String cmdStr = cmd;
        
        if (cmdStr == "set_temp") {
            String boiler = doc["boiler"] | "brew";
            float temp = doc["temp"] | 0.0f;
            
            // Pico expects: [target:1][temperature:int16] where temperature is Celsius * 10
            // Note: Pico (RP2350) is little-endian, so send LSB first
            uint8_t payload[3];
            payload[0] = (boiler == "steam") ? 0x01 : 0x00;  // 0=brew, 1=steam
            int16_t tempScaled = (int16_t)(temp * 10.0f);
            payload[1] = tempScaled & 0xFF;         // LSB first
            payload[2] = (tempScaled >> 8) & 0xFF;  // MSB second
            picoUart->sendCommand(MSG_CMD_SET_TEMP, payload, 3);
        }
        else if (cmdStr == "set_mode") {
            String mode = doc["mode"] | "";
            uint8_t modeCmd = 0;
            
            if (mode == "on" || mode == "ready") {
                // Validate machine state before allowing turn on
                // Only allow turning on from IDLE, READY, or ECO states
                uint8_t currentState = runtimeState().get().machine_state;
                if (currentState != UI_STATE_IDLE && 
                    currentState != UI_STATE_READY && 
                    currentState != UI_STATE_ECO) {
                    const char* stateNames[] = {"INIT", "IDLE", "HEATING", "READY", "BREWING", "FAULT", "SAFE", "ECO"};
                    LOG_W("MQTT: Cannot turn on machine: current state is %s. Machine must be in IDLE, READY, or ECO state.",
                          (currentState < 8) ? stateNames[currentState] : "UNKNOWN");
                    return;
                }
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
                ui_state_t& state = runtimeState().beginUpdate();
                state.target_weight = weight;
                runtimeState().endUpdate();
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
    
    // Initialize State Manager (schedules, settings persistence)
    Serial.println("[8/8] Initializing State Manager...");
    Serial.print("Free heap before State: ");
    Serial.println(ESP.getFreeHeap());
    // Serial.flush(); // Removed - can block on USB CDC
    State.begin();
    Serial.print("Free heap after State: ");
    Serial.println(ESP.getFreeHeap());
    Serial.println("State Manager initialized OK");
    // Serial.flush(); // Removed - can block on USB CDC
    
    // Sync MQTT settings from State Manager to MQTT client before initialization
    // This ensures the MQTT client respects the enabled/disabled setting from State Manager
    // IMPORTANT: This must happen after State.begin() so settings are loaded
    if (mqttClient) {
        const auto& mqttSettings = State.settings().mqtt;
        MQTTConfig mqttConfig;
        mqttConfig.enabled = mqttSettings.enabled;
        strncpy(mqttConfig.broker, mqttSettings.broker, sizeof(mqttConfig.broker) - 1);
        mqttConfig.broker[sizeof(mqttConfig.broker) - 1] = '\0';
        mqttConfig.port = mqttSettings.port;
        strncpy(mqttConfig.username, mqttSettings.username, sizeof(mqttConfig.username) - 1);
        mqttConfig.username[sizeof(mqttConfig.username) - 1] = '\0';
        strncpy(mqttConfig.password, mqttSettings.password, sizeof(mqttConfig.password) - 1);
        mqttConfig.password[sizeof(mqttConfig.password) - 1] = '\0';
        strncpy(mqttConfig.topic_prefix, mqttSettings.baseTopic, sizeof(mqttConfig.topic_prefix) - 1);
        mqttConfig.topic_prefix[sizeof(mqttConfig.topic_prefix) - 1] = '\0';
        mqttConfig.ha_discovery = mqttSettings.discovery;
        // client_id and ha_device_id will be auto-generated if empty
        mqttConfig.use_tls = false; // Not in MQTTSettings, use default
        
        if (mqttClient->setConfig(mqttConfig)) {
            LOG_I("MQTT config synced from State Manager: enabled=%d", mqttConfig.enabled);
        } else {
            LOG_W("Failed to sync MQTT config from State Manager");
        }
    }
    
    // Initialize MQTT (after syncing config from State Manager)
    Serial.println("[7/8] Initializing MQTT...");
    // Serial.flush(); // Removed - can block on USB CDC
    mqttClient->begin();
    Serial.println("MQTT initialized OK");
    // Serial.flush(); // Removed - can block on USB CDC
}

static void setupInitializeScaleAndBBW() {
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
    // Serial.flush(); // Removed - can block on USB CDC
    {
        ui_state_t& defaultState = runtimeState().beginUpdate();
        defaultState.brew_setpoint = 93.0f;
        defaultState.steam_setpoint = 145.0f;
        defaultState.target_weight = brewByWeight->getTargetWeight();
        defaultState.dose_weight = brewByWeight->getDoseWeight();
        defaultState.brew_max_temp = 105.0f;
        defaultState.steam_max_temp = 160.0f;
        defaultState.dose_weight = 18.0f;
        runtimeState().endUpdate();
    }
    LOG_I("Default values set");
    // Serial.flush(); // Removed - can block on USB CDC
}

static void setupInitializeStateManager() {
    // Initialize State Manager (schedules, settings persistence)
    Serial.println("[8/8] Initializing State Manager...");
    Serial.print("Free heap before State: ");
    Serial.println(ESP.getFreeHeap());
    // Serial.flush(); // Removed - can block on USB CDC
    State.begin();
    Serial.print("Free heap after State: ");
    Serial.println(ESP.getFreeHeap());
    Serial.println("State Manager initialized OK");
    // Serial.flush(); // Removed - can block on USB CDC
    
    // Apply debug log level setting early (so boot logs are included)
    if (State.settings().system.debugLogsEnabled) {
        setLogLevel(BREWOS_LOG_DEBUG);
        Serial.println("[Log] Debug logs enabled (from settings)");
    } else {
        setLogLevel(BREWOS_LOG_INFO);  // Ensure INFO level (default)
    }
    
    // Apply display settings from State
    const auto& displaySettings = State.settings().display;
    display.setBacklight(displaySettings.brightness);
    LOG_I("Display settings applied: brightness=%d, timeout=%ds", 
          displaySettings.brightness, displaySettings.screenTimeout);
}

static void setupInitializeLogManager() {
    // Enable Log Manager early to capture all boot logs
    // This is done right after filesystem initialization so we can restore from flash
    // and recover crash logs from RTC memory
    Serial.println("[Early] Initializing Log Manager to capture boot logs...");
    if (LogManager::instance().enable()) {
        Serial.print("Free heap after LogManager: ");
        Serial.println(ESP.getFreeHeap());
        Serial.println("Log Manager enabled - capturing all boot logs");
    } else {
        Serial.println("WARNING: Log Manager initialization failed");
    }
}

static void setupInitializeLogManagerSettings() {
    // Configure Log Manager settings after State is loaded
    // This configures Pico log forwarding based on user settings
    if (!LogManager::instance().isEnabled()) {
        // Log Manager wasn't enabled early (shouldn't happen, but handle gracefully)
        Serial.println("Log Manager not enabled - skipping settings configuration");
        return;
    }
    
    // Configure Pico log forwarding if enabled in settings
    if (State.settings().system.picoLogForwardingEnabled && picoUart) {
        // Wait a bit for Pico to be ready (if connected)
        delay(100);
        LogManager::instance().setPicoLogForwarding(true, [](uint8_t* payload, size_t len) {
            if (picoUart) {
                return picoUart->sendCommand(MSG_CMD_LOG_CONFIG, payload, len);
            }
            return false;
        });
        Serial.println("Pico log forwarding enabled from settings");
    } else {
        Serial.println("Pico log forwarding disabled in settings");
    }
}

static void setupInitializeCloudConnection() {
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
        
        // Initialize Cloud Connection for real-time state relay
        // Uses pairing manager's device key for secure authentication
        LOG_I("Initializing Cloud Connection...");
        cloudConnection->begin(
            String(cloudSettings.serverUrl),
            deviceId,
            deviceKey
        );
        
        // Set up registration callback - called when WiFi is connected before first connection
        // Uses static function to avoid PSRAM issues
        cloudConnection->onRegister([]() -> bool {
            return pairingManager->registerTokenWithCloud();
        });
        
        // Set up regenerate key callback - called when authentication fails
        // Regenerates device key and reinitializes connection
        cloudConnection->onRegenerateKey([]() -> bool {
            Serial.println("[Cloud] Regenerating device key due to auth failure...");
            if (pairingManager->regenerateDeviceKey()) {
                // Reload the new key into cloud connection
                String newKey = pairingManager->getDeviceKey();
                String newDeviceId = pairingManager->getDeviceId();
                auto& cs = State.settings().cloud;
                
                // Disconnect and reinitialize with new key
                cloudConnection->end();
                cloudConnection->begin(
                    String(cs.serverUrl),
                    newDeviceId,
                    newKey
                );
                
                // Re-setup all callbacks (begin() clears them)
                cloudConnection->onRegister([]() -> bool {
                    return pairingManager->registerTokenWithCloud();
                });
                cloudConnection->onRegenerateKey([]() -> bool {
                    // Retry regeneration (up to 3 times total)
                    Serial.println("[Cloud] Regenerating device key (retry 2/3)...");
                    if (pairingManager->regenerateDeviceKey()) {
                        String newKey = pairingManager->getDeviceKey();
                        String newDeviceId = pairingManager->getDeviceId();
                        auto& cs = State.settings().cloud;
                        cloudConnection->end();
                        cloudConnection->begin(String(cs.serverUrl), newDeviceId, newKey);
                        cloudConnection->onRegister([]() -> bool {
                            return pairingManager->registerTokenWithCloud();
                        });
                        cloudConnection->onRegenerateKey([]() -> bool {
                            // Final attempt - just regenerate (prevents infinite recursion)
                            Serial.println("[Cloud] Regenerating device key (final attempt)...");
                            return pairingManager->regenerateDeviceKey();
                        });
                        cloudConnection->onCommand(onCloudCommand);
                        return true;
                    }
                    return false;
                });
                cloudConnection->onCommand(onCloudCommand);
                
                Serial.println("[Cloud] Device key regenerated and connection reinitialized");
                return true;
            }
            Serial.println("[Cloud] Failed to regenerate device key");
            return false;
        });
        
        // Set up command handler using static function to avoid PSRAM issues
        cloudConnection->onCommand(onCloudCommand);
        
        // Connect cloud to WebServer for state broadcasting
        webServer->setCloudConnection(cloudConnection);
    }
    
    // Set up cloud screen callback for QR code generation
    // This is set regardless of whether cloud is enabled - the callback
    // will show appropriate error if cloud is not configured
    screen_cloud_set_refresh_callback([]() {
        auto& cloudSettings = State.settings().cloud;
        
        if (!cloudSettings.enabled || strlen(cloudSettings.serverUrl) == 0) {
            screen_cloud_show_error("Cloud not configured");
            return;
        }
        
        if (pairingManager) {
            // Generate new token and register with cloud
            pairingManager->generateToken();
            bool registered = pairingManager->registerTokenWithCloud();
            
            if (registered || pairingManager->isTokenValid()) {
                // Update cloud screen with real pairing data
                String deviceId = pairingManager->getDeviceId();
                String token = pairingManager->getCurrentToken();
                String url = pairingManager->getPairingUrl();
                uint32_t expiresIn = (pairingManager->getTokenExpiry() - millis()) / 1000;
                
                screen_cloud_update(deviceId.c_str(), token.c_str(), 
                                   url.c_str(), expiresIn);
            } else {
                screen_cloud_show_error("Cloud not connected");
            }
        } else {
            screen_cloud_show_error("Cloud not initialized");
        }
    });
}

static void setupInitializeNotificationManager() {
    // Initialize Notification Manager
    Serial.println("[8.5/8] Initializing Notification Manager...");
    // Serial.flush(); // Removed - can block on USB CDC
    notificationManager->begin();
    Serial.println("Notification Manager initialized OK");
    // Serial.flush(); // Removed - can block on USB CDC
    
    // Set up cloud notification callback using static function to avoid PSRAM issues
    notificationManager->onCloud(onCloudNotification);
    
    // Set up schedule callback using static function to avoid PSRAM issues
    State.onScheduleTriggered(onScheduleTriggered);
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
    
    loopCoreSystemUpdates();
    loopOptionalServiceUpdates();
    loopUpdateConnectionStates();
    loopHandlePicoBootInfo();
    loopUpdateBrewByWeight();
    State.loop();
    loopUpdateUI();
    loopUpdateMQTTStatus();
    loopPeriodicTasks();
    loopHandleWiFiTasks();
    loopMonitorMemoryAndTiming();
    
    // =========================================================================
    // PHASE 9: Loop throttling - Give network stack CPU time
    // =========================================================================
    // Yield to background tasks (WiFi, AsyncTCP)
    // 2ms is sufficient now that EMI is fixed via GPIO drive strength
    delay(2);
}

static void loopCoreSystemUpdates() {
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
}

static void loopOptionalServiceUpdates() {
    // =========================================================================
    // PHASE 3: Optional service updates
    // =========================================================================
    
    // Cloud connection runs in its own FreeRTOS task (Core 1, low priority)
    // No explicit loop() call needed - task handles SSL independently
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
    
    // Log Manager - periodic auto-save to flash
    if (g_logManager && g_logManager->isEnabled()) {
        g_logManager->loop();
    }
    yield();  // Feed watchdog
}

static void loopUpdateConnectionStates() {
    // =========================================================================
    // PHASE 4: State synchronization
    // Update machine state from various sources
    // =========================================================================
    
    // Connection states (defensive - default to false if object missing)
    bool picoConnected = picoUart->isConnected();
    ui_state_t& stateRef = runtimeState().beginUpdate();
    stateRef.pico_connected = picoConnected;
    stateRef.mqtt_connected = mqttClient ? mqttClient->isConnected() : false;
    stateRef.scale_connected = (scaleEnabled && scaleManager) ? scaleManager->isConnected() : false;
    stateRef.cloud_connected = cloudConnection ? cloudConnection->isConnected() : false;
    runtimeState().endUpdate();
}

static void loopHandlePicoBootInfo() {
    // =========================================================================
    // PHASE 5: Pico connection status
    // =========================================================================
    
    // If Pico is connected but machine type or version is unknown, request boot info
    // This handles the case where MSG_BOOT was missed (e.g., ESP32 rebooted while Pico was running)
    bool picoConnected = picoUart->isConnected();
    if (picoConnected) {
        static unsigned long lastBootInfoRequest = 0;
        static uint8_t bootInfoRequestCount = 0;
        const uint8_t MAX_BOOT_INFO_REQUESTS = 12;  // Try for 1 minute (12 * 5s = 60s)
        
        const char* picoVersion = State.getPicoVersion();
        bool picoVersionUnknown = !picoVersion || picoVersion[0] == '\0';
        bool needsBootInfo = (State.getMachineType() == 0 || picoVersionUnknown);
        
        if (needsBootInfo && (millis() - lastBootInfoRequest > 5000)) {
            if (bootInfoRequestCount < MAX_BOOT_INFO_REQUESTS) {
                bootInfoRequestCount++;
                lastBootInfoRequest = millis();
                if (State.getMachineType() == 0 && picoVersionUnknown) {
                    LOG_W("Pico connected but machine type and version unknown - requesting boot info (%u/%u)...", 
                          bootInfoRequestCount, MAX_BOOT_INFO_REQUESTS);
                } else if (State.getMachineType() == 0) {
                    LOG_W("Pico connected but machine type unknown - requesting boot info (%u/%u)...", 
                          bootInfoRequestCount, MAX_BOOT_INFO_REQUESTS);
                } else {
                    LOG_W("Pico connected but version unknown - requesting boot info (%u/%u)...", 
                          bootInfoRequestCount, MAX_BOOT_INFO_REQUESTS);
                }
                if (picoUart->requestBootInfo()) {
                    delay(100);
                    picoUart->loop();
                    // Re-check after processing
                    picoVersion = State.getPicoVersion();
                    picoVersionUnknown = !picoVersion || picoVersion[0] == '\0';
                    if (State.getMachineType() != 0 && !picoVersionUnknown) {
                        LOG_I("Boot info received: machine type=%d, version=%s", 
                              State.getMachineType(), State.getPicoVersion());
                        bootInfoRequestCount = 0;  // Reset on success
                    }
                }
            } else if (bootInfoRequestCount == MAX_BOOT_INFO_REQUESTS) {
                // Only log once when we give up
                bootInfoRequestCount++;  // Increment to prevent repeated warnings
                if (State.getMachineType() == 0 && picoVersionUnknown) {
                    LOG_E("Failed to get machine type and version from Pico after %u attempts. Giving up.", MAX_BOOT_INFO_REQUESTS);
                    LOG_W("Machine type and version will remain unknown. Some features may be limited.");
                } else if (State.getMachineType() == 0) {
                    LOG_E("Failed to get machine type from Pico after %u attempts. Giving up.", MAX_BOOT_INFO_REQUESTS);
                    LOG_W("Machine type will remain unknown. Some features may be limited.");
                } else {
                    LOG_E("Failed to get pico version after %u attempts. Giving up.", MAX_BOOT_INFO_REQUESTS);
                    LOG_W("Pico version will remain unknown.");
                }
            }
        } else if (!needsBootInfo && bootInfoRequestCount > 0) {
            // Boot info was received (maybe from a spontaneous MSG_BOOT), reset counter
            bootInfoRequestCount = 0;
        }
    }
}

static void loopUpdateBrewByWeight() {
    // =========================================================================
    // PHASE 6: Brew-by-Weight (only when actively brewing with scale)
    // This is DISABLED when:
    // - Scale not enabled or not connected
    // - Not brewing
    // =========================================================================
    
    // Only process BBW if we're actually brewing with a connected scale
    // This prevents any callbacks from firing when not needed
    const ui_state_t& state = runtimeState().get();
    bool shouldUpdateBBW = brewByWeight && 
                           scaleEnabled && 
                           scaleManager && 
                           scaleManager->isConnected() &&
                           state.is_brewing;
    
    // Update BBW with current brewing state and scale weight
    if (shouldUpdateBBW) {
        brewByWeight->update(
            state.is_brewing,
            scaleManager->getState().weight,
            true
        );
    }
    
    // Sync BBW state to machine state (with null check)
    if (brewByWeight && brewByWeight->isActive()) {
        ui_state_t& state = runtimeState().beginUpdate();
        state.brew_weight = brewByWeight->getState().current_weight;
        state.target_weight = brewByWeight->getTargetWeight();
        state.dose_weight = brewByWeight->getDoseWeight();
        runtimeState().endUpdate();
    }
}

static void loopUpdateUI() {
    // Update display and encoder
    unsigned long now = millis();
    unsigned long uiUpdateInterval = 100; // 10 FPS - good balance of responsiveness and CPU usage
    
    // Update encoder state FAST (every loop) to ensure responsiveness
    // Do not throttle input polling!
    encoder.update();
    
    // Check if encoder activity triggered immediate refresh
    bool needsImmediateRefresh = encoderActivityFlag;
    encoderActivityFlag = false;  // Clear flag
    
    // Update UI at regular intervals
    if (needsImmediateRefresh || (now - lastUIUpdate >= uiUpdateInterval)) {
        lastUIUpdate = now;
        
        if (webServer && webServer->isOtaInProgress()) {
            static screen_id_t lastScreenBeforeOta = SCREEN_HOME;
            if (ui.getCurrentScreen() != SCREEN_OTA) {
                lastScreenBeforeOta = ui.getCurrentScreen();
                ui.showScreen(SCREEN_OTA);
                screen_ota_set("Update in progress...");
            }
        } else {
            ui.update(runtimeState().get());
        }
        
        display.update();
    }
    
    static bool lastPressed = false;
    bool currentPressed = encoder.isPressed();
    const ui_state_t& state = runtimeState().get();
    if (state.is_brewing || (currentPressed && !lastPressed)) {
        State.resetIdleTimer();
    }
    lastPressed = currentPressed;
}

static void loopUpdateMQTTStatus() {
    // Publish MQTT status with delta updates for efficiency
    // Full status on major changes or periodic sync, delta for incremental changes
    static StatusChangeDetector mqttChangeDetector;
    static unsigned long lastMQTTHeartbeat = 0;
    static unsigned long lastMQTTFullStatus = 0;
    static bool lastMQTTConnected = false;
    const unsigned long MQTT_HEARTBEAT_INTERVAL = 30000;  // 30 seconds minimum heartbeat
    const unsigned long MQTT_FULL_STATUS_INTERVAL = 300000;  // 5 minutes for full status sync
    
    // Reset detector when MQTT connection is established (ensures first update is sent)
    bool mqttConnected = mqttClient && mqttClient->isConnected();
    if (mqttConnected && !lastMQTTConnected) {
        mqttChangeDetector.reset();
    }
    lastMQTTConnected = mqttConnected;
    
    if (mqttConnected) {
        const ui_state_t& state = runtimeState().get();
        bool hasChanged = mqttChangeDetector.hasChanged(state);
        ChangedFields changedFields;
        if (hasChanged) {
            changedFields = mqttChangeDetector.getChangedFields(state);
        }
        
        unsigned long now = millis();
        bool heartbeatDue = (now - lastMQTTHeartbeat >= MQTT_HEARTBEAT_INTERVAL);
        bool fullStatusDue = (now - lastMQTTFullStatus >= MQTT_FULL_STATUS_INTERVAL);
        
        // Send full status on: major state change, periodic sync, or heartbeat
        bool majorStateChange = hasChanged && changedFields.machine_state;
        bool sendFullStatus = majorStateChange || fullStatusDue || heartbeatDue;
        
        if (hasChanged || heartbeatDue) {
            if (sendFullStatus) {
                mqttClient->publishStatus(state);
                if (fullStatusDue) {
                    lastMQTTFullStatus = now;
                }
            } else {
                // Send delta update for incremental changes
                mqttClient->publishStatusDelta(state, changedFields);
            }
            
            if (heartbeatDue) {
                lastMQTTHeartbeat = now;
            }
        }
    }
}

static void loopPeriodicTasks() {
    // Periodic ping to Pico for connection monitoring
    if (millis() - lastPing > 5000) {
        lastPing = millis();
        if (picoUart->isConnected() || picoUart->getPacketsReceived() == 0) {
            picoUart->sendPing();
        }
    }
    
    // Monitor protocol health periodically (every 60 seconds)
    static unsigned long lastProtocolHealthCheck = 0;
    if (millis() - lastProtocolHealthCheck > 60000) {
        lastProtocolHealthCheck = millis();
        
        // Check protocol error rates
        uint32_t totalPackets = picoUart->getPacketsReceived();
        uint32_t totalErrors = picoUart->getPacketErrors();
        
        if (totalPackets > 0) {
            float errorRate = (float)totalErrors / (float)totalPackets * 100.0f;
            
            // Log protocol statistics
            LOG_D("Protocol: %u pkts, %u errors (%.1f%%)", 
                  totalPackets, totalErrors, errorRate);
            
            // Warn if error rate is high
            if (errorRate > 5.0f && totalPackets > 100) {
                LOG_W("High protocol error rate (%.1f%%) - check wiring/EMI", errorRate);
            }
        }
    }
    
    // Periodic unified status broadcast to WebSocket clients (500ms for responsive UI)
    // Only sends when something changes - WebSocket ping/pong handles keepalive
    if (millis() - lastStatusBroadcast > 500) {
        lastStatusBroadcast = millis();
        
        // Broadcast if we have local clients OR cloud connection
        if (webServer->getClientCount() > 0 || cloudConnection->isConnected()) {
            // Update connection status in machineState
            ui_state_t& state = runtimeState().beginUpdate();
            state.pico_connected = picoUart->isConnected();
            state.wifi_connected = wifiManager->isConnected();
            state.mqtt_connected = mqttClient->isConnected();
            state.cloud_connected = cloudConnection->isConnected();
            runtimeState().endUpdate();
            
            // Broadcast unified status (goes to both local and cloud clients)
            // This only sends when something changed, on first connect, or periodic sync
            webServer->broadcastFullStatus(runtimeState().get());
        }
    }
    
    // Periodic WebSocket ping for keepalive (every 3 seconds)
    // This keeps connections alive when device is idle and nothing changes
    static unsigned long lastWsPing = 0;
    if (millis() - lastWsPing > 3000) {
        lastWsPing = millis();
        if (webServer->getClientCount() > 0) {
            webServer->sendPingToClients();
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
}

static void loopHandleWiFiTasks() {
    const ui_state_t& state = runtimeState().get();
    
    // Handle WiFi connected tasks
    if (state.wifi_connected && wifiConnectedTime == 0) {
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
    // Start mDNS immediately when WiFi connects - no delay needed
    // Web server is already running, mDNS just makes it discoverable
    if (wifiConnectedTime > 0 && !mDNSStarted) {
        // Force restart of mDNS to ensure clean state
        MDNS.end();
        
        if (MDNS.begin("brewos")) {
            LOG_I("mDNS started: http://brewos.local");
            // Add service and check result
            if (MDNS.addService("http", "tcp", 80)) {
                LOG_I("mDNS service added");
                mDNSStarted = true;
            } else {
                LOG_E("mDNS addService failed - will retry");
                // Retry next loop
            }
        } else {
            LOG_W("mDNS failed to start - will retry");
            // Retry on next loop iteration (don't set mDNSStarted = true on failure)
        }
    }
    if (!state.wifi_connected) {
        // Reset when WiFi disconnects
        wifiConnectedTime = 0;
        wifiConnectedLogSent = false;
        mDNSStarted = false;
        ntpConfigured = false;
    }
    
    // Periodically ensure WiFi power save is disabled (every 30s when connected)
    // Some ESP32 SDKs re-enable power save after certain events
    static unsigned long lastPowerSaveCheck = 0;
    if (state.wifi_connected && millis() - lastPowerSaveCheck > 30000) {
        lastPowerSaveCheck = millis();
        wifi_ps_type_t ps_type;
        esp_wifi_get_ps(&ps_type);
        if (ps_type != WIFI_PS_NONE) {
            LOG_W("WiFi power save was re-enabled! Disabling...");
            esp_wifi_set_ps(WIFI_PS_NONE);
        }
    }
}

static void loopMonitorMemoryAndTiming() {
    // =========================================================================
    // PHASE 8: Memory and loop timing monitoring
    // =========================================================================
    static unsigned long lastMemoryLog = 0;
    static unsigned long loopStartTime = 0;
    static unsigned long maxLoopTime = 0;
    static unsigned long slowLoopCount = 0;
    
    // Track loop timing - detect blocking operations
    // This is diagnostic only - if loop takes >1 second, hardware watchdog (3-5s timeout)
    // may reset the chip before we can log the warning
    // All blocking operations should use yield() or vTaskDelay() to feed watchdog
    unsigned long loopTime = millis() - loopStartTime;
    if (loopStartTime > 0 && loopTime > 100) {  // Loop took > 100ms
        slowLoopCount++;
        if (loopTime > maxLoopTime) {
            maxLoopTime = loopTime;
        }
        if (loopTime > 1000) {  // Loop took > 1 second - something is very wrong
            // WARNING: If loop takes >1 second, hardware watchdog (3-5s) may reset chip
            // This log may not appear if watchdog resets before Serial output completes
            LOG_E("SLOW LOOP: %lu ms (this blocks network and may trigger watchdog reset!)", loopTime);
        }
    }
    loopStartTime = millis();
    
    if (millis() - lastMemoryLog >= 30000) {  // Log every 30 seconds
        lastMemoryLog = millis();
        size_t freeHeap = ESP.getFreeHeap();
        size_t minFreeHeap = ESP.getMinFreeHeap();
        size_t freePsram = ESP.getFreePsram();
        size_t totalPsram = ESP.getPsramSize();
        
        // Get heap fragmentation metric using memory_utils.h helpers
        size_t largestBlock = get_largest_free_block();
        int fragmentation = calculate_fragmentation(freeHeap, largestBlock);
        
        // Log both internal heap (critical for SSL) and PSRAM (for large buffers)
        // Fragmentation metric: 0% = perfect, 100% = completely fragmented
        LOG_I("Memory: heap=%zu/%zu (frag=%d%%, largest=%zu), PSRAM=%zuKB/%zuKB, maxLoop=%lums, slowLoops=%lu", 
              freeHeap, minFreeHeap, fragmentation, largestBlock, 
              freePsram/1024, totalPsram/1024, maxLoopTime, slowLoopCount);
        
        // Reset stats
        maxLoopTime = 0;
        slowLoopCount = 0;
        
        // Warn if internal heap is dangerously low
        if (freeHeap < 10000) {
            LOG_W("Low internal heap: %zu bytes", freeHeap);
        }
        
        // Warn if heap is highly fragmented (can't allocate SSL buffers even with "enough" free heap)
        // SSL handshake typically needs ~16KB contiguous block
        if (largestBlock < 20000 && freeHeap > 30000) {
            LOG_W("Heap fragmentation: %d%% (largest block=%zu, need 20KB for SSL)", 
                  fragmentation, largestBlock);
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
    
    // Begin update transaction - takes mutex and returns reference to writing buffer
    // The writing buffer is already initialized with current state (copied in beginUpdate)
    ui_state_t& state = runtimeState().beginUpdate();
    
    // Parse temperatures (int16 scaled by 10 -> float)
    int16_t brew_temp_raw, steam_temp_raw, group_temp_raw;
    memcpy(&brew_temp_raw, &payload[0], sizeof(int16_t));
    memcpy(&steam_temp_raw, &payload[2], sizeof(int16_t));
    memcpy(&group_temp_raw, &payload[4], sizeof(int16_t));
    
    state.brew_temp = brew_temp_raw / 10.0f;
    state.steam_temp = steam_temp_raw / 10.0f;
    state.group_temp = group_temp_raw / 10.0f;
    
    // Parse pressure (uint16 scaled by 100 -> float)
    uint16_t pressure_raw;
    memcpy(&pressure_raw, &payload[6], sizeof(uint16_t));
    state.pressure = pressure_raw / 100.0f;
    
    // Parse setpoints (int16 scaled by 10 -> float)
    int16_t brew_sp_raw, steam_sp_raw;
    memcpy(&brew_sp_raw, &payload[8], sizeof(int16_t));
    memcpy(&steam_sp_raw, &payload[10], sizeof(int16_t));
    state.brew_setpoint = brew_sp_raw / 10.0f;
    state.steam_setpoint = steam_sp_raw / 10.0f;
    
    // Parse state and flags
    state.machine_state = payload[15];
    uint8_t flags = payload[16];
    
    state.is_brewing = (flags & 0x01) != 0;
    state.is_heating = (flags & 0x02) != 0;
    state.water_low = (flags & 0x08) != 0;
    // Only update alarm_active from status if we have a valid alarm code
    // MSG_ALARM messages are the source of truth for alarm state
    // Status packet flag is just a hint - don't set alarm_active without alarm_code
    bool statusAlarmFlag = (flags & 0x10) != 0;
    if (state.alarm_code != ALARM_NONE) {
        // We have a real alarm code - status flag should match
        state.alarm_active = statusAlarmFlag;
    } else {
        // No alarm code - status flag is unreliable, keep alarm_active false
        state.alarm_active = false;
    }
    
    // Parse power watts (offset 18-19, if available)
    if (length >= 20) {
        uint16_t power_raw;
        memcpy(&power_raw, &payload[18], sizeof(uint16_t));
        state.power_watts = power_raw;
    }
    
    // Parse heating strategy (offset 28, if available)
    if (length >= 30) {
        state.heating_strategy = payload[28];
    }
    
    // Parse cleaning status (offsets 29-31, if available)
    if (length >= 32) {
        state.cleaning_reminder = payload[29] != 0;
        uint16_t brew_count_raw;
        memcpy(&brew_count_raw, &payload[30], sizeof(uint16_t));
        state.brew_count = brew_count_raw;
    }
    
    // End update transaction - swaps buffers atomically and releases mutex
    runtimeState().endUpdate();
    
    // Auto-switch screens is now handled by UI::checkAutoScreenSwitch()
}

/**
 * Handle encoder rotation and button events
 */
void handleEncoderEvent(int32_t diff, button_state_t btn) {
    // Track when we last woke up the display - ignore button presses shortly after wake
    // This prevents accidental actions when user presses button to wake screen
    static unsigned long lastWakeTime = 0;
    const unsigned long WAKE_IGNORE_PERIOD = 500;  // Ignore button presses for 500ms after wake
    
    // Check if display is fully OFF (not just dimmed) - if so, wake it up without triggering action
    // Dimmed screen (30s idle) should still respond to input immediately
    // Only fully OFF screen (60s idle) should ignore the first input
    bool wasOff = display.getBacklight() == 0;
    
    if (wasOff) {
        // Wake up the display from full sleep
        display.resetIdleTimer();
        encoderActivityFlag = true;
        lastWakeTime = millis();  // Record when we woke up
        
        // Don't trigger any button/encoder actions when waking from full sleep
        // The user just wants to see the screen, not interact with it yet
        if (btn != BTN_RELEASED || diff != 0) {
            LOG_I("Display woken from sleep - ignoring input");
            return;
        }
    } else if (display.isDimmed()) {
        // Just dimmed - wake it up but still process the input
        display.resetIdleTimer();
        encoderActivityFlag = true;
    }
    
    // Ignore button presses shortly after waking (catches button release events)
    // This handles the case where the display was woken by another event
    // but the button was pressed while it was still asleep
    if (btn != BTN_RELEASED && lastWakeTime > 0 && (millis() - lastWakeTime) < WAKE_IGNORE_PERIOD) {
        LOG_I("Ignoring button press shortly after wake (%lums)", millis() - lastWakeTime);
        return;
    }
    
    // Clear wake time after the ignore period
    if (lastWakeTime > 0 && (millis() - lastWakeTime) >= WAKE_IGNORE_PERIOD) {
        lastWakeTime = 0;
    }
    
    // Normal handling when display is already awake
    if (diff != 0) {
        LOG_I("Encoder rotate: %+d (screen=%d)", diff, (int)ui.getCurrentScreen());
        
        // Process each step individually to prevent lost steps when diff > 1
        // This can happen when main loop is blocked (e.g., during SSL handshake)
        int direction = (diff > 0) ? 1 : -1;
        int steps = (diff > 0) ? diff : -diff;
        for (int i = 0; i < steps; i++) {
            ui.handleEncoder(direction);
        }
        encoderActivityFlag = true;  // Trigger immediate UI refresh
        display.resetIdleTimer();    // Reset idle timer on activity
    }
    
    if (btn == BTN_PRESSED) {
        LOG_I("Encoder button: PRESS (screen=%d)", (int)ui.getCurrentScreen());
        ui.handleButtonPress();
        encoderActivityFlag = true;  // Trigger immediate UI refresh
        display.resetIdleTimer();    // Reset idle timer on activity
    } else if (btn == BTN_LONG_PRESSED) {
        LOG_I("Encoder button: LONG_PRESS (screen=%d)", (int)ui.getCurrentScreen());
        ui.handleLongPress();
        encoderActivityFlag = true;
        display.resetIdleTimer();
    } else if (btn == BTN_DOUBLE_PRESSED) {
        LOG_I("Encoder button: DOUBLE_PRESS (screen=%d)", (int)ui.getCurrentScreen());
        ui.handleDoublePress();
        encoderActivityFlag = true;
        display.resetIdleTimer();
    }
    
    // Notify cloud connection of user activity to defer blocking SSL operations
    // This keeps the UI responsive when user is interacting
    if (cloudConnection && (diff != 0 || btn != BTN_RELEASED)) {
        cloudConnection->notifyUserActivity();
    }
}
