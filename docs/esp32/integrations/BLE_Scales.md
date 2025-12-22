# BLE Scale Integration

> **Status:** ✅ Implemented  
> **Last Updated:** 2025-12-22

## Overview

BrewOS supports Bluetooth Low Energy (BLE) coffee scales for brew-by-weight functionality. Unfortunately, there is **no universal standard** for BLE scales - each brand uses its own proprietary protocol.

## Supported Scales

| Brand | Models | Status | Notes |
|-------|--------|--------|-------|
| **Acaia** | Lunar, Pearl, Pyxis, Cinco, Orion | ✅ Implemented | Encrypted protocol, heartbeat required |
| **Bookoo** | Themis Mini, Themis Ultra | ✅ Implemented | Acaia-compatible protocol |
| **Felicita** | Arc, Parallel, Incline | ✅ Implemented | ASCII weight format |
| **Decent** | Decent Scale | ✅ Implemented | Well-documented protocol |
| **Timemore** | Black Mirror 2 only | ✅ Implemented | Basic/Nano/Mini have no Bluetooth |
| **Hiroia** | Jimmy | ✅ Implemented | Via name detection |
| **Generic** | BLE Weight Scale Service | ✅ Implemented | Bluetooth SIG standard (0x181D) |

## Protocol Details

### Acaia Scales

Acaia scales use an encrypted proprietary protocol:

- **Service UUID:** `49535343-FE7D-4AE5-8FA9-9FAFD205E455`
- **Characteristic:** Custom write/notify
- **Authentication:** Requires specific handshake sequence
- **Data:** Encrypted weight packets

**Library:** [AcaiaArduinoBLE](https://github.com/tatemazer/AcaiaArduinoBLE)

```cpp
// Example with AcaiaArduinoBLE
#include <AcaiaArduinoBLE.h>

AcaiaScale scale;

void setup() {
    BLE.begin();
    scale.init();
    scale.setWeightCallback(onWeight);
}

void onWeight(float weight) {
    Serial.printf("Weight: %.1f g\n", weight);
}
```

### Bookoo Themis Scales

Bookoo Themis scales (Mini and Ultra) use an Acaia-compatible protocol:

- **Service UUID:** `49535343-FE7D-4AE5-8FA9-9FAFD205E455` (same as Acaia)
- **Characteristic:** Same as Acaia
- **Authentication:** Same handshake sequence as Acaia
- **Features:** Weight, tare, timer (start/stop/reset)

**Detected Names:**
- `BOOKOO-XXXX`
- `Themis-XXXX`
- Any name containing `Themis`

The Bookoo Themis scales are popular due to their:
- 0.1g precision
- IP67 waterproof rating (Ultra model)
- Built-in flow rate display
- Competitive pricing vs Acaia

### Felicita Scales

Felicita uses a simpler protocol:

- **Service UUID:** `FFE0`
- **Characteristic:** `FFE1` (notify)
- **Data Format:** ASCII weight values

### Decent Scale

The Decent Scale protocol is publicly documented:

- **Service UUID:** `FFF0`
- **Weight Characteristic:** `FFF4` (notify)
- **Command Characteristic:** `36F5`
- **Data Format:** Little-endian signed 24-bit weight (0.1g units)

**Commands:**
| Command | Bytes | Description |
|---------|-------|-------------|
| Tare | `03 0F 00 00 00 00 00 00 00 00 00 0C` | Zero the scale |
| LED On | `0A 01 00 00 00 00 00 00 00 00 00 0B` | Turn on LED |
| LED Off | `0A 00 00 00 00 00 00 00 00 00 00 0A` | Turn off LED |

### Generic Weight Scale Service (WSS)

The Bluetooth SIG defines a standard Weight Scale Service (0x181D):

- **Service UUID:** `0x181D`
- **Weight Measurement:** `0x2A9D` (indicate)
- **Weight Scale Feature:** `0x2A9E` (read)

Few coffee scales implement this standard, but some generic scales do.

## Implementation Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        ScaleManager                              │
│  - Scanning and auto-detection                                   │
│  - Connection management                                         │
│  - Protocol routing based on scale type                         │
└─────────────────────────────┬───────────────────────────────────┘
                              │
     ┌────────────┬───────────┼───────────┬────────────┐
     │            │           │           │            │
     ▼            ▼           ▼           ▼            ▼
┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐
│  Acaia  │ │ Bookoo  │ │Felicita │ │ Decent  │ │Timemore │
│ (driver)│ │ (driver)│ │ (driver)│ │ (driver)│ │ (driver)│
└─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘
     │            │           │           │            │
     └────────────┴───────────┴───────────┴────────────┘
                              │
                              ▼
                   ┌─────────────────────┐
                   │    NimBLE Stack     │
                   │   (ESP32 Bluetooth) │
                   └─────────────────────┘
```

## Interface

```cpp
// scale/scale_interface.h

class ScaleInterface {
public:
    virtual bool begin() = 0;
    virtual void startScan(uint32_t timeout_ms) = 0;
    virtual bool connect(const char* address) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual scale_state_t getState() const = 0;
    virtual void tare() = 0;
    virtual void startTimer() = 0;
    virtual void stopTimer() = 0;
    virtual void onWeight(scale_weight_callback_t callback) = 0;
    virtual void loop() = 0;
};
```

## Scale State

```cpp
typedef struct {
    bool connected;
    bool stable;            // Weight reading is stable
    float weight;           // Weight in grams
    float flow_rate;        // Calculated flow rate (g/s)
    int battery_percent;    // Battery level (-1 if unknown)
    uint32_t last_update_ms;
} scale_state_t;
```

## Flow Rate Calculation

Flow rate is calculated from weight change over time:

```cpp
void updateFlowRate() {
    static float lastWeight = 0;
    static uint32_t lastTime = 0;
    
    uint32_t now = millis();
    uint32_t dt = now - lastTime;
    
    if (dt >= 200) {  // Calculate every 200ms
        float dw = weight - lastWeight;
        flowRate = (dw / dt) * 1000.0f;  // g/s
        
        // Apply smoothing
        flowRate = flowRate * 0.3f + lastFlowRate * 0.7f;
        
        lastWeight = weight;
        lastTime = now;
    }
}
```

## Auto-Stop Feature

When target weight is reached:

1. ESP32 detects weight >= target - pre-infusion_offset
2. ESP32 pulls `WEIGHT_STOP_PIN` (GPIO 10) LOW
3. Pico receives signal and can alert user (not stop pump directly)
4. User lifts lever to stop

**Note:** The physical lever controls the actual brew. The ESP32 only provides a signal - it does not control the pump directly for safety reasons.

## Power Consumption

BLE scanning and connection increase power consumption:

| Mode | Current Draw |
|------|--------------|
| Idle (no BLE) | ~80 mA |
| Scanning | ~130 mA |
| Connected | ~100 mA |

Scales typically update at 5-10 Hz.

## Pairing Flow

1. User enters Scale Settings
2. ESP32 starts BLE scan
3. Discovered scales shown with type icons
4. User selects scale
5. ESP32 connects and authenticates
6. Scale MAC saved to NVS for auto-reconnect

## Resources

### Libraries
- [AcaiaArduinoBLE](https://github.com/tatemazer/AcaiaArduinoBLE) - Acaia and Bookoo scales
- [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) - ESP32 BLE stack
- [Bookoo Home Assistant](https://github.com/makerwolf/bookoo) - HA integration for Bookoo

### Protocol Documentation
- [Decent Scale Protocol](https://decentespresso.com/support/scale/protocol)
- [pyacaia](https://github.com/lucapinello/pyacaia) - Python Acaia implementation
- [btscale](https://github.com/bpowers/btscale) - Go multi-scale library

### Reverse Engineering
- [Reverse Engineering BLE Devices](https://reverse-engineering-ble-devices.readthedocs.io/)
- Use nRF Connect app to capture BLE traffic

## Implementation Status

All major scale types are now implemented with auto-detection:

| Phase | Scale Type | Status |
|-------|------------|--------|
| 1 | Acaia | ✅ Complete |
| 2 | Bookoo Themis | ✅ Complete |
| 3 | Felicita | ✅ Complete |
| 4 | Decent | ✅ Complete |
| 5 | Timemore | ✅ Complete |
| 6 | Generic WSS | ✅ Complete |

### Files

```
src/esp32/
├── include/scale/
│   ├── scale_interface.h    # Common types and utilities
│   └── scale_manager.h      # Main scale manager
└── src/scale/
    ├── scale_factory.cpp    # Type detection
    └── scale_manager.cpp    # Full implementation
```

---

## Related Documentation

- [Implementation Plan](../Implementation_Plan.md)
- [UI Design - Scale Screen](../UI_Design.md#8-scale-pairing-screen)

