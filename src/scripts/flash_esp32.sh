#!/bin/bash
# Flash ESP32 firmware and web files via USB
# Usage: ./scripts/flash_esp32.sh [port] [--skip-prompt] [--firmware-only]
#   port: Serial port (optional, auto-detects if not provided)
#   --skip-prompt: Skip the bootloader mode prompt (for automated scripts)
#   --firmware-only: Only flash firmware, skip web files (default: flash both)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ESP32_DIR="$SCRIPT_DIR/../esp32"
WEB_DIR="$SCRIPT_DIR/../web"
FIRMWARE="$ESP32_DIR/.pio/build/esp32s3/firmware.bin"
LITTLEFS_IMAGE="$ESP32_DIR/.pio/build/esp32s3/littlefs.bin"
WEB_DATA_DIR="$ESP32_DIR/data"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${CYAN}"
echo "========================================"
echo "  BrewOS ESP32 Flasher"
echo "========================================"
echo -e "${NC}"

# Check flags
FIRMWARE_ONLY=false
SKIP_PROMPT=false
for arg in "$@"; do
    case "$arg" in
        --firmware-only)
            FIRMWARE_ONLY=true
            ;;
        --skip-prompt)
            SKIP_PROMPT=true
            ;;
    esac
done

# Note: Web files will be built during firmware build if needed

# Always build firmware to ensure latest code is flashed
echo -e "${BLUE}Building firmware...${NC}"
cd "$ESP32_DIR"

# Build web UI if not firmware-only and not already built
if [ "$FIRMWARE_ONLY" = false ] && ([ ! -d "$WEB_DATA_DIR" ] || [ -z "$(ls -A "$WEB_DATA_DIR" 2>/dev/null)" ]); then
    if command -v npm &> /dev/null; then
        cd "$WEB_DIR"
        if [ ! -d "node_modules" ]; then
            echo -e "${BLUE}Installing web dependencies...${NC}"
            npm install
        fi
        echo -e "${BLUE}Building web UI for ESP32...${NC}"
        npm run build:esp32
        cd "$ESP32_DIR"
    fi
fi

# Build firmware
pio run -e esp32s3

# Build LittleFS image if not firmware-only
if [ "$FIRMWARE_ONLY" = false ]; then
    echo -e "${BLUE}Building LittleFS image...${NC}"
    pio run -e esp32s3 -t buildfs
fi
echo ""

# Note: LittleFS image will be built during firmware build if needed

# Detect port (first non-flag argument)
PORT=""
for arg in "$@"; do
    case "$arg" in
        --skip-prompt|--firmware-only)
            # Skip flags
            ;;
        *)
            # First non-flag argument is the port
            if [ -z "$PORT" ] && [ -n "$arg" ]; then
                PORT="$arg"
            fi
            ;;
    esac
done

# Auto-detect if no port specified
if [ -z "$PORT" ]; then
    # Auto-detect on macOS
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # Look for common ESP32-S3 USB patterns
        # Exclude debug-console (native USB CDC, not for programming)
        # Prioritize usbserial over usbmodem (more reliable for ESP32-S3)
        PORT=$(ls /dev/cu.usbserial* /dev/cu.usbmodem* /dev/cu.wchusbserial* 2>/dev/null | grep -v "debug-console" | head -1)
    else
        # Linux - exclude ttyACM0 if it's the debug console
        # ESP32-S3 programming port is usually ttyUSB* or ttyACM1
        PORT=$(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | grep -v "ttyACM0" | head -1)
    fi
fi

# Validate port is not the debug console
if [[ "$PORT" == *"debug-console"* ]]; then
    echo -e "${RED}✗ Cannot use debug-console port for flashing!${NC}"
    echo ""
    echo "The ESP32-S3 has two USB ports:"
    echo "  - Programming port (usbmodem/usbserial) - use this for flashing"
    echo "  - Debug console (debug-console) - use this for Serial monitor only"
    echo ""
    echo "Please specify the programming port manually:"
    echo "  $0 /dev/cu.usbserial-00000000"
    exit 1
fi

if [ -z "$PORT" ]; then
    echo -e "${RED}✗ No ESP32 device found!${NC}"
    echo ""
    echo "Available serial ports:"
    if [[ "$OSTYPE" == "darwin"* ]]; then
        echo "  Programming ports (use these):"
        ls /dev/cu.usbmodem* /dev/cu.usbserial* /dev/cu.wchusbserial* 2>/dev/null | grep -v "debug-console" || echo "    (none)"
        echo ""
        echo "  Debug console (do NOT use for flashing):"
        ls /dev/cu.debug-console 2>/dev/null || echo "    (none)"
    else
        echo "  Programming ports (use these):"
        ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | grep -v "ttyACM0" || echo "    (none)"
        echo ""
        echo "  Debug console (do NOT use for flashing):"
        ls /dev/ttyACM0 2>/dev/null || echo "    (none)"
    fi
    echo ""
    echo "Usage: $0 [port]"
    echo "Example: $0 /dev/cu.usbmodem14101"
    echo ""
    echo -e "${YELLOW}Note: Do NOT use /dev/cu.debug-console for flashing!${NC}"
    echo "The ESP32-S3 has two USB ports:"
    echo "  - Programming port (usbmodem/usbserial) - use this for flashing"
    echo "  - Debug console (debug-console) - use this for Serial monitor only"
    exit 1
fi

echo -e "📍 Port: ${GREEN}$PORT${NC}"
echo -e "📦 Firmware: ${GREEN}$(basename $FIRMWARE)${NC} ($(du -h "$FIRMWARE" | cut -f1))"
if [ "$FIRMWARE_ONLY" = false ]; then
    if [ -d "$WEB_DATA_DIR" ] && [ -n "$(ls -A "$WEB_DATA_DIR" 2>/dev/null)" ]; then
        WEB_SIZE=$(du -sh "$WEB_DATA_DIR" | cut -f1)
        echo -e "📦 Web Files: ${GREEN}$WEB_DATA_DIR${NC} ($WEB_SIZE)"
    else
        echo -e "📦 Web Files: ${YELLOW}(not found)${NC}"
    fi
fi
echo ""

if [ "$SKIP_PROMPT" = false ]; then
    echo -e "${CYAN}Instructions to enter bootloader mode:${NC}"
    echo "  1. Disconnect ESP32 from power (if connected)"
    echo "  2. Hold the BOOT button (or IO0 button) on the ESP32"
    echo "  3. While holding BOOT, press and release the RESET button"
    echo "  4. Release the BOOT button"
    echo "  5. ESP32 is now in bootloader mode"
    echo ""
    echo -e "${YELLOW}Press Enter when ready to flash, or Ctrl+C to cancel...${NC}"
    read -r
fi

# Flash using PlatformIO
echo -e "${YELLOW}⚡ Flashing ESP32 firmware...${NC}"
echo -e "${BLUE}Using port: $PORT${NC}"
cd "$ESP32_DIR"

# LittleFS image should already be built above

# Ensure PlatformIO uses the correct port by setting environment variable
export PLATFORMIO_UPLOAD_PORT="$PORT"

# Find esptool.py and Python with pyserial (PlatformIO's version)
ESPTOOL_PY=""
PYTHON_CMD=""
if [ -f "$HOME/.platformio/packages/tool-esptoolpy/esptool.py" ]; then
    ESPTOOL_PY="$HOME/.platformio/packages/tool-esptoolpy/esptool.py"
    # Prefer PlatformIO's Python which has all dependencies
    if [ -f "$HOME/.platformio/penv/bin/python" ] && "$HOME/.platformio/penv/bin/python" -c "import serial" 2>/dev/null; then
        PYTHON_CMD="$HOME/.platformio/penv/bin/python"
    elif python3 -c "import serial" 2>/dev/null; then
        PYTHON_CMD="python3"
    elif python -c "import serial" 2>/dev/null; then
        PYTHON_CMD="python"
    else
        echo -e "${YELLOW}⚠ pyserial not found. Using PlatformIO's Python...${NC}"
        # Fallback to PlatformIO's Python (should have pyserial)
        if [ -f "$HOME/.platformio/penv/bin/python" ]; then
            PYTHON_CMD="$HOME/.platformio/penv/bin/python"
        else
            echo -e "${RED}✗ Cannot find Python with pyserial. Please install: pip install pyserial${NC}"
            exit 1
        fi
    fi
elif command -v esptool.py &> /dev/null; then
    ESPTOOL_PY="esptool.py"
    if [ -f "$HOME/.platformio/penv/bin/python" ]; then
        PYTHON_CMD="$HOME/.platformio/penv/bin/python"
    else
        PYTHON_CMD="python3"
    fi
else
    echo -e "${RED}✗ esptool.py not found${NC}"
    exit 1
fi

# Flash firmware and filesystem together if not firmware-only
if [ "$FIRMWARE_ONLY" = false ] && [ -f "$LITTLEFS_IMAGE" ]; then
    echo -e "${YELLOW}⚡ Flashing firmware and filesystem together (one bootloader session)...${NC}"
    
    # Get LittleFS partition address from PlatformIO (default_8MB.csv uses 0x670000)
    # Try to get it from partition table, fallback to default
    LITTLEFS_ADDR="0x670000"
    PARTITION_TABLE="$ESP32_DIR/.pio/build/esp32s3/partitions.bin"
    if [ -f "$PARTITION_TABLE" ]; then
        # Try to extract from partition table (this is a fallback - PlatformIO uses 0x670000 for default_8MB.csv)
        # For now, we'll use the known address for default_8MB.csv
        LITTLEFS_ADDR="0x670000"
    fi
    
    # Use esptool.py to flash both partitions in one session
    # Firmware goes to 0x10000 (app partition), LittleFS goes to 0x670000 (for 8MB flash with default partition table)
    echo -e "${BLUE}Flashing firmware to 0x10000 and filesystem to $LITTLEFS_ADDR...${NC}"
    
    # Verify Python has pyserial before attempting flash
    if ! "$PYTHON_CMD" -c "import serial" 2>/dev/null; then
        echo -e "${RED}✗ pyserial not available in Python environment${NC}"
        echo -e "${YELLOW}Please install: pip install pyserial${NC}"
        exit 1
    fi
    
    # Use esptool.py directly to flash both partitions in one command
    # This keeps the ESP32 in bootloader mode for both operations
    if "$PYTHON_CMD" "$ESPTOOL_PY" \
        --chip esp32s3 \
        --port "$PORT" \
        --baud 921600 \
        write_flash \
        --flash_mode dio \
        --flash_freq 80m \
        --flash_size 8MB \
        0x10000 "$FIRMWARE" \
        "$LITTLEFS_ADDR" "$LITTLEFS_IMAGE"; then
        echo ""
        echo -e "${GREEN}✓ Firmware and filesystem flashed successfully in one session!${NC}"
    else
        echo ""
        echo -e "${RED}✗ Flash failed!${NC}"
        exit 1
    fi
else
    # Flash firmware only (either firmware-only flag or LittleFS not available)
    if pio run -e esp32s3 -t upload --upload-port "$PORT"; then
        echo ""
        echo -e "${GREEN}✓ Firmware flashed successfully!${NC}"
        
        # If LittleFS image exists but wasn't flashed, offer to flash it separately
        if [ "$FIRMWARE_ONLY" = false ] && [ -f "$LITTLEFS_IMAGE" ]; then
            echo ""
            echo -e "${YELLOW}⚠ Filesystem not flashed (use combined flash mode for both)${NC}"
            echo "To flash filesystem separately:"
            echo "  1. Put ESP32 in bootloader mode (hold BOOT, press RESET, release BOOT)"
            echo -e "  2. Run: ${CYAN}cd src/esp32 && pio run -e esp32s3 -t uploadfs${NC}"
        fi
    else
        echo ""
        echo -e "${RED}✗ Flash failed!${NC}"
        exit 1
    fi
fi

echo ""
echo -e "${GREEN}✓ Flash complete!${NC}"
echo ""
echo "Monitor serial output with:"
echo -e "  ${CYAN}cd src/esp32 && pio device monitor -p $PORT${NC}"

