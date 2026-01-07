# ESP32 Display Module Wiring Guide

## Overview

This document explains how to connect an ESP32 display module (including debug/development boards) to the ECM control PCB via the J15 connector.

## J15 Connector (8-pin JST-XH)

The control PCB provides an 8-pin JST-XH connector (J15) for the ESP32 display module:

| Pin | Signal      | Direction | Voltage | Notes                                                        |
| --- | ----------- | --------- | ------- | ------------------------------------------------------------ |
| 1   | 5V          | Power Out | 5V DC   | Power supply for ESP32 (300-500mA)                           |
| 2   | GND         | Ground    | 0V      | Common ground                                                |
| 3   | TX          | Input     | 3.3V    | RP2354 TX → ESP32 RX (GPIO0, 33Ω series R40 + TVS D_UART_TX) |
| 4   | RX          | Output    | 3.3V    | ESP32 TX → RP2354 RX (GPIO1, 33Ω series R41 + TVS D_UART_RX) |
| 5   | RUN         | Output    | 3.3V    | ESP32 GPIO8 → RP2354 RUN pin (reset control)                 |
| 6   | SWDIO       | I/O       | 3.3V    | ESP32 TX2 ↔ RP2354 SWDIO (dedicated pin, 47Ω series)         |
| 7   | WEIGHT_STOP | Output    | 3.3V    | ESP32 GPIO19 → RP2354 GPIO21 (4.7kΩ pull-down)               |
| 8   | SWCLK       | I/O       | 3.3V    | ESP32 RX2 ↔ RP2354 SWCLK (dedicated pin, 47Ω series)         |

**⚠️ CRITICAL:** Always power the control PCB BEFORE connecting USB to ESP32 module. See [Safety - 5V Tolerance](spec/09-Safety.md#rp2354-5v-tolerance-and-power-sequencing) for details.

**Note:** This document references "RP2354" throughout. The current design uses the discrete RP2354A chip (QFN-60) with 2MB stacked flash, replacing the previous Raspberry Pi Pico 2 module.

## ESP32 GPIO Pin Assignment

The ESP32 firmware uses the following GPIO pins (defined in `src/esp32/include/config.h`):

| ESP32 GPIO | Function         | J15 Pin | RP2354 Side       | Notes               |
| ---------- | ---------------- | ------- | ----------------- | ------------------- |
| GPIO43     | UART TX → RP2354 | Pin 4   | GPIO1 (RX)        |                     |
| GPIO44     | UART RX ← RP2354 | Pin 3   | GPIO0 (TX)        |                     |
| GPIO20     | RUN control      | Pin 5   | RP2354 RUN        | USB D- (repurposed) |
| TX2        | SWDIO            | Pin 6   | SWDIO (dedicated) | SWD data I/O        |
| GPIO19     | WEIGHT_STOP      | Pin 7   | GPIO21            | USB D+ (repurposed) |
| RX2        | SWCLK            | Pin 8   | SWCLK (dedicated) | SWD clock           |

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

   - ESP32 GPIO43 (TX) → J15 Pin 4 (RX)
   - ESP32 GPIO44 (RX) ← J15 Pin 3 (TX)

4. **Connect control pins:**

   - ESP32 GPIO20 → J15 Pin 5 (RUN) - USB D- repurposed as GPIO

5. **Connect SWD interface:**

   - ESP32 TX2 → J15 Pin 6 (SWDIO) - SWD data I/O
   - ESP32 RX2 → J15 Pin 8 (SWCLK) - SWD clock

6. **Connect brew-by-weight:**

   - **ESP32 GPIO19 → J15 Pin 7 (WEIGHT_STOP)** - USB D+ repurposed as GPIO

### Example Wiring Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32 DEBUG BOARD                        │
│                                                              │
│  ┌──────────────┐                                           │
│  │  ESP32-S3    │                                           │
│  │              │                                           │
│  │  GPIO43 ─────┼──► J15 Pin 4 (RX)                         │
│  │  GPIO44 ◄────┼─── J15 Pin 3 (TX)                         │
│  │  GPIO20 ─────┼──► J15 Pin 5 (RUN) USB D- repurposed  │
│  │  TX2    ─────┼──► J15 Pin 6 (SWDIO)                      │
│  │  GPIO19 ─────┼──► J15 Pin 7 (WEIGHT_STOP) USB D+ repurposed │
│  │  RX2    ─────┼──► J15 Pin 8 (SWCLK)                      │
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
│  Pin 5: RUN (GPIO20, USB D-) ────────────────────────────┐ │
│  Pin 6: SWDIO (TX2) ─────────────────────────────────────┐ │
│  Pin 7: WEIGHT_STOP (GPIO19, USB D+) ────────────────────┐ │
│  Pin 8: SWCLK (RX2) ─────────────────────────────────────┐ │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

## USB CDC Disabled

**Important:** USB CDC (Serial over USB) is disabled in the firmware to free up GPIO19 (D+) and GPIO20 (D-) for GPIO functions.

- **GPIO19 (D+)** is used for WEIGHT_STOP signal
- **GPIO20 (D-)** is used for RP2354 RUN (reset) control
- **USB bootloader** still works (separate from USB CDC)
- **Serial debugging** available via hardware UART (GPIO36/37) or WiFi/OTA

To re-enable USB CDC, see `docs/development/USB_CDC_Re-enable.md`.

## Brew-by-Weight Connection (WEIGHT_STOP)

### Purpose

The WEIGHT_STOP signal allows the ESP32 (connected to a Bluetooth scale) to signal the RP2354 to stop the brew pump when the target weight is reached.

### Wiring

**ESP32 Side:**

- Use **GPIO19** (USB D+ repurposed as GPIO - configure in `config.h`)
- Wire to J15 Pin 7

**Control PCB Side:**

- J15 Pin 7 connects to RP2354 GPIO21
- RP2354 has 4.7kΩ pull-down resistor (R73, normally LOW)
- When ESP32 sets GPIO19 HIGH, RP2354 GPIO21 reads HIGH

### ESP32 Firmware Implementation

```cpp
// In src/esp32/include/config.h
#define WEIGHT_STOP_PIN         19              // ESP32 GPIO19 (USB D+ repurposed)

// In your brew-by-weight code:
void onTargetWeightReached() {
    digitalWrite(WEIGHT_STOP_PIN, HIGH);  // Signal RP2354 to stop
    delay(100);  // Hold for at least 100ms
    digitalWrite(WEIGHT_STOP_PIN, LOW);   // Release
}
```

### RP2354 Firmware Behavior

The RP2354 monitors GPIO21 in the control loop:

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
- **Control PCB provides:** 5V @ 3A (via HLK-15M05C)
- **J15 Pin 1:** 5V power output
- **J15 Pin 2:** Ground

⚠️ **Important:** ESP32 modules have onboard 3.3V LDO. Do NOT power ESP32 from RP2354's 3.3V rail - use 5V from J15 Pin 1.

---

## USB Debugging Safety

### ⚠️ CRITICAL SAFETY WARNING

**NEVER connect a USB cable to the ESP32 or RP2354 debug ports while the machine is powered from mains AC.**

### Why This Is Dangerous

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         USB DEBUG GROUND LOOP HAZARD                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   Espresso Machine                        Your Computer                      │
│   ┌─────────────────┐                     ┌─────────────────┐               │
│   │                 │                     │                 │               │
│   │   MAINS AC      │                     │   USB Port      │               │
│   │   100-240V      │                     │   (5V, GND)     │               │
│   │       │         │                     │       │         │               │
│   │       ▼         │                     │       │         │               │
│   │   Control PCB   │     USB Cable       │   Laptop GND    │               │
│   │   GND ──────────┼─────────────────────┼──► tied to AC   │               │
│   │                 │                     │      ground     │               │
│   └─────────────────┘                     └─────────────────┘               │
│                                                                              │
│   ⚡ GROUND LOOP: Machine chassis → USB GND → Laptop → AC outlet ground     │
│   ⚡ RISK: Voltage potential between grounds can damage equipment or cause  │
│           electric shock if machine has a fault condition                    │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Safe Debugging Practices

| Scenario                                 | Safe?  | Notes                                          |
| ---------------------------------------- | ------ | ---------------------------------------------- |
| USB debug while machine **unplugged**    | ✅ Yes | Preferred method for firmware development      |
| USB debug while machine on **mains AC**  | ❌ NO  | Ground loop hazard, potential equipment damage |
| USB debug with **isolation transformer** | ✅ Yes | Isolates machine from mains ground             |
| Wireless debug (WiFi/BLE) while on mains | ✅ Yes | No galvanic connection to PC                   |

### Recommended Debug Workflow

1. **Power off and unplug** the espresso machine from mains
2. **Connect USB** to ESP32 or RP2354 debug port
3. **Power the board** via USB only (5V from PC)
4. **Flash firmware** and test basic functionality
5. **Disconnect USB** before restoring mains power
6. **Use WiFi/OTA** for debugging when machine is powered

### Alternative: USB Isolator

If you must debug while powered, use a **USB isolator** module:

- **Recommended:** Adafruit USB Isolator (ADuM3160-based)
- **Isolation:** 2500V RMS
- **Speed:** USB 2.0 Full Speed (sufficient for serial debug)

**Note:** USB isolators add latency and may not support all USB features. OTA updates are preferred for production debugging.

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
Pin 3 (TX)    ────────────────────►  GPIO44 (RX)
Pin 4 (RX)    ◄────────────────────  GPIO43 (TX)
Pin 5 (RUN)    ────────────────────►  GPIO20 (USB D- repurposed)
Pin 6 (SWDIO)  ────────────────────►  TX2 (SWD data I/O)
Pin 7 (WGHT)   ────────────────────►  GPIO19 (USB D+ repurposed)
Pin 8 (SWCLK)  ────────────────────►  RX2 (SWD clock)
```

## Troubleshooting

### ESP32 Not Communicating

1. **Check power:** Measure 5V at J15 Pin 1
2. **Check UART:** Verify TX/RX are not swapped
3. **Check baud rate:** UART communication uses 921600 baud (PROTOCOL_BAUD_RATE). USB serial debug uses 115200 baud.
4. **Check ground:** Ensure GND is connected (Pin 2)

### WEIGHT_STOP Not Working

1. **Check wiring:** Verify ESP32 GPIO19 → J15 Pin 7
2. **Check RP2354 side:** Verify J15 Pin 7 → RP2354 GPIO21
3. **Test signal:** Use multimeter to verify ESP32 drives pin HIGH
4. **Check pull-down:** RP2354 has 4.7kΩ pull-down (R73), should read LOW when ESP32 pin is LOW
5. **Note:** GPIO19 is USB D+ repurposed - USB CDC must be disabled in platformio.ini

### SWD Interface Not Working

1. **Check wiring:** Verify ESP32 TX2 → J15 Pin 6 (SWDIO), RX2 → J15 Pin 8 (SWCLK)
2. **Check RP2354 side:** Verify J15 Pins 6/8 connect to dedicated SWDIO/SWCLK pins (not GPIO16/22)
3. **Check series resistors:** Verify 47Ω series resistors (R_SWD) are present
4. **Note:** SWDIO and SWCLK are dedicated physical pins on RP2354, not multiplexed GPIOs

### OTA Updates Not Working

OTA updates use the **software bootloader** via UART. For blank chips, use SWD interface.

1. **Check UART connection:** Verify TX/RX are connected correctly
2. **Check RUN pin:** ESP32 GPIO20 → J15 Pin 5 → RP2354 RUN (for reset after update)
3. **Verify RP2354 firmware:** Software bootloader requires working firmware
4. **For blank chips:** Use SWD interface (J15 Pins 6/8) for factory flash
5. **Note:** GPIO20 is USB D- repurposed - USB CDC must be disabled in platformio.ini

## Custom GPIO Assignment

If your debug board uses different GPIO pins, update `src/esp32/include/config.h`:

```cpp
// Example: Using GPIO5 for WEIGHT_STOP instead of GPIO19
#define WEIGHT_STOP_PIN         5               // Your GPIO number
```

Then wire:

- ESP32 GPIO5 → J15 Pin 7 (WEIGHT_STOP)

**Note:** If using GPIO19/20, USB CDC must be disabled. See `docs/development/USB_CDC_Re-enable.md` for details.

**SWD Interface:** J15 Pins 6/8 connect to dedicated SWDIO/SWCLK pins on RP2354, not GPIOs. These cannot be reassigned.

The RP2354 side doesn't need changes - it always reads from GPIO21.

## Next Steps

1. **Assemble cable** from debug board to J15
2. **Test power** (5V at J15 Pin 1)
3. **Test UART** (check for boot messages)
4. **Test WEIGHT_STOP** (set ESP32 GPIO HIGH, verify RP2354 reads it)
5. **Implement brew-by-weight** in ESP32 firmware

---

**See Also:**

- [Hardware Specification](Specification.md) - Complete J15 pinout details
- [Communication Protocol](../shared/Communication_Protocol.md) - UART protocol details
- [ESP32 Firmware Config](../../src/esp32/include/config.h) - GPIO pin definitions
