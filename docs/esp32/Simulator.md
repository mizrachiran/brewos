# UI Simulator

The BrewOS UI Simulator lets you develop and preview the LVGL-based display UI on your desktop without flashing hardware.

## Prerequisites

- **macOS:** Install SDL2 via Homebrew
  ```bash
  brew install sdl2
  ```

- **Linux:** Install SDL2 dev package
  ```bash
  sudo apt install libsdl2-dev
  ```

- **PlatformIO CLI** installed and available in PATH

## Quick Start

```bash
./src/scripts/run_simulator.sh
```

This will:
1. Check for SDL2 (install if missing on macOS)
2. Build the simulator environment
3. Launch a 480×480 window

## Controls

| Input | Action |
|-------|--------|
| **Click** | Simulate touch |
| **ESC** | Close window |

## Manual Build & Run

```bash
cd src/esp32
pio run -e simulator
.pio/build/simulator/program
```

## What's Simulated

The simulator provides a **Theme Preview** with:

- **Temperature Arc Gauge** - Live color changes based on temp vs setpoint
- **Steam/Pressure Cards** - UI card styling
- **Color Palette Strip** - Visual preview of brand colors
- **Primary Button** - Button styling

### Simulated Values

| Value | Range | Update Rate |
|-------|-------|-------------|
| Brew Temp | 90-96°C | 500ms |
| Steam Temp | 145°C (static) | - |
| Pressure | 9.0 bar (static) | - |

## Editing Theme Colors

Theme colors are defined in `src/esp32/include/display/theme.h`:

```c
// Background colors (from brand palette)
#define COLOR_BG_DARK           lv_color_hex(0x1A0F0A)  // Darkest coffee
#define COLOR_BG_CARD           lv_color_hex(0x361E12)  // Dark brown
#define COLOR_BG_ELEVATED       lv_color_hex(0x4A2A1A)  // Elevated surface

// Accent colors
#define COLOR_ACCENT_PRIMARY    lv_color_hex(0xD5A071)  // Caramel/tan
#define COLOR_ACCENT_ORANGE     lv_color_hex(0xC4703C)  // Warm orange
#define COLOR_ACCENT_COPPER     lv_color_hex(0x714C30)  // Medium brown

// Text colors
#define COLOR_TEXT_PRIMARY      lv_color_hex(0xFBFCF8)  // Cream white
#define COLOR_TEXT_SECONDARY    lv_color_hex(0xD5A071)  // Caramel
#define COLOR_TEXT_MUTED        lv_color_hex(0x9B6E46)  // Light coffee
```

After editing, rebuild and run:

```bash
pio run -e simulator && .pio/build/simulator/program
```

## Simulator Source Files

| File | Purpose |
|------|---------|
| `src/esp32/src/simulator/main.cpp` | Simulator entry point and demo UI |
| `src/esp32/include/simulator/lv_tick.h` | Native tick source for LVGL |
| `src/esp32/platformio.ini` | `[env:simulator]` build config |

## Limitations

The simulator is a **standalone theme preview** - it does not compile the full ESP32 UI because those files depend on Arduino and ESP-IDF APIs.

To test the actual UI screens:
1. Flash to hardware: `pio run -t upload`
2. Use the web dashboard at `http://<device-ip>`

## Troubleshooting

### Build Fails: "SDL.h not found"

SDL2 is not installed or not in include path.

**macOS:**
```bash
brew install sdl2
```

**Linux:**
```bash
sudo apt install libsdl2-dev
```

### Build Fails: "Arduino.h not found"

You're building the wrong environment. Use:
```bash
pio run -e simulator
```
Not `pio run -e esp32s3` which targets real hardware.

### Window Doesn't Appear

Check if another process is using the display or if you're in a headless SSH session. The simulator requires a graphical environment.

