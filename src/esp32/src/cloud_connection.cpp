#include "cloud_connection.h"
#include <WiFi.h>
#include <esp_heap_caps.h>

// Logging macros (match project convention)
#define LOG_I(fmt, ...) Serial.printf("[Cloud] " fmt "\n", ##__VA_ARGS__)
#define LOG_W(fmt, ...) Serial.printf("[Cloud] WARN: " fmt "\n", ##__VA_ARGS__)
#define LOG_E(fmt, ...) Serial.printf("[Cloud] ERROR: " fmt "\n", ##__VA_ARGS__)
#define LOG_D(fmt, ...) Serial.printf("[Cloud] DEBUG: " fmt "\n", ##__VA_ARGS__)

// Minimum free heap required for SSL connection (SSL needs ~40KB + margin)
#define MIN_FREE_HEAP_FOR_SSL 70000

// If disconnected within this time of connecting, it's a "quick disconnect" (likely server rejection)
#define QUICK_DISCONNECT_THRESHOLD_MS 5000

// Max retries before giving up for a longer period
#define MAX_QUICK_DISCONNECT_RETRIES 3

// Long backoff after too many quick disconnects (30 seconds)
#define LONG_BACKOFF_MS 30000

CloudConnection::CloudConnection() {
}

void CloudConnection::begin(const String& serverUrl, const String& deviceId, const String& deviceKey) {
    _serverUrl = serverUrl;
    _deviceId = deviceId;
    _deviceKey = deviceKey;
    _enabled = true;
    _reconnectDelay = 5000;  // Start with 5 seconds
    _quickDisconnectCount = 0;
    _inLongBackoff = false;
    
    // Set up event handler ONCE here (not in connect() to avoid memory leaks)
    _ws.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
        handleEvent(type, payload, length);
    });
    
    // Disable automatic reconnection - we handle it ourselves
    _ws.setReconnectInterval(0);
    
    LOG_I("Initialized: server=%s, device=%s", serverUrl.c_str(), deviceId.c_str());
    
    // Don't connect immediately - wait for WiFi and loop() call
}

void CloudConnection::end() {
    // Step 1: Disable immediately to prevent loop() from interfering
    _enabled = false;
    
    // Step 2: Mark as not connected/connecting to prevent send() calls
    bool wasConnected = _connected;
    bool wasConnecting = _connecting;
    _connected = false;
    _connecting = false;
    
    if (wasConnected || wasConnecting) {
        // Step 3: Give any in-flight SSL operations time to complete
        // This prevents the "CIPHER - Bad input parameters" error
        yield();
        delay(100);
        yield();
        
        // Step 4: Process pending WebSocket events before disconnect
        // This allows clean shutdown of the SSL layer
        for (int i = 0; i < 5; i++) {
            _ws.loop();
            yield();
            delay(20);
        }
        
        // Step 5: Now safely disconnect
        _ws.disconnect();
        
        // Step 6: Allow disconnect to fully complete
        yield();
        delay(100);
        yield();
    }
    
    LOG_I("Disabled");
}

void CloudConnection::loop() {
    if (!_enabled) {
        return;
    }
    
    // Check WiFi
    if (WiFi.status() != WL_CONNECTED) {
        if (_connected) {
            _connected = false;
            _connecting = false;
            LOG_W("WiFi disconnected");
        }
        return;
    }
    
    // If in long backoff, don't process anything - just wait
    if (_inLongBackoff) {
        unsigned long now = millis();
        if (now - _lastConnectAttempt < _reconnectDelay) {
            // Still in backoff period - don't process WebSocket events
            return;
        }
        // Backoff expired, reset and try again
        LOG_I("Backoff expired, will retry connection");
        _inLongBackoff = false;
        _quickDisconnectCount = 0;
        _reconnectDelay = 5000;  // Reset to initial delay
    }
    
    // If not connected and not connecting, try to connect
    if (!_connected && !_connecting) {
        unsigned long now = millis();
        if (now - _lastConnectAttempt >= _reconnectDelay) {
            // Check free heap before attempting SSL connection
            size_t freeHeap = ESP.getFreeHeap();
            static bool lowMemLogged = false;
            
            if (freeHeap < MIN_FREE_HEAP_FOR_SSL) {
                // Don't spam the log - only log once when entering this state
                if (!lowMemLogged) {
                    LOG_W("Insufficient memory for SSL (free: %u, need: %u) - pausing cloud", 
                          freeHeap, MIN_FREE_HEAP_FOR_SSL);
                    lowMemLogged = true;
                }
                // Wait longer before retrying when memory is low
                _lastConnectAttempt = now;
                _reconnectDelay = LONG_BACKOFF_MS * 2;  // Double the normal backoff
                _inLongBackoff = true;
                return;  // Skip _ws.loop() when memory is low
            } else {
                // Memory recovered - reset the log flag
                lowMemLogged = false;
            }
            
            connect();
        }
        return;  // Don't call _ws.loop() when not connected - prevents hammering
    }
    
    // Process WebSocket events only when connected
    _ws.loop();
}

void CloudConnection::connect() {
    if (_serverUrl.isEmpty() || _deviceId.isEmpty()) {
        LOG_W("Cannot connect: missing server URL or device ID");
        _connecting = false;
        return;
    }
    
    // Register device key with cloud before first connection
    // This ensures the cloud server knows our key for authentication
    if (!_registered && _onRegister) {
        LOG_I("Registering device with cloud...");
        if (_onRegister()) {
            LOG_I("Device registered successfully");
            _registered = true;
        } else {
            LOG_W("Device registration failed - will retry next connection");
            // Don't block connection attempt - server might already know us
        }
    }
    
    // Disconnect and cleanup any existing connection first
    // This is critical for SSL memory management
    _ws.disconnect();
    delay(100);  // Give it time to clean up SSL resources
    
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
    
    size_t freeHeap = ESP.getFreeHeap();
    LOG_I("Connecting to %s:%d%s (SSL=%d, free heap: %u bytes)", 
          host.c_str(), port, wsPath.c_str(), useSSL, freeHeap);
    
    // Enable heartbeat (ping every 15s, timeout 3s, disconnect after 2 failures)
    _ws.enableHeartbeat(15000, 3000, 2);
    
    // Connect
    if (useSSL) {
        _ws.beginSSL(host.c_str(), port, wsPath.c_str());
    } else {
        _ws.begin(host.c_str(), port, wsPath.c_str());
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
    // Safety check: ignore events if we're shutting down
    // This prevents crashes during SSL teardown when end() was called
    if (!_enabled && type != WStype_DISCONNECTED) {
        return;
    }
    
    switch (type) {
        case WStype_DISCONNECTED:
            {
                bool wasConnected = _connected;
                unsigned long now = millis();
                
                _connected = false;
                _connecting = false;
                
                if (wasConnected) {
                    // Check if this was a "quick disconnect" (server rejection)
                    unsigned long connectedDuration = now - _lastConnectedTime;
                    if (connectedDuration < QUICK_DISCONNECT_THRESHOLD_MS) {
                        _quickDisconnectCount++;
                        LOG_W("Quick disconnect #%d (connected for %lums) - server may be rejecting", 
                              _quickDisconnectCount, connectedDuration);
                        
                        // Trigger long backoff immediately if too many quick disconnects
                        if (_quickDisconnectCount >= MAX_QUICK_DISCONNECT_RETRIES) {
                            LOG_W("Too many quick disconnects - backing off for 30 seconds");
                            _reconnectDelay = LONG_BACKOFF_MS;
                            _inLongBackoff = true;
                            _lastConnectAttempt = millis();  // Use fresh timestamp
                            _quickDisconnectCount = 0;  // Reset counter for next cycle
                        } else {
                            // Increase backoff significantly for quick disconnects
                            _reconnectDelay = min(_reconnectDelay * 3, MAX_RECONNECT_DELAY);
                        }
                    } else {
                        // Normal disconnect after stable connection
                        LOG_W("Disconnected from cloud (was connected for %lums)", connectedDuration);
                        _quickDisconnectCount = 0;  // Reset counter on stable connection
                        _reconnectDelay = min(_reconnectDelay * 2, MAX_RECONNECT_DELAY);
                    }
                } else {
                    // Disconnected while connecting (connection failed)
                    _reconnectDelay = min(_reconnectDelay * 2, MAX_RECONNECT_DELAY);
                }
            }
            break;
            
        case WStype_CONNECTED:
            LOG_I("Connected to cloud!");
            _connected = true;
            _connecting = false;
            _lastConnectedTime = millis();
            // Don't reset backoff immediately - wait for stable connection
            break;
            
        case WStype_TEXT:
            handleMessage(payload, length);
            break;
            
        case WStype_BIN:
            LOG_D("Received binary data (length=%d)", length);
            break;
            
        case WStype_ERROR:
            {
                // Check if it's an SSL memory error
                size_t freeHeap = ESP.getFreeHeap();
                LOG_E("WebSocket error (free heap: %u bytes)", freeHeap);
                _connecting = false;
                
                // If it's a memory issue, increase backoff significantly
                if (freeHeap < MIN_FREE_HEAP_FOR_SSL) {
                    _reconnectDelay = MAX_RECONNECT_DELAY;
                    LOG_W("SSL memory error - waiting %lu ms before retry", _reconnectDelay);
                } else {
                    // Exponential backoff for other errors
                    _reconnectDelay = min(_reconnectDelay * 2, MAX_RECONNECT_DELAY);
                }
            }
            break;
            
        case WStype_PING:
            // Ping handled automatically by library - no logging needed
            break;
            
        case WStype_PONG:
            // Pong handled automatically by library - no logging needed
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
    if (!_connected) {
        return;
    }
    
    // Use const char* overload to avoid unnecessary string copy
    _ws.sendTXT(json.c_str(), json.length());
}

void CloudConnection::send(const char* json) {
    if (!_connected || !json) {
        return;
    }
    
    // Send directly without String allocation
    _ws.sendTXT(json, strlen(json));
}

void CloudConnection::send(const JsonDocument& doc) {
    if (!_connected) {
        return;
    }
    
    // Allocate JSON buffer in internal RAM (not PSRAM) to avoid crashes
    size_t jsonSize = measureJson(doc) + 1;
    char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!jsonBuffer) {
        jsonBuffer = (char*)malloc(jsonSize);
    }
    
    if (jsonBuffer) {
        serializeJson(doc, jsonBuffer, jsonSize);
        _ws.sendTXT(jsonBuffer, strlen(jsonBuffer));
        free(jsonBuffer);
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

