#include "web_server.h"
#include "config.h"
#include "pico_uart.h"
#include "mqtt_client.h"
#include "brew_by_weight.h"
#include "scale/scale_manager.h"
#include "pairing_manager.h"
#include "cloud_connection.h"
#include "state/state_manager.h"
#include "statistics/statistics_manager.h"
#include "power_meter/power_meter_manager.h"
#include "ui/ui.h"
#include <WiFi.h>
#include <LittleFS.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_heap_caps.h>
#include <esp_partition.h>
#include <pgmspace.h>
#include <stdarg.h>
#include <stdio.h>

// Deferred WiFi connection state (allows HTTP response to be sent before disconnecting AP)
static bool _pendingWiFiConnect = false;
static unsigned long _wifiConnectRequestTime = 0;

// Track when WiFi is ready to serve requests (prevents PSRAM crashes from early HTTP requests)
static unsigned long _wifiReadyTime = 0;
static const unsigned long WIFI_READY_DELAY_MS = 1000;  // Reduced from 5s to 1s - faster responsiveness

// External reference to machine state defined in main.cpp
extern ui_state_t machineState;

// Static WebServer pointer for WebSocket callback
static WebServer* _wsInstance = nullptr;

// Note: All JsonDocument instances should use StaticJsonDocument with stack allocation
// to avoid PSRAM crashes. Use the pragma pattern from handleGetWiFiNetworks.

WebServer::WebServer(WiFiManager& wifiManager, PicoUART& picoUart, MQTTClient& mqttClient, PairingManager* pairingManager)
    : _server(WEB_SERVER_PORT)
    , _ws("/ws")  // WebSocket on same port 80, endpoint /ws
    , _wifiManager(wifiManager)
    , _picoUart(picoUart)
    , _mqttClient(mqttClient)
    , _pairingManager(pairingManager) {
}

void WebServer::begin() {
    LOG_I("Starting web server...");
    
    // Initialize LittleFS with more open files to handle parallel asset requests
    // Default is 5, increasing to 15 to prevent "fopen failed" errors
    if (!LittleFS.begin(true, "/littlefs", 15)) {
        LOG_E("Failed to mount LittleFS");
    } else {
        LOG_I("LittleFS mounted");
    }
    
    // Setup routes
    setupRoutes();
    
    // Setup WebSocket handler
    _wsInstance = this;
    _ws.onEvent([](AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
        if (_wsInstance) {
            _wsInstance->handleWsEvent(server, client, type, arg, data, len);
        }
    });
    _server.addHandler(&_ws);
    
    // Start HTTP server
    _server.begin();
    LOG_I("HTTP server started on port %d", WEB_SERVER_PORT);
    LOG_I("WebSocket available at ws://brewos.local/ws");
}

void WebServer::setCloudConnection(CloudConnection* cloudConnection) {
    _cloudConnection = cloudConnection;
}

// Static callback wrapper for cloud commands - avoids lambda capture issues
static void cloudCommandCallback(const String& type, JsonDocument& doc) {
    if (_wsInstance) {
        _wsInstance->processCommand(doc);
    }
}

// Static pairing manager pointer for registration callback
static PairingManager* _staticPairingManager = nullptr;

static bool cloudRegisterCallback() {
    if (_staticPairingManager) {
        return _staticPairingManager->registerTokenWithCloud();
    }
    return false;
}

void WebServer::startCloudConnection(const String& serverUrl, const String& deviceId, const String& deviceKey) {
    if (!_cloudConnection) {
        LOG_W("Cannot start cloud connection: not initialized");
        return;
    }
    
    LOG_I("Starting cloud connection to %s", serverUrl.c_str());
    
    // Initialize cloud connection
    _cloudConnection->begin(serverUrl, deviceId, deviceKey);
    
    // Set up registration callback using static function pointer
    if (_pairingManager) {
        _staticPairingManager = _pairingManager;
        _cloudConnection->onRegister(cloudRegisterCallback);
    }
    
    // Set up command handler using static function pointer
    _cloudConnection->onCommand(cloudCommandCallback);
    
    LOG_I("Cloud connection started");
}

void WebServer::setWiFiConnected() {
    _wifiReadyTime = millis();
    LOG_I("WiFi connected - requests will be served after %lu ms delay", WIFI_READY_DELAY_MS);
}

bool WebServer::isWiFiReady() {
    if (_wifiReadyTime == 0) {
        return false;  // WiFi not connected yet
    }
    return (millis() - _wifiReadyTime) >= WIFI_READY_DELAY_MS;
}

// The React app is served from LittleFS via serveStatic()
// Users can access it at http://brewos.local after WiFi connects

void WebServer::loop() {
    // AsyncWebSocket is event-driven, no loop() needed
    // Periodically clean up disconnected clients
    static unsigned long lastCleanup = 0;
    if (millis() - lastCleanup > 1000) {
        _ws.cleanupClients();
        lastCleanup = millis();
    }
    
    // Handle deferred WiFi connection
    // Wait 500ms after request to ensure HTTP response is fully sent
    if (_pendingWiFiConnect && _wifiConnectRequestTime == 0) {
        _wifiConnectRequestTime = millis();
    }
    
    if (_pendingWiFiConnect && _wifiConnectRequestTime > 0 && 
        millis() - _wifiConnectRequestTime > 500) {
        _pendingWiFiConnect = false;
        _wifiConnectRequestTime = 0;
        LOG_I("Starting WiFi connection (deferred)");
        _wifiManager.connectToWiFi();
    }
}

void WebServer::setupRoutes() {
    // Simple test endpoint - no LittleFS needed - for diagnosing web server performance
    _server.on("/test", HTTP_GET, [this](AsyncWebServerRequest* request) {
        unsigned long startTime = millis();
        // Pause cloud to ensure web server is responsive
        if (_cloudConnection) {
            _cloudConnection->pause();
        }
        char response[128];
        snprintf(response, sizeof(response), "BrewOS Web Server OK\nHeap: %zu bytes\nTime: %lu ms", 
                 ESP.getFreeHeap(), millis() - startTime);
        request->send(200, "text/plain", response);
    });
    
    // Health check endpoint - fastest possible response
    _server.on("/health", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/plain", "OK");
    });
    
    // WiFi Setup page - inline HTML (no file operations, no PSRAM issues)
    // This follows IoT best practices: minimal setup page served directly
    _server.on("/setup", HTTP_GET, [this](AsyncWebServerRequest* request) {
        // Inline HTML WiFi setup page - completely self-contained
        // No file operations, no PSRAM usage, works reliably
        // Using PROGMEM to store in flash (not RAM/PSRAM)
        const char html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <title>BrewOS WiFi Setup</title>
    <style>
        *{box-sizing:border-box;margin:0;padding:0}
        body{font-family:'Inter',-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;background:linear-gradient(145deg,#1a1412 0%,#2d1f18 50%,#1a1412 100%);min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px}
        .card{background:linear-gradient(180deg,#1e1714 0%,#171210 100%);border-radius:24px;box-shadow:0 25px 80px rgba(0,0,0,0.5),0 0 0 1px rgba(186,132,86,0.1);max-width:420px;width:100%;padding:40px 32px;position:relative;overflow:hidden}
        .card::before{content:'';position:absolute;top:0;left:0;right:0;height:3px;background:linear-gradient(90deg,#ba8456,#c38f5f,#a06b3d)}
        .logo{width:80px;height:80px;margin:0 auto 24px;display:block;filter:drop-shadow(0 4px 12px rgba(186,132,86,0.3))}
        h1{color:#f5f0eb;text-align:center;margin-bottom:8px;font-size:26px;font-weight:600;letter-spacing:-0.5px}
        .subtitle{color:#9a8578;text-align:center;margin-bottom:32px;font-size:14px}
        .form-group{margin-bottom:20px}
        label{display:block;color:#c4b5a9;font-weight:500;margin-bottom:10px;font-size:13px;text-transform:uppercase;letter-spacing:0.5px}
        input{width:100%;padding:14px 16px;background:#0d0a09;border:1px solid #3d2e24;border-radius:12px;font-size:15px;color:#f5f0eb;transition:all 0.2s}
        input::placeholder{color:#5c4d42}
        input:focus{outline:none;border-color:#ba8456;box-shadow:0 0 0 3px rgba(186,132,86,0.15)}
        .btn{width:100%;padding:16px;background:linear-gradient(135deg,#ba8456 0%,#a06b3d 100%);color:#fff;border:none;border-radius:12px;font-size:15px;font-weight:600;cursor:pointer;transition:all 0.2s;text-transform:uppercase;letter-spacing:0.5px}
        .btn:hover{transform:translateY(-1px);box-shadow:0 8px 24px rgba(186,132,86,0.3)}
        .btn:active{transform:translateY(0)}
        .btn:disabled{opacity:0.4;cursor:not-allowed;transform:none}
        .btn-secondary{background:#2d241e;color:#c4b5a9;margin-top:12px}
        .btn-secondary:hover{background:#3d2e24}
        .status{margin-top:20px;padding:14px 16px;border-radius:12px;font-size:14px;display:none;text-align:center}
        .status.success{background:rgba(34,197,94,0.1);color:#4ade80;border:1px solid rgba(34,197,94,0.2)}
        .status.error{background:rgba(239,68,68,0.1);color:#f87171;border:1px solid rgba(239,68,68,0.2)}
        .status.info{background:rgba(186,132,86,0.1);color:#d5a071;border:1px solid rgba(186,132,86,0.2)}
        .network-list{max-height:280px;overflow-y:auto;background:#0d0a09;border:1px solid #3d2e24;border-radius:12px;margin-bottom:16px}
        .network-list::-webkit-scrollbar{width:6px}
        .network-list::-webkit-scrollbar-track{background:#1a1412}
        .network-list::-webkit-scrollbar-thumb{background:#3d2e24;border-radius:3px}
        .network-item{padding:14px 16px;border-bottom:1px solid #2d241e;cursor:pointer;transition:all 0.15s}
        .network-item:hover{background:#1a1412}
        .network-item:last-child{border-bottom:none}
        .network-item.selected{background:rgba(186,132,86,0.1);border-color:rgba(186,132,86,0.3)}
        .network-ssid{font-weight:500;color:#f5f0eb;font-size:15px;display:flex;align-items:center;gap:8px}
        .network-ssid .lock{color:#ba8456;font-size:12px}
        .network-rssi{font-size:12px;color:#7a6b5f;margin-top:4px}
        .signal-bars{display:inline-flex;gap:2px;margin-left:auto}
        .signal-bar{width:3px;background:#3d2e24;border-radius:1px}
        .signal-bar.active{background:#ba8456}
        .empty-state{text-align:center;padding:40px 20px;color:#5c4d42}
        .empty-state svg{width:48px;height:48px;margin-bottom:16px;opacity:0.5}
        .spinner{display:inline-block;width:18px;height:18px;border:2px solid rgba(255,255,255,0.3);border-top-color:#fff;border-radius:50%;animation:spin 0.6s linear infinite;margin-right:8px;vertical-align:middle}
        @keyframes spin{to{transform:rotate(360deg)}}
        .divider{height:1px;background:linear-gradient(90deg,transparent,#3d2e24,transparent);margin:24px 0}
    </style>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600&display=swap" rel="stylesheet">
</head>
<body>
    <div class="card">
        <svg class="logo" viewBox="0 0 100 100" fill="none" xmlns="http://www.w3.org/2000/svg">
            <circle cx="50" cy="50" r="48" fill="url(#grad1)" stroke="#ba8456" stroke-width="2"/>
            <path d="M30 35C30 35 32 25 50 25C68 25 70 35 70 35V60C70 70 60 75 50 75C40 75 30 70 30 60V35Z" fill="#2d1f18" stroke="#ba8456" stroke-width="2"/>
            <path d="M70 40H75C80 40 82 45 82 50C82 55 80 60 75 60H70" stroke="#ba8456" stroke-width="2" fill="none"/>
            <ellipse cx="50" cy="35" rx="18" ry="6" fill="#ba8456" opacity="0.3"/>
            <path d="M40 50C42 55 48 58 50 58C52 58 58 55 60 50" stroke="#d5a071" stroke-width="2" stroke-linecap="round" opacity="0.6"/>
            <defs><linearGradient id="grad1" x1="0%" y1="0%" x2="100%" y2="100%"><stop offset="0%" stop-color="#1e1714"/><stop offset="100%" stop-color="#0d0a09"/></linearGradient></defs>
        </svg>
        
        <h1>BrewOS</h1>
        <p class="subtitle">Connect your espresso machine to WiFi</p>
        
        <div class="form-group">
            <label>Available Networks</label>
            <div id="networkList" class="network-list">
                <div class="empty-state">
                    <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
                        <path d="M8.288 15.038a5.25 5.25 0 017.424 0M5.106 11.856c3.807-3.808 9.98-3.808 13.788 0M1.924 8.674c5.565-5.565 14.587-5.565 20.152 0M12.53 18.22l-.53.53-.53-.53a.75.75 0 011.06 0z"/>
                    </svg>
                    <p>Tap "Scan" to find networks</p>
                </div>
            </div>
        </div>
        
        <button id="scanBtn" class="btn btn-secondary" onclick="scanNetworks()">
            <span id="scanSpinner" class="spinner" style="display:none"></span>
            <span id="scanText">Scan for Networks</span>
        </button>
        
        <div class="divider"></div>
        
        <div class="form-group" id="passwordGroup" style="display:none">
            <label>WiFi Password</label>
            <input type="password" id="password" placeholder="Enter password">
        </div>
        
        <button id="connectBtn" class="btn" onclick="connectWiFi()" disabled>
            <span id="connectSpinner" class="spinner" style="display:none"></span>
            <span id="connectText">Connect to Network</span>
        </button>
        
        <div id="status" class="status"></div>
    </div>
    
    <script>
        let selectedSSID = '';
        
        function showStatus(message, type) {
            const status = document.getElementById('status');
            status.textContent = message;
            status.className = 'status ' + type;
            status.style.display = 'block';
        }
        
        function hideStatus() {
            document.getElementById('status').style.display = 'none';
        }
        
        function getSignalBars(rssi) {
            const bars = rssi > -50 ? 4 : rssi > -60 ? 3 : rssi > -70 ? 2 : 1;
            return Array(4).fill(0).map((_, i) => 
                `<div class="signal-bar${i < bars ? ' active' : ''}" style="height:${6 + i * 3}px"></div>`
            ).join('');
        }
        
        async function scanNetworks() {
            const btn = document.getElementById('scanBtn');
            const spinner = document.getElementById('scanSpinner');
            const text = document.getElementById('scanText');
            const list = document.getElementById('networkList');
            
            btn.disabled = true;
            spinner.style.display = 'inline-block';
            text.textContent = 'Scanning...';
            hideStatus();
            
            try {
                const response = await fetch('/api/wifi/networks');
                const data = await response.json();
                
                if (data.networks && data.networks.length > 0) {
                    list.innerHTML = '';
                    data.networks.sort((a,b) => b.rssi - a.rssi).forEach(network => {
                        const item = document.createElement('div');
                        item.className = 'network-item';
                        item.onclick = () => selectNetwork(network.ssid, network.secure, item);
                        item.innerHTML = `
                            <div class="network-ssid">
                                ${escapeHtml(network.ssid)}
                                ${network.secure ? '<span class="lock">🔒</span>' : ''}
                                <span class="signal-bars">${getSignalBars(network.rssi)}</span>
                            </div>
                            <div class="network-rssi">${network.rssi} dBm</div>
                        `;
                        list.appendChild(item);
                    });
                    showStatus(data.networks.length + ' networks found', 'success');
                } else {
                    list.innerHTML = '<div class="empty-state"><p>No networks found</p></div>';
                    showStatus('No networks found. Try again.', 'error');
                }
            } catch (error) {
                showStatus('Scan failed. Please try again.', 'error');
                list.innerHTML = '<div class="empty-state"><p>Scan failed</p></div>';
            }
            
            btn.disabled = false;
            spinner.style.display = 'none';
            text.textContent = 'Scan for Networks';
        }
        
        function selectNetwork(ssid, secure, element) {
            selectedSSID = ssid;
            document.getElementById('passwordGroup').style.display = secure ? 'block' : 'none';
            document.getElementById('connectBtn').disabled = false;
            
            document.querySelectorAll('.network-item').forEach(item => item.classList.remove('selected'));
            element.classList.add('selected');
            
            showStatus('Selected: ' + ssid, 'info');
        }
        
        async function connectWiFi() {
            if (!selectedSSID) {
                showStatus('Please select a network first', 'error');
                return;
            }
            
            const password = document.getElementById('password').value;
            const btn = document.getElementById('connectBtn');
            const spinner = document.getElementById('connectSpinner');
            const text = document.getElementById('connectText');
            
            btn.disabled = true;
            spinner.style.display = 'inline-block';
            text.textContent = 'Connecting...';
            hideStatus();
            
            try {
                const response = await fetch('/api/wifi/connect', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({ssid: selectedSSID, password: password})
                });
                
                const data = await response.json();
                
                if (response.ok) {
                    showStatus('Connected! Redirecting to BrewOS...', 'success');
                    text.textContent = 'Connected!';
                    setTimeout(() => {
                        window.location.href = 'http://brewos.local';
                    }, 3000);
                } else {
                    showStatus(data.error || 'Connection failed', 'error');
                    btn.disabled = false;
                    spinner.style.display = 'none';
                    text.textContent = 'Connect to Network';
                }
            } catch (error) {
                showStatus('Connection error. Please try again.', 'error');
                btn.disabled = false;
                spinner.style.display = 'none';
                text.textContent = 'Connect to Network';
            }
        }
        
        function escapeHtml(text) {
            const div = document.createElement('div');
            div.textContent = text;
            return div.innerHTML;
        }
        
        // Auto-scan on load
        window.onload = () => scanNetworks();
    </script>
</body>
</html>
)rawliteral";
        // Copy from PROGMEM to regular RAM for AsyncWebServer
        size_t htmlLen = strlen_P(html);
        char* htmlBuffer = (char*)heap_caps_malloc(htmlLen + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (htmlBuffer) {
            strcpy_P(htmlBuffer, html);
            request->send(200, "text/html", htmlBuffer);
            // Buffer will be freed by AsyncWebServer after response
        } else {
            request->send(500, "text/plain", "Out of memory");
        }
    });
    
    // Root route - serve React app
    _server.on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        unsigned long startTime = millis();
        size_t freeHeap = ESP.getFreeHeap();
        LOG_I("/ hit - serving index.html (heap: %zu bytes)", freeHeap);
        
        // Pause cloud connection IMMEDIATELY to free network/memory
        if (_cloudConnection) {
            _cloudConnection->pause();
        }
        
        // Check memory before serving
        if (freeHeap < 20000) {
            LOG_W("Low heap (%zu bytes) - web response may be slow", freeHeap);
        }
        
        // Use direct file send - more efficient than beginResponse
        if (LittleFS.exists("/index.html")) {
            request->send(LittleFS, "/index.html", "text/html", false);
            LOG_I("/ served in %lu ms", millis() - startTime);
        } else {
            LOG_E("index.html not found!");
            request->send(404, "text/plain", "index.html not found");
        }
    });
    
    // NOTE: serveStatic is registered at the END of setupRoutes() to ensure
    // API routes have priority over static file serving
    
    // Captive portal detection routes - redirect to lightweight setup page
    // Android/Chrome
    _server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->redirect("/setup");
    });
    _server.on("/gen_204", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->redirect("/setup");
    });
    // Apple
    _server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->redirect("/setup");
    });
    _server.on("/library/test/success.html", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->redirect("/setup");
    });
    // Windows
    _server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->redirect("/setup");
    });
    _server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->redirect("/setup");
    });
    // Firefox
    _server.on("/success.txt", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->redirect("/setup");
    });
    // Generic captive portal check
    _server.on("/fwlink", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->redirect("/setup");
    });
    
    // API endpoints
    
    // Check if in AP mode (for WiFi setup detection)
    _server.on("/api/mode", HTTP_GET, [this](AsyncWebServerRequest* request) {
        // Delay serving if WiFi just connected (prevents PSRAM crashes)
        // Use const char* to avoid String allocation in PSRAM
        if (!_wifiManager.isAPMode() && !isWiFiReady()) {
            const char* error = "{\"error\":\"WiFi initializing, please wait\"}";
            request->send(503, "application/json", error);
            return;
        }
        
        // Use stack allocation to avoid PSRAM crashes
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        StaticJsonDocument<256> doc;
        #pragma GCC diagnostic pop
        doc["mode"] = "local";
        doc["apMode"] = _wifiManager.isAPMode();
        // Copy hostname to stack buffer to avoid PSRAM
        // Use a local scope to ensure String is destroyed before any other operations
        char hostnameBuf[64];
        {
            String hostnameStr = WiFi.getHostname();
            if (hostnameStr.length() > 0) {
                strncpy(hostnameBuf, hostnameStr.c_str(), sizeof(hostnameBuf) - 1);
                hostnameBuf[sizeof(hostnameBuf) - 1] = '\0';
            } else {
                strncpy(hostnameBuf, "brewos", sizeof(hostnameBuf) - 1);
                hostnameBuf[sizeof(hostnameBuf) - 1] = '\0';
            }
            // String destructor runs here at end of scope, before any other operations
        }
        doc["hostname"] = hostnameBuf;
        
        // Serialize JSON to buffer in internal RAM (not PSRAM)
        size_t jsonSize = measureJson(doc) + 1;
        char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!jsonBuffer) {
            jsonBuffer = (char*)malloc(jsonSize);
        }
        
        if (jsonBuffer) {
            serializeJson(doc, jsonBuffer, jsonSize);
            request->send(200, "application/json", jsonBuffer);
            free(jsonBuffer);
        } else {
            request->send(500, "application/json", "{\"error\":\"Out of memory\"}");
        }
    });
    
    // API info endpoint - provides version and feature detection for web UI compatibility
    // This is the primary endpoint for version negotiation between web UI and backend
    _server.on("/api/info", HTTP_GET, [this](AsyncWebServerRequest* request) {
        // Delay serving if WiFi just connected (prevents PSRAM crashes)
        // Use const char* to avoid String allocation in PSRAM
        if (!_wifiManager.isAPMode() && !isWiFiReady()) {
            const char* error = "{\"error\":\"WiFi initializing, please wait\"}";
            request->send(503, "application/json", error);
            return;
        }
        
        // Use stack allocation to avoid PSRAM crashes
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        StaticJsonDocument<1024> doc;
        #pragma GCC diagnostic pop
        
        // API version - increment ONLY for breaking changes to REST/WebSocket APIs
        // Web UI checks this to determine compatibility
        doc["apiVersion"] = 1;
        
        // Component versions
        doc["firmwareVersion"] = ESP32_VERSION;
        doc["webVersion"] = ESP32_VERSION;  // Web UI bundled with this firmware
        doc["protocolVersion"] = PROTOCOL_VERSION;
        
        // ESP32 build timestamp (for dev builds)
        char esp32Build[24];
        snprintf(esp32Build, sizeof(esp32Build), "%s %s", BUILD_DATE, BUILD_TIME);
        doc["buildDate"] = esp32Build;
        
        // Pico version (if connected) - with safety check
        if (_picoUart.isConnected()) {
            doc["picoConnected"] = true;
            // Safely get Pico version - State might not be fully initialized
            const char* picoVer = State.getPicoVersion();
            if (picoVer && picoVer[0] != '\0') {
                doc["picoVersion"] = picoVer;
            }
            // Pico build timestamp
            const char* picoBuild = State.getPicoBuildDate();
            if (picoBuild && picoBuild[0] != '\0') {
                doc["picoBuildDate"] = picoBuild;
            }
        } else {
            doc["picoConnected"] = false;
        }
        
        // Mode detection
        doc["mode"] = "local";
        doc["apMode"] = _wifiManager.isAPMode();
        
        // Feature flags - granular capability detection
        // Web UI uses these to conditionally show/hide features
        JsonArray features = doc["features"].to<JsonArray>();
        
        // Core features (always available)
        features.add("temperature_control");
        features.add("pressure_monitoring");
        features.add("power_monitoring");
        
        // Advanced features
        features.add("bbw");              // Brew-by-weight
        features.add("scale");            // BLE scale support
        features.add("mqtt");             // MQTT integration
        features.add("eco_mode");         // Eco mode
        features.add("statistics");       // Statistics tracking
        features.add("schedules");        // Schedule management
        
        // OTA features
        features.add("pico_ota");         // Pico firmware updates
        features.add("esp32_ota");        // ESP32 firmware updates
        
        // Debug features
        features.add("debug_console");    // Debug console
        features.add("protocol_debug");   // Protocol debugging
        
        // Device info - get MAC and hostname directly to avoid String allocations
        // WiFi.macAddress() and WiFi.getHostname() return String, but we copy immediately
        // to minimize PSRAM exposure
        uint8_t mac[6];
        WiFi.macAddress(mac);
        char macBuf[18];  // MAC address format: "XX:XX:XX:XX:XX:XX" + null
        snprintf(macBuf, sizeof(macBuf), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        
        // Get hostname - must use String but copy immediately to minimize PSRAM exposure
        // Use a local scope to ensure String is destroyed before any other operations
        char hostnameBuf[64];
        {
            String hostnameStr = WiFi.getHostname();
            if (hostnameStr.length() > 0) {
                strncpy(hostnameBuf, hostnameStr.c_str(), sizeof(hostnameBuf) - 1);
                hostnameBuf[sizeof(hostnameBuf) - 1] = '\0';
            } else {
                strncpy(hostnameBuf, "brewos", sizeof(hostnameBuf) - 1);
                hostnameBuf[sizeof(hostnameBuf) - 1] = '\0';
            }
            // String destructor runs here at end of scope, before any other operations
        }
        
        doc["deviceId"] = macBuf;
        doc["hostname"] = hostnameBuf;
        
        // Serialize JSON to buffer in internal RAM (not PSRAM)
        size_t jsonSize = measureJson(doc) + 1;
        char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!jsonBuffer) {
            jsonBuffer = (char*)malloc(jsonSize);
        }
        
        if (jsonBuffer) {
            serializeJson(doc, jsonBuffer, jsonSize);
            request->send(200, "application/json", jsonBuffer);
            free(jsonBuffer);
        } else {
            request->send(500, "application/json", "{\"error\":\"Out of memory\"}");
        }
    });
    
    _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetStatus(request);
    });
    
    // ==========================================================================
    // Statistics API endpoints
    // ==========================================================================
    
    // Get full statistics
    _server.on("/api/stats", HTTP_GET, [](AsyncWebServerRequest* request) {
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        StaticJsonDocument<2048> doc;
        #pragma GCC diagnostic pop
        BrewOS::FullStatistics stats;
        Stats.getFullStatistics(stats);
        
        JsonObject obj = doc.to<JsonObject>();
        stats.toJson(obj);
        
        
        // Allocate JSON buffer in internal RAM (not PSRAM)
        size_t jsonSize = measureJson(doc) + 1;
        char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!jsonBuffer) {
            jsonBuffer = (char*)malloc(jsonSize);
        }
        
        if (jsonBuffer) {
            serializeJson(doc, jsonBuffer, jsonSize);
            request->send(200, "application/json", jsonBuffer);
            free(jsonBuffer);
        } else {
            request->send(500, "application/json", "{\"error\":\"Out of memory\"}");
        }
    });
    
    // Get extended statistics with history data
    _server.on("/api/stats/extended", HTTP_GET, [](AsyncWebServerRequest* request) {
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        StaticJsonDocument<2048> doc;
        #pragma GCC diagnostic pop
        
        // Get full statistics
        BrewOS::FullStatistics stats;
        Stats.getFullStatistics(stats);
        
        JsonObject statsObj = doc["stats"].to<JsonObject>();
        stats.toJson(statsObj);
        
        // Weekly chart data
        JsonArray weeklyArr = doc["weekly"].to<JsonArray>();
        Stats.getWeeklyBrewChart(weeklyArr);
        
        // Hourly distribution
        JsonArray hourlyArr = doc["hourlyDistribution"].to<JsonArray>();
        Stats.getHourlyDistribution(hourlyArr);
        
        // Brew history (last 50)
        JsonArray brewArr = doc["brewHistory"].to<JsonArray>();
        Stats.getBrewHistory(brewArr, 50);
        
        // Power history (last 24 hours)
        JsonArray powerArr = doc["powerHistory"].to<JsonArray>();
        Stats.getPowerHistory(powerArr);
        
        // Daily history (last 30 days)
        JsonArray dailyArr = doc["dailyHistory"].to<JsonArray>();
        Stats.getDailyHistory(dailyArr);
        
        
        // Allocate JSON buffer in internal RAM (not PSRAM)
        size_t jsonSize = measureJson(doc) + 1;
        char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!jsonBuffer) {
            jsonBuffer = (char*)malloc(jsonSize);
        }
        
        if (jsonBuffer) {
            serializeJson(doc, jsonBuffer, jsonSize);
            request->send(200, "application/json", jsonBuffer);
            free(jsonBuffer);
        } else {
            request->send(500, "application/json", "{\"error\":\"Out of memory\"}");
        }
    });
    
    // Get brew history
    _server.on("/api/stats/brews", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        
        // Check for limit parameter
        size_t limit = 50;
        if (request->hasParam("limit")) {
            limit = request->getParam("limit")->value().toInt();
            if (limit > 200) limit = 200;
        }
        
        Stats.getBrewHistory(arr, limit);
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Get power history
    _server.on("/api/stats/power", HTTP_GET, [](AsyncWebServerRequest* request) {
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        StaticJsonDocument<2048> doc;
        #pragma GCC diagnostic pop
        JsonArray arr = doc.to<JsonArray>();
        Stats.getPowerHistory(arr);
        
        
        // Allocate JSON buffer in internal RAM (not PSRAM)
        size_t jsonSize = measureJson(doc) + 1;
        char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!jsonBuffer) {
            jsonBuffer = (char*)malloc(jsonSize);
        }
        
        if (jsonBuffer) {
            serializeJson(doc, jsonBuffer, jsonSize);
            request->send(200, "application/json", jsonBuffer);
            free(jsonBuffer);
        } else {
            request->send(500, "application/json", "{\"error\":\"Out of memory\"}");
        }
    });
    
    // Reset statistics (with confirmation)
    _server.on("/api/stats/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
        Stats.resetAll();
        broadcastLog("Statistics reset", static_cast<const char*>("warn"));
        request->send(200, "application/json", "{\"status\":\"ok\"}");
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
    
    // Filesystem space check endpoint
    _server.on("/api/filesystem/space", HTTP_GET, [](AsyncWebServerRequest* request) {
        size_t used = LittleFS.usedBytes();
        size_t total = LittleFS.totalBytes();
        size_t free = total - used;
        
        char response[128];
        snprintf(response, sizeof(response), 
            "{\"used\":%u,\"total\":%u,\"free\":%u,\"usedPercent\":%.1f}", 
            (unsigned)used, (unsigned)total, (unsigned)free, (used * 100.0f / total));
        request->send(200, "application/json", response);
    });
    
    _server.on("/api/pico/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
        _picoUart.resetPico();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // Setup status endpoint (check if first-run wizard is complete)
    _server.on("/api/setup/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        bool complete = State.settings().system.setupComplete;
        char response[64];
        snprintf(response, sizeof(response), "{\"complete\":%s}", complete ? "true" : "false");
        request->send(200, "application/json", response);
    });
    
    // Setup complete endpoint (marks first-run wizard as done)
    // Note: No auth required - this endpoint is only accessible on local network
    // during initial device setup before WiFi is configured
    _server.on("/api/setup/complete", HTTP_POST, [](AsyncWebServerRequest* request) {
        // Only allow if not already completed (prevent re-triggering)
        if (State.settings().system.setupComplete) {
            request->send(200, "application/json", "{\"success\":true,\"alreadyComplete\":true}");
            return;
        }
        
        State.settings().system.setupComplete = true;
        
        // Save all settings to ensure everything configured during wizard is persisted
        // This includes machine info, power settings, cloud settings, etc.
        State.saveSettings();
        
        LOG_I("Setup wizard completed - all settings saved");
        request->send(200, "application/json", "{\"success\":true}");
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
        bbw_settings_t settings = brewByWeight ? brewByWeight->getSettings() : bbw_settings_t{};
        
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
                brewByWeight->setTargetWeight(doc["target_weight"].as<float>());
            }
            if (!doc["dose_weight"].isNull()) {
                brewByWeight->setDoseWeight(doc["dose_weight"].as<float>());
            }
            if (!doc["stop_offset"].isNull()) {
                brewByWeight->setStopOffset(doc["stop_offset"].as<float>());
            }
            if (!doc["auto_stop"].isNull()) {
                brewByWeight->setAutoStop(doc["auto_stop"].as<bool>());
            }
            if (!doc["auto_tare"].isNull()) {
                brewByWeight->setAutoTare(doc["auto_tare"].as<bool>());
            }
            
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        }
    );
    
    _server.on("/api/scale/state", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        bbw_state_t state = brewByWeight ? brewByWeight->getState() : bbw_state_t{};
        bbw_settings_t settings = brewByWeight ? brewByWeight->getSettings() : bbw_settings_t{};
        
        doc["active"] = state.active;
        doc["current_weight"] = state.current_weight;
        doc["target_weight"] = settings.target_weight;
        doc["progress"] = brewByWeight ? brewByWeight->getProgress() : 0.0f;
        doc["ratio"] = brewByWeight ? brewByWeight->getCurrentRatio() : 0.0f;
        doc["target_reached"] = state.target_reached;
        doc["stop_signaled"] = state.stop_signaled;
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    _server.on("/api/scale/tare", HTTP_POST, [](AsyncWebServerRequest* request) {
        scaleManager->tare();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // Scale connection status
    _server.on("/api/scale/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        scale_state_t state = scaleManager ? scaleManager->getState() : scale_state_t{};
        
        doc["connected"] = scaleManager ? scaleManager->isConnected() : false;
        doc["scanning"] = scaleManager ? scaleManager->isScanning() : false;
        doc["name"] = scaleManager ? scaleManager->getScaleName() : "";
        doc["type"] = scaleManager ? (int)scaleManager->getScaleType() : 0;
        doc["type_name"] = scaleManager ? getScaleTypeName(scaleManager->getScaleType()) : "";
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
        if (scaleManager && scaleManager->isScanning()) {
            request->send(400, "application/json", "{\"error\":\"Already scanning\"}");
            return;
        }
        if (scaleManager && scaleManager->isConnected()) {
            scaleManager->disconnect();
        }
        scaleManager->clearDiscovered();
        scaleManager->startScan(15000);  // 15 second scan
        broadcastLogLevel("info", "BLE scale scan started");
        request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Scanning...\"}");
    });
    
    // Stop BLE scan
    _server.on("/api/scale/scan/stop", HTTP_POST, [](AsyncWebServerRequest* request) {
        scaleManager->stopScan();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // Get discovered scales
    _server.on("/api/scale/devices", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        JsonArray devices = doc["devices"].to<JsonArray>();
        
        static const std::vector<scale_info_t> empty_devices;
        const auto& discovered = scaleManager ? scaleManager->getDiscoveredScales() : empty_devices;
        for (size_t i = 0; i < discovered.size(); i++) {
            JsonObject device = devices.add<JsonObject>();
            device["index"] = i;
            device["name"] = discovered[i].name;
            device["address"] = discovered[i].address;
            device["type"] = (int)discovered[i].type;
            device["type_name"] = getScaleTypeName(discovered[i].type);
            device["rssi"] = discovered[i].rssi;
        }
        
        doc["scanning"] = scaleManager ? scaleManager->isScanning() : false;
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
                    success = scaleManager ? scaleManager->connect(addr) : false;
                }
            } else if (!doc["index"].isNull()) {
                // Connect by index from discovered list
                int idx = doc["index"].as<int>();
                success = scaleManager ? scaleManager->connectByIndex(idx) : false;
            } else {
                // Try to reconnect to saved scale
                success = scaleManager ? scaleManager->connect(nullptr) : false;
            }
            
            if (success) {
                broadcastLogLevel("info", "Connecting to scale...");
                request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Connecting...\"}");
            } else {
                request->send(400, "application/json", "{\"error\":\"Connection failed\"}");
            }
        }
    );
    
    // Disconnect from scale
    _server.on("/api/scale/disconnect", HTTP_POST, [](AsyncWebServerRequest* request) {
        scaleManager->disconnect();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // Forget saved scale
    _server.on("/api/scale/forget", HTTP_POST, [this](AsyncWebServerRequest* request) {
        scaleManager->forgetScale();
        broadcastLogLevel("info", "Scale forgotten");
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // Timer control (for scales that support it)
    _server.on("/api/scale/timer/start", HTTP_POST, [](AsyncWebServerRequest* request) {
        scaleManager->startTimer();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    _server.on("/api/scale/timer/stop", HTTP_POST, [](AsyncWebServerRequest* request) {
        scaleManager->stopTimer();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    _server.on("/api/scale/timer/reset", HTTP_POST, [](AsyncWebServerRequest* request) {
        scaleManager->resetTimer();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // ==========================================================================
    // Schedule API endpoints
    // ==========================================================================
    
    // Get all schedules
    _server.on("/api/schedules", HTTP_GET, [](AsyncWebServerRequest* request) {
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        StaticJsonDocument<2048> doc;
        #pragma GCC diagnostic pop
        JsonObject obj = doc.to<JsonObject>();
        State.settings().schedule.toJson(obj);
        
        
        // Allocate JSON buffer in internal RAM (not PSRAM)
        size_t jsonSize = measureJson(doc) + 1;
        char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!jsonBuffer) {
            jsonBuffer = (char*)malloc(jsonSize);
        }
        
        if (jsonBuffer) {
            serializeJson(doc, jsonBuffer, jsonSize);
            request->send(200, "application/json", jsonBuffer);
            free(jsonBuffer);
        } else {
            request->send(500, "application/json", "{\"error\":\"Out of memory\"}");
        }
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
                broadcastLog("Schedule added: %s", entry.name);
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
                broadcastLog("Schedule updated: %s", entry.name);
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
                broadcastLogLevel("info", "Schedule deleted");
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
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        StaticJsonDocument<2048> doc;
        #pragma GCC diagnostic pop
        doc["enabled"] = State.getAutoPowerOffEnabled();
        doc["minutes"] = State.getAutoPowerOffMinutes();
        
        
        // Allocate JSON buffer in internal RAM (not PSRAM)
        size_t jsonSize = measureJson(doc) + 1;
        char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!jsonBuffer) {
            jsonBuffer = (char*)malloc(jsonSize);
        }
        
        if (jsonBuffer) {
            serializeJson(doc, jsonBuffer, jsonSize);
            request->send(200, "application/json", jsonBuffer);
            free(jsonBuffer);
        } else {
            request->send(500, "application/json", "{\"error\":\"Out of memory\"}");
        }
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
            broadcastLog("Auto power-off: %s (%d min)", enabled ? "enabled" : "disabled", minutes);
        }
    );
    
    // ==========================================================================
    // Time/NTP API endpoints
    // ==========================================================================
    
    // Get time status and settings
    _server.on("/api/time", HTTP_GET, [this](AsyncWebServerRequest* request) {
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        StaticJsonDocument<2048> doc;
        #pragma GCC diagnostic pop
        
        // Current time status
        TimeStatus timeStatus = _wifiManager.getTimeStatus();
        doc["synced"] = timeStatus.ntpSynced;
        doc["currentTime"] = timeStatus.currentTime;
        doc["timezone"] = timeStatus.timezone;
        doc["utcOffset"] = timeStatus.utcOffset;
        
        // Settings
        JsonObject settings = doc["settings"].to<JsonObject>();
        State.settings().time.toJson(settings);
        
        
        // Allocate JSON buffer in internal RAM (not PSRAM)
        size_t jsonSize = measureJson(doc) + 1;
        char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!jsonBuffer) {
            jsonBuffer = (char*)malloc(jsonSize);
        }
        
        if (jsonBuffer) {
            serializeJson(doc, jsonBuffer, jsonSize);
            request->send(200, "application/json", jsonBuffer);
            free(jsonBuffer);
        } else {
            request->send(500, "application/json", "{\"error\":\"Out of memory\"}");
        }
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
            broadcastLogLevel("info", "Time settings updated");
        }
    );
    
    // Force NTP sync
    _server.on("/api/time/sync", HTTP_POST, [this](AsyncWebServerRequest* request) {
        // Check if WiFi is connected (NTP requires internet)
        if (!_wifiManager.isConnected()) {
            request->send(503, "application/json", "{\"error\":\"WiFi not connected\"}");
            return;
        }
        
        _wifiManager.syncNTP();
        request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"NTP sync initiated\"}");
        broadcastLogLevel("info", "NTP sync initiated");
    });
    
    // Handle OPTIONS for CORS preflight
    _server.on("/api/time/sync", HTTP_OPTIONS, [](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", "");
        response->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
        response->addHeader("Access-Control-Allow-Headers", "Content-Type");
        request->send(response);
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
            // Pico expects: [target:1][temperature:int16] where temperature is Celsius * 10
            // Note: Pico (RP2350) is little-endian, so send LSB first
            uint8_t payload[3];
            payload[0] = 0x00;  // 0=brew
            int16_t tempScaled = (int16_t)(temp * 10.0f);
            payload[1] = tempScaled & 0xFF;         // LSB first
            payload[2] = (tempScaled >> 8) & 0xFF;  // MSB second
            if (_picoUart.sendCommand(MSG_CMD_SET_TEMP, payload, 3)) {
                broadcastLog("Brew temp set to %.1f°C", temp);
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
            // Pico expects: [target:1][temperature:int16] where temperature is Celsius * 10
            // Note: Pico (RP2350) is little-endian, so send LSB first
            uint8_t payload[3];
            payload[0] = 0x01;  // 1=steam
            int16_t tempScaled = (int16_t)(temp * 10.0f);
            payload[1] = tempScaled & 0xFF;         // LSB first
            payload[2] = (tempScaled >> 8) & 0xFF;  // MSB second
            if (_picoUart.sendCommand(MSG_CMD_SET_TEMP, payload, 3)) {
                broadcastLog("Steam temp set to %.1f°C", temp);
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
                // Validate machine state before allowing turn on
                // Only allow turning on from IDLE, READY, or ECO states
                uint8_t currentState = machineState.machine_state;
                if (currentState != UI_STATE_IDLE && 
                    currentState != UI_STATE_READY && 
                    currentState != UI_STATE_ECO) {
                    const char* stateNames[] = {"INIT", "IDLE", "HEATING", "READY", "BREWING", "FAULT", "SAFE", "ECO"};
                    char errorMsg[128];
                    snprintf(errorMsg, sizeof(errorMsg), 
                        "{\"error\":\"Cannot turn on machine: current state is %s. Machine must be in IDLE, READY, or ECO state.\"}",
                        (currentState < 8) ? stateNames[currentState] : "UNKNOWN");
                    request->send(400, "application/json", errorMsg);
                    return;
                }
                cmd = 0x01;  // Turn on
            } else if (mode == "off" || mode == "standby") {
                cmd = 0x00;  // Turn off
            } else {
                request->send(400, "application/json", "{\"error\":\"Invalid mode (use: on, off, ready, standby)\"}");
                return;
            }
            
            if (_picoUart.sendCommand(MSG_CMD_MODE, &cmd, 1)) {
                // Pre-format message to avoid String.c_str() issues
                char modeMsg[64];
                snprintf(modeMsg, sizeof(modeMsg), "Machine mode set to: %s", mode.c_str());
                broadcastLog(modeMsg);
                
                // If setting to standby/idle, immediately force state to IDLE
                // This prevents the machine from staying in cooldown state
                // The state will be updated from Pico status packets, but this ensures
                // immediate response to the user command
                if (cmd == 0x00) {  // MODE_IDLE
                    machineState.machine_state = UI_STATE_IDLE;
                    machineState.is_heating = false;
                }
                
                request->send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                request->send(500, "application/json", "{\"error\":\"Failed to send command\"}");
            }
        }
    );
    
    // Cloud status endpoint
    _server.on("/api/cloud/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        auto& cloudSettings = State.settings().cloud;
        
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        StaticJsonDocument<2048> doc;
        #pragma GCC diagnostic pop
        doc["enabled"] = cloudSettings.enabled;
        doc["connected"] = _cloudConnection ? _cloudConnection->isConnected() : false;
        doc["serverUrl"] = cloudSettings.serverUrl;
        
        
        // Allocate JSON buffer in internal RAM (not PSRAM)
        size_t jsonSize = measureJson(doc) + 1;
        char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!jsonBuffer) {
            jsonBuffer = (char*)malloc(jsonSize);
        }
        
        if (jsonBuffer) {
            serializeJson(doc, jsonBuffer, jsonSize);
            request->send(200, "application/json", jsonBuffer);
            free(jsonBuffer);
        } else {
            request->send(500, "application/json", "{\"error\":\"Out of memory\"}");
        }
    });
    
    // Push notification preferences endpoint (GET)
    _server.on("/api/push/preferences", HTTP_GET, [this](AsyncWebServerRequest* request) {
        auto& notifSettings = State.settings().notifications;
        
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        StaticJsonDocument<2048> doc;
        #pragma GCC diagnostic pop
        doc["machineReady"] = notifSettings.machineReady;
        doc["waterEmpty"] = notifSettings.waterEmpty;
        doc["descaleDue"] = notifSettings.descaleDue;
        doc["serviceDue"] = notifSettings.serviceDue;
        doc["backflushDue"] = notifSettings.backflushDue;
        doc["machineError"] = notifSettings.machineError;
        doc["picoOffline"] = notifSettings.picoOffline;
        doc["scheduleTriggered"] = notifSettings.scheduleTriggered;
        doc["brewComplete"] = notifSettings.brewComplete;
        
        
        // Allocate JSON buffer in internal RAM (not PSRAM)
        size_t jsonSize = measureJson(doc) + 1;
        char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!jsonBuffer) {
            jsonBuffer = (char*)malloc(jsonSize);
        }
        
        if (jsonBuffer) {
            serializeJson(doc, jsonBuffer, jsonSize);
            request->send(200, "application/json", jsonBuffer);
            free(jsonBuffer);
        } else {
            request->send(500, "application/json", "{\"error\":\"Out of memory\"}");
        }
    });
    
    // Push notification preferences endpoint (POST)
    _server.on(
        "/api/push/preferences",
        HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (index + len == total) {
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, data, len);
                
                if (error) {
                    request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                    return;
                }
                
                auto& notifSettings = State.settings().notifications;
                
                if (!doc["machineReady"].isNull()) notifSettings.machineReady = doc["machineReady"];
                if (!doc["waterEmpty"].isNull()) notifSettings.waterEmpty = doc["waterEmpty"];
                if (!doc["descaleDue"].isNull()) notifSettings.descaleDue = doc["descaleDue"];
                if (!doc["serviceDue"].isNull()) notifSettings.serviceDue = doc["serviceDue"];
                if (!doc["backflushDue"].isNull()) notifSettings.backflushDue = doc["backflushDue"];
                if (!doc["machineError"].isNull()) notifSettings.machineError = doc["machineError"];
                if (!doc["picoOffline"].isNull()) notifSettings.picoOffline = doc["picoOffline"];
                if (!doc["scheduleTriggered"].isNull()) notifSettings.scheduleTriggered = doc["scheduleTriggered"];
                if (!doc["brewComplete"].isNull()) notifSettings.brewComplete = doc["brewComplete"];
                
                State.saveNotificationSettings();
                
                request->send(200, "application/json", "{\"success\":true}");
            }
        }
    );
    
    // Pairing API endpoints
    _server.on("/api/pairing/qr", HTTP_GET, [this](AsyncWebServerRequest* request) {
        // Check if cloud is enabled
        auto& cloudSettings = State.settings().cloud;
        if (!cloudSettings.enabled || !_pairingManager) {
            request->send(503, "application/json", "{\"error\":\"Cloud integration not enabled\"}");
            return;
        }
        
        // Generate a new token if needed
        bool tokenGenerated = false;
        if (!_pairingManager->isTokenValid()) {
            _pairingManager->generateToken();
            tokenGenerated = true;
        }
        
        // Register token with cloud if newly generated (or not yet registered)
        // This ensures the token is valid in the cloud before user scans QR
        if (tokenGenerated && WiFi.isConnected()) {
            bool registered = _pairingManager->registerTokenWithCloud();
            if (!registered) {
                LOG_W("Failed to register pairing token with cloud");
                // Continue anyway - user can retry
            }
        }
        
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        StaticJsonDocument<2048> doc;
        #pragma GCC diagnostic pop
        doc["deviceId"] = _pairingManager->getDeviceId();
        doc["token"] = _pairingManager->getCurrentToken();
        doc["url"] = _pairingManager->getPairingUrl();
        doc["expiresIn"] = (_pairingManager->getTokenExpiry() - millis()) / 1000;
        
        
        // Allocate JSON buffer in internal RAM (not PSRAM)
        size_t jsonSize = measureJson(doc) + 1;
        char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!jsonBuffer) {
            jsonBuffer = (char*)malloc(jsonSize);
        }
        
        if (jsonBuffer) {
            serializeJson(doc, jsonBuffer, jsonSize);
            request->send(200, "application/json", jsonBuffer);
            free(jsonBuffer);
        } else {
            request->send(500, "application/json", "{\"error\":\"Out of memory\"}");
        }
    });
    
    _server.on("/api/pairing/refresh", HTTP_POST, [this](AsyncWebServerRequest* request) {
        // Check if cloud is enabled
        auto& cloudSettings = State.settings().cloud;
        if (!cloudSettings.enabled || !_pairingManager) {
            request->send(503, "application/json", "{\"error\":\"Cloud integration not enabled\"}");
            return;
        }
        
        _pairingManager->generateToken();
        
        // Register the new token with cloud immediately
        // This ensures the token is valid before user scans QR
        bool registered = false;
        if (WiFi.isConnected()) {
            registered = _pairingManager->registerTokenWithCloud();
            if (!registered) {
                LOG_W("Failed to register pairing token with cloud");
                // Continue anyway - user can retry
            }
        }
        
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        StaticJsonDocument<2048> doc;
        #pragma GCC diagnostic pop
        doc["deviceId"] = _pairingManager->getDeviceId();
        doc["token"] = _pairingManager->getCurrentToken();
        doc["url"] = _pairingManager->getPairingUrl();
        doc["expiresIn"] = 600;  // 10 minutes
        doc["registered"] = registered;  // Let UI know if registration succeeded
        
        
        // Allocate JSON buffer in internal RAM (not PSRAM)
        size_t jsonSize = measureJson(doc) + 1;
        char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!jsonBuffer) {
            jsonBuffer = (char*)malloc(jsonSize);
        }
        
        if (jsonBuffer) {
            serializeJson(doc, jsonBuffer, jsonSize);
            request->send(200, "application/json", jsonBuffer);
            free(jsonBuffer);
        } else {
            request->send(500, "application/json", "{\"error\":\"Out of memory\"}");
        }
    });
    
    // ==========================================================================
    // Diagnostics API endpoints
    // ==========================================================================
    
    // Run all diagnostic tests
    _server.on("/api/diagnostics/run", HTTP_POST, [this](AsyncWebServerRequest* request) {
        // Send command to Pico to run all diagnostics
        uint8_t payload[1] = { 0x00 };  // DIAG_TEST_ALL
        if (_picoUart.sendCommand(MSG_CMD_DIAGNOSTICS, payload, 1)) {
            broadcastLogLevel("info", "Running hardware diagnostics...");
            request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Diagnostics started\"}");
        } else {
            request->send(500, "application/json", "{\"error\":\"Failed to send diagnostic command\"}");
        }
    });
    
    // Run a single diagnostic test
    _server.on("/api/diagnostics/test", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }
            
            uint8_t testId = doc["testId"] | 0;
            uint8_t payload[1] = { testId };
            
            if (_picoUart.sendCommand(MSG_CMD_DIAGNOSTICS, payload, 1)) {
                broadcastLog("Running diagnostic test %d", testId);
                request->send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                request->send(500, "application/json", "{\"error\":\"Failed to send command\"}");
            }
        }
    );
    
    // ==========================================================================
    // Web Assets OTA Update
    // ==========================================================================
    
    // Start web OTA - cleans old assets and prepares for upload
    // This is called once at the beginning of a web update session
    _server.on("/api/ota/web/start", HTTP_POST, [this](AsyncWebServerRequest* request) {
        LOG_I("Starting web OTA - cleaning old assets...");
        int deleted = 0;
        
        // Remove files in /assets directory (hashed JS/CSS bundles)
        if (LittleFS.exists("/assets")) {
            File assets = LittleFS.open("/assets");
            File file = assets.openNextFile();
            std::vector<String> toDelete;
            while (file) {
                toDelete.push_back(String("/assets/") + file.name());
                file = assets.openNextFile();
            }
            for (const String& path : toDelete) {
                if (LittleFS.remove(path)) {
                    deleted++;
                }
            }
            LittleFS.rmdir("/assets");
        }
        
        // Remove root-level web files (keep system config files)
        const char* webFiles[] = {"index.html", "favicon.svg", "favicon.ico", "logo.png", 
                                   "logo-icon.svg", "manifest.json", "sw.js", "version-manifest.json"};
        for (const char* filename : webFiles) {
            String path = String("/") + filename;
            if (LittleFS.exists(path) && LittleFS.remove(path)) {
                deleted++;
            }
        }
        
        // Recreate assets directory
        LittleFS.mkdir("/assets");
        
        LOG_I("Cleaned %d old web files, ready for upload", deleted);
        
        char response[64];
        snprintf(response, sizeof(response), "{\"cleaned\":%d,\"status\":\"ready\"}", deleted);
        request->send(200, "application/json", response);
    });
    
    // Upload web asset file - called for each file in the web bundle
    _server.on("/api/ota/web/upload", HTTP_POST,
        [](AsyncWebServerRequest* request) {
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        },
        [this](AsyncWebServerRequest* request, const String& filename, size_t index, 
               uint8_t* data, size_t len, bool final) {
            static File uploadFile;
            String path = "/" + filename;
            
            if (index == 0) {
                uploadFile = LittleFS.open(path, "w");
                if (!uploadFile) {
                    LOG_E("Failed to open %s for writing", path.c_str());
                    return;
                }
            }
            
            if (uploadFile && len > 0) {
                uploadFile.write(data, len);
            }
            
            if (final && uploadFile) {
                uploadFile.close();
                LOG_D("Web OTA: %s (%u bytes)", path.c_str(), (unsigned)(index + len));
            }
        }
    );
    
    // Complete web OTA - logs completion
    _server.on("/api/ota/web/complete", HTTP_POST, [this](AsyncWebServerRequest* request) {
        size_t used = LittleFS.usedBytes();
        size_t total = LittleFS.totalBytes();
        LOG_I("Web OTA complete. Filesystem: %uKB / %uKB", (unsigned)(used/1024), (unsigned)(total/1024));
        broadcastLog("Web update complete");
        
        char response[96];
        snprintf(response, sizeof(response), 
            "{\"status\":\"complete\",\"used\":%u,\"total\":%u}", 
            (unsigned)used, (unsigned)total);
        request->send(200, "application/json", response);
    });
    
    // Use built-in serveStatic which is more efficient
    // It automatically handles Content-Length and caching
    _server.serveStatic("/", LittleFS, "/")
           .setDefaultFile("index.html")
           .setCacheControl("public, max-age=31536000, immutable");
    
    // Handle SPA fallback and API 404s
    _server.onNotFound([this](AsyncWebServerRequest* request) {
        String url = request->url();
        
        // API routes should return 404 if not found
        if (url.startsWith("/api/")) {
            request->send(404, "application/json", "{\"error\":\"Not found\"}");
            return;
        }
        
        // Asset files that don't exist should 404 (don't serve index.html)
        if (url.startsWith("/assets/") || url.endsWith(".js") || url.endsWith(".css") || url.endsWith(".png") || url.endsWith(".jpg") || url.endsWith(".ico")) {
            LOG_W("Asset not found: %s", url.c_str());
            request->send(404, "text/plain", "Not found");
            return;
        }
        
        // SPA fallback - serve index.html for React Router paths
        // (paths like /brewing, /stats, /settings, etc.)
        if (LittleFS.exists("/index.html")) {
            request->send(LittleFS, "/index.html", "text/html");
        } else {
            request->send(404, "text/plain", "index.html not found");
        }
    });
    
    LOG_I("Routes setup complete");
}

void WebServer::handleGetStatus(AsyncWebServerRequest* request) {
    // Delay serving if WiFi just connected (prevents PSRAM crashes)
    // Use const char* to avoid String allocation in PSRAM
    if (!_wifiManager.isAPMode() && !isWiFiReady()) {
        const char* error = "{\"error\":\"WiFi initializing, please wait\"}";
        request->send(503, "application/json", error);
        return;
    }
    
    // Use stack allocation to avoid PSRAM crashes
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    StaticJsonDocument<2048> doc;
    #pragma GCC diagnostic pop
    
    // WiFi status
    // CRITICAL: Copy WiFiStatus Strings to stack buffers immediately to avoid PSRAM pointer issues
    // WiFiStatus contains String fields that might be allocated in PSRAM
    WiFiStatus wifi = _wifiManager.getStatus();
    
    // Copy Strings to stack buffers (internal RAM) before using them
    char ssidBuf[64];
    char ipBuf[16];
    char gatewayBuf[16];
    char subnetBuf[16];
    char dns1Buf[16];
    char dns2Buf[16];
    
    strncpy(ssidBuf, wifi.ssid.c_str(), sizeof(ssidBuf) - 1);
    ssidBuf[sizeof(ssidBuf) - 1] = '\0';
    strncpy(ipBuf, wifi.ip.c_str(), sizeof(ipBuf) - 1);
    ipBuf[sizeof(ipBuf) - 1] = '\0';
    strncpy(gatewayBuf, wifi.gateway.c_str(), sizeof(gatewayBuf) - 1);
    gatewayBuf[sizeof(gatewayBuf) - 1] = '\0';
    strncpy(subnetBuf, wifi.subnet.c_str(), sizeof(subnetBuf) - 1);
    subnetBuf[sizeof(subnetBuf) - 1] = '\0';
    strncpy(dns1Buf, wifi.dns1.c_str(), sizeof(dns1Buf) - 1);
    dns1Buf[sizeof(dns1Buf) - 1] = '\0';
    strncpy(dns2Buf, wifi.dns2.c_str(), sizeof(dns2Buf) - 1);
    dns2Buf[sizeof(dns2Buf) - 1] = '\0';
    
    doc["wifi"]["mode"] = (int)wifi.mode;
    doc["wifi"]["ssid"] = ssidBuf;
    doc["wifi"]["ip"] = ipBuf;
    doc["wifi"]["rssi"] = wifi.rssi;
    doc["wifi"]["configured"] = wifi.configured;
    doc["wifi"]["staticIp"] = wifi.staticIp;
    doc["wifi"]["gateway"] = gatewayBuf;
    doc["wifi"]["subnet"] = subnetBuf;
    doc["wifi"]["dns1"] = dns1Buf;
    doc["wifi"]["dns2"] = dns2Buf;
    
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
    // Copy MQTT status string to stack buffer to avoid PSRAM
    String mqttStatusStr = _mqttClient.getStatusString();
    char mqttStatusBuf[32];
    strncpy(mqttStatusBuf, mqttStatusStr.c_str(), sizeof(mqttStatusBuf) - 1);
    mqttStatusBuf[sizeof(mqttStatusBuf) - 1] = '\0';
    doc["mqtt"]["status"] = mqttStatusBuf;
    
    // Scale status
    doc["scale"]["connected"] = scaleManager ? scaleManager->isConnected() : false;
    doc["scale"]["scanning"] = scaleManager ? scaleManager->isScanning() : false;
    // Copy scale name to stack buffer to avoid PSRAM
    String scaleNameStr = scaleManager ? scaleManager->getScaleName() : "";
    char scaleNameBuf[64];
    strncpy(scaleNameBuf, scaleNameStr.c_str(), sizeof(scaleNameBuf) - 1);
    scaleNameBuf[sizeof(scaleNameBuf) - 1] = '\0';
    doc["scale"]["name"] = scaleNameBuf;
    if (scaleManager && scaleManager->isConnected()) {
        scale_state_t scaleState = scaleManager->getState();
        doc["scale"]["weight"] = scaleState.weight;
        doc["scale"]["flow_rate"] = scaleState.flow_rate;
        doc["scale"]["stable"] = scaleState.stable;
    }
    
    // WebSocket clients
    doc["clients"] = getClientCount();
    
    // Setup status
    doc["setupComplete"] = State.settings().system.setupComplete;
    
    // Serialize JSON to buffer in internal RAM (not PSRAM)
    size_t jsonSize = measureJson(doc) + 1;
    char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!jsonBuffer) {
        jsonBuffer = (char*)malloc(jsonSize);
    }
    
    if (jsonBuffer) {
        serializeJson(doc, jsonBuffer, jsonSize);
        request->send(200, "application/json", jsonBuffer);
        // Buffer will be freed by AsyncWebServer after response
    } else {
        request->send(500, "application/json", "{\"error\":\"Out of memory\"}");
    }
}

// Static cache for async WiFi scan results
static bool _scanInProgress = false;
static bool _scanResultsReady = false;
static int _cachedNetworkCount = 0;
static unsigned long _lastScanTime = 0;
static const unsigned long SCAN_CACHE_TIMEOUT_MS = 30000;  // Cache results for 30 seconds

void WebServer::handleGetWiFiNetworks(AsyncWebServerRequest* request) {
    // Use cached results if available and fresh
    unsigned long now = millis();
    
    // If we have fresh cached results, return them immediately
    if (_scanResultsReady && (now - _lastScanTime < SCAN_CACHE_TIMEOUT_MS)) {
        LOG_I("Returning cached WiFi scan results (%d networks)", _cachedNetworkCount);
        
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        StaticJsonDocument<2048> doc;
        #pragma GCC diagnostic pop
        JsonArray networks = doc["networks"].to<JsonArray>();
        
        int n = WiFi.scanComplete();
        if (n > 0) {
            int maxNetworks = (n > 20) ? 20 : n;
            for (int i = 0; i < maxNetworks; i++) {
                String ssid = WiFi.SSID(i);
                if (ssid.length() > 0) {
                    JsonObject network = networks.add<JsonObject>();
                    network["ssid"] = ssid;
                    network["rssi"] = WiFi.RSSI(i);
                    network["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
                }
            }
        }
        
        size_t jsonSize = measureJson(doc) + 1;
        char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!jsonBuffer) jsonBuffer = (char*)malloc(jsonSize);
        if (jsonBuffer) {
            serializeJson(doc, jsonBuffer, jsonSize);
            request->send(200, "application/json", jsonBuffer);
            free(jsonBuffer);
        } else {
            request->send(500, "application/json", "{\"error\":\"Out of memory\"}");
        }
        return;
    }
    
    // Check if async scan is complete
    int scanResult = WiFi.scanComplete();
    
    if (scanResult == WIFI_SCAN_RUNNING) {
        // Scan still in progress - return status
        LOG_I("WiFi scan in progress...");
        request->send(202, "application/json", "{\"status\":\"scanning\",\"networks\":[]}");
        return;
    }
    
    if (scanResult >= 0) {
        // Scan complete - return results
        LOG_I("WiFi scan complete, found %d networks", scanResult);
        _scanResultsReady = true;
        _cachedNetworkCount = scanResult;
        _lastScanTime = now;
        _scanInProgress = false;
        
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        StaticJsonDocument<2048> doc;
        #pragma GCC diagnostic pop
        JsonArray networks = doc["networks"].to<JsonArray>();
        
        int maxNetworks = (scanResult > 20) ? 20 : scanResult;
        for (int i = 0; i < maxNetworks; i++) {
            String ssid = WiFi.SSID(i);
            if (ssid.length() > 0) {
                JsonObject network = networks.add<JsonObject>();
                network["ssid"] = ssid;
                network["rssi"] = WiFi.RSSI(i);
                network["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
            }
        }
        
        size_t jsonSize = measureJson(doc) + 1;
        char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!jsonBuffer) jsonBuffer = (char*)malloc(jsonSize);
        if (jsonBuffer) {
            serializeJson(doc, jsonBuffer, jsonSize);
            request->send(200, "application/json", jsonBuffer);
            free(jsonBuffer);
        } else {
            request->send(500, "application/json", "{\"error\":\"Out of memory\"}");
        }
        return;
    }
    
    // No scan in progress and no results - start async scan
    LOG_I("Starting async WiFi scan...");
    _scanInProgress = true;
    _scanResultsReady = false;
    
    // Switch to AP+STA mode if in pure AP mode
    bool wasAPMode = _wifiManager.isAPMode();
    if (wasAPMode && WiFi.getMode() == WIFI_AP) {
        WiFi.mode(WIFI_AP_STA);
        delay(100);  // Short delay for mode switch
    }
    
    // Clear previous results and start ASYNC scan (non-blocking!)
    WiFi.scanDelete();
    WiFi.scanNetworks(true, false);  // async=true, show_hidden=false
    
    // Return "scanning" status - client should poll again
    request->send(202, "application/json", "{\"status\":\"scanning\",\"networks\":[]}");
}

void WebServer::handleSetWiFi(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    // Use stack allocation to avoid PSRAM crashes
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    StaticJsonDocument<512> doc;
    #pragma GCC diagnostic pop
    DeserializationError error = deserializeJson(doc, data, len);
    
    if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    String ssid = doc["ssid"] | "";
    String password = doc["password"] | "";
    
    if (_wifiManager.setCredentials(ssid, password)) {
        // Send response first - connection will happen in loop() after delay
        request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Connecting...\"}");
        
        // Set flag to trigger connection in the next loop iteration
        // This gives time for the HTTP response to be fully sent
        _pendingWiFiConnect = true;
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
        
        // Check available space before starting upload
        size_t freeSpace = LittleFS.totalBytes() - LittleFS.usedBytes();
        if (totalSize > freeSpace) {
            LOG_E("Not enough space: need %d bytes, have %d bytes", totalSize, freeSpace);
            broadcastLogLevel("error", "Upload failed: Not enough storage space");
            request->send(507, "application/json", "{\"error\":\"Not enough storage space\"}");
            return;
        }
        
        // Delete old firmware if exists to free up space
        if (LittleFS.exists(OTA_FILE_PATH)) {
            LittleFS.remove(OTA_FILE_PATH);
            // Recalculate free space after deletion
            freeSpace = LittleFS.totalBytes() - LittleFS.usedBytes();
            if (totalSize > freeSpace) {
                LOG_E("Still not enough space after cleanup: need %d bytes, have %d bytes", totalSize, freeSpace);
                broadcastLogLevel("error", "Upload failed: Not enough storage space (even after cleanup)");
                request->send(507, "application/json", "{\"error\":\"Not enough storage space\"}");
                return;
            }
        }
        
        LOG_I("Available space: %d bytes, required: %d bytes", freeSpace, totalSize);
        
        otaFile = LittleFS.open(OTA_FILE_PATH, "w");
        if (!otaFile) {
            LOG_E("Failed to open OTA file for writing");
            broadcastLogLevel("error", "Upload failed: Cannot create file");
            request->send(500, "application/json", "{\"error\":\"Failed to open file\"}");
            return;
        }
    }
    
    if (otaFile && len > 0) {
        size_t written = otaFile.write(data, len);
        if (written != len) {
            LOG_E("Failed to write all data: %d/%d (filesystem may be full)", written, len);
            // Close file and abort upload if write fails
            otaFile.close();
            LittleFS.remove(OTA_FILE_PATH);  // Clean up partial file
            broadcastLogLevel("error", "Upload failed: Filesystem full or write error");
            request->send(507, "application/json", "{\"error\":\"Filesystem full\"}");
            return;
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
            
            // Use heap_caps_malloc to avoid PSRAM
            size_t jsonSize = measureJson(doc) + 1;
            char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (!jsonBuffer) jsonBuffer = (char*)malloc(jsonSize);
            if (jsonBuffer) {
                serializeJson(doc, jsonBuffer, jsonSize);
                _ws.textAll(jsonBuffer);
                free(jsonBuffer);
            }
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
                broadcastLogLevel("error", "Upload failed: file size mismatch");
                uploadSuccess = false;
            }
        } else {
            LOG_E("Failed to verify uploaded file");
            broadcastLogLevel("error", "Upload failed: file verification error");
            uploadSuccess = false;
        }
        
        // Notify clients using stack allocation
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        StaticJsonDocument<256> finalDoc;
        #pragma GCC diagnostic pop
        finalDoc["type"] = "ota_progress";
        finalDoc["stage"] = "upload";
        finalDoc["progress"] = uploadSuccess ? 100 : 0;
        finalDoc["uploaded"] = uploadedSize;
        finalDoc["total"] = totalSize;
        finalDoc["success"] = uploadSuccess;
        
        size_t jsonSize = measureJson(finalDoc) + 1;
        char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!jsonBuffer) jsonBuffer = (char*)malloc(jsonSize);
        if (jsonBuffer) {
            serializeJson(finalDoc, jsonBuffer, jsonSize);
            _ws.textAll(jsonBuffer);
            free(jsonBuffer);
        }
        
        if (uploadSuccess) {
            broadcastLog("Firmware uploaded: %zu bytes", uploadedSize);
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
    
    broadcastLogLevel("info", "Starting Pico firmware update...");
    
    // IMPORTANT: Pause packet processing BEFORE sending bootloader command
    // This prevents the main loop's picoUart.loop() from consuming the bootloader ACK bytes
    _picoUart.pause();
    
    // Step 1: Send bootloader command via UART (serial bootloader - preferred method)
    // Retry up to 3 times if command fails
    broadcastLogLevel("info", "Sending bootloader command to Pico...");
    bool commandSent = false;
    for (int attempt = 1; attempt <= 3 && !commandSent; attempt++) {
        if (_picoUart.sendCommand(MSG_CMD_BOOTLOADER, nullptr, 0)) {
            commandSent = true;
        } else if (attempt < 3) {
            broadcastLogLevel("warning", "Retry sending bootloader command...");
            delay(100);
        }
    }
    
    if (!commandSent) {
        broadcastLogLevel("error", "Failed to send bootloader command after 3 attempts");
        _picoUart.resume();
        firmwareFile.close();
        return;
    }
    
    // Step 2: Wait for bootloader ACK (0xAA 0x55)
    // The bootloader sends this ACK to confirm it's ready to receive firmware
    broadcastLogLevel("info", "Waiting for bootloader ACK...");
    if (!_picoUart.waitForBootloaderAck(3000)) {
        broadcastLogLevel("error", "Bootloader ACK timeout - bootloader may not be ready");
        _picoUart.resume();
        firmwareFile.close();
        return;
    }
    broadcastLogLevel("info", "Bootloader ACK received, ready to stream firmware");
    
    // Step 3: Stream firmware
    broadcastLogLevel("info", "Streaming firmware to Pico...");
    bool success = streamFirmwareToPico(firmwareFile, firmwareSize);
    
    firmwareFile.close();
    
    if (!success) {
        broadcastLogLevel("error", "Firmware update failed");
        _picoUart.resume();
        // Fallback: Try hardware bootloader entry
        broadcastLogLevel("info", "Attempting hardware bootloader entry (fallback)...");
        _picoUart.enterBootloader();
        delay(500);
        // Note: Hardware bootloader entry requires different protocol (USB bootloader)
        // This is a fallback for recovery only
        return;
    }
    
    // Step 4: Reset Pico to boot new firmware
    delay(1000);
    broadcastLogLevel("info", "Resetting Pico...");
    _picoUart.resetPico();
    
    // Resume packet processing to receive boot info from Pico
    _picoUart.resume();
    
    broadcastLogLevel("info", "Firmware update complete. Pico should boot with new firmware.");
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
    if (filename.endsWith(".jpg") || filename.endsWith(".jpeg")) return "image/jpeg";
    if (filename.endsWith(".gif")) return "image/gif";
    if (filename.endsWith(".svg")) return "image/svg+xml";
    if (filename.endsWith(".ico")) return "image/x-icon";
    if (filename.endsWith(".woff")) return "font/woff";
    if (filename.endsWith(".woff2")) return "font/woff2";
    if (filename.endsWith(".webp")) return "image/webp";
    if (filename.endsWith(".webmanifest")) return "application/manifest+json";
    return "application/octet-stream";
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
            broadcastLogLevel("error", "Firmware read error");
            return false;
        }
        
        // Stream chunk via bootloader protocol (raw UART, not packet protocol)
        size_t sent = _picoUart.streamFirmwareChunk(buffer, bytesRead, chunkNumber);
        if (sent != bytesRead) {
            LOG_E("Failed to send chunk %d: %d/%d bytes", chunkNumber, sent, bytesRead);
            broadcastLogLevel("error", "Firmware streaming error at chunk %d", chunkNumber);
            return false;
        }
        
        // LOCK-STEP PROTOCOL: Wait for Pico to ACK (0xAA) after each chunk
        // This ensures the Pico has finished flash operations before we send more data.
        // The Pico's UART FIFO is only 32 bytes - without this handshake, we overflow it
        // during slow flash erase operations (~50ms).
        bool ackReceived = false;
        unsigned long ackStart = millis();
        const unsigned long ACK_TIMEOUT_MS = 2000;  // 2 second timeout (flash ops can be slow)
        
        while ((millis() - ackStart) < ACK_TIMEOUT_MS) {
            if (Serial1.available()) {
                uint8_t byte = Serial1.read();
                if (byte == 0xAA) {
                    ackReceived = true;
                    break;
                } else if (byte == 0xFF) {
                    // Error marker from Pico
                    uint8_t errorCode = 0;
                    if (Serial1.available()) {
                        errorCode = Serial1.read();
                    }
                    LOG_E("Pico reported error 0x%02X during chunk %d", errorCode, chunkNumber);
                    broadcastLogLevel("error", "Pico error during flash at chunk %d", chunkNumber);
                    return false;
                }
                // Ignore other bytes (could be debug output on wrong line)
            }
            delay(1);  // Small delay to avoid busy-waiting
        }
        
        if (!ackReceived) {
            LOG_E("Timeout waiting for ACK after chunk %d", chunkNumber);
            broadcastLogLevel("error", "Pico not responding at chunk %d", chunkNumber);
            return false;
        }
        
        bytesSent += bytesRead;
        chunkNumber++;
        
        // Report progress every 10%
        // Use non-static to reset between OTA attempts
        size_t progress = (bytesSent * 100) / firmwareSize;
        static size_t lastProgress = 0;
        if (bytesSent == 0) lastProgress = 0;  // Reset on new OTA
        if (progress >= lastProgress + 10 || bytesSent == firmwareSize) {
            lastProgress = progress;
            LOG_I("Flash progress: %d%% (%d/%d bytes)", progress, bytesSent, firmwareSize);
            
            // Only send WebSocket update if clients can receive (prevents queue overflow)
            if (_ws.count() > 0 && _ws.availableForWriteAll()) {
                #pragma GCC diagnostic push
                #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
                StaticJsonDocument<256> doc;
                #pragma GCC diagnostic pop
                doc["type"] = "ota_progress";
                doc["stage"] = "flash";
                doc["progress"] = progress;
                doc["sent"] = bytesSent;
                doc["total"] = firmwareSize;
                
                size_t jsonSize = measureJson(doc) + 1;
                char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                if (!jsonBuffer) jsonBuffer = (char*)malloc(jsonSize);
                if (jsonBuffer) {
                    serializeJson(doc, jsonBuffer, jsonSize);
                    _ws.textAll(jsonBuffer);
                    free(jsonBuffer);
                }
            }
        }
        
        // NOTE: No delay needed here - we use lock-step protocol with ACK after each chunk.
        // The ACK wait above ensures proper flow control regardless of flash timing.
    }
    
    // Send end marker (chunk number 0xFFFFFFFF signals end of firmware)
    uint8_t endMarker[2] = {0xAA, 0x55};  // Bootloader end magic
    size_t sent = _picoUart.streamFirmwareChunk(endMarker, 2, 0xFFFFFFFF);
    if (sent != 2) {
        LOG_E("Failed to send end marker");
        broadcastLogLevel("error", "Failed to send end marker");
        return false;
    }
    
    LOG_I("Firmware streaming complete: %d bytes in %d chunks", bytesSent, chunkNumber);
    broadcastLog("Firmware streaming complete: %zu bytes in %d chunks", bytesSent, chunkNumber);
    return true;
}

void WebServer::handleGetMQTTConfig(AsyncWebServerRequest* request) {
    MQTTConfig config = _mqttClient.getConfig();
    
    #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        StaticJsonDocument<2048> doc;
        #pragma GCC diagnostic pop
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
    // Get MQTT status - copy String to stack buffer to avoid PSRAM
    String mqttStatusStr = _mqttClient.getStatusString();
    char mqttStatusBuf[32];
    strncpy(mqttStatusBuf, mqttStatusStr.c_str(), sizeof(mqttStatusBuf) - 1);
    mqttStatusBuf[sizeof(mqttStatusBuf) - 1] = '\0';
    doc["status"] = mqttStatusBuf;
    
    
        // Allocate JSON buffer in internal RAM (not PSRAM)
        size_t jsonSize = measureJson(doc) + 1;
        char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!jsonBuffer) {
            jsonBuffer = (char*)malloc(jsonSize);
        }
        
        if (jsonBuffer) {
            serializeJson(doc, jsonBuffer, jsonSize);
            request->send(200, "application/json", jsonBuffer);
            free(jsonBuffer);
        } else {
            request->send(500, "application/json", "{\"error\":\"Out of memory\"}");
        }
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
        broadcastLogLevel("info", "MQTT configuration updated");
    } else {
        request->send(400, "application/json", "{\"error\":\"Invalid configuration\"}");
    }
}

void WebServer::handleTestMQTT(AsyncWebServerRequest* request) {
    if (_mqttClient.testConnection()) {
        request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Connection successful\"}");
        broadcastLogLevel("info", "MQTT connection test successful");
    } else {
        request->send(500, "application/json", "{\"error\":\"Connection failed\"}");
        broadcastLogLevel("error", "MQTT connection test failed");
    }
}
