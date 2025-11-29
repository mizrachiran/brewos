# ESP32 Firmware Documentation

The ESP32-S3 handles connectivity, user interface, and advanced features for the BrewOS coffee machine controller.

## Quick Links

| Document | Description |
|----------|-------------|
| [Implementation Plan](Implementation_Plan.md) | Development roadmap and status |
| [UI Design](UI_Design.md) | Display screens and navigation |
| [MQTT Integration](integrations/MQTT.md) | MQTT setup and Home Assistant |
| [Web API Reference](integrations/Web_API.md) | HTTP endpoints and WebSocket |
| [BLE Scales](integrations/BLE_Scales.md) | Bluetooth scale integration |
| [Brew-by-Weight](integrations/Brew_By_Weight.md) | Auto-stop at target weight |

## Hardware

- **MCU:** ESP32-S3
- **Display:** 2.1" Round IPS (480×480)
- **Input:** Rotary Encoder + Push Button
- **Memory:** 8 MB PSRAM, 16 MB Flash

## Features

### Implemented ✅

- WiFi (AP setup + STA mode)
- Web dashboard with real-time updates
- UART bridge to Pico
- OTA Pico firmware updates
- MQTT with Home Assistant discovery
- RESTful API

### In Progress 🔲

- BLE scale integration
- Brew-by-weight auto-stop
- Cloud connectivity

## Building

```bash
cd src/esp32
pio run           # Build
pio run -t upload # Flash
pio run -t uploadfs # Upload web UI
```

## Configuration

1. Power on device - creates `BrewOS-Setup` WiFi
2. Connect and open `http://192.168.4.1`
3. Configure WiFi and MQTT
4. Device restarts and connects to network

## Folder Structure

```
docs/esp32/
├── README.md              # This file
├── Implementation_Plan.md # Development status
├── UI_Design.md           # Display UI specification
└── integrations/
    ├── MQTT.md              # MQTT documentation
    ├── Web_API.md           # API reference
    ├── BLE_Scales.md        # Bluetooth scales
    └── Brew_By_Weight.md    # Auto-stop feature
```

