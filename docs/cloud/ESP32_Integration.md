# ESP32 Cloud Integration

This guide explains how to connect your BrewOS ESP32 device to the cloud service for remote access.

## Overview

The ESP32 maintains a persistent WebSocket connection to the cloud service, enabling:

- Remote monitoring from anywhere
- Remote control via web/mobile apps
- Multi-device management

## Connection Flow

```
┌─────────┐                    ┌─────────────┐                    ┌────────┐
│  ESP32  │                    │    Cloud    │                    │  App   │
└────┬────┘                    └──────┬──────┘                    └───┬────┘
     │                                │                               │
     │  1. Connect (id + key)         │                               │
     │ ──────────────────────────────►│                               │
     │                                │                               │
     │  2. { type: "connected" }      │                               │
     │ ◄──────────────────────────────│                               │
     │                                │                               │
     │                                │  3. Connect (token + device)  │
     │                                │◄──────────────────────────────│
     │                                │                               │
     │                                │  4. { type: "connected",      │
     │                                │       deviceOnline: true }    │
     │                                │──────────────────────────────►│
     │                                │                               │
     │  5. Status updates             │                               │
     │ ──────────────────────────────►│──────────────────────────────►│
     │                                │                               │
     │                                │  6. Commands                  │
     │ ◄──────────────────────────────│◄──────────────────────────────│
     │                                │                               │
```

## ESP32 Implementation

### Connection Manager

Add to your ESP32 firmware:

```cpp
// cloud_client.h
#pragma once

#include <WebSocketsClient.h>

class CloudClient {
public:
    void init(const char* cloudUrl, const char* deviceId, const char* deviceKey);
    void loop();
    void sendStatus(const char* statusJson);

    void onCommand(std::function<void(const char* cmd, JsonObject& data)> callback);

    bool isConnected() const { return _connected; }

private:
    WebSocketsClient _ws;
    String _deviceId;
    String _deviceKey;
    bool _connected = false;
    unsigned long _lastReconnect = 0;
    std::function<void(const char*, JsonObject&)> _commandCallback;

    void connect();
    void handleMessage(uint8_t* payload, size_t length);
};
```

```cpp
// cloud_client.cpp
#include "cloud_client.h"
#include <ArduinoJson.h>

void CloudClient::init(const char* cloudUrl, const char* deviceId, const char* deviceKey) {
    _deviceId = deviceId;
    _deviceKey = deviceKey;

    // Parse URL and extract host/port
    // Example: wss://cloud.brewos.dev/ws/device

    _ws.beginSSL("cloud.brewos.dev", 443,
        String("/ws/device?id=" + _deviceId + "&key=" + _deviceKey).c_str());

    _ws.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
        switch (type) {
            case WStype_CONNECTED:
                _connected = true;
                Serial.println("[Cloud] Connected");
                break;

            case WStype_DISCONNECTED:
                _connected = false;
                Serial.println("[Cloud] Disconnected");
                break;

            case WStype_TEXT:
                handleMessage(payload, length);
                break;
        }
    });

    _ws.setReconnectInterval(5000);
}

void CloudClient::loop() {
    _ws.loop();
}

void CloudClient::sendStatus(const char* statusJson) {
    if (_connected) {
        _ws.sendTXT(statusJson);
    }
}

void CloudClient::handleMessage(uint8_t* payload, size_t length) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error) {
        Serial.println("[Cloud] JSON parse error");
        return;
    }

    const char* type = doc["type"];

    if (strcmp(type, "command") == 0) {
        const char* cmd = doc["cmd"];
        JsonObject data = doc.as<JsonObject>();

        if (_commandCallback) {
            _commandCallback(cmd, data);
        }
    }
}

void CloudClient::onCommand(std::function<void(const char*, JsonObject&)> callback) {
    _commandCallback = callback;
}
```

### Usage

```cpp
#include "cloud_client.h"

CloudClient cloudClient;

void setup() {
    // ... WiFi setup ...

    cloudClient.init(
        "wss://cloud.brewos.dev/ws/device",
        "my-device-id",
        "my-device-key"
    );

    cloudClient.onCommand([](const char* cmd, JsonObject& data) {
        if (strcmp(cmd, "set_temp") == 0) {
            float temp = data["temp"];
            const char* boiler = data["boiler"];
            // Handle command
        }
    });
}

void loop() {
    cloudClient.loop();

    // Send status every 2 seconds
    static unsigned long lastStatus = 0;
    if (millis() - lastStatus > 2000) {
        lastStatus = millis();

        StaticJsonDocument<256> doc;
        doc["type"] = "status";
        doc["temps"]["brew"]["current"] = brewTemp;
        doc["temps"]["steam"]["current"] = steamTemp;
        doc["pressure"] = pressure;

        String json;
        serializeJson(doc, json);
        cloudClient.sendStatus(json.c_str());
    }
}
```

## Configuration

### Device Registration

Each device needs:

1. **Device ID** - Unique identifier (e.g., MAC address or UUID)
2. **Device Key** - Secret key for authentication

Store in NVS:

```cpp
void saveCloudConfig(const char* deviceId, const char* deviceKey) {
    preferences.begin("cloud", false);
    preferences.putString("device_id", deviceId);
    preferences.putString("device_key", deviceKey);
    preferences.end();
}
```

### Cloud URL Configuration

Allow users to configure the cloud URL (for self-hosted instances):

```cpp
String cloudUrl = preferences.getString("cloud_url", "wss://cloud.brewos.dev/ws/device");
```

## Security

### Device Key Authentication

Every ESP32 device has a unique device key used for authentication:

1. **First Boot**: ESP32 generates a cryptographically random device key (32 bytes, base64url encoded)
2. **NVS Storage**: Key is persisted in secure NVS partition under `brewos_sec/devKey`
3. **Registration**: During pairing (QR code scan), ESP32 sends key to cloud via `/api/devices/register-claim`
4. **Connection**: Every WebSocket connection includes the key as a query parameter
5. **Validation**: Cloud verifies key hash before allowing connection

```cpp
// Key generation happens automatically in PairingManager
// Connection URL includes key:
// wss://cloud.brewos.io/ws/device?id=BRW-12345678&key=<device_key>
```

### TLS/SSL

Always use WSS (WebSocket Secure) in production:

- ESP32 supports TLS 1.2
- Verify server certificate if possible

### Device Key Rotation

Implement key rotation:

1. Cloud generates new key
2. Sends `key_rotate` command with new key
3. ESP32 saves new key, reconnects

### Secure Storage

Store device key in encrypted NVS partition.

## Reconnection

Handle disconnections gracefully:

```cpp
void CloudClient::loop() {
    _ws.loop();

    // Manual reconnection with backoff
    if (!_connected && millis() - _lastReconnect > _reconnectDelay) {
        _lastReconnect = millis();
        _reconnectDelay = min(_reconnectDelay * 2, 60000UL);  // Max 1 minute
        Serial.println("[Cloud] Reconnecting...");
    }

    if (_connected) {
        _reconnectDelay = 5000;  // Reset on successful connection
    }
}
```

## Status Messages

Send regular status updates:

```cpp
void sendFullStatus() {
    StaticJsonDocument<512> doc;
    doc["type"] = "pico_status";
    doc["version"] = FIRMWARE_VERSION;
    doc["uptime"] = millis();
    doc["state"] = getMachineState();
    doc["brewTemp"] = brewBoiler.getTemperature();
    doc["brewSetpoint"] = brewBoiler.getSetpoint();
    doc["steamTemp"] = steamBoiler.getTemperature();
    doc["steamSetpoint"] = steamBoiler.getSetpoint();
    doc["pressure"] = getPressure();
    doc["power"] = getPower();
    doc["isHeating"] = isHeating();
    doc["isBrewing"] = isBrewing();

    String json;
    serializeJson(doc, json);
    cloudClient.sendStatus(json.c_str());
}
```

## Bandwidth Optimization

Minimize data usage:

1. Send status updates every 2-5 seconds (not faster)
2. Use short field names in JSON
3. Only send changed values when possible
4. Batch multiple updates into single message

## Offline Operation

The ESP32 should work fully offline:

- Cloud connection is optional
- Local WebSocket still works
- Queue commands when offline (optional)

```cpp
bool hasCloudConnection() {
    return cloudClient.isConnected();
}

bool hasLocalConnection() {
    return webSocket.connectedClients() > 0;
}

bool hasAnyConnection() {
    return hasCloudConnection() || hasLocalConnection();
}
```
