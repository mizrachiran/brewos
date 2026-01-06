#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
// Using ESPAsyncWebServer's built-in AsyncWebSocket (same port 80)
#include <ArduinoJson.h>
#include "wifi_manager.h"
#include "ui/ui.h"
#include "utils/status_change_detector.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// Forward declarations
class PicoUART;
class MQTTClient;
class PairingManager;
class CloudConnection;

// Initialize pre-allocated broadcast buffers in PSRAM
// Call this once during setup() to avoid repeated allocations
void initBroadcastBuffers();

// Pending OTA management (for reboot-first approach when memory is fragmented)
bool hasPendingOTA(String& version);
uint8_t getPendingOTARetries();
uint8_t incrementPendingOTARetries();
void clearPendingOTA();

class BrewWebServer {
public:
    BrewWebServer(WiFiManager& wifiManager, PicoUART& picoUart, MQTTClient& mqttClient, PairingManager* pairingManager = nullptr);
    
    // Initialize
    void begin();
    
    // Update - call in loop
    void loop();
    
    // Set cloud connection for remote state broadcasting
    void setCloudConnection(CloudConnection* cloudConnection);
    
    // Start cloud connection (called when cloud is enabled dynamically)
    void startCloudConnection(const String& serverUrl, const String& deviceId, const String& deviceKey);
    
    // Mark WiFi as connected - starts delay timer before serving requests
    void setWiFiConnected();
    
    // Check if WiFi is ready to serve requests (prevents crashes from early requests)
    bool isWiFiReady();
    
    // Send data to all WebSocket clients - Unified Status Broadcast
    bool buildDeltaStatus(const ui_state_t& state, const ChangedFields& changed, 
                          uint32_t sequence, JsonDocument& doc);  // Build delta status (only changed fields)
    void broadcastFullStatus(const ui_state_t& machineState);  // Comprehensive status (periodic)
    void sendPingToClients();  // Send WebSocket ping frames to all clients for keepalive
    void broadcastDeviceInfo();   // Device info (on connect only)
    void broadcastPowerMeterStatus();  // Power meter status update
    void broadcastBBWSettings();  // BBW settings (after save)
    void broadcastMqttStatus();   // MQTT status (after config change)
    // Log messages - variadic format string (like printf) to avoid PSRAM
    // Usage: broadcastLog("Message: %s", value) or broadcastLogLevel("warn", "Message: %s", value)
    void broadcastLog(const char* format, ...) __attribute__((format(printf, 2, 3)));  // Variadic: defaults to "info"
    void broadcastLogLevel(const char* level, const char* format, ...) __attribute__((format(printf, 3, 4)));  // Level first, then format
    void broadcastLogMessage(const char* level, const char* message);  // Direct message (no formatting) - for platform_log
    void broadcastLogMessageWithSource(const char* level, const char* message, const char* source);  // Direct message with source (for Pico logs)
    void broadcastEvent(const String& event, const JsonDocument* data = nullptr);  // Events (shot_start, shot_end, etc.)
    
    // Legacy/debug - raw pico message forwarding
    void broadcastPicoMessage(uint8_t type, const uint8_t* payload, size_t len);
    
    // Broadcast raw JSON string to all WebSocket clients
    void broadcastRaw(const String& json);
    void broadcastRaw(const char* json);  // const char* overload to avoid String allocation
    
    // Get client count
    size_t getClientCount();
    
    // Check if OTA update is in progress
    bool isOtaInProgress() const { return _otaInProgress; }
    
    // Start combined OTA update (Pico first, then ESP32)
    // Public so it can be called from main.cpp for pending OTA after reboot
    // isPendingOTA: if true, skip memory check (we already rebooted, don't loop)
    void startCombinedOTA(const String& version, bool isPendingOTA = false);
    
    // Process a command from any source (local WebSocket or cloud)
    // Used by CloudConnection to forward cloud commands to the same handler
    void processCommand(JsonDocument& doc);
    
    // WebSocket event handler (public - called from static callback)
    void handleWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len);

private:
    AsyncWebServer _server;
    AsyncWebSocket _ws;  // Built-in WebSocket on same port 80
    WiFiManager& _wifiManager;
    PicoUART& _picoUart;
    MQTTClient& _mqttClient;
    PairingManager* _pairingManager;
    CloudConnection* _cloudConnection = nullptr;
    
    // Request handlers
    void setupRoutes();
    void handleRoot(AsyncWebServerRequest* request);
    void handleGetStatus(AsyncWebServerRequest* request);
    void handleGetWiFiNetworks(AsyncWebServerRequest* request);
    void handleSetWiFi(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleGetConfig(AsyncWebServerRequest* request);
    void handleCommand(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleGetMQTTConfig(AsyncWebServerRequest* request);
    void handleSetMQTTConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleTestMQTT(AsyncWebServerRequest* request);
    void handleOTAUpload(AsyncWebServerRequest* request, const String& filename,
                         size_t index, uint8_t* data, size_t len, bool final);
    void handleStartOTA(AsyncWebServerRequest* request);
    
    // WebSocket message handler (processes JSON commands)
    void handleWsMessage(uint32_t clientNum, uint8_t* payload, size_t length);
    
    // WebSocket event helpers
    void handleWsConnect(AsyncWebSocket* server, AsyncWebSocketClient* client);
    void handleWsDisconnect(AsyncWebSocket* server, AsyncWebSocketClient* client);
    void handleWsError(AsyncWebSocketClient* client, uint8_t* data, size_t len);
    
    // Command handlers - organized by category
    void handleTemperatureCommand(JsonDocument& doc, const String& cmd);
    void handleModeCommand(JsonDocument& doc, const String& cmd);
    void handleMqttCommand(JsonDocument& doc, const String& cmd);
    void handleCloudCommand(JsonDocument& doc, const String& cmd);
    void handleScheduleCommand(JsonDocument& doc, const String& cmd);
    void handleScaleCommand(JsonDocument& doc, const String& cmd);
    void handleBrewByWeightCommand(JsonDocument& doc, const String& cmd);
    void handlePreinfusionCommand(JsonDocument& doc, const String& cmd);
    void handlePowerCommand(JsonDocument& doc, const String& cmd);
    void handlePowerMeterCommand(JsonDocument& doc, const String& cmd);
    void handleWiFiCommand(JsonDocument& doc, const String& cmd);
    void handleSystemCommand(JsonDocument& doc, const String& cmd);
    void handleOtaCommand(JsonDocument& doc, const String& cmd);
    void handleMachineInfoCommand(JsonDocument& doc, const String& cmd);
    void handlePreferencesCommand(JsonDocument& doc, const String& cmd);
    void handleTimeCommand(JsonDocument& doc, const String& cmd);
    void handleMaintenanceCommand(JsonDocument& doc, const String& cmd);
    void handleDiagnosticsCommand(JsonDocument& doc, const String& cmd);
    
    // Helpers
    String getContentType(const String& filename);
    bool streamFirmwareToPico(File& firmwareFile, size_t firmwareSize);
    
    // GitHub OTA - Download and install firmware from GitHub releases
    void startGitHubOTA(const String& version);        // ESP32 only
    bool startPicoGitHubOTA(const String& version);    // Pico only (based on machine type), returns true on success
    void updateLittleFS(const char* tag);              // Update web UI filesystem
    
    // Check for updates from GitHub releases
    void checkForUpdates();
    
    // Get Pico firmware asset name based on machine type
    const char* getPicoAssetName(uint8_t machineType);
    
    // Version mismatch check
    bool checkVersionMismatch();
    
    // OTA in progress flag - suppresses non-essential broadcasts
    bool _otaInProgress = false;
    
    // Deferred cloud state broadcast (when heap is low right after SSL connect)
    bool _pendingCloudStateBroadcast = false;
    unsigned long _pendingCloudStateBroadcastTime = 0;
    
    // OTA task management (for non-blocking OTA operations)
    QueueHandle_t _otaQueue = nullptr;
    TaskHandle_t _otaTaskHandle = nullptr;
    static constexpr int OTA_QUEUE_SIZE = 2;  // Small queue - only one OTA at a time
    
    // OTA command structure
    struct OTACommand {
        enum Type {
            START_PICO_OTA
        } type;
    };
    
    // Static task function for OTA operations
    static void otaTask(void* parameter);
    
    // Process OTA command in background task
    void processOTACommand(const OTACommand& cmd);
    
    // Execute Pico OTA update (called from background task)
    void executePicoOTA();
};

#endif // WEB_SERVER_H

