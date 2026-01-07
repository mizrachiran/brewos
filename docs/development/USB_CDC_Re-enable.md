# USB CDC Re-enable Guide

## Overview

BrewOS ESP32 firmware disables USB CDC (Serial over USB) by default to repurpose GPIO19 (D+) and GPIO20 (D-) for GPIO functions:

- **GPIO19 (D+)**: Used for WEIGHT_STOP signal (brew-by-weight)
- **GPIO20 (D-)**: Used for Pico RUN pin (reset control)

This document explains how to re-enable USB CDC if needed, and the trade-offs involved.

## Why USB CDC is Disabled

The ESP32-S3 has limited GPIO pins, and several are already used for:

- Display interface (RGB parallel + SPI)
- Rotary encoder
- UART communication with Pico
- Other peripherals

GPIO19 and GPIO20 are USB data pins (D+ and D-). By disabling USB CDC, these pins can be used as regular GPIO pins, providing two additional GPIOs without hardware changes.

## Re-enabling USB CDC

### Step 1: Edit platformio.ini

Open `firmware/src/esp32/platformio.ini` and locate the build flags section for your environment (`esp32s3` or `esp32s3-8mb`).

**Find:**

```ini
build_flags =
	-DCORE_DEBUG_LEVEL=3
	; USB CDC disabled - GPIO19 (D+) and GPIO20 (D-) repurposed for GPIO functions
	; GPIO19 = WEIGHT_STOP_PIN, GPIO20 = PICO_RUN_PIN
	; To re-enable USB CDC: change ARDUINO_USB_MODE=1 and ARDUINO_USB_CDC_ON_BOOT=1
	; Note: Re-enabling USB CDC will cause GPIO conflicts with display pins
	-DARDUINO_USB_MODE=0
	-DARDUINO_USB_CDC_ON_BOOT=0
	-DARDUINO_USB_CDC_WAIT_FOR_USB=0
```

**Change to:**

```ini
build_flags =
	-DCORE_DEBUG_LEVEL=3
	-DARDUINO_USB_MODE=1
	-DARDUINO_USB_CDC_ON_BOOT=1
	-DARDUINO_USB_CDC_WAIT_FOR_USB=0
```

### Step 2: Update Pin Definitions

You must change the pin assignments in `firmware/src/esp32/include/config.h` to use different GPIO pins:

**Find:**

```cpp
#define PICO_RUN_PIN            20              // Controls Pico RUN (reset) → J15 Pin 5
#define WEIGHT_STOP_PIN         19              // ESP32 GPIO19 → J15 Pin 7 → Pico GPIO21
```

**Change to available GPIO pins** (check display_config.h for conflicts):

```cpp
#define PICO_RUN_PIN            8               // Controls Pico RUN (reset) → J15 Pin 5
                                                // NOTE: Conflicts with DISPLAY_RST_PIN
#define WEIGHT_STOP_PIN         10              // ESP32 GPIO10 → J15 Pin 7 → Pico GPIO21
                                                // NOTE: Conflicts with DISPLAY_B0_PIN
```

**⚠️ WARNING:** GPIO8 conflicts with display reset, and GPIO10 conflicts with display data pin B0. You may need to:

- Use different GPIO pins (check available pins in display_config.h)
- Accept display conflicts (display may not work correctly)
- Modify hardware wiring to use different pins

### Step 3: Rebuild Firmware

```bash
cd firmware/src/esp32
pio run -e esp32s3
```

### Step 4: Flash Firmware

Flash the new firmware to your ESP32. USB CDC will be available after boot.

## Trade-offs

### Advantages of Re-enabling USB CDC

- ✅ Serial debugging via USB (convenient for development)
- ✅ No need for external UART adapter
- ✅ Faster serial communication (USB vs UART)

### Disadvantages of Re-enabling USB CDC

- ❌ Lose GPIO19 and GPIO20 for GPIO functions
- ❌ Must use different pins for WEIGHT_STOP and PICO_RUN
- ❌ May cause GPIO conflicts with display or other peripherals
- ❌ Requires hardware wiring changes if using different pins

## Alternative Debugging Methods

If you need serial debugging but want to keep GPIO19/20:

### 1. Hardware UART (Auto-selected)

The firmware uses hardware UART for Serial when USB CDC is disabled (screen variant):

- The Arduino framework automatically selects UART pins
- **Note:** GPIO36/37 cannot be used as they are reserved for Octal PSRAM on ESP32-S3 N16R8
- Connect a USB-to-UART adapter to the auto-selected UART pins
- **Baud:** 115200

### 2. WiFi Serial

Use the web interface or WebSocket for debugging:

- Access web UI at `http://brewos.local` or device IP
- View logs via web interface
- Use WebSocket for real-time debugging

### 3. OTA Updates

Update firmware wirelessly via OTA:

- No USB cable needed
- Update from web interface
- View logs via web interface

## Recommended Configuration

**For Production:**

- Keep USB CDC disabled
- Use GPIO19/20 for WEIGHT_STOP and PICO_RUN
- Use hardware UART (auto-selected by framework) or WiFi for debugging

**For Development:**

- Enable USB CDC if you need convenient serial debugging
- Accept GPIO conflicts or use different pins
- Revert to disabled USB CDC before production deployment

## Troubleshooting

### USB CDC Not Working After Re-enable

1. **Check build flags:** Verify `ARDUINO_USB_MODE=1` and `ARDUINO_USB_CDC_ON_BOOT=1`
2. **Rebuild firmware:** Clean build may be needed (`pio run -t clean`)
3. **Check USB cable:** Use data-capable USB cable (not charge-only)
4. **Check drivers:** Install ESP32 USB CDC drivers if needed

### GPIO Conflicts After Re-enable

1. **Check pin assignments:** Verify pins don't conflict with display or other peripherals
2. **Review display_config.h:** Check which GPIOs are used by display
3. **Use different pins:** Change WEIGHT_STOP_PIN and PICO_RUN_PIN to available GPIOs
4. **Update hardware wiring:** Wire new pins to J15 connector

## See Also

- [ESP32 Wiring Guide](../hardware/ESP32_Wiring.md) - Complete wiring documentation
- [GPIO Allocation](../hardware/spec/02-GPIO-Allocation.md) - GPIO pin assignments
- [Display Configuration](../../src/esp32/include/display/display_config.h) - Display pin definitions
