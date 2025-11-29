# MQTT Integration

> **Status:** ✅ Implemented  
> **Last Updated:** 2025-11-28

## Overview

MQTT provides lightweight pub/sub messaging for home automation integration (Home Assistant, Node-RED, etc.). The ESP32 acts as an MQTT client, publishing machine status and subscribing to commands.

## Configuration

### Web UI

Configure MQTT via the web interface at `http://<device-ip>/` → MQTT Configuration card.

### NVS Storage

Configuration is persisted in ESP32 NVS (Non-Volatile Storage):

```cpp
struct MQTTConfig {
    bool enabled;           // Enable/disable MQTT
    char broker[64];        // Broker hostname or IP
    uint16_t port;          // Broker port (default: 1883)
    char username[32];      // Authentication username (optional)
    char password[64];      // Authentication password (optional)
    char client_id[32];     // MQTT client ID (auto-generated if empty)
    char topic_prefix[32];  // Topic prefix (default: "brewos")
    bool use_tls;           // Enable TLS/SSL (not yet implemented)
    bool ha_discovery;      // Enable Home Assistant auto-discovery
    char ha_device_id[32];  // Device ID for topics (auto-generated from MAC)
};
```

---

## Topic Structure

All topics follow the pattern: `{topic_prefix}/{device_id}/{topic_name}`

Default prefix is `brewos`, device_id is derived from MAC address.

| Topic | Direction | QoS | Retained | Description |
|-------|-----------|-----|----------|-------------|
| `brewos/{id}/availability` | Publish | 1 | Yes | Online/offline status (LWT) |
| `brewos/{id}/status` | Publish | 0 | Yes | Full machine status JSON |
| `brewos/{id}/shot` | Publish | 0 | No | Shot data when brewing completes |
| `brewos/{id}/statistics` | Publish | 0 | Yes | Usage statistics |
| `brewos/{id}/command` | Subscribe | 1 | No | Incoming commands |

---

## Status Payload

Published every 1 second to `brewos/{id}/status`:

```json
{
  "state": 3,
  "brew_temp": 93.5,
  "brew_setpoint": 94.0,
  "steam_temp": 145.2,
  "steam_setpoint": 145.0,
  "pressure": 9.1,
  "is_brewing": false,
  "is_heating": true,
  "water_low": false,
  "alarm_active": false,
  "pico_connected": true,
  "wifi_connected": true,
  "scale_connected": false
}
```

### Machine States

| Value | State | Description |
|-------|-------|-------------|
| 0 | INIT | Initializing |
| 1 | IDLE | Powered on, not heating |
| 2 | HEATING | Heating to setpoint |
| 3 | READY | At temperature, ready to brew |
| 4 | BREWING | Actively brewing |
| 5 | STEAMING | Steam wand active |
| 6 | COOLDOWN | Cooling down |
| 7 | FAULT | Error condition |
| 8 | SAFE | Safe mode (heaters disabled) |

---

## Command Payload

Subscribe to `brewos/{id}/command` to receive commands:

### Set Temperature

```json
{
  "cmd": "set_temp",
  "boiler": "brew",
  "temp": 94.0
}
```

| Field | Type | Values |
|-------|------|--------|
| `cmd` | string | `"set_temp"` |
| `boiler` | string | `"brew"` or `"steam"` |
| `temp` | float | Temperature in °C |

### Set Mode

```json
{
  "cmd": "set_mode",
  "mode": "standby"
}
```

| Field | Type | Values |
|-------|------|--------|
| `cmd` | string | `"set_mode"` |
| `mode` | string | `"idle"`, `"standby"`, `"ready"` |

> **Note:** Brew start/stop is controlled by the physical lever, not via MQTT.

---

## Home Assistant Integration

### Auto-Discovery

When `ha_discovery` is enabled, the ESP32 publishes discovery messages to Home Assistant's MQTT discovery topic.

### Discovered Entities

**Sensors:**

| Entity ID | Type | Unit | Device Class |
|-----------|------|------|--------------|
| `sensor.brewos_brew_temp` | sensor | °C | temperature |
| `sensor.brewos_steam_temp` | sensor | °C | temperature |
| `sensor.brewos_pressure` | sensor | bar | pressure |
| `sensor.brewos_brew_setpoint` | sensor | °C | temperature |
| `sensor.brewos_steam_setpoint` | sensor | °C | temperature |

**Binary Sensors:**

| Entity ID | Type | Device Class |
|-----------|------|--------------|
| `binary_sensor.brewos_is_brewing` | binary_sensor | running |
| `binary_sensor.brewos_is_heating` | binary_sensor | heat |
| `binary_sensor.brewos_water_low` | binary_sensor | problem |
| `binary_sensor.brewos_alarm_active` | binary_sensor | problem |
| `binary_sensor.brewos_pico_connected` | binary_sensor | connectivity |

### Discovery Payload Example

```json
{
  "device": {
    "identifiers": ["brewos_xxxxxxxxxxxx"],
    "name": "BrewOS Coffee Machine",
    "model": "ECM Controller",
    "manufacturer": "BrewOS",
    "sw_version": "1.0.0"
  },
  "name": "Brew Temperature",
  "unique_id": "brewos_xxxxxxxxxxxx_brew_temp",
  "state_topic": "brewos/xxxxxxxxxxxx/status",
  "value_template": "{{ value_json.brew_temp }}",
  "unit_of_measurement": "°C",
  "device_class": "temperature",
  "state_class": "measurement",
  "availability_topic": "brewos/xxxxxxxxxxxx/availability",
  "payload_available": "online",
  "payload_not_available": "offline"
}
```

### Home Assistant Configuration

No manual configuration needed! Entities appear automatically when:
1. MQTT is enabled and connected
2. Home Assistant Discovery is enabled
3. HA is connected to the same MQTT broker

---

## Connection Management

### Auto-Reconnect

- Exponential backoff starting at 1 second
- Maximum delay: 60 seconds
- Resets to 1 second on successful connection

### Last Will Testament (LWT)

- Topic: `brewos/{id}/availability`
- Payload: `"offline"`
- QoS: 1
- Retained: Yes

When the ESP32 connects, it publishes `"online"` to the same topic.

### Keep-Alive

- Interval: 15 seconds (PubSubClient default)
- Clean session: Yes

---

## Implementation Details

### Files

| File | Description |
|------|-------------|
| `include/mqtt_client.h` | MQTTClient class interface |
| `src/mqtt_client.cpp` | Implementation |

### Dependencies

- `PubSubClient` - MQTT client library
- `ArduinoJson` - JSON serialization

### Buffer Size

MQTT buffer is set to 1024 bytes to accommodate Home Assistant discovery payloads.

---

## Troubleshooting

### Connection Failed

Check:
1. Broker address and port are correct
2. Username/password if authentication is required
3. WiFi is connected
4. Firewall allows MQTT traffic (port 1883 or 8883 for TLS)

### Entities Not Appearing in Home Assistant

Check:
1. HA Discovery is enabled in MQTT settings
2. Home Assistant is connected to the same MQTT broker
3. Check MQTT Explorer for discovery messages at `homeassistant/sensor/brewos_*`

### Status Not Updating

Check:
1. MQTT is connected (check status in web UI)
2. Status topic is being published (use MQTT Explorer)
3. Check ESP32 logs for publish errors

---

## Future Enhancements

- [ ] TLS/SSL support for encrypted connections
- [ ] Additional HA entities (shot timer, flow rate)
- [ ] MQTT command queue for offline operation
- [ ] Custom entity icons

