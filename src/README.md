# BrewOS Source Code

This directory contains all source code for the BrewOS project.

## Directory Structure

```
src/
├── cloud/          # Cloud service (Node.js + WebSocket relay)
│   ├── src/        # TypeScript source
│   └── Dockerfile
│
├── esp32/          # ESP32-S3 firmware (PlatformIO)
│   ├── src/        # C++ source files
│   ├── include/    # Header files
│   ├── data/       # Built web UI (LittleFS)
│   └── platformio.ini
│
├── pico/           # Pico firmware (CMake + Pico SDK)
│   ├── src/        # C source files
│   ├── include/    # Header files
│   └── CMakeLists.txt
│
├── web/            # React web interface
│   ├── src/        # TypeScript/React source
│   └── vite.config.ts
│
├── shared/         # Shared protocol definitions
│   └── protocol_defs.h
│
└── scripts/        # Build and utility scripts
    ├── build_firmware.sh
    ├── build_web.sh
    └── version.js
```

## Quick Start

### Build Script (Recommended)

Use the unified build script to build both firmwares or each separately:

```bash
cd src/scripts

# Build both Pico and ESP32
./build.sh

# Build only Pico
./build.sh pico

# Build only ESP32
./build.sh esp32

# Clean all build artifacts
./build.sh clean
```

### ESP32-S3 (Display Module)

**Prerequisites:**
- [PlatformIO](https://platformio.org/) (VSCode extension or CLI)

**Build & Upload:**
```bash
cd src/esp32

# Build
pio run

# Upload firmware
pio run -t upload

# Upload web UI files
pio run -t uploadfs

# Monitor serial
pio device monitor
```

### Pico (Control Board)

**Prerequisites:**
- [Pico SDK](https://github.com/raspberrypi/pico-sdk)
- CMake 3.13+
- ARM GCC toolchain

**Build:**
```bash
cd src/pico

# Set up SDK path (use your actual path)
export PICO_SDK_PATH=~/pico-sdk

# Create build directory
mkdir -p build && cd build

# Configure (first time, or after cleaning)
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..

# Build
make -j4
```

**Upload:**
1. Hold BOOTSEL button on Pico
2. Connect USB
3. Copy `ecm_pico.uf2` to the RPI-RP2 drive

## Development Workflow

### 1. First Boot (WiFi Setup)

1. Upload ESP32 firmware and web UI
2. Connect to "BrewOS-XXXXXX" WiFi network
3. Open http://192.168.4.1
4. Configure your home WiFi
5. ESP32 will reconnect and be accessible at `http://brewos.local`

### 2. Monitoring & Debugging

1. Open the web UI in your browser
2. View real-time Pico status and messages
3. Use the console log for debugging

### 3. OTA Updates

1. Build new Pico firmware (.uf2 or .bin)
2. Upload via web UI "Firmware Update" section
3. Click "Flash Pico" to trigger update

## Hardware Connections

### ESP32-S3 to Pico

| ESP32 Pin | Pico Pin | Function |
|-----------|----------|----------|
| GPIO17    | GPIO1    | UART TX → RX |
| GPIO18    | GPIO0    | UART RX ← TX |
| GPIO8     | RUN      | Pico Reset |
| GPIO9     | BOOTSEL  | Bootloader |

### UART Settings
- Baud rate: 921600
- 8N1 (8 data bits, no parity, 1 stop bit)

## Architecture

### ESP32-S3 Responsibilities
- WiFi connectivity (AP setup + STA mode)
- Web server for UI and API
- WebSocket for real-time updates
- UART bridge to Pico
- OTA firmware updates for Pico

### Pico Responsibilities
- Real-time control loop (Core 0)
- Safety monitoring (watchdog, over-temp, water level)
- Sensor reading (temperature, pressure)
- PID control for heaters
- Communication with ESP32 (Core 1)

## Communication Protocol

Binary protocol over UART:

```
| SYNC (0xAA) | TYPE | LENGTH | SEQ | PAYLOAD... | CRC16 |
```

See `docs/shared/Communication_Protocol.md` for details.

## Version Management

Versions are managed centrally using `src/scripts/version.js`:

```bash
# Show current version
node src/scripts/version.js

# Bump patch version (0.1.0 → 0.1.1)
node src/scripts/version.js --bump patch

# Bump minor version (0.1.0 → 0.2.0)
node src/scripts/version.js --bump minor

# Set specific version
node src/scripts/version.js --set 1.2.3

# Create a release (bump, commit, tag, push)
node src/scripts/version.js --bump minor --release
```

See `docs/pico/Versioning.md` for complete versioning guide.

## Simulation Mode

The Pico firmware includes a simulation mode for development without hardware:
- Simulated temperature readings that respond to heating
- Simulated pressure values
- Allows full end-to-end testing of the system

To disable simulation, set `sensors_set_simulation(false)` after sensor initialization.

