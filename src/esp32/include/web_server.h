#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
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
    
    // Send data to all WebSocket clients - Unified Status Broadcast
    void broadcastFullStatus(const ui_state_t& machineState);  // Comprehensive status (periodic)
    void broadcastDeviceInfo();   // Device info (on connect only)
    void broadcastPowerMeterStatus();  // Power meter status update
    void broadcastLog(const String& message, const String& level = "info");  // Log messages
    void broadcastEvent(const String& event, const JsonDocument* data = nullptr);  // Events (shot_start, shot_end, etc.)
    
    // Legacy/debug - raw pico message forwarding
    void broadcastPicoMessage(uint8_t type, const uint8_t* payload, size_t len);
    
    // Broadcast raw JSON string to all WebSocket clients
    void broadcastRaw(const String& json);
    
    // Get client count
    size_t getClientCount();
    
    // Process a command from any source (local WebSocket or cloud)
    // Used by CloudConnection to forward cloud commands to the same handler
    void processCommand(JsonDocument& doc);

private:
    AsyncWebServer _server;
    AsyncWebSocket _ws;
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
    
    // WebSocket handlers
    void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                   AwsEventType type, void* arg, uint8_t* data, size_t len);
    void handleWsMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len);
    
    // Helpers
    String getContentType(const String& filename);
    bool streamFirmwareToPico(File& firmwareFile, size_t firmwareSize);
    
    // GitHub OTA - Download and install firmware from GitHub releases
    void startGitHubOTA(const String& version);
};

#endif // WEB_SERVER_H

