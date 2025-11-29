#include "web_server.h"
#include "config.h"
#include "pico_uart.h"
#include "mqtt_client.h"
#include <LittleFS.h>

WebServer::WebServer(WiFiManager& wifiManager, PicoUART& picoUart, MQTTClient& mqttClient)
    : _server(WEB_SERVER_PORT)
    , _ws(WEBSOCKET_PATH)
    , _wifiManager(wifiManager)
    , _picoUart(picoUart)
    , _mqttClient(mqttClient) {
}

void WebServer::begin() {
    LOG_I("Starting web server...");
    
    // Initialize LittleFS
    if (!LittleFS.begin(true)) {
        LOG_E("Failed to mount LittleFS");
    } else {
        LOG_I("LittleFS mounted");
    }
    
    // Setup routes
    setupRoutes();
    
    // Setup WebSocket
    _ws.onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                       AwsEventType type, void* arg, uint8_t* data, size_t len) {
        onWsEvent(server, client, type, arg, data, len);
    });
    _server.addHandler(&_ws);
    
    // Start server
    _server.begin();
    LOG_I("Web server started on port %d", WEB_SERVER_PORT);
}

void WebServer::loop() {
    // Cleanup disconnected WebSocket clients
    _ws.cleanupClients();
}

void WebServer::setupRoutes() {
    // Serve static files from LittleFS
    _server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
    
    // API endpoints
    _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetStatus(request);
    });
    
    _server.on("/api/wifi/networks", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetWiFiNetworks(request);
    });
    
    _server.on("/api/wifi/connect", HTTP_POST, 
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            handleSetWiFi(request, data, len);
        }
    );
    
    _server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetConfig(request);
    });
    
    _server.on("/api/command", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            handleCommand(request, data, len);
        }
    );
    
    // OTA upload
    _server.on("/api/ota/upload", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            // Initial response - upload starting
            request->send(200, "application/json", "{\"status\":\"uploading\"}");
        },
        [this](AsyncWebServerRequest* request, const String& filename, size_t index, 
               uint8_t* data, size_t len, bool final) {
            handleOTAUpload(request, filename, index, data, len, final);
        }
    );
    
    _server.on("/api/ota/start", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleStartOTA(request);
    });
    
    _server.on("/api/pico/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
        _picoUart.resetPico();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // MQTT endpoints
    _server.on("/api/mqtt/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetMQTTConfig(request);
    });
    
    _server.on("/api/mqtt/config", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            handleSetMQTTConfig(request, data, len);
        }
    );
    
    _server.on("/api/mqtt/test", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleTestMQTT(request);
    });
    
    // 404 handler
    _server.onNotFound([](AsyncWebServerRequest* request) {
        request->send(404, "text/plain", "Not found");
    });
}

void WebServer::handleGetStatus(AsyncWebServerRequest* request) {
    JsonDocument doc;
    
    // WiFi status
    WiFiStatus wifi = _wifiManager.getStatus();
    doc["wifi"]["mode"] = (int)wifi.mode;
    doc["wifi"]["ssid"] = wifi.ssid;
    doc["wifi"]["ip"] = wifi.ip;
    doc["wifi"]["rssi"] = wifi.rssi;
    doc["wifi"]["configured"] = wifi.configured;
    
    // Pico status
    doc["pico"]["connected"] = _picoUart.isConnected();
    doc["pico"]["packetsReceived"] = _picoUart.getPacketsReceived();
    doc["pico"]["packetErrors"] = _picoUart.getPacketErrors();
    
    // ESP32 status
    doc["esp32"]["uptime"] = millis();
    doc["esp32"]["freeHeap"] = ESP.getFreeHeap();
    doc["esp32"]["version"] = ESP32_VERSION;
    
    // MQTT status
    doc["mqtt"]["enabled"] = _mqttClient.getConfig().enabled;
    doc["mqtt"]["connected"] = _mqttClient.isConnected();
    doc["mqtt"]["status"] = _mqttClient.getStatusString();
    
    // WebSocket clients
    doc["clients"] = getClientCount();
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServer::handleGetWiFiNetworks(AsyncWebServerRequest* request) {
    LOG_I("Scanning WiFi networks...");
    
    int n = WiFi.scanNetworks();
    
    JsonDocument doc;
    JsonArray networks = doc["networks"].to<JsonArray>();
    
    for (int i = 0; i < n; i++) {
        JsonObject network = networks.add<JsonObject>();
        network["ssid"] = WiFi.SSID(i);
        network["rssi"] = WiFi.RSSI(i);
        network["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
    
    WiFi.scanDelete();
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServer::handleSetWiFi(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    
    if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    String ssid = doc["ssid"] | "";
    String password = doc["password"] | "";
    
    if (_wifiManager.setCredentials(ssid, password)) {
        request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Connecting...\"}");
        
        // Connect after sending response
        delay(100);
        _wifiManager.connectToWiFi();
    } else {
        request->send(400, "application/json", "{\"error\":\"Invalid credentials\"}");
    }
}

void WebServer::handleGetConfig(AsyncWebServerRequest* request) {
    // Request config from Pico
    _picoUart.requestConfig();
    
    // Return acknowledgment (actual config will come via WebSocket)
    request->send(200, "application/json", "{\"status\":\"requested\"}");
}

void WebServer::handleCommand(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    
    if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    String cmd = doc["cmd"] | "";
    
    if (cmd == "ping") {
        _picoUart.sendPing();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    } 
    else if (cmd == "getConfig") {
        _picoUart.requestConfig();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    }
    else {
        request->send(400, "application/json", "{\"error\":\"Unknown command\"}");
    }
}

void WebServer::handleOTAUpload(AsyncWebServerRequest* request, const String& filename,
                                size_t index, uint8_t* data, size_t len, bool final) {
    static File otaFile;
    static size_t totalSize = 0;
    static size_t uploadedSize = 0;
    
    if (index == 0) {
        LOG_I("OTA upload started: %s", filename.c_str());
        totalSize = request->contentLength();
        uploadedSize = 0;
        
        // Delete old firmware if exists
        if (LittleFS.exists(OTA_FILE_PATH)) {
            LittleFS.remove(OTA_FILE_PATH);
        }
        
        otaFile = LittleFS.open(OTA_FILE_PATH, "w");
        if (!otaFile) {
            LOG_E("Failed to open OTA file for writing");
            request->send(500, "application/json", "{\"error\":\"Failed to open file\"}");
            return;
        }
    }
    
    if (otaFile && len > 0) {
        size_t written = otaFile.write(data, len);
        if (written != len) {
            LOG_E("Failed to write all data: %d/%d", written, len);
        }
        uploadedSize += written;
        
        // Report progress every 10%
        static size_t lastProgress = 0;
        size_t progress = (uploadedSize * 100) / totalSize;
        if (progress >= lastProgress + 10) {
            lastProgress = progress;
            JsonDocument doc;
            doc["type"] = "ota_progress";
            doc["stage"] = "upload";
            doc["progress"] = progress;
            doc["uploaded"] = uploadedSize;
            doc["total"] = totalSize;
            String json;
            serializeJson(doc, json);
            _ws.textAll(json);
            LOG_I("Upload progress: %d%% (%d/%d bytes)", progress, uploadedSize, totalSize);
        }
    }
    
    if (final) {
        if (otaFile) {
            otaFile.close();
        }
        LOG_I("OTA upload complete: %d bytes", uploadedSize);
        
        // Verify file size
        File verifyFile = LittleFS.open(OTA_FILE_PATH, "r");
        bool uploadSuccess = true;
        if (verifyFile) {
            size_t fileSize = verifyFile.size();
            verifyFile.close();
            
            if (fileSize != uploadedSize) {
                LOG_E("File size mismatch: expected %d, got %d", uploadedSize, fileSize);
                broadcastLog("Upload failed: file size mismatch", "error");
                uploadSuccess = false;
            }
        } else {
            LOG_E("Failed to verify uploaded file");
            broadcastLog("Upload failed: file verification error", "error");
            uploadSuccess = false;
        }
        
        // Notify clients
        JsonDocument doc;
        doc["type"] = "ota_progress";
        doc["stage"] = "upload";
        doc["progress"] = uploadSuccess ? 100 : 0;
        doc["uploaded"] = uploadedSize;
        doc["total"] = totalSize;
        doc["success"] = uploadSuccess;
        String json;
        serializeJson(doc, json);
        _ws.textAll(json);
        
        if (uploadSuccess) {
            broadcastLog("Firmware uploaded: " + String(uploadedSize) + " bytes", "info");
        }
    }
}

void WebServer::handleStartOTA(AsyncWebServerRequest* request) {
    // Check if firmware file exists
    if (!LittleFS.exists(OTA_FILE_PATH)) {
        request->send(400, "application/json", "{\"error\":\"No firmware uploaded\"}");
        return;
    }
    
    File firmwareFile = LittleFS.open(OTA_FILE_PATH, "r");
    if (!firmwareFile) {
        request->send(500, "application/json", "{\"error\":\"Failed to open firmware file\"}");
        return;
    }
    
    size_t firmwareSize = firmwareFile.size();
    if (firmwareSize == 0 || firmwareSize > OTA_MAX_SIZE) {
        firmwareFile.close();
        request->send(400, "application/json", "{\"error\":\"Invalid firmware size\"}");
        return;
    }
    
    request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Starting OTA...\"}");
    
    broadcastLog("Starting Pico firmware update...", "info");
    
    // Step 1: Send bootloader command via UART (serial bootloader - preferred method)
    broadcastLog("Sending bootloader command to Pico...", "info");
    if (!_picoUart.sendCommand(MSG_CMD_BOOTLOADER, nullptr, 0)) {
        broadcastLog("Failed to send bootloader command", "error");
        firmwareFile.close();
        return;
    }
    
    // Step 2: Wait for bootloader ACK (0xAA 0x55)
    // The bootloader sends this ACK to confirm it's ready to receive firmware
    broadcastLog("Waiting for bootloader ACK...", "info");
    if (!_picoUart.waitForBootloaderAck(2000)) {
        broadcastLog("Bootloader ACK timeout - bootloader may not be ready", "error");
        firmwareFile.close();
        return;
    }
    broadcastLog("Bootloader ACK received, ready to stream firmware", "info");
    
    // Step 3: Stream firmware
    broadcastLog("Streaming firmware to Pico...", "info");
    bool success = streamFirmwareToPico(firmwareFile, firmwareSize);
    
    firmwareFile.close();
    
    if (!success) {
        broadcastLog("Firmware update failed", "error");
        // Fallback: Try hardware bootloader entry
        broadcastLog("Attempting hardware bootloader entry (fallback)...", "info");
        _picoUart.enterBootloader();
        delay(500);
        // Note: Hardware bootloader entry requires different protocol (USB bootloader)
        // This is a fallback for recovery only
        return;
    }
    
    // Step 4: Reset Pico to boot new firmware
    delay(1000);
    broadcastLog("Resetting Pico...", "info");
    _picoUart.resetPico();
    
    broadcastLog("Firmware update complete. Pico should boot with new firmware.", "info");
}

void WebServer::onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                          AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            LOG_I("WebSocket client connected: %u", client->id());
            broadcastLog("Client connected", "info");
            break;
            
        case WS_EVT_DISCONNECT:
            LOG_I("WebSocket client disconnected: %u", client->id());
            break;
            
        case WS_EVT_DATA:
            handleWsMessage(client, data, len);
            break;
            
        case WS_EVT_ERROR:
            LOG_E("WebSocket error: %u", client->id());
            break;
            
        case WS_EVT_PONG:
            break;
    }
}

void WebServer::handleWsMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len) {
    // Parse JSON command from client
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    
    if (error) {
        LOG_W("Invalid WebSocket message");
        return;
    }
    
    String type = doc["type"] | "";
    
    if (type == "ping") {
        _picoUart.sendPing();
    } 
    else if (type == "getConfig") {
        _picoUart.requestConfig();
    }
    else if (type == "setLogLevel") {
        // TODO: Implement log level control
    }
}

void WebServer::broadcastStatus(const String& json) {
    _ws.textAll(json);
}

void WebServer::broadcastLog(const String& message, const String& level) {
    JsonDocument doc;
    doc["type"] = "log";
    doc["level"] = level;
    doc["message"] = message;
    doc["timestamp"] = millis();
    
    String json;
    serializeJson(doc, json);
    _ws.textAll(json);
}

void WebServer::broadcastPicoMessage(uint8_t type, const uint8_t* payload, size_t len) {
    JsonDocument doc;
    doc["type"] = "pico";
    doc["msgType"] = type;
    doc["timestamp"] = millis();
    
    // Convert payload to hex string for debugging
    String hexPayload = "";
    for (size_t i = 0; i < len && i < 56; i++) {
        char hex[3];
        sprintf(hex, "%02X", payload[i]);
        hexPayload += hex;
    }
    doc["payload"] = hexPayload;
    doc["length"] = len;
    
    String json;
    serializeJson(doc, json);
    _ws.textAll(json);
}

size_t WebServer::getClientCount() {
    return _ws.count();
}

String WebServer::getContentType(const String& filename) {
    if (filename.endsWith(".html")) return "text/html";
    if (filename.endsWith(".css")) return "text/css";
    if (filename.endsWith(".js")) return "application/javascript";
    if (filename.endsWith(".json")) return "application/json";
    if (filename.endsWith(".png")) return "image/png";
    if (filename.endsWith(".ico")) return "image/x-icon";
    return "text/plain";
}

bool WebServer::streamFirmwareToPico(File& firmwareFile, size_t firmwareSize) {
    const size_t CHUNK_SIZE = 200;  // Bootloader protocol supports up to 256 bytes per chunk
    uint8_t buffer[CHUNK_SIZE];
    size_t bytesSent = 0;
    uint32_t chunkNumber = 0;
    
    firmwareFile.seek(0);  // Start from beginning
    
    // Stream firmware in chunks using bootloader protocol
    while (bytesSent < firmwareSize) {
        size_t bytesToRead = min(CHUNK_SIZE, firmwareSize - bytesSent);
        size_t bytesRead = firmwareFile.read(buffer, bytesToRead);
        
        if (bytesRead == 0) {
            LOG_E("Failed to read firmware chunk at offset %d", bytesSent);
            broadcastLog("Firmware read error", "error");
            return false;
        }
        
        // Stream chunk via bootloader protocol (raw UART, not packet protocol)
        size_t sent = _picoUart.streamFirmwareChunk(buffer, bytesRead, chunkNumber);
        if (sent != bytesRead) {
            LOG_E("Failed to send chunk %d: %d/%d bytes", chunkNumber, sent, bytesRead);
            broadcastLog("Firmware streaming error at chunk " + String(chunkNumber), "error");
            return false;
        }
        
        bytesSent += bytesRead;
        chunkNumber++;
        
        // Report progress every 10%
        static size_t lastProgress = 0;
        size_t progress = (bytesSent * 100) / firmwareSize;
        if (progress >= lastProgress + 10 || bytesSent == firmwareSize) {
            lastProgress = progress;
            JsonDocument doc;
            doc["type"] = "ota_progress";
            doc["stage"] = "flash";
            doc["progress"] = progress;
            doc["sent"] = bytesSent;
            doc["total"] = firmwareSize;
            String json;
            serializeJson(doc, json);
            _ws.textAll(json);
            LOG_I("Flash progress: %d%% (%d/%d bytes)", progress, bytesSent, firmwareSize);
        }
        
        // Small delay to prevent UART buffer overflow
        delay(10);
    }
    
    // Send end marker (chunk number 0xFFFFFFFF signals end of firmware)
    uint8_t endMarker[2] = {0xAA, 0x55};  // Bootloader end magic
    size_t sent = _picoUart.streamFirmwareChunk(endMarker, 2, 0xFFFFFFFF);
    if (sent != 2) {
        LOG_E("Failed to send end marker");
        broadcastLog("Failed to send end marker", "error");
        return false;
    }
    
    LOG_I("Firmware streaming complete: %d bytes in %d chunks", bytesSent, chunkNumber);
    broadcastLog("Firmware streaming complete: " + String(bytesSent) + " bytes in " + String(chunkNumber) + " chunks", "info");
    return true;
}

void WebServer::handleGetMQTTConfig(AsyncWebServerRequest* request) {
    MQTTConfig config = _mqttClient.getConfig();
    
    JsonDocument doc;
    doc["enabled"] = config.enabled;
    doc["broker"] = config.broker;
    doc["port"] = config.port;
    doc["username"] = config.username;
    doc["password"] = "";  // Don't send password back
    doc["client_id"] = config.client_id;
    doc["topic_prefix"] = config.topic_prefix;
    doc["use_tls"] = config.use_tls;
    doc["ha_discovery"] = config.ha_discovery;
    doc["ha_device_id"] = config.ha_device_id;
    doc["connected"] = _mqttClient.isConnected();
    doc["status"] = _mqttClient.getStatusString();
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServer::handleSetMQTTConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    
    if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    MQTTConfig config = _mqttClient.getConfig();
    
    // Update from JSON - use is<T>() to check if key exists (ArduinoJson 7.x API)
    if (!doc["enabled"].isNull()) config.enabled = doc["enabled"].as<bool>();
    if (!doc["broker"].isNull()) strncpy(config.broker, doc["broker"].as<const char*>(), sizeof(config.broker) - 1);
    if (!doc["port"].isNull()) config.port = doc["port"].as<uint16_t>();
    if (!doc["username"].isNull()) strncpy(config.username, doc["username"].as<const char*>(), sizeof(config.username) - 1);
    
    // Only update password if provided and non-empty
    if (!doc["password"].isNull()) {
        const char* pwd = doc["password"].as<const char*>();
        if (pwd && strlen(pwd) > 0) {
            strncpy(config.password, pwd, sizeof(config.password) - 1);
        }
    }
    
    if (!doc["client_id"].isNull()) strncpy(config.client_id, doc["client_id"].as<const char*>(), sizeof(config.client_id) - 1);
    if (!doc["topic_prefix"].isNull()) {
        const char* prefix = doc["topic_prefix"].as<const char*>();
        if (prefix && strlen(prefix) > 0) {
            strncpy(config.topic_prefix, prefix, sizeof(config.topic_prefix) - 1);
        }
    }
    if (!doc["use_tls"].isNull()) config.use_tls = doc["use_tls"].as<bool>();
    if (!doc["ha_discovery"].isNull()) config.ha_discovery = doc["ha_discovery"].as<bool>();
    if (!doc["ha_device_id"].isNull()) strncpy(config.ha_device_id, doc["ha_device_id"].as<const char*>(), sizeof(config.ha_device_id) - 1);
    
    if (_mqttClient.setConfig(config)) {
        request->send(200, "application/json", "{\"status\":\"ok\"}");
        broadcastLog("MQTT configuration updated", "info");
    } else {
        request->send(400, "application/json", "{\"error\":\"Invalid configuration\"}");
    }
}

void WebServer::handleTestMQTT(AsyncWebServerRequest* request) {
    if (_mqttClient.testConnection()) {
        request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Connection successful\"}");
        broadcastLog("MQTT connection test successful", "info");
    } else {
        request->send(500, "application/json", "{\"error\":\"Connection failed\"}");
        broadcastLog("MQTT connection test failed", "error");
    }
}

