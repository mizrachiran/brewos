#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
// Using ESPAsyncWebServer's built-in AsyncWebSocket (same port 80)
#include <ArduinoJson.h>
#include "wifi_manager.h"
#include "ui/ui.h"

// Forward declarations
class PicoUART;
class MQTTClient;
class PairingManager;
class CloudConnection;

class WebServer {
public:
    WebServer(WiFiManager& wifiManager, PicoUART& picoUart, MQTTClient& mqttClient, PairingManager* pairingManager = nullptr);
    
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
    void broadcastFullStatus(const ui_state_t& machineState);  // Comprehensive status (periodic)
    void broadcastDeviceInfo();   // Device info (on connect only)
    void broadcastPowerMeterStatus();  // Power meter status update
    void broadcastBBWSettings();  // BBW settings (after save)
    // Log messages - variadic format string (like printf) to avoid PSRAM
    // Usage: broadcastLog("Message: %s", value) or broadcastLogLevel("warn", "Message: %s", value)
    void broadcastLog(const char* format, ...) __attribute__((format(printf, 2, 3)));  // Variadic: defaults to "info"
    void broadcastLogLevel(const char* level, const char* format, ...) __attribute__((format(printf, 3, 4)));  // Level first, then format
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
    
    // Helpers
    String getContentType(const String& filename);
    bool streamFirmwareToPico(File& firmwareFile, size_t firmwareSize);
    
    // GitHub OTA - Download and install firmware from GitHub releases
    void startGitHubOTA(const String& version);        // ESP32 only
    bool startPicoGitHubOTA(const String& version);    // Pico only (based on machine type), returns true on success
    void startCombinedOTA(const String& version);      // Pico first, then ESP32
    void updateLittleFS(const char* tag);              // Update web UI filesystem
    
    // Check for updates from GitHub releases
    void checkForUpdates();
    
    // Get Pico firmware asset name based on machine type
    const char* getPicoAssetName(uint8_t machineType);
    
    // Version mismatch check
    bool checkVersionMismatch();
    
    // OTA in progress flag - suppresses non-essential broadcasts
    bool _otaInProgress = false;
};

#endif // WEB_SERVER_H

