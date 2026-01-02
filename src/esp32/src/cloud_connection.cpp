#include "cloud_connection.h"
#include "memory_utils.h"
#include <WiFi.h>
#include <esp_heap_caps.h>  // For heap_caps_get_largest_free_block

// Logging macros (match project convention)
#define LOG_I(fmt, ...) Serial.printf("[Cloud] " fmt "\n", ##__VA_ARGS__)
#define LOG_W(fmt, ...) Serial.printf("[Cloud] WARN: " fmt "\n", ##__VA_ARGS__)
#define LOG_E(fmt, ...) Serial.printf("[Cloud] ERROR: " fmt "\n", ##__VA_ARGS__)
#define LOG_D(fmt, ...) Serial.printf("[Cloud] DEBUG: " fmt "\n", ##__VA_ARGS__)

// Reconnection settings - stay connected as much as possible
#define RECONNECT_DELAY_MS 5000  // 5s between attempts
#define STARTUP_GRACE_PERIOD_MS 10000  // 10s grace period after WiFi for local access
// Memory Management
// SSL Context takes ~25-30KB. With optimized SSL buffers (2048 instead of 4096), it's ~20-25KB.
// We need:
// 40KB to start safely (reduced from 45KB due to SSL buffer optimization)
// 12KB to stay alive (lower threshold to prevent self-disconnecting)
#define MIN_HEAP_FOR_CONNECT 40000  // Need 40KB heap for SSL buffers + web server headroom (reduced from 45KB)
#define MIN_HEAP_TO_STAY_CONNECTED 12000  // Disconnect if heap drops below this (lowered to prevent thrashing)
#define HEAP_THRESHOLD_FOR_PAUSE 35000   // Only disconnect for local UI if heap is below this
#define SSL_HANDSHAKE_TIMEOUT_MS 20000  // 20s timeout
#define CLOUD_TASK_STACK_SIZE 8192  // 8KB stack for SSL operations (increased for safety)
#define CLOUD_TASK_PRIORITY 1  // Low priority - below web server

CloudConnection::CloudConnection() {
    _failureCount = 0;
    // Use recursive mutex to prevent deadlock when callbacks try to disconnect
    // (e.g., if _onCommand calls forceDisconnect() while _ws.loop() holds the mutex)
    _mutex = xSemaphoreCreateRecursiveMutex();
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
        if (_mutex && xSemaphoreTakeRecursive(_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            _ws.disconnect();
            xSemaphoreGiveRecursive(_mutex);
        }
        delay(100);
    }
    
    // Clear send queue
    // Note: Queue items are PSRAM-allocated (heap_caps_malloc), use heap_caps_free
    if (_sendQueue) {
        char* msg = nullptr;
        while (xQueueReceive(_sendQueue, &msg, 0) == pdTRUE && msg) {
            heap_caps_free(msg);
        }
    }
    
    LOG_I("Disabled");
}

void CloudConnection::forceDisconnect() {
    if (xSemaphoreTakeRecursive(_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        _ws.disconnect();
        
        // Explicitly stop SSL client to free buffers (~20-30KB)
        // Note: getCClient() may not be available in all WebSocketsClient versions
        #ifdef WEBSOCKETS_CLIENT_HAS_GET_CLIENT
        WiFiClientSecure* sslClient = _ws.getCClient();
        if (sslClient) {
            sslClient->stop();
        }
        #endif
        
        xSemaphoreGiveRecursive(_mutex);
    }
    _connected = false;
    _connecting = false;
    _lastConnectAttempt = millis();
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
        size_t currentHeap = ESP.getFreeHeap();
        // Check largest contiguous block to avoid fragmentation trap
        // SSL needs ~16-20KB contiguous block for handshake buffers
        size_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);

        // 1. Critical Memory Protection
        // Disconnect only if memory is critically low to save the system
        if (currentHeap < MIN_HEAP_TO_STAY_CONNECTED && (self->_connected || self->_connecting)) {
            LOG_W("Critical heap (%zu) - disconnecting", currentHeap);
            self->forceDisconnect();
            vTaskDelay(pdMS_TO_TICKS(5000)); // Give system time to recover
            continue;
        }

        // 2. Pause Logic (Optimized)
        // Only disconnect for local activity if we are actually tight on memory
        // Otherwise, just stay connected but maybe defer polling
        bool isPaused = (now < self->_pausedUntil);
        bool lowMemPause = isPaused && (currentHeap < HEAP_THRESHOLD_FOR_PAUSE);

        if (lowMemPause) {
            if (self->_connected || self->_connecting) {
                LOG_I("Pausing cloud for local priority (Low Mem: %zu)", currentHeap);
                self->forceDisconnect();
            }
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // 3. Network Check
        if (WiFi.status() != WL_CONNECTED) {
            self->_connected = false;
            self->_connecting = false;
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        
        // 4. Connection Logic
        // Track low memory state (static to persist across loop iterations)
        static unsigned long firstLowHeapTime = 0;
        static unsigned long lastHeapLogTime = 0;
        static unsigned long lastBackoffLog = 0;
        
        if (!self->_connected && !self->_connecting) {
            if (now - self->_lastConnectAttempt >= self->_reconnectDelay) {
                // Check BOTH total memory AND largest contiguous block
                // SSL handshake needs ~20KB contiguous block, not just total free bytes
                if (currentHeap < MIN_HEAP_FOR_CONNECT || largestBlock < 20000) {
                    // Track when we first encountered low memory
                    if (firstLowHeapTime == 0) {
                        firstLowHeapTime = now;
                        LOG_W("Low memory resources (Heap: %zu, MaxBlock: %zu) - deferring cloud connection", 
                              currentHeap, largestBlock);
                    }
                    
                    unsigned long waitDuration = now - firstLowHeapTime;
                    
                    // Log every 30 seconds (instead of every 10 seconds) to reduce spam
                    if (now - lastHeapLogTime >= 30000) {
                        LOG_D("Waiting for memory to connect (need %d heap + 20KB block, have %zu heap + %zu block, waiting %lu s)", 
                              (int)MIN_HEAP_FOR_CONNECT, currentHeap, largestBlock, waitDuration / 1000);
                        lastHeapLogTime = now;
                    }
                    
                    // If we've been waiting for more than 5 minutes, increase reconnect delay
                    // This prevents constant checking when memory is truly unavailable
                    if (waitDuration > 300000) {  // 5 minutes
                        self->_reconnectDelay = 60000;  // Check every 60 seconds instead of 5
                        // Log once per minute when backing off
                        if (now - lastBackoffLog >= 60000) {
                            LOG_W("Low memory persists - backing off connection attempts (heap: %zu, waited %lu s)", 
                                  currentHeap, waitDuration / 1000);
                            lastBackoffLog = now;
                        }
                    } else {
                        // Normal reconnect delay
                        self->_reconnectDelay = RECONNECT_DELAY_MS;
                    }
                    
                    // Update last connect attempt to prevent immediate retry
                    self->_lastConnectAttempt = now;
                    } else {
                        // Memory is available - reset tracking and try to connect
                        if (firstLowHeapTime != 0) {
                            unsigned long waitDuration = now - firstLowHeapTime;
                            LOG_I("Memory recovered (Heap: %zu, MaxBlock: %zu) after %lu s - attempting connection", 
                                  currentHeap, largestBlock, waitDuration / 1000);
                            firstLowHeapTime = 0;
                            lastHeapLogTime = 0;
                            lastBackoffLog = 0;
                        }
                        self->_reconnectDelay = RECONNECT_DELAY_MS;
                        self->connect();
                        
                        // FIX: Update 'now' because connect() updated _lastConnectAttempt
                        // to a time newer than the loop's 'now' variable.
                        // Without this, the timeout check sees a negative value (wrapped to huge number)
                        // and instantly kills the connection.
                        now = millis();
                    }
            }
        } else {
            // Connected or connecting - reset low memory tracking
            if (firstLowHeapTime != 0) {
                firstLowHeapTime = 0;
                lastHeapLogTime = 0;
                lastBackoffLog = 0;
            }
        }
        
        // Check for SSL handshake timeout
        if (self->_connecting) {
            unsigned long connectTime = now - self->_lastConnectAttempt;
            
            // Log progress every 5 seconds
            static unsigned long lastProgressLog = 0;
            if (connectTime - lastProgressLog >= 5000) {
                LOG_I("SSL handshake in progress... (%lu s)", connectTime / 1000);
                lastProgressLog = connectTime;
            }
            
            if (connectTime > SSL_HANDSHAKE_TIMEOUT_MS) {
                LOG_E("SSL handshake timeout (%ds)", SSL_HANDSHAKE_TIMEOUT_MS/1000);
                self->forceDisconnect();
                self->_lastConnectAttempt = now;
                lastProgressLog = 0;
                
                // Quick retry
                self->_failureCount++;
                self->_reconnectDelay = 10000; // Retry in 10s
                LOG_W("Timeout (%d), retry in 10s", self->_failureCount);
            }
        }
        
        // 5. Processing Loop
        if (xSemaphoreTakeRecursive(self->_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (self->_connected || self->_connecting) {
                self->_ws.loop();
            }
            
            // Process queue only if fully connected
            if (self->_connected) {
                self->processSendQueue();
            }
            
            xSemaphoreGiveRecursive(self->_mutex);
        }
        
        // 6. Proactive State Broadcast
        if (self->_connected && self->_pendingInitialStateBroadcast && 
            millis() >= self->_initialStateBroadcastTime) {
            
            self->_pendingInitialStateBroadcast = false;
            if (self->_onCommand) {
                // Use stack doc to request update
                JsonDocument doc;
                doc["type"] = "request_state";
                doc["source"] = "proactive";
                self->_onCommand("request_state", doc);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10)); // Yield to allow other tasks (Web Server) to run
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
    
    // CRITICAL: Ensure any previous connection is fully cleaned up before creating new one
    // This prevents SSL buffer memory leaks when reconnecting after disconnection
    if (xSemaphoreTakeRecursive(_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        // REMOVED: if (_connected || _connecting) check.
        // reason: handleEvent() sets these to false, causing this cleanup to be 
        // skipped during reconnection, leading to double SSL allocation.
        
        _ws.disconnect();
        
        // Note: getCClient() may not be available in all WebSocketsClient versions
        #ifdef WEBSOCKETS_CLIENT_HAS_GET_CLIENT
        WiFiClientSecure* sslClient = _ws.getCClient();
        if (sslClient) {
            sslClient->stop();
            // Optional: flushing can help ensure the socket is truly dead
            sslClient->flush(); 
        }
        #endif
        
        xSemaphoreGiveRecursive(_mutex);
    }
    
    // Add a small delay to allow the LWIP stack to reclaim the socket memory
    // before we allocate a huge new chunk for the next SSL handshake.
    vTaskDelay(pdMS_TO_TICKS(250));
    
    // CRITICAL: Never start connection during grace period
    // SSL handshake blocks entire LWIP stack - must not start during web server activity
    // Note: Pause logic is now handled by taskCode() based on memory, not here
    unsigned long now = millis();
    if (now < STARTUP_GRACE_PERIOD_MS) {
        LOG_I("Skipping connection - in grace period");
        _connecting = false;
        _lastConnectAttempt = now;
        return;
    }
    
    // Skip registration if we already have a device key (device already paired)
    // Registration is only needed for initial pairing - it adds 5-15s SSL overhead
    // If we had auth failures, the key was regenerated and begin() was called again
    // so _deviceKey should already be updated
    //
    // NOTE: Registration and WebSocket are TWO SEPARATE SSL connections:
    // 1. Registration: HTTPS POST using WiFiClientSecure (one-time, closes after response)
    // 2. WebSocket: WSS connection using WebSocketsClient (persistent, stays open)
    // Each requires its own SSL handshake. The WebSocket handshake may timeout if:
    // - Memory is low after the first SSL connection
    // - Network conditions change between connections
    // - WebSocket library has shorter internal timeout than registration
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
                // Delay after registration to let network stack stabilize and SSL connection fully close
                // This prevents WebSocket SSL handshake timeout after registration
                // ESP32 network stack needs time to clean up first SSL connection before starting second
                LOG_I("Waiting 2s for network stack to stabilize after registration...");
                vTaskDelay(pdMS_TO_TICKS(2000));
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
            // Delay after registration to let network stack stabilize and SSL connection fully close
            // This prevents WebSocket SSL handshake timeout after registration
            // ESP32 network stack needs time to clean up first SSL connection before starting second
            LOG_I("Waiting 2s for network stack to stabilize after registration...");
            vTaskDelay(pdMS_TO_TICKS(2000));
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
    
    // Enable heartbeat (ping every 15s, timeout 8s, 2 failures to disconnect)
    // Mutex protection for all _ws calls
    if (xSemaphoreTakeRecursive(_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        _ws.enableHeartbeat(15000, 8000, 2);
        
        // Configure SSL client timeout (default 5s is too short for slow networks)
        // WebSocketsClient uses WiFiClientSecure internally
        // Try to configure timeout - links2004/WebSockets@^2.4.1 should support getCClient()
        if (useSSL) {
            // Try to get and configure the underlying SSL client
            // Note: getCClient() may not be available in all WebSocketsClient versions
            // links2004/WebSockets@2.7.2 doesn't expose this method
            #ifdef WEBSOCKETS_CLIENT_HAS_GET_CLIENT
            WiFiClientSecure* sslClient = _ws.getCClient();
            if (sslClient) {
                sslClient->setTimeout(15000);  // 15 second timeout for SSL operations
                sslClient->setInsecure();  // Skip certificate validation (faster, acceptable for known server)
                
                // Attempt to force large buffers to PSRAM
                // Note: setBufferSizes() may not be available in all ESP32 Arduino Core versions
                #ifdef ESP32_ARDUINO_CORE_HAS_SETBUFFERSIZES
                sslClient->setBufferSizes(16384, 4096); // Receive 16k, Transmit 4k
                LOG_I("SSL client configured: timeout=15s, insecure mode, PSRAM buffers (16k/4k)");
                #else
                LOG_I("SSL client configured: timeout=15s, insecure mode (PSRAM buffers not configurable)");
                #endif
            }
            #else
            // WebSocketsClient library doesn't expose getCClient() - timeout uses library default
            // The library should handle SSL internally, but timeout may be limited to 5s
            // This is a known limitation with weak WiFi signals - the handshake may timeout
            LOG_I("SSL client: using library default timeout (may be 5s) - getCClient() not available");
            #endif
        }
        
        // Connect WebSocket using resolved IP (more reliable than hostname)
        if (useSSL) {
            LOG_I("Starting SSL WebSocket...");
            _ws.beginSSL(host.c_str(), port, wsPath.c_str());
        } else {
            _ws.begin(host.c_str(), port, wsPath.c_str());
        }
        xSemaphoreGiveRecursive(_mutex);
    } else {
        LOG_W("Could not acquire mutex for connect");
        _connecting = false;
    }
}

void CloudConnection::pause() {
    unsigned long now = millis();
    // Simply extend the timer. The task decides whether to disconnect 
    // based on available HEAP_THRESHOLD_FOR_PAUSE.
    _pausedUntil = now + 30000; 
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
                
                // CRITICAL: Explicitly disconnect to ensure SSL resources are cleaned up
                // When server disconnects us (e.g., during deployment), the WebSocket client
                // may not automatically free SSL buffers, causing memory leaks
                // Note: This event handler is called from _ws.loop() which is already mutex-protected,
                // so we can safely call disconnect() without taking the mutex again
                _ws.disconnect();
                
                // Try to explicitly stop the underlying SSL client to free buffers
                // This is critical for memory cleanup - SSL buffers are ~20-30KB
                // Note: getCClient() may not be available in all WebSocketsClient versions
                #ifdef WEBSOCKETS_CLIENT_HAS_GET_CLIENT
                WiFiClientSecure* sslClient = _ws.getCClient();
                if (sslClient) {
                    sslClient->stop();  // Explicitly stop SSL client to free buffers
                    LOG_D("SSL client stopped for cleanup");
                }
                #endif
                
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
    
    // Extract type as const char* to avoid String allocation
    const char* type = doc["type"] | "";
    
    // Handle cloud-specific messages
    if (strcmp(type, "connected") == 0) {
        LOG_I("Cloud acknowledged connection");
        return;
    }
    
    if (strcmp(type, "error") == 0) {
        const char* errorMsg = doc["error"] | "Unknown error";
        LOG_E("Cloud error: %s", errorMsg);
        return;
    }
    
    // Forward commands to callback
    if (_onCommand) {
        _onCommand(type, doc);
    } else {
        LOG_D("Received message type=%s (no handler)", type);
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
    int processed = 0;
    
    // IMPORTANT: Do NOT sleep inside this function, it is called within a Mutex!
    // Just process a batch and return.
    
    while (processed < 5 && xQueueReceive(_sendQueue, &msg, 0) == pdTRUE && msg) {
        uint8_t* msgPtr = (uint8_t*)msg;
        
        // Check for binary marker
        if (msgPtr[4] == 0x01) {
            uint32_t len = (msgPtr[0] << 24) | (msgPtr[1] << 16) | (msgPtr[2] << 8) | msgPtr[3];
            _ws.sendBIN(msgPtr + 5, len);
        } else {
            _ws.sendTXT((char*)msg);
        }
        
        // Use heap_caps_free because these were allocated with heap_caps_malloc in send()
        heap_caps_free(msg);
        processed++;
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

