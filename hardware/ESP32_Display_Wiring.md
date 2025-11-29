# ESP32 Display Module Wiring Guide

## Overview

This document explains how to connect an ESP32 display module (including debug/development boards) to the ECM control PCB via the J15 connector.

## J15 Connector (8-pin JST-XH)

The control PCB provides an 8-pin JST-XH connector (J15) for the ESP32 display module:

| Pin | Signal | Direction | Voltage | Notes |
|-----|--------|-----------|---------|-------|
| 1 | 5V | Power Out | 5V DC | Power supply for ESP32 (300-500mA) |
| 2 | GND | Ground | 0V | Common ground |
| 3 | TX | Input | 3.3V | Pico TX → ESP32 RX (GPIO0) |
| 4 | RX | Output | 3.3V | ESP32 TX → Pico RX (GPIO1) |
| 5 | RUN | Output | 3.3V | ESP32 GPIO → Pico RUN pin (reset control) |
| 6 | BOOT | Output | 3.3V | ESP32 GPIO → Pico BOOTSEL pin (bootloader entry) |
| 7 | WEIGHT_STOP | Output | 3.3V | ESP32 GPIO → Pico GPIO21 (brew-by-weight signal) |
| 8 | SPARE | I/O | 3.3V | Reserved for future expansion |

## ESP32 GPIO Pin Assignment

The ESP32 firmware uses the following GPIO pins (defined in `src/esp32/include/config.h`):

| ESP32 GPIO | Function | J15 Pin | Pico Side |
|------------|----------|---------|-----------|
| GPIO17 | UART TX → Pico | Pin 3 | GPIO0 (RX) |
| GPIO18 | UART RX ← Pico | Pin 4 | GPIO1 (TX) |
| GPIO8 | RUN control | Pin 5 | Pico RUN pin |
| GPIO9 | BOOTSEL control | Pin 6 | Pico BOOTSEL pin |
| **GPIO10** | **WEIGHT_STOP** | **Pin 7** | **GPIO21** |
| GPIO22 | SPARE (future) | Pin 8 | GPIO22 |

## Debug Board Compatibility

### Using a Debug/Development Board

If you're using an ESP32 debug board (like the one shown in the image), you need to:

1. **Identify accessible GPIO pins** on your debug board
2. **Wire those GPIOs to J15 connector** on the control PCB
3. **Configure ESP32 firmware** to use the correct GPIO numbers

### Typical Debug Board Setup

Most ESP32 debug boards have:
- **Test points or headers** for GPIO access
- **FPC connector** for display connection
- **USB port** for programming/debugging

**Wiring Steps:**

1. **Create a cable** from your debug board to J15 (8-pin JST-XH connector)
2. **Connect power first:**
   - Debug board 5V → J15 Pin 1 (5V)
   - Debug board GND → J15 Pin 2 (GND)

3. **Connect UART:**
   - ESP32 GPIO17 (TX) → J15 Pin 3 (TX)
   - ESP32 GPIO18 (RX) → J15 Pin 4 (RX)

4. **Connect control pins:**
   - ESP32 GPIO8 → J15 Pin 5 (RUN)
   - ESP32 GPIO9 → J15 Pin 6 (BOOT)

5. **Connect brew-by-weight:**
   - **ESP32 GPIO10 → J15 Pin 7 (WEIGHT_STOP)**

6. **Optional (future):**
   - ESP32 GPIO22 → J15 Pin 8 (SPARE)

### Example Wiring Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32 DEBUG BOARD                        │
│                                                              │
│  ┌──────────────┐                                           │
│  │  ESP32-S3    │                                           │
│  │              │                                           │
│  │  GPIO17 ─────┼──► J15 Pin 3 (TX)                         │
│  │  GPIO18 ◄────┼─── J15 Pin 4 (RX)                         │
│  │  GPIO8  ─────┼──► J15 Pin 5 (RUN)                        │
│  │  GPIO9  ─────┼──► J15 Pin 6 (BOOT)                       │
│  │  GPIO10 ─────┼──► J15 Pin 7 (WEIGHT_STOP) ⭐             │
│  │  GPIO22 ─────┼──► J15 Pin 8 (SPARE)                      │
│  │  5V     ─────┼──► J15 Pin 1 (5V)                         │
│  │  GND    ─────┼──► J15 Pin 2 (GND)                        │
│  └──────────────┘                                           │
│                                                              │
└─────────────────────────────────────────────────────────────┘
                              │
                              │ 8-pin JST-XH cable
                              │ (50cm recommended)
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    CONTROL PCB (J15)                         │
│                                                              │
│  Pin 1: 5V  ──────────────────────────────────────────────┐ │
│  Pin 2: GND ──────────────────────────────────────────────┐ │
│  Pin 3: TX  ──────────────────────────────────────────────┐ │
│  Pin 4: RX  ──────────────────────────────────────────────┐ │
│  Pin 5: RUN ──────────────────────────────────────────────┐ │
│  Pin 6: BOOT ─────────────────────────────────────────────┐ │
│  Pin 7: WEIGHT_STOP ──────────────────────────────────────┐ │
│  Pin 8: SPARE ───────────────────────────────────────────┐ │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

## Brew-by-Weight Connection (WEIGHT_STOP)

### Purpose

The WEIGHT_STOP signal allows the ESP32 (connected to a Bluetooth scale) to signal the Pico to stop the brew pump when the target weight is reached.

### Wiring

**ESP32 Side:**
- Use **GPIO10** (or any available GPIO - configure in `config.h`)
- Wire to J15 Pin 7

**Control PCB Side:**
- J15 Pin 7 connects to Pico GPIO21
- Pico has 10kΩ pull-down resistor (normally LOW)
- When ESP32 sets GPIO10 HIGH, Pico GPIO21 reads HIGH

### ESP32 Firmware Implementation

```cpp
// In src/esp32/include/config.h
#define WEIGHT_STOP_PIN         10              // ESP32 GPIO10

// In your brew-by-weight code:
void onTargetWeightReached() {
    digitalWrite(WEIGHT_STOP_PIN, HIGH);  // Signal Pico to stop
    delay(100);  // Hold for at least 100ms
    digitalWrite(WEIGHT_STOP_PIN, LOW);   // Release
}
```

### Pico Firmware Behavior

The Pico monitors GPIO21 in the control loop:

```c
// In control loop
if (gpio_get(PCB_PINS.input_weight_stop)) {
    // Target weight reached - stop pump immediately
    control_set_pump(0);
}
```

## Debug Board Test Points

If your debug board has test points (like TX1, RX1, TX2, RX2, PBZ, etc.), you can:

1. **Use test points for GPIO access:**
   - Solder wires to test points
   - Connect to J15 connector
   - Or use a breakout board

2. **Identify GPIO mapping:**
   - Check your debug board's schematic/datasheet
   - Map test points to ESP32 GPIO numbers
   - Update `config.h` if using different GPIOs

## Power Requirements

- **ESP32 + Display:** 300-500mA @ 5V
- **Control PCB provides:** 5V @ 3A (via HLK-5M05)
- **J15 Pin 1:** 5V power output
- **J15 Pin 2:** Ground

⚠️ **Important:** ESP32 modules have onboard 3.3V LDO. Do NOT power ESP32 from Pico's 3.3V rail - use 5V from J15 Pin 1.

## Cable Assembly

### Recommended Cable

- **Connector:** JST-XH 8-pin (2.54mm pitch)
- **Length:** 50cm (adjust based on installation)
- **Wire gauge:** 24-26 AWG for signals, 22 AWG for power
- **Shielding:** Optional, but recommended for UART lines if cable >30cm

### Pinout Reference

When assembling the cable, use this pinout:

```
J15 Connector (Control PCB)          ESP32 Debug Board
───────────────────────────          ──────────────────
Pin 1 (5V)    ────────────────────►  5V
Pin 2 (GND)   ────────────────────►  GND
Pin 3 (TX)    ────────────────────►  GPIO18 (RX)
Pin 4 (RX)    ◄────────────────────  GPIO17 (TX)
Pin 5 (RUN)   ────────────────────►  GPIO8
Pin 6 (BOOT)  ────────────────────►  GPIO9
Pin 7 (WGHT)  ────────────────────►  GPIO10 ⭐
Pin 8 (SPARE) ────────────────────►  GPIO22 (optional)
```

## Troubleshooting

### ESP32 Not Communicating

1. **Check power:** Measure 5V at J15 Pin 1
2. **Check UART:** Verify TX/RX are not swapped
3. **Check baud rate:** UART communication uses 921600 baud (PROTOCOL_BAUD_RATE). USB serial debug uses 115200 baud.
4. **Check ground:** Ensure GND is connected (Pin 2)

### WEIGHT_STOP Not Working

1. **Check wiring:** Verify ESP32 GPIO10 → J15 Pin 7
2. **Check Pico side:** Verify J15 Pin 7 → Pico GPIO21
3. **Test signal:** Use multimeter to verify ESP32 drives pin HIGH
4. **Check pull-down:** Pico has 10kΩ pull-down, should read LOW when ESP32 pin is LOW

### OTA Updates Not Working

1. **Check RUN pin:** ESP32 GPIO8 → J15 Pin 5 → Pico RUN
2. **Check BOOTSEL pin:** ESP32 GPIO9 → J15 Pin 6 → Pico BOOTSEL
3. **Verify open-drain:** ESP32 pins should be configured as open-drain outputs

## Custom GPIO Assignment

If your debug board uses different GPIO pins, update `src/esp32/include/config.h`:

```cpp
// Example: Using GPIO5 for WEIGHT_STOP instead of GPIO10
#define WEIGHT_STOP_PIN         5               // Your GPIO number
```

Then wire:
- ESP32 GPIO5 → J15 Pin 7 (WEIGHT_STOP)

The Pico side doesn't need changes - it always reads from GPIO21.

## Next Steps

1. **Assemble cable** from debug board to J15
2. **Test power** (5V at J15 Pin 1)
3. **Test UART** (check for boot messages)
4. **Test WEIGHT_STOP** (set ESP32 GPIO HIGH, verify Pico reads it)
5. **Implement brew-by-weight** in ESP32 firmware

---

**See Also:**
- [Hardware Specification](Specification.md) - Complete J15 pinout details
- [Communication Protocol](../docs/shared/Communication_Protocol.md) - UART protocol details
- [ESP32 Firmware Config](../src/esp32/include/config.h) - GPIO pin definitions

