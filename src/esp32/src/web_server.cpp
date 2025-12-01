#include "web_server.h"
#include "config.h"
#include "pico_uart.h"
#include "mqtt_client.h"
#include "brew_by_weight.h"
#include "scale/scale_manager.h"
#include "pairing_manager.h"
#include "state/state_manager.h"
#include <LittleFS.h>

WebServer::WebServer(WiFiManager& wifiManager, PicoUART& picoUart, MQTTClient& mqttClient, PairingManager* pairingManager)
    : _server(WEB_SERVER_PORT)
    , _ws(WEBSOCKET_PATH)
    , _wifiManager(wifiManager)
    , _picoUart(picoUart)
    , _mqttClient(mqttClient)
    , _pairingManager(pairingManager) {
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
    // Serve static files from LittleFS (assets, favicon, logo)
    _server.serveStatic("/assets", LittleFS, "/assets");
    _server.serveStatic("/favicon.svg", LittleFS, "/favicon.svg");
    _server.serveStatic("/logo.png", LittleFS, "/logo.png");
    
    // Root route - always serve index.html (React handles AP mode detection)
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(LittleFS, "/index.html", "text/html");
    });
    
    // API endpoints
    
    // Check if in AP mode (for WiFi setup detection)
    _server.on("/api/mode", HTTP_GET, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        doc["mode"] = "local";
        doc["apMode"] = _wifiManager.isAPMode();
        doc["hostname"] = WiFi.getHostname();
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
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
    
    // Brew-by-Weight settings
    _server.on("/api/scale/settings", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        bbw_settings_t settings = brewByWeight.getSettings();
        
        doc["target_weight"] = settings.target_weight;
        doc["dose_weight"] = settings.dose_weight;
        doc["stop_offset"] = settings.stop_offset;
        doc["auto_stop"] = settings.auto_stop;
        doc["auto_tare"] = settings.auto_tare;
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    _server.on("/api/scale/settings", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }
            
            if (!doc["target_weight"].isNull()) {
                brewByWeight.setTargetWeight(doc["target_weight"].as<float>());
            }
            if (!doc["dose_weight"].isNull()) {
                brewByWeight.setDoseWeight(doc["dose_weight"].as<float>());
            }
            if (!doc["stop_offset"].isNull()) {
                brewByWeight.setStopOffset(doc["stop_offset"].as<float>());
            }
            if (!doc["auto_stop"].isNull()) {
                brewByWeight.setAutoStop(doc["auto_stop"].as<bool>());
            }
            if (!doc["auto_tare"].isNull()) {
                brewByWeight.setAutoTare(doc["auto_tare"].as<bool>());
            }
            
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        }
    );
    
    _server.on("/api/scale/state", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        bbw_state_t state = brewByWeight.getState();
        bbw_settings_t settings = brewByWeight.getSettings();
        
        doc["active"] = state.active;
        doc["current_weight"] = state.current_weight;
        doc["target_weight"] = settings.target_weight;
        doc["progress"] = brewByWeight.getProgress();
        doc["ratio"] = brewByWeight.getCurrentRatio();
        doc["target_reached"] = state.target_reached;
        doc["stop_signaled"] = state.stop_signaled;
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    _server.on("/api/scale/tare", HTTP_POST, [](AsyncWebServerRequest* request) {
        scaleManager.tare();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // Scale connection status
    _server.on("/api/scale/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        scale_state_t state = scaleManager.getState();
        
        doc["connected"] = scaleManager.isConnected();
        doc["scanning"] = scaleManager.isScanning();
        doc["name"] = scaleManager.getScaleName();
        doc["type"] = (int)scaleManager.getScaleType();
        doc["type_name"] = getScaleTypeName(scaleManager.getScaleType());
        doc["weight"] = state.weight;
        doc["stable"] = state.stable;
        doc["flow_rate"] = state.flow_rate;
        doc["battery"] = state.battery_percent;
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Start BLE scale scan
    _server.on("/api/scale/scan", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (scaleManager.isScanning()) {
            request->send(400, "application/json", "{\"error\":\"Already scanning\"}");
            return;
        }
        if (scaleManager.isConnected()) {
            scaleManager.disconnect();
        }
        scaleManager.clearDiscovered();
        scaleManager.startScan(15000);  // 15 second scan
        broadcastLog("BLE scale scan started", "info");
        request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Scanning...\"}");
    });
    
    // Stop BLE scan
    _server.on("/api/scale/scan/stop", HTTP_POST, [](AsyncWebServerRequest* request) {
        scaleManager.stopScan();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // Get discovered scales
    _server.on("/api/scale/devices", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        JsonArray devices = doc["devices"].to<JsonArray>();
        
        const auto& discovered = scaleManager.getDiscoveredScales();
        for (size_t i = 0; i < discovered.size(); i++) {
            JsonObject device = devices.add<JsonObject>();
            device["index"] = i;
            device["name"] = discovered[i].name;
            device["address"] = discovered[i].address;
            device["type"] = (int)discovered[i].type;
            device["type_name"] = getScaleTypeName(discovered[i].type);
            device["rssi"] = discovered[i].rssi;
        }
        
        doc["scanning"] = scaleManager.isScanning();
        doc["count"] = discovered.size();
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Connect to scale by address or index
    _server.on("/api/scale/connect", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }
            
            bool success = false;
            
            if (!doc["address"].isNull()) {
                // Connect by address
                const char* addr = doc["address"].as<const char*>();
                if (addr && strlen(addr) > 0) {
                    success = scaleManager.connect(addr);
                }
            } else if (!doc["index"].isNull()) {
                // Connect by index from discovered list
                int idx = doc["index"].as<int>();
                success = scaleManager.connectByIndex(idx);
            } else {
                // Try to reconnect to saved scale
                success = scaleManager.connect(nullptr);
            }
            
            if (success) {
                broadcastLog("Connecting to scale...", "info");
                request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Connecting...\"}");
            } else {
                request->send(400, "application/json", "{\"error\":\"Connection failed\"}");
            }
        }
    );
    
    // Disconnect from scale
    _server.on("/api/scale/disconnect", HTTP_POST, [](AsyncWebServerRequest* request) {
        scaleManager.disconnect();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // Forget saved scale
    _server.on("/api/scale/forget", HTTP_POST, [this](AsyncWebServerRequest* request) {
        scaleManager.forgetScale();
        broadcastLog("Scale forgotten", "info");
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // Timer control (for scales that support it)
    _server.on("/api/scale/timer/start", HTTP_POST, [](AsyncWebServerRequest* request) {
        scaleManager.startTimer();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    _server.on("/api/scale/timer/stop", HTTP_POST, [](AsyncWebServerRequest* request) {
        scaleManager.stopTimer();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    _server.on("/api/scale/timer/reset", HTTP_POST, [](AsyncWebServerRequest* request) {
        scaleManager.resetTimer();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // ==========================================================================
    // Schedule API endpoints
    // ==========================================================================
    
    // Get all schedules
    _server.on("/api/schedules", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        JsonObject obj = doc.to<JsonObject>();
        State.settings().schedule.toJson(obj);
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Add a new schedule
    _server.on("/api/schedules", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }
            
            BrewOS::ScheduleEntry entry;
            entry.fromJson(doc.as<JsonObjectConst>());
            
            uint8_t newId = State.addSchedule(entry);
            if (newId > 0) {
                JsonDocument resp;
                resp["status"] = "ok";
                resp["id"] = newId;
                String response;
                serializeJson(resp, response);
                request->send(200, "application/json", response);
                broadcastLog("Schedule added: " + String(entry.name), "info");
            } else {
                request->send(400, "application/json", "{\"error\":\"Max schedules reached\"}");
            }
        }
    );
    
    // Update a schedule
    _server.on("/api/schedules/update", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }
            
            uint8_t id = doc["id"] | 0;
            if (id == 0) {
                request->send(400, "application/json", "{\"error\":\"Missing schedule ID\"}");
                return;
            }
            
            BrewOS::ScheduleEntry entry;
            entry.fromJson(doc.as<JsonObjectConst>());
            
            if (State.updateSchedule(id, entry)) {
                request->send(200, "application/json", "{\"status\":\"ok\"}");
                broadcastLog("Schedule updated: " + String(entry.name), "info");
            } else {
                request->send(404, "application/json", "{\"error\":\"Schedule not found\"}");
            }
        }
    );
    
    // Delete a schedule
    _server.on("/api/schedules/delete", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }
            
            uint8_t id = doc["id"] | 0;
            if (id == 0) {
                request->send(400, "application/json", "{\"error\":\"Missing schedule ID\"}");
                return;
            }
            
            if (State.removeSchedule(id)) {
                request->send(200, "application/json", "{\"status\":\"ok\"}");
                broadcastLog("Schedule deleted", "info");
            } else {
                request->send(404, "application/json", "{\"error\":\"Schedule not found\"}");
            }
        }
    );
    
    // Enable/disable a schedule
    _server.on("/api/schedules/toggle", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }
            
            uint8_t id = doc["id"] | 0;
            bool enabled = doc["enabled"] | false;
            
            if (id == 0) {
                request->send(400, "application/json", "{\"error\":\"Missing schedule ID\"}");
                return;
            }
            
            if (State.enableSchedule(id, enabled)) {
                request->send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                request->send(404, "application/json", "{\"error\":\"Schedule not found\"}");
            }
        }
    );
    
    // Auto power-off settings
    _server.on("/api/schedules/auto-off", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        doc["enabled"] = State.getAutoPowerOffEnabled();
        doc["minutes"] = State.getAutoPowerOffMinutes();
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    _server.on("/api/schedules/auto-off", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }
            
            bool enabled = doc["enabled"] | false;
            uint16_t minutes = doc["minutes"] | 60;
            
            State.setAutoPowerOff(enabled, minutes);
            request->send(200, "application/json", "{\"status\":\"ok\"}");
            broadcastLog("Auto power-off: " + String(enabled ? "enabled" : "disabled") + 
                        " (" + String(minutes) + " min)", "info");
        }
    );
    
    // ==========================================================================
    // Time/NTP API endpoints
    // ==========================================================================
    
    // Get time status and settings
    _server.on("/api/time", HTTP_GET, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        
        // Current time status
        TimeStatus timeStatus = _wifiManager.getTimeStatus();
        doc["synced"] = timeStatus.ntpSynced;
        doc["currentTime"] = timeStatus.currentTime;
        doc["timezone"] = timeStatus.timezone;
        doc["utcOffset"] = timeStatus.utcOffset;
        
        // Settings
        JsonObject settings = doc["settings"].to<JsonObject>();
        State.settings().time.toJson(settings);
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Update time settings
    _server.on("/api/time", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }
            
            auto& timeSettings = State.settings().time;
            
            if (!doc["useNTP"].isNull()) timeSettings.useNTP = doc["useNTP"].as<bool>();
            if (!doc["ntpServer"].isNull()) strncpy(timeSettings.ntpServer, doc["ntpServer"].as<const char*>(), sizeof(timeSettings.ntpServer) - 1);
            if (!doc["utcOffsetMinutes"].isNull()) timeSettings.utcOffsetMinutes = doc["utcOffsetMinutes"].as<int16_t>();
            if (!doc["dstEnabled"].isNull()) timeSettings.dstEnabled = doc["dstEnabled"].as<bool>();
            if (!doc["dstOffsetMinutes"].isNull()) timeSettings.dstOffsetMinutes = doc["dstOffsetMinutes"].as<int16_t>();
            
            State.saveTimeSettings();
            
            // Apply new NTP settings
            _wifiManager.configureNTP(
                timeSettings.ntpServer,
                timeSettings.utcOffsetMinutes,
                timeSettings.dstEnabled,
                timeSettings.dstOffsetMinutes
            );
            
            request->send(200, "application/json", "{\"status\":\"ok\"}");
            broadcastLog("Time settings updated", "info");
        }
    );
    
    // Force NTP sync
    _server.on("/api/time/sync", HTTP_POST, [this](AsyncWebServerRequest* request) {
        _wifiManager.syncNTP();
        request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"NTP sync initiated\"}");
        broadcastLog("NTP sync initiated", "info");
    });
    
    // Temperature control endpoints
    _server.on("/api/temp/brew", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }
            
            float temp = doc["temp"] | 0.0f;
            if (temp < 80.0f || temp > 105.0f) {
                request->send(400, "application/json", "{\"error\":\"Temperature out of range (80-105°C)\"}");
                return;
            }
            
            // Send to Pico
            uint8_t payload[5];
            payload[0] = 0x01;  // Brew boiler ID
            memcpy(&payload[1], &temp, sizeof(float));
            if (_picoUart.sendCommand(MSG_CMD_SET_TEMP, payload, 5)) {
                broadcastLog("Brew temp set to " + String(temp, 1) + "°C", "info");
                request->send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                request->send(500, "application/json", "{\"error\":\"Failed to send command\"}");
            }
        }
    );
    
    _server.on("/api/temp/steam", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }
            
            float temp = doc["temp"] | 0.0f;
            if (temp < 120.0f || temp > 160.0f) {
                request->send(400, "application/json", "{\"error\":\"Temperature out of range (120-160°C)\"}");
                return;
            }
            
            // Send to Pico
            uint8_t payload[5];
            payload[0] = 0x02;  // Steam boiler ID
            memcpy(&payload[1], &temp, sizeof(float));
            if (_picoUart.sendCommand(MSG_CMD_SET_TEMP, payload, 5)) {
                broadcastLog("Steam temp set to " + String(temp, 1) + "°C", "info");
                request->send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                request->send(500, "application/json", "{\"error\":\"Failed to send command\"}");
            }
        }
    );
    
    // Machine mode control
    _server.on("/api/mode", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }
            
            String mode = doc["mode"] | "";
            uint8_t cmd = 0;
            
            if (mode == "on" || mode == "ready") {
                cmd = 0x01;  // Turn on
            } else if (mode == "off" || mode == "standby") {
                cmd = 0x00;  // Turn off
            } else {
                request->send(400, "application/json", "{\"error\":\"Invalid mode (use: on, off, ready, standby)\"}");
                return;
            }
            
            if (_picoUart.sendCommand(MSG_CMD_MODE, &cmd, 1)) {
                broadcastLog("Machine mode set to: " + mode, "info");
                request->send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                request->send(500, "application/json", "{\"error\":\"Failed to send command\"}");
            }
        }
    );
    
    // Cloud status endpoint
    _server.on("/api/cloud/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        auto& cloudSettings = State.settings().cloud;
        
        JsonDocument doc;
        doc["enabled"] = cloudSettings.enabled;
        doc["connected"] = false;  // TODO: Track actual connection status if needed
        doc["serverUrl"] = cloudSettings.serverUrl;
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Pairing API endpoints
    _server.on("/api/pairing/qr", HTTP_GET, [this](AsyncWebServerRequest* request) {
        // Check if cloud is enabled
        auto& cloudSettings = State.settings().cloud;
        if (!cloudSettings.enabled || !_pairingManager) {
            request->send(503, "application/json", "{\"error\":\"Cloud integration not enabled\"}");
            return;
        }
        
        // Generate a new token if needed
        if (!_pairingManager->isTokenValid()) {
            _pairingManager->generateToken();
        }
        
        JsonDocument doc;
        doc["deviceId"] = _pairingManager->getDeviceId();
        doc["token"] = _pairingManager->getCurrentToken();
        doc["url"] = _pairingManager->getPairingUrl();
        doc["expiresIn"] = (_pairingManager->getTokenExpiry() - millis()) / 1000;
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    _server.on("/api/pairing/refresh", HTTP_POST, [this](AsyncWebServerRequest* request) {
        // Check if cloud is enabled
        auto& cloudSettings = State.settings().cloud;
        if (!cloudSettings.enabled || !_pairingManager) {
            request->send(503, "application/json", "{\"error\":\"Cloud integration not enabled\"}");
            return;
        }
        
        _pairingManager->generateToken();
        
        JsonDocument doc;
        doc["deviceId"] = _pairingManager->getDeviceId();
        doc["token"] = _pairingManager->getCurrentToken();
        doc["url"] = _pairingManager->getPairingUrl();
        doc["expiresIn"] = 600;  // 10 minutes
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // SPA fallback - serve index.html for client-side routes
    // (any route that doesn't match API or static files)
    _server.onNotFound([](AsyncWebServerRequest* request) {
        // If it's an API call, return 404
        if (request->url().startsWith("/api/")) {
            request->send(404, "application/json", "{\"error\":\"Not found\"}");
            return;
        }
        // For all other routes, serve index.html (React Router handles it)
        request->send(LittleFS, "/index.html", "text/html");
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
    
    // Scale status
    doc["scale"]["connected"] = scaleManager.isConnected();
    doc["scale"]["scanning"] = scaleManager.isScanning();
    doc["scale"]["name"] = scaleManager.getScaleName();
    if (scaleManager.isConnected()) {
        scale_state_t scaleState = scaleManager.getState();
        doc["scale"]["weight"] = scaleState.weight;
        doc["scale"]["flow_rate"] = scaleState.flow_rate;
        doc["scale"]["stable"] = scaleState.stable;
    }
    
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
    else if (type == "command") {
        // Handle commands from web UI
        String cmd = doc["cmd"] | "";
        
        if (cmd == "set_eco") {
            // Set eco mode configuration
            // Payload: enabled (bool), brewTemp (float), timeout (int minutes)
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
            
            if (_picoUart.sendCommand(MSG_CMD_SET_ECO, payload, 5)) {
                broadcastLog("Eco mode config sent: enabled=" + String(enabled) + 
                            ", temp=" + String(brewTemp, 1) + "°C, timeout=" + String(timeout) + "min", "info");
            } else {
                broadcastLog("Failed to send eco config", "error");
            }
        }
        else if (cmd == "enter_eco") {
            // Manually enter eco mode
            uint8_t payload[1] = {1};  // 1 = enter eco
            if (_picoUart.sendCommand(MSG_CMD_SET_ECO, payload, 1)) {
                broadcastLog("Entering eco mode", "info");
            } else {
                broadcastLog("Failed to enter eco mode", "error");
            }
        }
        else if (cmd == "exit_eco") {
            // Manually exit eco mode (wake up)
            uint8_t payload[1] = {0};  // 0 = exit eco
            if (_picoUart.sendCommand(MSG_CMD_SET_ECO, payload, 1)) {
                broadcastLog("Exiting eco mode", "info");
            } else {
                broadcastLog("Failed to exit eco mode", "error");
            }
        }
        else if (cmd == "set_temp") {
            // Set temperature setpoint
            String boiler = doc["boiler"] | "brew";
            float temp = doc["temp"] | 0.0f;
            
            uint8_t payload[5];
            payload[0] = (boiler == "steam") ? 0x02 : 0x01;
            memcpy(&payload[1], &temp, sizeof(float));
            if (_picoUart.sendCommand(MSG_CMD_SET_TEMP, payload, 5)) {
                broadcastLog(boiler + " temp set to " + String(temp, 1) + "°C", "info");
            }
        }
        else if (cmd == "set_mode") {
            // Set machine mode
            String mode = doc["mode"] | "";
            uint8_t modeCmd = 0;
            
            // Check for optional heating strategy parameter
            if (doc.containsKey("strategy") && mode == "on") {
                uint8_t strategy = doc["strategy"].as<uint8_t>();
                if (strategy <= 3) {  // Valid strategy range: 0-3
                    // Set heating strategy first
                    uint8_t strategyPayload[2] = {0x01, strategy};  // CONFIG_HEATING_STRATEGY = 0x01
                    _picoUart.sendCommand(MSG_CMD_CONFIG, strategyPayload, 2);
                    broadcastLog("Heating strategy set to: " + String(strategy), "info");
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
                broadcastLog("Mode set to: " + mode, "info");
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
                broadcastLog("MQTT connection test successful", "info");
            } else {
                broadcastLog("MQTT connection test failed", "error");
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
                broadcastLog("MQTT configuration updated", "info");
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
                    broadcastLog("Cloud enabled: " + String(cloudSettings.serverUrl), "info");
                } else if (!cloudSettings.enabled && wasEnabled) {
                    // Cloud was disabled - clear pairing manager
                    _pairingManager->begin("");  // Clear cloud URL
                    broadcastLog("Cloud disabled", "info");
                }
            }
            
            broadcastLog("Cloud configuration updated: " + String(cloudSettings.enabled ? "enabled" : "disabled"), "info");
        }
        else if (cmd == "add_schedule") {
            // Add a new schedule
            BrewOS::ScheduleEntry entry;
            entry.fromJson(doc.as<JsonObjectConst>());
            
            uint8_t newId = State.addSchedule(entry);
            if (newId > 0) {
                broadcastLog("Schedule added: " + String(entry.name), "info");
            }
        }
        else if (cmd == "update_schedule") {
            // Update existing schedule
            uint8_t id = doc["id"] | 0;
            if (id > 0) {
                BrewOS::ScheduleEntry entry;
                entry.fromJson(doc.as<JsonObjectConst>());
                if (State.updateSchedule(id, entry)) {
                    broadcastLog("Schedule updated", "info");
                }
            }
        }
        else if (cmd == "delete_schedule") {
            // Delete schedule
            uint8_t id = doc["id"] | 0;
            if (id > 0 && State.removeSchedule(id)) {
                broadcastLog("Schedule deleted", "info");
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
            broadcastLog("Auto power-off updated", "info");
        }
        else if (cmd == "get_schedules") {
            // Return schedules to requesting client (if needed)
            // Usually schedules are sent via full state broadcast
        }
        // Scale commands
        else if (cmd == "scale_scan") {
            if (!scaleManager.isScanning()) {
                if (scaleManager.isConnected()) {
                    scaleManager.disconnect();
                }
                scaleManager.clearDiscovered();
                scaleManager.startScan(15000);
                broadcastLog("BLE scale scan started", "info");
            }
        }
        else if (cmd == "scale_scan_stop") {
            scaleManager.stopScan();
            broadcastLog("BLE scale scan stopped", "info");
        }
        else if (cmd == "scale_connect") {
            String address = doc["address"] | "";
            if (!address.isEmpty()) {
                scaleManager.connect(address.c_str());
                broadcastLog("Connecting to scale: " + address, "info");
            }
        }
        else if (cmd == "scale_disconnect") {
            scaleManager.disconnect();
            broadcastLog("Scale disconnected", "info");
        }
        else if (cmd == "tare" || cmd == "scale_tare") {
            scaleManager.tare();
            broadcastLog("Scale tared", "info");
        }
        else if (cmd == "scale_reset") {
            scaleManager.tare();
            brewByWeight.reset();
            broadcastLog("Scale reset", "info");
        }
        // Brew-by-weight settings
        else if (cmd == "set_bbw") {
            if (!doc["target_weight"].isNull()) {
                brewByWeight.setTargetWeight(doc["target_weight"].as<float>());
            }
            if (!doc["dose_weight"].isNull()) {
                brewByWeight.setDoseWeight(doc["dose_weight"].as<float>());
            }
            if (!doc["stop_offset"].isNull()) {
                brewByWeight.setStopOffset(doc["stop_offset"].as<float>());
            }
            if (!doc["auto_stop"].isNull()) {
                brewByWeight.setAutoStop(doc["auto_stop"].as<bool>());
            }
            if (!doc["auto_tare"].isNull()) {
                brewByWeight.setAutoTare(doc["auto_tare"].as<bool>());
            }
            broadcastLog("Brew-by-weight settings updated", "info");
        }
        // Power settings
        else if (cmd == "set_power") {
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
            
            broadcastLog("Power settings updated: " + String(voltage) + "V, " + String(maxCurrent) + "A", "info");
        }
        // WiFi commands
        else if (cmd == "wifi_forget") {
            _wifiManager.clearCredentials();
            broadcastLog("WiFi credentials cleared. Device will restart.", "warning");
            delay(1000);
            ESP.restart();
        }
        // System commands
        else if (cmd == "restart") {
            broadcastLog("Device restarting...", "warning");
            delay(500);
            ESP.restart();
        }
        else if (cmd == "factory_reset") {
            broadcastLog("Factory reset initiated...", "warning");
            State.factoryReset();
            _wifiManager.clearCredentials();
            delay(1000);
            ESP.restart();
        }
        else if (cmd == "check_update") {
            // TODO: Implement OTA update check
            broadcastLog("Update check not implemented yet", "info");
        }
        else if (cmd == "ota_start") {
            // TODO: Implement OTA update start
            broadcastLog("OTA update not implemented yet", "info");
        }
        // Machine info (stored in network hostname for now)
        else if (cmd == "set_machine_info" || cmd == "set_device_info") {
            auto& networkSettings = State.settings().network;
            
            if (!doc["name"].isNull()) {
                strncpy(networkSettings.hostname, doc["name"].as<const char*>(), sizeof(networkSettings.hostname) - 1);
            }
            // Note: model and machineType would need a dedicated struct in Settings
            // For now, we just store the device name in hostname
            
            State.saveNetworkSettings();
            broadcastLog("Device info updated: " + String(networkSettings.hostname), "info");
        }
        // Maintenance records
        else if (cmd == "record_maintenance") {
            String type = doc["type"] | "";
            if (!type.isEmpty()) {
                State.recordMaintenance(type.c_str());
                broadcastLog("Maintenance recorded: " + type, "info");
            }
        }
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

