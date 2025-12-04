#include "cloud_connection.h"
#include <WiFi.h>

// Logging macros (match project convention)
#define LOG_I(fmt, ...) Serial.printf("[Cloud] " fmt "\n", ##__VA_ARGS__)
#define LOG_W(fmt, ...) Serial.printf("[Cloud] WARN: " fmt "\n", ##__VA_ARGS__)
#define LOG_E(fmt, ...) Serial.printf("[Cloud] ERROR: " fmt "\n", ##__VA_ARGS__)
#define LOG_D(fmt, ...) Serial.printf("[Cloud] DEBUG: " fmt "\n", ##__VA_ARGS__)

CloudConnection::CloudConnection() {
}

void CloudConnection::begin(const String& serverUrl, const String& deviceId, const String& deviceKey) {
    _serverUrl = serverUrl;
    _deviceId = deviceId;
    _deviceKey = deviceKey;
    _enabled = true;
    _reconnectDelay = 1000;
    
    LOG_I("Initialized: server=%s, device=%s", serverUrl.c_str(), deviceId.c_str());
    
    // Don't connect immediately - wait for WiFi and loop() call
}

void CloudConnection::end() {
    _enabled = false;
    if (_connected) {
        _ws.disconnect();
        _connected = false;
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
            LOG_W("WiFi disconnected");
        }
        return;
    }
    
    // If not connected and not connecting, try to connect
    if (!_connected && !_connecting) {
        unsigned long now = millis();
        if (now - _lastConnectAttempt >= _reconnectDelay) {
            connect();
        }
    }
    
    // Process WebSocket events
    _ws.loop();
}

void CloudConnection::connect() {
    if (_serverUrl.isEmpty() || _deviceId.isEmpty()) {
        LOG_W("Cannot connect: missing server URL or device ID");
        return;
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
    
    LOG_I("Connecting to %s:%d%s (SSL=%d)", host.c_str(), port, wsPath.c_str(), useSSL);
    
    // Set up event handler
    _ws.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
        handleEvent(type, payload, length);
    });
    
    // Connect
    if (useSSL) {
        _ws.beginSSL(host.c_str(), port, wsPath.c_str());
    } else {
        _ws.begin(host.c_str(), port, wsPath.c_str());
    }
    
    // Set reconnect interval (WebSocketsClient handles reconnection internally)
    _ws.setReconnectInterval(_reconnectDelay);
    
    // Enable heartbeat (ping every 15s, timeout 3s, disconnect after 2 failures)
    _ws.enableHeartbeat(15000, 3000, 2);
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
    switch (type) {
        case WStype_DISCONNECTED:
            if (_connected) {
                LOG_W("Disconnected from cloud");
                _connected = false;
            }
            _connecting = false;
            
            // Exponential backoff for reconnection
            _reconnectDelay = min(_reconnectDelay * 2, MAX_RECONNECT_DELAY);
            break;
            
        case WStype_CONNECTED:
            LOG_I("Connected to cloud!");
            _connected = true;
            _connecting = false;
            _reconnectDelay = 1000;  // Reset backoff on successful connection
            break;
            
        case WStype_TEXT:
            handleMessage(payload, length);
            break;
            
        case WStype_BIN:
            LOG_D("Received binary data (length=%d)", length);
            break;
            
        case WStype_ERROR:
            LOG_E("WebSocket error");
            _connecting = false;
            break;
            
        case WStype_PING:
            LOG_D("Ping received");
            break;
            
        case WStype_PONG:
            LOG_D("Pong received");
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

void CloudConnection::send(const JsonDocument& doc) {
    if (!_connected) {
        return;
    }
    
    String json;
    serializeJson(doc, json);
    _ws.sendTXT(json);
}

void CloudConnection::onCommand(CommandCallback callback) {
    _onCommand = callback;
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

