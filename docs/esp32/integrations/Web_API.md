# Web API Reference

> **Status:** ✅ Implemented  
> **Last Updated:** 2025-11-28  
> **Base URL:** `http://<device-ip>/api`

## Overview

The ESP32 provides a RESTful HTTP API for machine control, configuration, and monitoring. All responses are JSON.

---

## Endpoint Summary

| Method | Path | Description | Status |
|--------|------|-------------|--------|
| GET | `/api/status` | Machine status | ✅ |
| GET | `/api/wifi/networks` | Scan WiFi | ✅ |
| POST | `/api/wifi/connect` | Connect to WiFi | ✅ |
| GET | `/api/config` | Get config | ✅ |
| POST | `/api/command` | Send command | ✅ |
| POST | `/api/ota/upload` | Upload firmware | ✅ |
| POST | `/api/ota/start` | Flash Pico | ✅ |
| POST | `/api/pico/reset` | Reset Pico | ✅ |
| GET | `/api/mqtt/config` | Get MQTT config | ✅ |
| POST | `/api/mqtt/config` | Set MQTT config | ✅ |
| POST | `/api/mqtt/test` | Test MQTT connection | ✅ |

---

## Status Endpoints

### GET /api/status

Returns current machine and ESP32 status.

**Response:**
```json
{
  "wifi": {
    "mode": 2,
    "ssid": "MyNetwork",
    "ip": "192.168.1.100",
    "rssi": -45,
    "configured": true
  },
  "pico": {
    "connected": true,
    "packetsReceived": 12345,
    "packetErrors": 0
  },
  "esp32": {
    "uptime": 3600000,
    "freeHeap": 180000,
    "version": "1.0.0"
  },
  "mqtt": {
    "enabled": true,
    "connected": true,
    "status": "Connected"
  },
  "clients": 2
}
```

**WiFi Modes:**
| Value | Mode | Description |
|-------|------|-------------|
| 0 | DISCONNECTED | Not connected |
| 1 | AP_MODE | Access point (setup mode) |
| 2 | STA_MODE | Connected to network |
| 3 | CONNECTING | Connection in progress |

---

## WiFi Endpoints

### GET /api/wifi/networks

Scans for available WiFi networks.

**Response:**
```json
{
  "networks": [
    {
      "ssid": "MyNetwork",
      "rssi": -45,
      "secure": true
    },
    {
      "ssid": "Guest",
      "rssi": -60,
      "secure": false
    }
  ]
}
```

### POST /api/wifi/connect

Connect to a WiFi network.

**Request:**
```json
{
  "ssid": "MyNetwork",
  "password": "mypassword123"
}
```

**Response:**
```json
{
  "status": "ok",
  "message": "Connecting to MyNetwork..."
}
```

> **Note:** The device will restart after connecting. The web page will need to be reloaded at the new IP address.

---

## Configuration Endpoints

### GET /api/config

Returns current machine configuration from Pico.

**Response:**
```json
{
  "brew_temp_setpoint": 94.0,
  "steam_temp_setpoint": 145.0,
  "preinfusion_time": 3.0,
  "preinfusion_pressure": 2.0,
  "machine_type": "dual_boiler"
}
```

### POST /api/command

Send a command to the Pico.

**Request:**
```json
{
  "cmd": "set_temp",
  "boiler": "brew",
  "value": 94.0
}
```

**Response:**
```json
{
  "status": "ok"
}
```

---

## OTA Update Endpoints

### POST /api/ota/upload

Upload firmware file for Pico OTA update.

**Request:** `multipart/form-data` with firmware file (.uf2 or .bin)

**Response:** Progress updates via WebSocket:
```json
{
  "type": "ota_progress",
  "stage": "upload",
  "progress": 50,
  "uploaded": 102400,
  "total": 204800
}
```

### POST /api/ota/start

Flash uploaded firmware to Pico.

**Response:** Progress updates via WebSocket:
```json
{
  "type": "ota_progress",
  "stage": "flash",
  "progress": 75,
  "sent": 153600,
  "total": 204800
}
```

### POST /api/pico/reset

Trigger Pico reset via RESET pin.

**Response:**
```json
{
  "status": "ok"
}
```

---

## MQTT Configuration Endpoints

### GET /api/mqtt/config

Get current MQTT configuration.

**Response:**
```json
{
  "enabled": true,
  "broker": "mqtt.example.com",
  "port": 1883,
  "username": "mqtt_user",
  "password": "",
  "client_id": "brewos_xxxxxxxxxxxx",
  "topic_prefix": "brewos",
  "use_tls": false,
  "ha_discovery": true,
  "ha_device_id": "xxxxxxxxxxxx",
  "connected": true,
  "status": "Connected"
}
```

> **Note:** Password is not returned for security.

### POST /api/mqtt/config

Update MQTT configuration.

**Request:**
```json
{
  "enabled": true,
  "broker": "mqtt.example.com",
  "port": 1883,
  "username": "mqtt_user",
  "password": "secret123",
  "topic_prefix": "brewos",
  "ha_discovery": true,
  "ha_device_id": ""
}
```

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `enabled` | boolean | No | false | Enable MQTT |
| `broker` | string | Yes* | - | Broker hostname/IP |
| `port` | integer | No | 1883 | Broker port |
| `username` | string | No | "" | Auth username |
| `password` | string | No | "" | Auth password (only updated if provided) |
| `topic_prefix` | string | No | "brewos" | Topic prefix |
| `ha_discovery` | boolean | No | true | Enable HA discovery |
| `ha_device_id` | string | No | auto | Device ID (auto-generated from MAC if empty) |

**Response:**
```json
{
  "status": "ok"
}
```

### POST /api/mqtt/test

Test MQTT connection with current configuration.

**Response (success):**
```json
{
  "status": "ok",
  "message": "Connection successful"
}
```

**Response (failure):**
```json
{
  "error": "Connection failed"
}
```

---

## WebSocket API

Connect to `/ws` for real-time updates.

### Message Types (Server → Client)

**Status Update:**
```json
{
  "type": "status",
  "brew_temp": 93.5,
  "steam_temp": 145.2,
  "pressure": 9.1,
  "state": 3,
  "state_name": "READY"
}
```

**Log Message:**
```json
{
  "type": "log",
  "message": "WiFi connected",
  "level": "info"
}
```

**Pico Message:**
```json
{
  "type": "pico",
  "msg_type": 1,
  "payload": "base64_encoded_data"
}
```

**OTA Progress:**
```json
{
  "type": "ota_progress",
  "stage": "flash",
  "progress": 50,
  "sent": 102400,
  "total": 204800
}
```

### Message Types (Client → Server)

**Request Status:**
```json
{
  "type": "get_status"
}
```

**Ping:**
```json
{
  "type": "ping"
}
```

---

## Planned Endpoints

### Machine Control
```
POST /api/temp/brew             # Set brew temperature
POST /api/temp/steam            # Set steam temperature
POST /api/mode                  # Set machine mode (idle/standby)
```

> **Note:** Brew start/stop is controlled by the physical lever on the machine, not via API.

### Configuration
```
GET  /api/config/all            # Full configuration
POST /api/config/pid            # Set PID parameters
POST /api/config/preinfusion    # Pre-infusion settings
POST /api/config/environmental  # Voltage/current config
POST /api/config/schedule       # Power-on schedule
```

### Statistics & History
```
GET  /api/stats                 # Usage statistics
GET  /api/shots                 # Recent shot history
GET  /api/shots/{id}            # Specific shot details
DELETE /api/shots               # Clear shot history
```

### Cloud Integration
```
GET  /api/cloud/status          # Cloud connection status
POST /api/cloud/link            # Link to cloud account
POST /api/cloud/unlink          # Unlink from cloud
```

### Brew by Weight
```
GET  /api/scale/status          # Scale connection status
POST /api/scale/pair            # Pair BLE scale
POST /api/scale/tare            # Tare scale
POST /api/weight/target         # Set target weight
```

---

## Authentication (Planned)

```
[ ] API key authentication (header or query param)
[ ] Session-based authentication for web UI
[ ] Rate limiting
[ ] CORS configuration
```

---

## Error Responses

All errors follow this format:

```json
{
  "error": "Error message description"
}
```

| HTTP Code | Description |
|-----------|-------------|
| 400 | Bad Request - Invalid JSON or parameters |
| 404 | Not Found - Endpoint doesn't exist |
| 500 | Server Error - Internal error |

---

## Implementation Details

### Files

| File | Description |
|------|-------------|
| `include/web_server.h` | WebServer class interface |
| `src/web_server.cpp` | HTTP/WebSocket implementation |
| `data/index.html` | Web UI HTML |
| `data/style.css` | Web UI styles |
| `data/app.js` | Web UI JavaScript |

### Dependencies

- `ESPAsyncWebServer` - Async HTTP server
- `AsyncTCP` - Async TCP library
- `ArduinoJson` - JSON parsing/serialization
- `LittleFS` - File system for static files

