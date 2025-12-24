#include "cloud_connection.h"
#include <WiFi.h>

// Logging macros (match project convention)
#define LOG_I(fmt, ...) Serial.printf("[Cloud] " fmt "\n", ##__VA_ARGS__)
#define LOG_W(fmt, ...) Serial.printf("[Cloud] WARN: " fmt "\n", ##__VA_ARGS__)
#define LOG_E(fmt, ...) Serial.printf("[Cloud] ERROR: " fmt "\n", ##__VA_ARGS__)
#define LOG_D(fmt, ...) Serial.printf("[Cloud] DEBUG: " fmt "\n", ##__VA_ARGS__)

// Reconnection settings - stay connected as much as possible
#define RECONNECT_DELAY_MS 5000  // 5s between attempts
#define STARTUP_GRACE_PERIOD_MS 5000  // 5s grace period for WiFi to stabilize
#define MIN_HEAP_FOR_CONNECT 40000  // Need 40KB heap for SSL buffers
#define SSL_HANDSHAKE_TIMEOUT_MS 15000  // 15s timeout (RSA should take 5-10s)
#define CLOUD_TASK_STACK_SIZE 8192  // 8KB stack for SSL operations
#define CLOUD_TASK_PRIORITY 1  // Low priority - below web server

CloudConnection::CloudConnection() {
    _failureCount = 0;
    _mutex = xSemaphoreCreateMutex();
    // Queue holds pointers to PSRAM-allocated message buffers
    _sendQueue = xQueueCreate(SEND_QUEUE_SIZE, sizeof(char*));
}

void CloudConnection::begin(const String& serverUrl, const String& deviceId, const String& deviceKey) {
    _serverUrl = serverUrl;
    _deviceId = deviceId;
    _deviceKey = deviceKey;
    _enabled = true;
    _reconnectDelay = RECONNECT_DELAY_MS;
    
    // Set up event handler ONCE here
    _ws.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
        handleEvent(type, payload, length);
    });
    
    // Disable automatic reconnection - we handle it ourselves
    _ws.setReconnectInterval(0);
    
    // Start background task for cloud connection
    // This runs SSL operations without blocking the main loop
    if (_taskHandle == nullptr) {
        xTaskCreatePinnedToCore(
            taskCode,
            "CloudTask",
            CLOUD_TASK_STACK_SIZE,
            this,
            CLOUD_TASK_PRIORITY,
            &_taskHandle,
            1  // Run on Core 1 (same as Arduino loop)
        );
        LOG_I("Cloud task started on Core 1");
    }
    
    LOG_I("Initialized: server=%s, device=%s", serverUrl.c_str(), deviceId.c_str());
}

void CloudConnection::end() {
    _enabled = false;
    
    // Stop background task
    if (_taskHandle != nullptr) {
        vTaskDelete(_taskHandle);
        _taskHandle = nullptr;
        LOG_I("Cloud task stopped");
    }
    
    bool wasConnected = _connected;
    bool wasConnecting = _connecting;
    _connected = false;
    _connecting = false;
    
    if (wasConnected || wasConnecting) {
        if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            _ws.disconnect();
            xSemaphoreGive(_mutex);
        }
        delay(100);
    }
    
    // Clear send queue
    if (_sendQueue) {
        char* msg = nullptr;
        while (xQueueReceive(_sendQueue, &msg, 0) == pdTRUE && msg) {
            free(msg);
        }
    }
    
    LOG_I("Disabled");
}

void CloudConnection::loop() {
    // Cloud connection runs in its own FreeRTOS task
    // This function is kept for API compatibility but does nothing
    // The task handles all connection logic independently
}

// FreeRTOS task that runs cloud connection in background
void CloudConnection::taskCode(void* parameter) {
    CloudConnection* self = static_cast<CloudConnection*>(parameter);
    
    LOG_I("Task started, waiting for WiFi...");
    
    // Wait for startup grace period
    vTaskDelay(pdMS_TO_TICKS(STARTUP_GRACE_PERIOD_MS));
    
    while (self->_enabled) {
        // Check WiFi
        if (WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
            if (self->_connected) {
                self->_connected = false;
                self->_connecting = false;
                LOG_W("WiFi disconnected");
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        
        // If not connected, try to connect
        if (!self->_connected && !self->_connecting) {
            unsigned long now = millis();
            
            if (now - self->_lastConnectAttempt >= self->_reconnectDelay) {
                // Check heap before attempting connection
                size_t freeHeap = ESP.getFreeHeap();
                if (freeHeap < MIN_HEAP_FOR_CONNECT) {
                    LOG_W("Low heap (%d bytes) - deferring cloud connection", freeHeap);
                    self->_lastConnectAttempt = now;
                } else {
                    self->connect();
                }
            }
        }
        
        // Check for SSL handshake timeout
        if (self->_connecting) {
            unsigned long now = millis();
            unsigned long connectTime = now - self->_lastConnectAttempt;
            
            // Log progress every 5 seconds
            static unsigned long lastProgressLog = 0;
            if (connectTime - lastProgressLog >= 5000) {
                LOG_I("SSL handshake in progress... (%lu s)", connectTime / 1000);
                lastProgressLog = connectTime;
            }
            
            if (connectTime > SSL_HANDSHAKE_TIMEOUT_MS) {
                LOG_E("SSL handshake timeout (%ds)", SSL_HANDSHAKE_TIMEOUT_MS/1000);
                if (xSemaphoreTake(self->_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    self->_ws.disconnect();
                    xSemaphoreGive(self->_mutex);
                }
                self->_connecting = false;
                self->_connected = false;
                self->_lastConnectAttempt = now;
                lastProgressLog = 0;
                
                // Quick retry - always stay connected
                self->_failureCount++;
                self->_reconnectDelay = 10000; // Always retry in 10s
                LOG_W("Timeout (%d), retry in 10s", self->_failureCount);
            }
        }
        
        // All WebSocket operations under mutex
        if (xSemaphoreTake(self->_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            // Process WebSocket events - this drives the SSL handshake
            self->_ws.loop();
            
            // Send queued messages (only when connected)
            if (self->_connected) {
                self->processSendQueue();
            }
            
            xSemaphoreGive(self->_mutex);
        }
        
        // Yield to other tasks
        if (self->_connected) {
            vTaskDelay(pdMS_TO_TICKS(100));  // Connected: responsive but not too aggressive
        } else if (self->_connecting) {
            vTaskDelay(pdMS_TO_TICKS(20));   // Handshake: fast loop
        } else {
            vTaskDelay(pdMS_TO_TICKS(500));  // Idle: save CPU
        }
    }
    
    LOG_I("Task ending");
    vTaskDelete(NULL);
}

void CloudConnection::notifyUserActivity() {
    _lastUserActivity = millis();
}

void CloudConnection::connect() {
    if (_serverUrl.isEmpty() || _deviceId.isEmpty()) {
        LOG_W("Cannot connect: missing server URL or device ID");
        return;
    }
    
    // CRITICAL: Never start connection during grace period or pause
    // SSL handshake blocks entire LWIP stack - must not start during web server activity
    unsigned long now = millis();
    if (now < _pausedUntil || now < STARTUP_GRACE_PERIOD_MS) {
        LOG_I("Skipping connection - paused or in grace period");
        _connecting = false;
        _lastConnectAttempt = now;
        return;
    }
    
    // Skip registration if we already have a device key (device already paired)
    // Registration is only needed for initial pairing - it adds 5-15s SSL overhead
    if (!_registered) {
        if (!_deviceKey.isEmpty()) {
            // Device key exists - already paired, skip registration
            LOG_I("Device key present - skipping registration (already paired)");
            _registered = true;
        } else if (_onRegister) {
            // No device key - need to register for pairing
            LOG_I("No device key - registering with cloud...");
            _registered = _onRegister();
            if (!_registered) {
                LOG_W("Registration failed - will retry in 30s");
                _failureCount++;
                _lastConnectAttempt = millis();
                _reconnectDelay = 30000;
                return;
            }
        }
    }
    
    _lastConnectAttempt = millis();
    _connecting = true;
    
    // Parse URL
    String host;
    uint16_t port;
    String path;
    bool useSSL;
    
    if (!parseUrl(_serverUrl, host, port, path, useSSL)) {
        LOG_E("Invalid server URL: %s", _serverUrl.c_str());
        _connecting = false;
        return;
    }
    
    // Build WebSocket path with auth params
    String wsPath = "/ws/device?id=" + _deviceId;
    if (!_deviceKey.isEmpty()) {
        wsPath += "&key=" + _deviceKey;
    }
    
    // Resolve DNS first - WebSocketsClient doesn't handle DNS failures well
    // This prevents repeated SSL timeout errors when DNS fails
    IPAddress serverIP;
    if (!WiFi.hostByName(host.c_str(), serverIP)) {
        LOG_W("DNS failed for %s - will retry", host.c_str());
        _connecting = false;
        _failureCount++;
        _reconnectDelay = 10000;  // Retry in 10s
        return;
    }
    
    LOG_I("Connecting to %s (%s):%d (SSL=%d)", host.c_str(), serverIP.toString().c_str(), port, useSSL);
    
    // Enable heartbeat (ping every 15s, timeout 10s, 2 failures to disconnect)
    // Mutex protection for all _ws calls
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        _ws.enableHeartbeat(15000, 10000, 2);
        
        // Connect WebSocket using resolved IP (more reliable than hostname)
        if (useSSL) {
            LOG_I("Starting SSL WebSocket...");
            _ws.beginSSL(host.c_str(), port, wsPath.c_str());
        } else {
            _ws.begin(host.c_str(), port, wsPath.c_str());
        }
        xSemaphoreGive(_mutex);
    } else {
        LOG_W("Could not acquire mutex for connect");
        _connecting = false;
    }
}

void CloudConnection::pause() {
    // No-op: Cloud connection is always on
    // Cloud runs in separate FreeRTOS task and doesn't interfere with web server
    LOG_D("Cloud pause requested (ignored - always on)");
}

bool CloudConnection::parseUrl(const String& url, String& host, uint16_t& port, String& path, bool& useSSL) {
    // Determine protocol (case-insensitive, only check first 8 chars)
    String proto = url.substring(0, 8);
    proto.toLowerCase();
    int protoEnd;
    if (proto.startsWith("https://") || proto.startsWith("wss://")) {
        useSSL = true;
        protoEnd = proto.startsWith("https://") ? 8 : 6;
        port = 443;
    } else if (proto.startsWith("http://") || proto.startsWith("ws://")) {
        useSSL = false;
        protoEnd = proto.startsWith("http://") ? 7 : 5;
        port = 80;
    } else {
        // Assume https if no protocol
        useSSL = true;
        protoEnd = 0;
        port = 443;
    }
    
    // Extract host and optional port
    String remainder = url.substring(protoEnd);
    int pathStart = remainder.indexOf('/');
    String hostPort;
    
    if (pathStart >= 0) {
        hostPort = remainder.substring(0, pathStart);
        path = remainder.substring(pathStart);
    } else {
        hostPort = remainder;
        path = "/";
    }
    
    // Check for port in host
    int colonPos = hostPort.indexOf(':');
    if (colonPos >= 0) {
        host = hostPort.substring(0, colonPos);
        port = hostPort.substring(colonPos + 1).toInt();
    } else {
        host = hostPort;
    }
    
    return !host.isEmpty();
}

void CloudConnection::handleEvent(WStype_t type, uint8_t* payload, size_t length) {
    if (!_enabled && type != WStype_DISCONNECTED) {
        return;
    }
    
    switch (type) {
        case WStype_DISCONNECTED:
            if (_connected) {
                LOG_W("Disconnected from cloud");
            }
            _connected = false;
            _connecting = false;
            _lastConnectAttempt = millis();
            
            // Quick retry - always stay connected
            _failureCount++;
            _reconnectDelay = 10000; // Retry in 10s
            LOG_W("Cloud disconnected, reconnecting in 10s");
            break;
            
        case WStype_CONNECTED:
            LOG_I("Connected to cloud!");
            _connected = true;
            _connecting = false;
            _failureCount = 0; // Reset failures
            _reconnectDelay = RECONNECT_DELAY_MS; // Reset to default (2 min)
            break;
            
        case WStype_TEXT:
            handleMessage(payload, length);
            break;
            
        case WStype_BIN:
            break;
            
        case WStype_ERROR:
            {
                String errorMsg = (length > 0 && payload) ? String((char*)payload, length) : "unknown";
                LOG_E("WebSocket error: %s", errorMsg.c_str());
                _connecting = false;
                _connected = false;
                _lastConnectAttempt = millis();
                _failureCount++;
                _reconnectDelay = 120000; // 2 min
            }
            break;
            
        case WStype_PING:
        case WStype_PONG:
            // Heartbeat handled by library
            break;
            
        default:
            break;
    }
}

void CloudConnection::handleMessage(uint8_t* payload, size_t length) {
    // Parse JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    
    if (error) {
        LOG_W("Invalid JSON message: %s", error.c_str());
        return;
    }
    
    String type = doc["type"] | "";
    
    // Handle cloud-specific messages
    if (type == "connected") {
        LOG_I("Cloud acknowledged connection");
        return;
    }
    
    if (type == "error") {
        String errorMsg = doc["error"] | "Unknown error";
        LOG_E("Cloud error: %s", errorMsg.c_str());
        return;
    }
    
    // Forward commands to callback
    if (_onCommand) {
        _onCommand(type, doc);
    } else {
        LOG_D("Received message type=%s (no handler)", type.c_str());
    }
}

void CloudConnection::send(const String& json) {
    send(json.c_str());
}

void CloudConnection::send(const char* json) {
    if (!_connected || !json || !_sendQueue) {
        return;
    }
    
    size_t len = strlen(json);
    if (len >= MAX_MSG_SIZE) {
        LOG_W("Message too large (%d bytes), dropping", len);
        return;
    }
    
    // Allocate in PSRAM and queue the pointer
    char* msgCopy = (char*)heap_caps_malloc(len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!msgCopy) {
        msgCopy = (char*)malloc(len + 1);
    }
    
    if (msgCopy) {
        memcpy(msgCopy, json, len + 1);
        // Non-blocking queue - drop if full
        if (xQueueSend(_sendQueue, &msgCopy, 0) != pdTRUE) {
            LOG_W("Send queue full, dropping message");
            free(msgCopy);
        }
    }
}

void CloudConnection::send(const JsonDocument& doc) {
    if (!_connected) {
        return;
    }
    
    // Serialize to PSRAM buffer and queue
    size_t jsonSize = measureJson(doc) + 1;
    if (jsonSize >= MAX_MSG_SIZE) {
        LOG_W("JSON too large (%d bytes), dropping", jsonSize);
        return;
    }
    
    char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!jsonBuffer) {
        jsonBuffer = (char*)malloc(jsonSize);
    }
    
    if (jsonBuffer) {
        serializeJson(doc, jsonBuffer, jsonSize);
        // Non-blocking queue
        if (xQueueSend(_sendQueue, &jsonBuffer, 0) != pdTRUE) {
            LOG_W("Send queue full, dropping message");
            free(jsonBuffer);
        }
    }
}

void CloudConnection::processSendQueue() {
    if (!_sendQueue || !_connected) {
        return;
    }
    
    char* msg = nullptr;
    // Process up to 3 messages per loop to avoid blocking
    for (int i = 0; i < 3; i++) {
        if (xQueueReceive(_sendQueue, &msg, 0) == pdTRUE && msg) {
            _ws.sendTXT(msg);
            free(msg);
        } else {
            break;
        }
    }
}

void CloudConnection::onCommand(CommandCallback callback) {
    _onCommand = callback;
}

void CloudConnection::onRegister(RegisterCallback callback) {
    _onRegister = callback;
}

bool CloudConnection::isConnected() const {
    return _connected;
}

String CloudConnection::getStatus() const {
    if (!_enabled) {
        return "disabled";
    }
    if (_connected) {
        return "connected";
    }
    if (_connecting) {
        return "connecting";
    }
    return "disconnected";
}

void CloudConnection::setEnabled(bool enabled) {
    if (enabled && !_enabled) {
        _enabled = true;
        _reconnectDelay = 1000;
        LOG_I("Enabled");
    } else if (!enabled && _enabled) {
        end();
    }
}

bool CloudConnection::isEnabled() const {
    return _enabled;
}

