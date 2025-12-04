# WebSocket Protocol

The BrewOS web interface communicates with the ESP32 (and cloud service) using a JSON-based WebSocket protocol.

## Connection

### Local (ESP32 Direct)

```
ws://brewos.local/ws
```

No authentication required when on the local network.

### Cloud (Remote Access)

```
wss://your-cloud-domain.com/ws/client?token=JWT_TOKEN&device=DEVICE_ID
```

Requires JWT authentication and device ID.

## Message Format

All messages are JSON objects with a `type` field:

```json
{
  "type": "message_type",
  "timestamp": 1699876543210,
  ...payload
}
```

## Messages from Device (ESP32 → Client)

### Status Updates

**`status`** - Combined status update:
```json
{
  "type": "status",
  "machine": {
    "state": "ready",
    "mode": "on",
    "isHeating": false,
    "isBrewing": false
  },
  "temps": {
    "brew": { "current": 93.2, "setpoint": 93.5 },
    "steam": { "current": 145.0, "setpoint": 145.0 }
  },
  "pressure": 9.2,
  "power": { "current": 50, "voltage": 220 }
}
```

**`esp_status`** - ESP32-specific status:
```json
{
  "type": "esp_status",
  "version": "1.0.0",
  "freeHeap": 156432,
  "uptime": 3600000,
  "wifi": {
    "connected": true,
    "ssid": "HomeNetwork",
    "ip": "192.168.1.100",
    "rssi": -45
  },
  "mqtt": {
    "enabled": true,
    "connected": true
  }
}
```

**`pico_status`** - Pico controller status:
```json
{
  "type": "pico_status",
  "version": "1.0.0",
  "uptime": 3600000,
  "state": "ready",
  "brewTemp": 93.2,
  "brewSetpoint": 93.5,
  "steamTemp": 145.0,
  "steamSetpoint": 145.0,
  "pressure": 0.0,
  "power": 50,
  "voltage": 220,
  "isHeating": false,
  "isBrewing": false,
  "waterLevel": "ok",
  "dripTrayFull": false
}
```

**`scale_status`** - BLE scale status:
```json
{
  "type": "scale_status",
  "connected": true,
  "name": "Lunar-ABC123",
  "type": "acaia",
  "weight": 36.2,
  "flowRate": 2.1,
  "stable": true,
  "battery": 85
}
```

### Events

**`event`** - Machine events:
```json
{
  "type": "event",
  "event": "shot_start" | "shot_end" | "alert",
  "level": "info" | "warning" | "error",
  "message": "Optional message"
}
```

### Logs

**`log`** - System log entry:
```json
{
  "type": "log",
  "level": "info" | "warn" | "error" | "debug",
  "message": "Log message text"
}
```

### Errors

**`error`** - Error notification:
```json
{
  "type": "error",
  "error": "error_code",
  "message": "Human readable error message"
}
```

### Scale Scan

**`scan_result`** - Found scale during scan:
```json
{
  "type": "scan_result",
  "address": "AA:BB:CC:DD:EE:FF",
  "name": "Lunar-ABC123",
  "rssi": -65,
  "scaleType": "acaia"
}
```

**`scan_complete`** - Scan finished:
```json
{
  "type": "scan_complete"
}
```

## Messages to Device (Client → ESP32)

### Commands

All commands use the `command` type:

```json
{
  "type": "command",
  "cmd": "command_name",
  ...parameters
}
```

**Available Commands:**

| Command | Parameters | Description |
|---------|------------|-------------|
| `set_mode` | `mode: "standby" \| "on" \| "eco"` | Set machine mode |
| `set_temp` | `boiler: "brew" \| "steam", temp: number` | Set temperature setpoint |
| `set_power` | `voltage: number, maxCurrent: number` | Set power limits |
| `set_bbw` | `enabled, targetWeight, doseWeight, stopOffset, autoTare` | Configure brew-by-weight |
| `set_eco` | `brewTemp: number, timeout: number` | Configure eco mode |
| `tare` | - | Tare the scale |
| `scale_reset` | - | Reset scale reading |
| `scale_scan` | - | Start scanning for scales |
| `scale_scan_stop` | - | Stop scale scan |
| `scale_connect` | `address: string` | Connect to scale |
| `scale_disconnect` | - | Disconnect scale |
| `wifi_forget` | - | Forget WiFi, start AP mode |
| `mqtt_test` | `broker, port, username, password` | Test MQTT connection |
| `mqtt_config` | `enabled, broker, port, username, password, discovery` | Save MQTT config |
| `check_update` | - | Check for firmware updates |
| `ota_start` | - | Start OTA update |
| `restart` | - | Restart ESP32 |
| `factory_reset` | - | Factory reset |

## Connection Health

Connection health is detected by monitoring incoming status messages rather than explicit ping/pong.

**Status Update Frequency:**
- ESP32 broadcasts status every **500ms**
- If no messages received for **3 seconds**, the connection is considered stale

**Stale Connection Handling:**
1. Client detects no messages for 3+ seconds
2. Client closes the WebSocket connection
3. Automatic reconnection is triggered
4. Reconnection uses exponential backoff (1s → 5s max)

This approach is more efficient than ping/pong since:
- Status updates already provide a heartbeat signal
- No additional network traffic required
- Faster detection of dead connections (3s vs typical 30s ping interval)

## Cloud-Specific Messages

When connecting through the cloud service, additional messages are used:

**`connected`** - Connection established:
```json
{
  "type": "connected",
  "sessionId": "uuid",
  "deviceId": "device-id",
  "deviceOnline": true,
  "timestamp": 1699876543210
}
```

**`device_online`** - Device came online:
```json
{
  "type": "device_online"
}
```

**`device_offline`** - Device went offline:
```json
{
  "type": "device_offline"
}
```

## Error Codes

| Code | Description |
|------|-------------|
| `device_offline` | Target device is not connected |
| `unauthorized` | Authentication failed |
| `invalid_command` | Unknown or malformed command |
| `timeout` | Operation timed out |

