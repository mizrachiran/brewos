#include "cloud_connection.h"
#include "memory_utils.h"
#include <WiFi.h>

// Logging macros (match project convention)
#define LOG_I(fmt, ...) Serial.printf("[Cloud] " fmt "\n", ##__VA_ARGS__)
#define LOG_W(fmt, ...) Serial.printf("[Cloud] WARN: " fmt "\n", ##__VA_ARGS__)
#define LOG_E(fmt, ...) Serial.printf("[Cloud] ERROR: " fmt "\n", ##__VA_ARGS__)
#define LOG_D(fmt, ...) Serial.printf("[Cloud] DEBUG: " fmt "\n", ##__VA_ARGS__)

// Reconnection settings - stay connected as much as possible
#define RECONNECT_DELAY_MS 5000  // 5s between attempts
#define STARTUP_GRACE_PERIOD_MS 15000  // 15s grace period after WiFi for local access
#define MIN_HEAP_FOR_CONNECT 40000  // Need 40KB heap for SSL buffers + web server headroom
#define MIN_HEAP_TO_STAY_CONNECTED 28000  // Disconnect if heap drops below this (state broadcast temporarily uses ~7KB)
#define SSL_HANDSHAKE_TIMEOUT_MS 30000  // 30s timeout (increased for slow networks)
#define CLOUD_TASK_STACK_SIZE 6144  // 6KB stack for SSL operations (reduced from 8KB)
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
    _authFailureCount = 0;  // Reset auth failures on begin()
    _enabled = true;
    _reconnectDelay = RECONNECT_DELAY_MS;
    
    // Set up event handler ONCE here
    _ws.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
        handleEvent(type, payload, length);
    });
    
    // Disable automatic reconnection - we handle it ourselves
    _ws.setReconnectInterval(0);
    
    // Start background task for cloud connection on Core 0
    // Core 0 is separate from Arduino loop (Core 1) so SSL doesn't block web server
    if (_taskHandle == nullptr) {
        xTaskCreatePinnedToCore(
            taskCode,
            "CloudTask",
            CLOUD_TASK_STACK_SIZE,
            this,
            CLOUD_TASK_PRIORITY,
            &_taskHandle,
            0  // Run on Core 0 (separate from Arduino loop on Core 1)
        );
        LOG_I("Cloud task started on Core 0");
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
    
    // Wait for WiFi to connect first, then apply grace period
    while (WiFi.status() != WL_CONNECTED && self->_enabled) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    // Now wait for grace period AFTER WiFi connects
    // This ensures local web server has time to serve initial requests
    LOG_I("WiFi connected, waiting %d seconds grace period...", STARTUP_GRACE_PERIOD_MS/1000);
    vTaskDelay(pdMS_TO_TICKS(STARTUP_GRACE_PERIOD_MS));
    
    while (self->_enabled) {
        unsigned long now = millis();
        
        // Check if paused for local activity - skip all connection logic
        if (now < self->_pausedUntil) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        
        // Emergency heap check - disconnect if memory is critically low
        // This ensures web server has enough memory to serve requests
        // BUT: Give a 10-second grace period after connection for SSL + state broadcast to complete
        // (SSL handshake uses ~40KB, state broadcast temporarily uses ~7KB more)
        size_t currentHeap = ESP.getFreeHeap();
        bool recentlyConnected = (self->_connectedAt > 0 && (now - self->_connectedAt) < 10000);
        
        if (currentHeap < MIN_HEAP_TO_STAY_CONNECTED && (self->_connected || self->_connecting) && !recentlyConnected) {
            LOG_W("Critical heap (%d bytes) - disconnecting cloud, retry in 30s", currentHeap);
            if (xSemaphoreTake(self->_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                self->_ws.disconnect();
                xSemaphoreGive(self->_mutex);
            }
            self->_connected = false;
            self->_connecting = false;
            self->_connectedAt = 0;
            self->_lastConnectAttempt = now;
            self->_reconnectDelay = 30000;  // Wait 30s before retrying after heap issue
            vTaskDelay(pdMS_TO_TICKS(30000));  // Sleep 30s to let heap recover
            continue;
        }
        
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
            if (now - self->_lastConnectAttempt >= self->_reconnectDelay) {
                // Double-check not paused (could have been set between checks)
                if (now < self->_pausedUntil) {
                    continue;
                }
                
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
        
        // Check for SSL handshake timeout or abort if paused
        if (self->_connecting) {
            unsigned long now = millis();
            unsigned long connectTime = now - self->_lastConnectAttempt;
            
            // Abort handshake immediately if paused (local web access takes priority)
            bool shouldAbort = (now < self->_pausedUntil);
            
            // Log progress every 5 seconds
            static unsigned long lastProgressLog = 0;
            if (connectTime - lastProgressLog >= 5000) {
                LOG_I("SSL handshake in progress... (%lu s)", connectTime / 1000);
                lastProgressLog = connectTime;
            }
            
            if (shouldAbort) {
                LOG_I("Aborting SSL handshake for local web access");
                if (xSemaphoreTake(self->_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    self->_ws.disconnect();
                    xSemaphoreGive(self->_mutex);
                }
                self->_connecting = false;
                self->_connected = false;
                lastProgressLog = 0;
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;  // Skip to next iteration, check pause again
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
        
        // Skip WebSocket operations when paused - don't let SSL handshake block local access
        unsigned long nowForWs = millis();
        if (nowForWs < self->_pausedUntil) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        
        // Skip WebSocket operations when waiting for reconnect delay
        // This prevents the library from internally retrying and blocking the device
        if (!self->_connected && !self->_connecting) {
            if (nowForWs - self->_lastConnectAttempt < self->_reconnectDelay) {
                // Still waiting - sleep and skip _ws.loop()
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
        }
        
        // All WebSocket operations under mutex
        if (xSemaphoreTake(self->_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            // Double-check pause (could have been set while waiting for mutex)
            if (millis() < self->_pausedUntil) {
                xSemaphoreGive(self->_mutex);
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }
            
            // Only call _ws.loop() when connected or connecting
            // Skip when idle/waiting to prevent library's internal reconnect attempts
            if (self->_connected || self->_connecting) {
                // Process WebSocket events - this drives the SSL handshake
                self->_ws.loop();
            }
            
            // Send queued messages (only when connected)
            if (self->_connected) {
                self->processSendQueue();
            }
            
            xSemaphoreGive(self->_mutex);
        }
        
        // Handle proactive initial state broadcast after cloud connects
        // This ensures state is synced even if request_state message is delayed/lost
        if (self->_connected && self->_pendingInitialStateBroadcast && 
            millis() >= self->_initialStateBroadcastTime) {
            self->_pendingInitialStateBroadcast = false;
            
            // Check heap before broadcasting
            size_t freeHeap = ESP.getFreeHeap();
            if (freeHeap >= 35000 && self->_onCommand) {
                LOG_I("Proactive state broadcast to cloud (heap=%zu)", freeHeap);
                // Create synthetic request_state command
                JsonDocument doc;
                doc["type"] = "request_state";
                doc["source"] = "proactive";  // Mark as proactive for debugging
                self->_onCommand("request_state", doc);
            } else if (freeHeap < 35000) {
                // Defer if heap too low
                self->_pendingInitialStateBroadcast = true;
                self->_initialStateBroadcastTime = millis() + 2000;
                LOG_W("Deferring proactive state broadcast (heap=%zu)", freeHeap);
            }
        }
        
        // Yield to other tasks
        if (self->_connected) {
            vTaskDelay(pdMS_TO_TICKS(50));   // Connected: responsive to process queue quickly
        } else if (self->_connecting) {
            vTaskDelay(pdMS_TO_TICKS(20));   // Handshake: fast loop
        } else {
            vTaskDelay(pdMS_TO_TICKS(1000)); // Idle/waiting: save CPU, check every 1s
        }
    }
    
    LOG_I("Task ending");
    vTaskDelete(NULL);
}

void CloudConnection::notifyUserActivity() {
    _lastUserActivity = millis();
}

void CloudConnection::cancelPendingStateBroadcast() {
    _pendingInitialStateBroadcast = false;
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
    // If we had auth failures, the key was regenerated and begin() was called again
    // so _deviceKey should already be updated
    if (!_registered) {
        if (!_deviceKey.isEmpty()) {
            // Device key exists - try registration if not already registered
            if (_onRegister) {
                LOG_I("Device key present but not registered - attempting registration...");
                _registered = _onRegister();
                if (!_registered) {
                    LOG_W("Registration failed - will retry in 30s");
                    _failureCount++;
                    _lastConnectAttempt = millis();
                    _reconnectDelay = 30000;
                    return;
                }
                LOG_I("Registration successful");
            } else {
                // No registration callback - assume already paired
                LOG_I("Device key present - assuming already paired (no registration callback)");
                _registered = true;
            }
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
        LOG_I("Connecting with device key (length: %d)", _deviceKey.length());
    } else {
        LOG_W("WARNING: Connecting WITHOUT device key - server will reject!");
        LOG_W("Device ID: %s", _deviceId.c_str());
    }
    
    // Resolve DNS first - WebSocketsClient doesn't handle DNS failures well
    // This prevents repeated SSL timeout errors when DNS fails
    IPAddress serverIP;
    unsigned long dnsStart = millis();
    bool dnsSuccess = WiFi.hostByName(host.c_str(), serverIP);
    unsigned long dnsTime = millis() - dnsStart;
    
    if (!dnsSuccess) {
        LOG_W("DNS failed for %s (took %lu ms) - will retry", host.c_str(), dnsTime);
        _connecting = false;
        _failureCount++;
        _reconnectDelay = 10000;  // Retry in 10s
        return;
    }
    
    LOG_I("DNS resolved: %s -> %s (%lu ms)", host.c_str(), serverIP.toString().c_str(), dnsTime);
    LOG_I("Connecting to %s (%s):%d (SSL=%d)", host.c_str(), serverIP.toString().c_str(), port, useSSL);
    
    // Log network diagnostics before connecting
    int32_t rssi = WiFi.RSSI();
    IPAddress localIP = WiFi.localIP();
    LOG_I("Network: IP=%s, RSSI=%d dBm, Gateway=%s", 
          localIP.toString().c_str(), rssi, WiFi.gatewayIP().toString().c_str());
    
    // Enable heartbeat (ping every 15s, timeout 10s, 2 failures to disconnect)
    // Mutex protection for all _ws calls
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        _ws.enableHeartbeat(15000, 10000, 2);
        
        // Configure SSL client timeout (default 5s is too short for slow networks)
        // WebSocketsClient uses WiFiClientSecure internally
        // Try to configure timeout - method availability depends on library version
        if (useSSL) {
            // Try to get and configure the underlying SSL client
            // Note: getCClient() may not be available in all WebSocketsClient versions
            // If compilation fails, comment out this section
            #ifdef WEBSOCKETS_CLIENT_HAS_GET_CLIENT
            WiFiClientSecure* sslClient = _ws.getCClient();
            if (sslClient) {
                sslClient->setTimeout(20000);  // 20 second timeout for SSL operations
                sslClient->setInsecure();  // Skip certificate validation (faster, acceptable for known server)
                LOG_I("SSL client configured: timeout=20s, insecure mode");
            }
            #else
            // WebSocketsClient library doesn't expose getCClient() - timeout uses library default
            // The library should handle SSL internally, but timeout may be limited
            LOG_I("SSL client: using library default timeout (may be 5s)");
            #endif
        }
        
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
    unsigned long now = millis();
    unsigned long newPauseUntil = now + 30000; // Pause for 30s
    
    // Extend pause if already paused (web browsing session)
    if (now < _pausedUntil) {
        _pausedUntil = newPauseUntil;
        LOG_D("Extended cloud pause until %lu", _pausedUntil);
        return;
    }
    
    LOG_I("Pausing cloud for local activity (30s)");
    _pausedUntil = newPauseUntil;
    
    // Disconnect to free memory and network for local requests
    if (_connected || _connecting) {
        LOG_I("Disconnecting cloud to free resources for local");
        if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            _ws.disconnect();
            xSemaphoreGive(_mutex);
        }
        _connected = false;
        _connecting = false;
        _lastConnectAttempt = now;
    }
}

void CloudConnection::resume() {
    if (_pausedUntil > 0) {
        LOG_I("Resuming cloud connection (local client disconnected)");
        _pausedUntil = 0;
        _lastConnectAttempt = 0;  // Allow immediate reconnection
    }
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
            {
                // Try to get disconnect reason if available
                String reason = (length > 0 && payload) ? String((char*)payload, length) : "unknown";
                unsigned long now = millis();
                
                // Detect authentication failure: connected then immediately disconnected (< 5 seconds)
                // This indicates server rejected the connection due to invalid credentials
                bool isAuthFailure = false;
                if (_connected && _connectedAt > 0 && (now - _connectedAt) < 5000) {
                    isAuthFailure = true;
                    _authFailureCount++;
                    LOG_W("Authentication failure detected (disconnected after %lu ms)", now - _connectedAt);
                    LOG_W("Disconnect reason: %s", reason.c_str());
                } else if (_connected) {
                    LOG_W("Disconnected from cloud (reason: %s, length: %zu)", reason.c_str(), length);
                } else {
                    LOG_W("Connection failed (reason: %s, length: %zu)", reason.c_str(), length);
                }
                
                _connected = false;
                _connecting = false;
                _lastDisconnectTime = now;
                _connectedAt = 0;  // Reset connection timestamp
                _pendingInitialStateBroadcast = false;  // Cancel pending broadcast
                
                // If auth failure detected, regenerate key and retry registration
                if (isAuthFailure && _onRegenerateKey && _authFailureCount <= 3) {
                    LOG_W("Attempting recovery: regenerating device key (attempt %d/3)", _authFailureCount);
                    if (_onRegenerateKey()) {
                        // Key regenerated - need to reload it from PairingManager
                        // The callback should return true if successful, but we need to get the new key
                        // For now, mark as unregistered and the connect() will reload key via begin()
                        _registered = false;
                        _failureCount = 0;  // Reset failure count
                        _reconnectDelay = 10000;  // Retry in 10s with new key
                        LOG_I("Device key regenerated - will reload and retry registration");
                    } else {
                        LOG_E("Failed to regenerate device key - will retry later");
                        _reconnectDelay = 30000;
                    }
                } else if (isAuthFailure && _authFailureCount > 3) {
                    LOG_E("Too many auth failures (%d) - giving up. Manual pairing required.", _authFailureCount);
                    _reconnectDelay = 300000;  // 5 min before retry
                } else {
                    // Normal disconnect - only set quick retry if not already set to longer delay
                    if (_reconnectDelay < 30000) {
                        _lastConnectAttempt = now;
                        _failureCount++;
                        _reconnectDelay = 30000; // Retry in 30s for stability
                        LOG_W("Cloud disconnected, reconnecting in 30s");
                    }
                }
            }
            break;
            
        case WStype_CONNECTED:
            LOG_I("Connected to cloud!");
            LOG_I("Device ID: %s, Key length: %d", _deviceId.c_str(), _deviceKey.length());
            _connected = true;
            _connecting = false;
            _failureCount = 0; // Reset failures
            _authFailureCount = 0; // Reset auth failures on successful connection
            _reconnectDelay = RECONNECT_DELAY_MS; // Reset to default (2 min)
            _connectedAt = millis();  // Track connection time for grace period
            _pendingInitialStateBroadcast = true;  // Schedule state broadcast after stabilization
            _initialStateBroadcastTime = millis() + 3000;  // Wait 3s for heap to stabilize after SSL
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

void CloudConnection::sendBinary(const uint8_t* data, size_t len) {
    if (!_connected || !data || !_sendQueue || len == 0) {
        return;
    }
    
    if (len >= MAX_MSG_SIZE - 5) {  // Reserve 5 bytes for length + marker
        LOG_W("Binary message too large (%d bytes), dropping", len);
        return;
    }
    
    // Allocate in PSRAM and queue the pointer
    // Format: [4 bytes length (big-endian)] [1 byte marker 0x01] [data]
    uint8_t* msgCopy = (uint8_t*)heap_caps_malloc(len + 5, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!msgCopy) {
        msgCopy = (uint8_t*)malloc(len + 5);
    }
    
    if (msgCopy) {
        // Store length as big-endian uint32_t
        uint32_t len32 = len;
        msgCopy[0] = (len32 >> 24) & 0xFF;
        msgCopy[1] = (len32 >> 16) & 0xFF;
        msgCopy[2] = (len32 >> 8) & 0xFF;
        msgCopy[3] = len32 & 0xFF;
        msgCopy[4] = 0x01;  // Binary format marker
        memcpy(msgCopy + 5, data, len);
        // Non-blocking queue - drop if full
        if (xQueueSend(_sendQueue, &msgCopy, 0) != pdTRUE) {
            LOG_W("Send queue full, dropping binary message");
            free(msgCopy);
        }
    }
}

void CloudConnection::processSendQueue() {
    if (!_sendQueue || !_connected) {
        return;
    }
    
    char* msg = nullptr;
    // Process messages in batches to prevent blocking SSL writes
    // Add yields between sends to prevent timeouts
    int processed = 0;
    const int MAX_PER_CALL = 10;  // Process up to 10 messages per call
    const int MAX_AGGRESSIVE = 20;  // Max messages even when queue is full (prevents long blocks)
    
    // First pass: process up to MAX_PER_CALL messages
    while (processed < MAX_PER_CALL && xQueueReceive(_sendQueue, &msg, 0) == pdTRUE && msg) {
        // Check if binary format (5th byte = 0x01)
        uint8_t* msgPtr = (uint8_t*)msg;
        if (msgPtr[4] == 0x01) {
            // Binary MessagePack format: [4 bytes length] [0x01 marker] [data]
            uint32_t len = (msgPtr[0] << 24) | (msgPtr[1] << 16) | (msgPtr[2] << 8) | msgPtr[3];
            _ws.sendBIN(msgPtr + 5, len);
        } else {
            // Legacy text/JSON format (null-terminated string)
            _ws.sendTXT((char*)msg);
        }
        free(msg);
        processed++;
        
        // Yield every 5 messages to prevent blocking SSL writes
        if (processed % 5 == 0) {
            vTaskDelay(pdMS_TO_TICKS(10));  // 10ms yield to allow SSL to process
        }
    }
    
    // If queue is getting full (< 5 spaces left), process more aggressively but still limit
    UBaseType_t queueSpace = uxQueueSpacesAvailable(_sendQueue);
    if (queueSpace < 5 && processed < MAX_AGGRESSIVE) {
        // Queue is getting full - process more messages but still limit to prevent long blocks
        int aggressiveCount = 0;
        while (processed < MAX_AGGRESSIVE && xQueueReceive(_sendQueue, &msg, 0) == pdTRUE && msg) {
            // Check if binary format (5th byte = 0x01)
            uint8_t* msgPtr = (uint8_t*)msg;
            if (msgPtr[4] == 0x01) {
                // Binary MessagePack format: [4 bytes length] [0x01 marker] [data]
                uint32_t len = (msgPtr[0] << 24) | (msgPtr[1] << 16) | (msgPtr[2] << 8) | msgPtr[3];
                _ws.sendBIN(msgPtr + 5, len);
            } else {
                // Legacy text/JSON format (null-terminated string)
                _ws.sendTXT((char*)msg);
            }
            free(msg);
            processed++;
            aggressiveCount++;
            
            // Yield every 3 messages during aggressive processing
            if (aggressiveCount % 3 == 0) {
                vTaskDelay(pdMS_TO_TICKS(10));  // 10ms yield to allow SSL to process
            }
        }
        
        if (queueSpace < 2) {
            // Queue is critically full - log warning
            LOG_W("Cloud send queue critically full (%d/%d), processed %d messages", 
                  uxQueueMessagesWaiting(_sendQueue), SEND_QUEUE_SIZE, processed);
        }
    }
}

void CloudConnection::onCommand(CommandCallback callback) {
    _onCommand = callback;
}

void CloudConnection::onRegister(RegisterCallback callback) {
    _onRegister = callback;
}

void CloudConnection::onRegenerateKey(RegenerateKeyCallback callback) {
    _onRegenerateKey = callback;
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

