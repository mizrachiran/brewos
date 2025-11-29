# Brew-by-Weight Integration

The Brew-by-Weight feature automatically stops espresso extraction when a target output weight is reached. This provides highly repeatable shots without manual timing.

## Overview

```
+-------------+      BLE       +-------------+      UART      +-------------+
|   Scale     |<-------------->|   ESP32     |<-------------->|    Pico     |
| (Bluetooth) |   Weight data  |  (Control)  |  WEIGHT_STOP   |  (Machine)  |
+-------------+                +-------------+     GPIO 10->21 +-------------+
                                     |
                                     | Reads weight
                                     | Checks target
                                     | Signals stop
                                     v
```

## Hardware Connection

The ESP32 signals the Pico to stop brewing via a dedicated GPIO:

| ESP32 | Wire | Pico | Function |
|-------|------|------|----------|
| GPIO10 | J15 Pin 7 | GPIO21 | WEIGHT_STOP |

- **Normal operation**: GPIO10 = LOW
- **Stop brewing**: GPIO10 = HIGH (pulse)

## Flow

### 1. Setup
1. Connect a BLE scale (see [BLE Scales](BLE_Scales.md))
2. Configure settings via UI or Web API:
   - Target weight (e.g., 36g)
   - Dose weight (e.g., 18g for 1:2 ratio)
   - Stop offset (e.g., 2g to account for drip after stop)
3. Enable auto-stop

### 2. Brewing Cycle
1. User pulls the brew lever (physical, not controlled by ESP32)
2. Pico detects lever and enters brewing state
3. ESP32 receives brewing status via UART
4. If auto-tare enabled, ESP32 tares the scale
5. Scale weight is monitored at ~10Hz
6. Progress is calculated: `current_weight / target_weight`
7. When `current_weight >= (target_weight - stop_offset)`:
   - ESP32 sets `WEIGHT_STOP` GPIO HIGH
   - Pico detects signal and stops pump/solenoid
   - ESP32 clears `WEIGHT_STOP` after 100ms
8. User releases lever
9. Shot complete screen shows final stats

## Settings

### NVS Keys (Namespace: `bbw`)

| Key | Type | Default | Range |
|-----|------|---------|-------|
| `target` | float | 36.0g | 10-100g |
| `dose` | float | 18.0g | 5-30g |
| `offset` | float | 2.0g | 0-10g |
| `auto_stop` | bool | true | - |
| `auto_tare` | bool | true | - |

### Stop Offset

The stop offset accounts for coffee that continues to drip after the pump stops. A typical offset of 2g means:
- Target: 36g
- Stop at: 34g
- Expected final weight: ~36g (after drip)

Calibrate based on your flow rate and workflow.

## API

### REST Endpoints

#### Get Settings
```http
GET /api/scale/settings
```

Response:
```json
{
  "target_weight": 36.0,
  "dose_weight": 18.0,
  "stop_offset": 2.0,
  "auto_stop": true,
  "auto_tare": true
}
```

#### Update Settings
```http
POST /api/scale/settings
Content-Type: application/json

{
  "target_weight": 40.0,
  "stop_offset": 2.5
}
```

#### Get Current State
```http
GET /api/scale/state
```

Response:
```json
{
  "active": true,
  "current_weight": 28.4,
  "target_weight": 36.0,
  "progress": 0.79,
  "ratio": 1.58,
  "target_reached": false,
  "stop_signaled": false
}
```

### WebSocket Events

Status updates include BBW data:
```json
{
  "type": "status",
  "is_brewing": true,
  "brew_weight": 28.4,
  "target_weight": 36.0,
  "dose_weight": 18.0,
  "scale_connected": true,
  "scale_stable": true,
  "flow_rate": 2.1
}
```

### MQTT Topics

Status published to `brewos/status` includes:
```json
{
  "scale": {
    "connected": true,
    "weight": 28.4,
    "flow_rate": 2.1,
    "stable": true
  },
  "brewing": {
    "active": true,
    "target_weight": 36.0,
    "dose_weight": 18.0
  }
}
```

## UI Screen

The Brew-by-Weight settings screen provides:

```
+----------------------------------------+
|         Brew by Weight                 |
|                                        |
|  +----------------------------------+  |
|  | Target Weight           36.0g   |  |
|  | Dose Weight             18.0g   |  |
|  |              Ratio: 1:2.0       |  |
|  | Stop Offset              2.0g   |  |
|  | Auto-Stop               [ON]    |  |
|  | Auto-Tare               [ON]    |  |
|  +----------------------------------+  |
|                                        |
|              [ Save ]                  |
|                                        |
|  Rotate to navigate - Press to edit   |
+----------------------------------------+
```

Navigation:
- **Rotate**: Move between fields / Adjust value when editing
- **Press**: Enter/exit edit mode / Toggle switches
- **Long press**: Exit to settings menu

## Code Structure

```
src/esp32/
  include/
    brew_by_weight.h      # BBW controller class
    ui/
      screen_bbw.h        # Settings screen header
  src/
    brew_by_weight.cpp    # BBW implementation
    ui/
      screen_bbw.cpp      # Settings screen
```

### BrewByWeight Class

```cpp
class BrewByWeight {
public:
    bool begin();                    // Initialize, load NVS settings
    void update(bool brewing,        // Call in main loop
                float weight,
                bool connected);
    
    // Settings
    float getTargetWeight();
    void setTargetWeight(float g);
    float getDoseWeight();
    void setDoseWeight(float g);
    float getStopOffset();
    void setStopOffset(float g);
    bool isAutoStopEnabled();
    void setAutoStop(bool enable);
    bool isAutoTareEnabled();
    void setAutoTare(bool enable);
    
    // State
    bool isActive();
    bool isTargetReached();
    float getProgress();      // 0.0 - 1.0
    float getCurrentRatio();  // output / dose
    
    // Callbacks
    void onStop(callback);    // Called when target reached
    void onTare(callback);    // Called for auto-tare
};
```

### Integration in main.cpp

```cpp
#include "brew_by_weight.h"

void setup() {
    brewByWeight.begin();
    
    brewByWeight.onStop([]() {
        picoUart.setWeightStop(true);
        delay(100);
        picoUart.setWeightStop(false);
    });
    
    brewByWeight.onTare([]() {
        scaleManager.tare();
    });
}

void loop() {
    brewByWeight.update(
        machineState.is_brewing,
        scaleManager.getState().weight,
        scaleManager.isConnected()
    );
}
```

## Pico Side

The Pico monitors GPIO21 for the WEIGHT_STOP signal:

```c
// In state.c
if (PIN_VALID(pcb->pins.input_weight_stop)) {
    bool weight_stop = hw_read_gpio(pcb->pins.input_weight_stop);
    if (weight_stop && g_brewing) {
        DEBUG_PRINT("WEIGHT_STOP signal - stopping brew\n");
        stop_brewing();
    }
}
```

## Accuracy Tips

1. **Calibrate offset**: Brew a few shots and measure final vs target weight. Adjust offset accordingly.

2. **Use stable reading**: The scale reports stability - avoid stopping during rapid weight changes.

3. **Consistent workflow**: Place cup on scale before pulling shot for best results.

4. **Clean scale**: Ensure scale platform is clean and level.

5. **Battery check**: Low battery can affect scale accuracy - monitor `scale_battery` field.

## Troubleshooting

### Weight not updating
- Check scale connection status (`scale_connected`)
- Verify scale is paired and awake
- Check BLE signal strength

### Shot stops too early/late
- Adjust `stop_offset` value
- Check flow rate consistency
- Ensure scale readings are stable

### Auto-stop not triggering
- Verify `auto_stop` is enabled
- Check `WEIGHT_STOP` GPIO wiring
- Check Pico is reading GPIO21

### Ratio incorrect
- Update `dose_weight` to match actual grounds used
- Tare scale with empty cup before pulling shot

