# BrewOS - Development Setup Guide

This guide will help you set up your development environment for BrewOS.

**See also:**
- [Pico Documentation](docs/pico/README.md) - Pico firmware overview and architecture
- [ESP32 Documentation](docs/esp32/README.md) - ESP32 firmware and integrations
- [Communication Protocol](docs/shared/Communication_Protocol.md) - UART protocol details
- [Documentation Index](docs/README.md) - Complete documentation index

## Prerequisites

### For ESP32-S3 Development

**Option 1: PlatformIO (Recommended)**

1. Install [Visual Studio Code](https://code.visualstudio.com/)
2. Install the [PlatformIO extension](https://platformio.org/install/ide?install=vscode)
3. Open the `src/esp32` folder in VSCode
4. PlatformIO will automatically install required tools

**Option 2: Arduino IDE**

1. Install [Arduino IDE 2.x](https://www.arduino.cc/en/software)
2. Add ESP32 board support via Board Manager
3. Install required libraries (see platformio.ini for list)

### For Pico Development

**macOS:**

```bash
# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install cmake
brew install git

# Install ARM toolchain (includes newlib)
# Option 1: Official ARM GNU Toolchain (Recommended)
# Download from: https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
# Choose: arm-gnu-toolchain-*-darwin-arm64-arm-none-eabi.pkg
# After installation, add to PATH:
export PATH="$PATH:/Applications/ArmGNUToolchain/*/arm-none-eabi/bin"

# Option 2: Homebrew (requires manual newlib fix)
brew install arm-none-eabi-gcc
# Then create nosys.specs (see Troubleshooting section)

# Clone Pico SDK
cd ~
git clone https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk
git submodule update --init

# Set environment variable (add to ~/.zshrc)
export PICO_SDK_PATH=~/pico-sdk
```

**Linux (Ubuntu/Debian):**
```bash
# Install dependencies
sudo apt update
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential git

# Clone Pico SDK
cd ~
git clone https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk
git submodule update --init

# Set environment variable (add to ~/.bashrc)
export PICO_SDK_PATH=~/pico-sdk
```

**Windows:**

1. Download and install the [Pico Setup for Windows](https://github.com/raspberrypi/pico-setup-windows/releases)
2. This installs CMake, GCC, and the SDK automatically

## Building the Firmware

### ESP32-S3

```bash
cd src/esp32

# Build
pio run

# Upload firmware
pio run -t upload

# Upload web UI files to LittleFS
pio run -t uploadfs

# Monitor serial output
pio device monitor
```

### Pico

```bash
cd src/pico

# Set Pico SDK path (if not already in ~/.zshrc)
export PICO_SDK_PATH=~/pico-sdk

# Create build directory
mkdir -p build
cd build

# Configure for your machine type
cmake -DMACHINE_TYPE=DUAL_BOILER ..      # For dual boiler machines
cmake -DMACHINE_TYPE=SINGLE_BOILER ..    # For single boiler machines
cmake -DMACHINE_TYPE=HEAT_EXCHANGER ..   # For HX machines

# Or build all machine types
cmake -DBUILD_ALL_MACHINES=ON ..

# Build
make -j4

# Output: brewos_dual_boiler.uf2 (or other variants)
```

## Uploading Pico Firmware

### Method 1: USB (Manual)

1. Disconnect Pico from power
2. Hold the BOOTSEL button
3. Connect USB cable (while holding BOOTSEL)
4. Release BOOTSEL - Pico appears as "RPI-RP2" drive
5. Copy `brewos_*.uf2` to the drive
6. Pico automatically reboots

### Method 2: OTA via ESP32 (when wired)

**Prerequisites:**
- ESP32 and Pico must be connected via UART (TX↔RX, RX↔TX, GND↔GND)
- ESP32 must have WiFi connectivity
- Pico firmware must be built and available as `.uf2` file

**Steps:**

1. **Build the Pico firmware:**
   ```bash
   cd src/pico
   mkdir -p build && cd build
   cmake -DMACHINE_TYPE=DUAL_BOILER ..
   make -j4
   # Output: brewos_dual_boiler.uf2 in build/ directory
   ```

2. **Connect to ESP32 web UI:**
   - Connect to ESP32 WiFi (AP mode: `BrewOS-Setup` or STA mode: your configured network)
   - Open browser to ESP32 IP address (default: `192.168.4.1` in AP mode)

3. **Upload firmware:**
   - Navigate to "Firmware Update" section in the web UI
   - Click "Choose File" and select the `brewos_*.uf2` file
   - Click "Upload" button
   - Wait for upload to complete (progress bar shows upload status)

4. **Flash to Pico:**
   - Click "Flash Pico" button
   - Confirm the action in the dialog
   - ESP32 will automatically:
     - Send bootloader command to Pico via UART
     - Pico enters serial bootloader mode (no hardware pins needed)
     - ESP32 streams firmware in chunks over UART
     - Pico receives, verifies checksums, and writes to flash
     - Pico resets automatically when complete
   - Monitor progress via the progress bar and console messages

5. **Verification:**
   - Pico will reboot automatically
   - Check console for "Pico booted" message
   - Verify Pico status shows "Connected" in the web UI

**Note:** Both ESP32 and Pico sides are fully implemented. The **serial bootloader** on the Pico:
- Works entirely over UART (no hardware pin control needed)
- Receives firmware chunks, verifies checksums, and writes to flash
- Handles errors gracefully with timeout detection
- Resets automatically after successful firmware update

The ESP32 implementation supports both serial bootloader (primary method) and hardware bootloader entry (BOOTSEL + RUN pins) as a fallback method.

### Method 3: Picoprobe (for debugging)

1. Get a second Pico
2. Flash it with Picoprobe firmware
3. Connect SWD pins (see Pico datasheet)
4. Use OpenOCD for upload and debugging

## Testing Without Hardware

Both firmwares can run without the full hardware setup:

### ESP32 Only
- Runs normally
- Shows "Pico: Disconnected" in UI
- All UI functions work
- WiFi setup works

### Pico Only
- Runs in simulation mode
- Generates fake sensor readings
- Outputs to USB serial (for debugging)
- Can be monitored via `minicom -D /dev/tty.usbmodem*`

### Full System Test
1. Flash both devices
2. Connect UART wires (TX↔RX, RX↔TX, GND↔GND)
3. Power both devices
4. Connect to ESP32 WiFi
5. Open web UI and watch real-time data

## Debugging

### ESP32 Serial Output
```bash
pio device monitor
# or
screen /dev/tty.usbserial-* 115200
```

### Pico Serial Output (USB)
```bash
minicom -D /dev/tty.usbmodem* -b 115200
# or
screen /dev/tty.usbmodem* 115200
```

### Web UI Console
- Open browser dev tools (F12)
- Check Console tab for JavaScript errors
- Network tab shows WebSocket messages

## Project Structure After Setup

```
brewos/
├── docs/                    # Documentation
│   ├── firmware/           # Firmware docs
│   └── ...
├── schematics/             # Hardware schematics
├── src/                    # Source code
│   ├── esp32/             # ESP32 PlatformIO project
│   │   ├── src/           # C++ source
│   │   ├── include/       # Headers
│   │   ├── data/          # Web UI (LittleFS)
│   │   └── platformio.ini
│   └── pico/              # Pico CMake project
│       ├── src/           # C source
│       ├── include/       # Headers
│       ├── build/         # Build output (generated)
│       └── CMakeLists.txt
├── SETUP.md               # This file
└── README.md              # Project overview
```

## Troubleshooting

### ESP32 won't upload
- Check USB cable (must support data, not just charging)
- Try holding BOOT button during upload
- Check correct COM port in platformio.ini

### Pico not recognized
- Check USB cable
- Try holding BOOTSEL and reconnecting
- On macOS, check System Report → USB

### Pico build fails: "cannot read spec file 'nosys.specs'"

This means newlib is missing from your ARM toolchain.

**Solution 1 (Recommended):** Install official ARM GNU Toolchain
```bash
# Download from: https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
# Install the .pkg file, then:
export PATH="$PATH:/Applications/ArmGNUToolchain/*/arm-none-eabi/bin"
```

**Solution 2:** Create nosys.specs manually
```bash
# Create minimal nosys.specs file
sudo mkdir -p /opt/homebrew/Cellar/arm-none-eabi-gcc/15.2.0/lib/gcc/arm-none-eabi/15.2.0/
sudo tee /opt/homebrew/Cellar/arm-none-eabi-gcc/15.2.0/lib/gcc/arm-none-eabi/15.2.0/nosys.specs > /dev/null << 'EOF'
%rename link_gcc_c_sequence nosys_link_gcc_c_sequence

*nosys_libgcc:
-lgcc

*link_gcc_c_sequence:
%(nosys_link_gcc_c_sequence) --start-group %G %(nosys_libgcc) --end-group

*startfile:

*link:
%(link_gcc_c_sequence) %{!r:--build-id=none} %{shared:-shared} %{!shared:-pie} %{static:-static}
EOF
```

### LittleFS upload fails
- Ensure `board_build.filesystem = littlefs` in platformio.ini
- Check partition table has enough space

### UART communication fails
- Verify TX↔RX crossover (ESP TX → Pico RX)
- Check baud rate matches (921600)
- Verify common ground connection

## Next Steps

1. ✅ Set up development environment
2. ✅ Build and upload both firmwares
3. ⬜ Connect hardware (ESP32 ↔ Pico)
4. ⬜ Configure WiFi via AP mode
5. ⬜ Monitor system via web UI
6. ⬜ Add real sensors to Pico
