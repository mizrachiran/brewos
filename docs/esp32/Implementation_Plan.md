# ESP32 Implementation Plan

> **Status:** In Development  
> **Last Updated:** 2025-11-28

## Overview

The ESP32-S3 serves as the connectivity and UI hub for the BrewOS coffee machine controller. It bridges the Raspberry Pi Pico (which handles real-time control) with the user, providing WiFi, web interface, MQTT, cloud integration, and brew-by-weight features.

---

## Implementation Status

### Core Features

| Feature | Status | Notes |
|---------|--------|-------|
| WiFi AP Mode | ✅ Complete | `BrewOS-Setup` access point |
| WiFi STA Mode | ✅ Complete | Connect to home network |
| UART Bridge to Pico | ✅ Complete | 921600 baud, CRC-16 packets |
| Basic Web Server | ✅ Complete | LittleFS static files |
| WebSocket Status | ✅ Complete | Real-time status updates |
| OTA Pico Update | ✅ Complete | Firmware streaming |
| Basic Dashboard UI | ✅ Complete | Temperature, pressure display |
| MQTT Integration | ✅ Complete | [Details](integrations/MQTT.md) |
| Web API | ✅ Complete | [Details](integrations/Web_API.md) |
| LVGL Display | 🔲 In Progress | Round display UI |
| BLE Scale | 🔲 Planned | Brew by weight |
| Cloud Integration | 🔲 Planned | Remote monitoring |

---

## 1. Hardware Integration

### Target Device

**Model:** UEDX48480021-MD80E (ESP32-S3 Knob Display)

| Specification | Value |
|---------------|-------|
| Screen | 2.1" Round IPS, 480×480 |
| MCU | ESP32-S3 |
| RAM | 8 MB PSRAM |
| Flash | 16 MB |
| Input | Rotary Encoder + Push Button |
| Graphics | LVGL |

### Pin Configuration

| GPIO | Function | Status |
|------|----------|--------|
| 17 | UART TX → Pico RX | ✅ |
| 18 | UART RX ← Pico TX | ✅ |
| 8 | PICO_RUN (Reset) | ✅ |
| 9 | PICO_BOOTSEL | ✅ |
| 10 | WEIGHT_STOP | ✅ |
| 14 | Encoder CLK | 🔲 |
| 13 | Encoder DT | 🔲 |
| 15 | Encoder SW | 🔲 |

### Tasks

```
[ ] HW-1: Configure LVGL for 480x480 round display
[ ] HW-2: Initialize display driver (ST7701)
[ ] HW-3: Implement rotary encoder with debouncing
[ ] HW-4: Button press detection (short/long/double)
[ ] HW-5: Backlight PWM control
[ ] HW-6: Display sleep mode
[ ] HW-7: ESP32 OTA self-update
[ ] HW-8: Hardware watchdog
```

---

## 2. Display UI (LVGL)

Round display screens with rotary encoder navigation.

### Screens

| Screen | Description | Status |
|--------|-------------|--------|
| Home | Brew/Steam temps, pressure | 🔲 |
| Brew | Shot timer, weight, flow | 🔲 |
| Settings | WiFi, MQTT, Cloud config | 🔲 |
| Statistics | Shot count, usage | 🔲 |

### Tasks

```
[ ] UI-1: Home screen with temperature arcs
[ ] UI-2: Brew screen with timer/weight
[ ] UI-3: Settings radial menu
[ ] UI-4: Statistics screen
[ ] UI-5: Screen transitions
[ ] UI-6: Idle dimming
```

---

## 3. MQTT Integration

**Status:** ✅ Complete

See [MQTT Integration](integrations/MQTT.md) for full documentation.

### Summary

- PubSubClient library
- Home Assistant auto-discovery
- Status publishing (1s interval)
- Command subscription
- Exponential backoff reconnect

---

## 4. Web API

**Status:** ✅ Complete

See [Web API Reference](integrations/Web_API.md) for full documentation.

### Summary

- RESTful HTTP endpoints
- WebSocket real-time updates
- MQTT configuration
- OTA firmware upload

---

## 5. Cloud Integration

**Status:** 🔲 Planned

### Features

| Feature | Description | Priority |
|---------|-------------|----------|
| Account Linking | OAuth2 or device code | High |
| Remote Monitoring | View status from anywhere | High |
| Push Notifications | Alerts for water/errors | Medium |
| Shot History Sync | Backup to cloud | Medium |
| Remote Config | Update settings remotely | Low |

### Tasks

```
[ ] CLOUD-1: WebSocket client to cloud server
[ ] CLOUD-2: Device registration flow
[ ] CLOUD-3: Status streaming
[ ] CLOUD-4: Command handling
[ ] CLOUD-5: Secure token storage
```

---

## 6. Brew by Weight (BLE Scale)

**Status:** 🔲 Planned

### Supported Scales

| Scale | Protocol | Priority |
|-------|----------|----------|
| Acaia Lunar | Acaia BLE | High |
| Acaia Pearl | Acaia BLE | High |
| Decent Scale | Decent BLE | Medium |
| Timemore Black Mirror | Generic | Low |

### Features

| Feature | Description |
|---------|-------------|
| Auto-connect | Reconnect to paired scale |
| Weight streaming | Real-time weight display |
| Tare | Remote tare command |
| Auto-stop | Stop at target weight |

### Tasks

```
[ ] BLE-1: NimBLE stack initialization
[ ] BLE-2: Scale scanning and pairing
[ ] BLE-3: Acaia protocol implementation
[ ] BLE-4: Weight notification handling
[ ] BLE-5: Auto-stop signal to Pico
[ ] BLE-6: Scale config persistence
```

---

## Memory Budget

| Component | RAM | Notes |
|-----------|-----|-------|
| LVGL Core | ~64 KB | Graphics library |
| LVGL Display Buffer | ~38 KB | 480×40 lines |
| WiFi Stack | ~40 KB | ESP-IDF managed |
| Web Server | ~20 KB | AsyncWebServer |
| MQTT Client | ~5 KB | PubSubClient |
| BLE Stack | ~50 KB | NimBLE |
| JSON Buffers | ~8 KB | ArduinoJson |
| **Total** | ~225 KB | From 328 KB available |

---

## File Structure

```
src/esp32/
├── include/
│   ├── config.h
│   ├── display/
│   │   ├── display.h
│   │   ├── encoder.h
│   │   └── theme.h
│   ├── network/           # Planned reorganization
│   │   ├── mqtt_client.h
│   │   ├── web_server.h
│   │   └── wifi_manager.h
│   └── ui/
│       ├── screen_home.h
│       └── ui.h
├── src/
│   ├── main.cpp
│   ├── display/
│   ├── network/
│   └── ui/
├── data/                  # Web UI (LittleFS)
└── platformio.ini
```

---

## Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| ESP Async WebServer | latest | HTTP server |
| AsyncTCP | 1.1.1 | TCP for ESP32 |
| ArduinoJson | latest | JSON parsing |
| lvgl | 8.3.x | Graphics |
| LovyanGFX | 1.1.x | Display driver |
| PubSubClient | 2.8 | MQTT |
| NimBLE-Arduino | 1.4.x | BLE |

---

## Related Documentation

- [MQTT Integration](integrations/MQTT.md)
- [Web API Reference](integrations/Web_API.md)
- [Communication Protocol](../shared/Communication_Protocol.md)

