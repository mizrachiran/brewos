#!/bin/bash
# Flash ESP32 firmware and web files via USB
# Usage: ./scripts/flash_esp32.sh [port] [--skip-prompt] [--firmware-only]
#   port: Serial port (optional, auto-detects if not provided)
#   --skip-prompt: Skip the bootloader mode prompt (for automated scripts)
#   --firmware-only: Only flash firmware, skip web files (default: flash both)

set -e
set -o pipefail  # Make pipes fail if any command in the pipeline fails

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
echo -e "${BLUE}Building firmware (always rebuild to ensure latest code)...${NC}"
cd "$ESP32_DIR"

# Build web UI if not firmware-only (always clean and rebuild for consistency)
if [ "$FIRMWARE_ONLY" = false ]; then
    if command -v npm &> /dev/null; then
        cd "$WEB_DIR"
        if [ ! -d "node_modules" ]; then
            echo -e "${BLUE}Installing web dependencies...${NC}"
            npm install
        fi
        # Always clean old files before building (prevent stale hashed assets)
        echo -e "${BLUE}Cleaning old web files...${NC}"
        rm -rf "$WEB_DATA_DIR/assets" 2>/dev/null || true
        rm -f "$WEB_DATA_DIR/index.html" "$WEB_DATA_DIR/favicon.svg" \
              "$WEB_DATA_DIR/logo.png" "$WEB_DATA_DIR/logo-icon.svg" \
              "$WEB_DATA_DIR/manifest.json" "$WEB_DATA_DIR/sw.js" \
              "$WEB_DATA_DIR/version-manifest.json" 2>/dev/null || true
        rm -rf "$WEB_DATA_DIR/.well-known" 2>/dev/null || true
        
        echo -e "${BLUE}Building web UI for ESP32...${NC}"
        npm run build:esp32
        cd "$ESP32_DIR"
    else
        echo -e "${YELLOW}⚠ npm not found - skipping web build${NC}"
    fi
fi

# Always build firmware (PlatformIO will rebuild only changed files, but ensures latest code)
# Use --verbose to see what's being rebuilt
echo -e "${BLUE}Running PlatformIO build...${NC}"
if ! pio run -e esp32s3; then
    echo -e "${RED}✗ Build failed!${NC}"
    exit 1
fi

# Build LittleFS image if not firmware-only
if [ "$FIRMWARE_ONLY" = false ]; then
    echo -e "${BLUE}Building LittleFS image...${NC}"
    if ! pio run -e esp32s3 -t buildfs; then
        echo -e "${RED}✗ LittleFS build failed!${NC}"
        exit 1
    fi
fi
echo ""

# Note: LittleFS image will be built during firmware build if needed

# Now proceed to port detection and flashing
echo -e "${BLUE}Detecting serial port...${NC}"

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
        # Use set +e temporarily to avoid exiting if ls finds no files
        set +e
        AVAILABLE_PORTS=($(ls /dev/cu.usbserial* /dev/cu.usbmodem* /dev/cu.wchusbserial* 2>/dev/null | grep -v "debug-console"))
        set -e
    else
        # Linux - exclude ttyACM0 if it's the debug console
        # ESP32-S3 programming port is usually ttyUSB* or ttyACM1
        set +e
        AVAILABLE_PORTS=($(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | grep -v "ttyACM0"))
        set -e
    fi
    
    # If multiple ports found, let user choose
    if [ ${#AVAILABLE_PORTS[@]} -gt 1 ]; then
        echo -e "${YELLOW}Multiple serial ports found:${NC}"
        for i in "${!AVAILABLE_PORTS[@]}"; do
            echo -e "  ${CYAN}[$((i+1))]${NC} ${AVAILABLE_PORTS[$i]}"
        done
        echo ""
        echo -e "${YELLOW}Please select a port (1-${#AVAILABLE_PORTS[@]}):${NC}"
        read -r SELECTION
        if [[ "$SELECTION" =~ ^[0-9]+$ ]] && [ "$SELECTION" -ge 1 ] && [ "$SELECTION" -le ${#AVAILABLE_PORTS[@]} ]; then
            PORT="${AVAILABLE_PORTS[$((SELECTION-1))]}"
            echo -e "${GREEN}Selected: $PORT${NC}"
        else
            echo -e "${RED}✗ Invalid selection${NC}"
            exit 1
        fi
    elif [ ${#AVAILABLE_PORTS[@]} -eq 1 ]; then
        PORT="${AVAILABLE_PORTS[0]}"
        echo -e "${BLUE}Auto-detected port: ${GREEN}$PORT${NC}"
    else
        PORT=""
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
    echo -e "${YELLOW}⚠ No ESP32 device auto-detected${NC}"
    echo ""
    echo "Available serial ports:"
    if [[ "$OSTYPE" == "darwin"* ]]; then
        echo "  Programming ports (use these):"
        set +e  # Temporarily disable exit on error for ls commands
        FOUND_PORTS=$(ls /dev/cu.usbmodem* /dev/cu.usbserial* /dev/cu.wchusbserial* 2>/dev/null | grep -v "debug-console" || echo "    (none)")
        echo "$FOUND_PORTS"
        echo ""
        echo "  Debug console (do NOT use for flashing):"
        DEBUG_CONSOLE=$(ls /dev/cu.debug-console 2>/dev/null || echo "    (none)")
        echo "$DEBUG_CONSOLE"
        set -e  # Re-enable exit on error
    else
        echo "  Programming ports (use these):"
        set +e  # Temporarily disable exit on error for ls commands
        FOUND_PORTS=$(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | grep -v "ttyACM0" || echo "    (none)")
        echo "$FOUND_PORTS"
        echo ""
        echo "  Debug console (do NOT use for flashing):"
        DEBUG_CONSOLE=$(ls /dev/ttyACM0 2>/dev/null || echo "    (none)")
        echo "$DEBUG_CONSOLE"
        set -e  # Re-enable exit on error
    fi
    echo ""
    echo -e "${CYAN}Please enter the serial port path manually:${NC}"
    echo "Example: /dev/cu.usbserial-00000000 or /dev/cu.usbmodem14101"
    echo ""
    echo -e "${YELLOW}Note: Do NOT use /dev/cu.debug-console for flashing!${NC}"
    echo "The ESP32-S3 has two USB ports:"
    echo "  - Programming port (usbmodem/usbserial) - use this for flashing"
    echo "  - Debug console (debug-console) - use this for Serial monitor only"
    echo ""
    echo -e "${YELLOW}Enter port path (or Ctrl+C to cancel):${NC}"
    read -r PORT
    
    if [ -z "$PORT" ]; then
        echo -e "${RED}✗ No port specified${NC}"
        exit 1
    fi
    
    if [ ! -e "$PORT" ]; then
        echo -e "${RED}✗ Port does not exist: $PORT${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}Using port: $PORT${NC}"
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

# Track flash success across both paths
FLASH_SUCCESS=false

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
    
    # Verify Python has pyserial before attempting flash
    if ! "$PYTHON_CMD" -c "import serial" 2>/dev/null; then
        echo -e "${RED}✗ pyserial not available in Python environment${NC}"
        echo -e "${YELLOW}Please install: pip install pyserial${NC}"
        exit 1
    fi
    
    # Verify firmware files exist
    if [ ! -f "$FIRMWARE" ]; then
        echo -e "${RED}✗ Firmware file not found: $FIRMWARE${NC}"
        exit 1
    fi
    if [ ! -f "$LITTLEFS_IMAGE" ]; then
        echo -e "${RED}✗ LittleFS image not found: $LITTLEFS_IMAGE${NC}"
        exit 1
    fi
    
    echo -e "${BLUE}Firmware size: $(du -h "$FIRMWARE" | cut -f1)${NC}"
    echo -e "${BLUE}LittleFS size: $(du -h "$LITTLEFS_IMAGE" | cut -f1)${NC}"
    echo ""
    
    # Use esptool.py to flash both partitions in one session
    # Firmware goes to 0x10000 (app partition), LittleFS goes to 0x670000 (for 8MB flash with default partition table)
    # write_flash automatically erases sectors before writing, so no explicit erase needed
    echo -e "${BLUE}Flashing firmware to 0x10000 and filesystem to $LITTLEFS_ADDR...${NC}"
    echo -e "${YELLOW}(Sectors will be automatically erased before writing)${NC}"
    
    # Check if bootloader, partition table, and other required files exist
    BOOTLOADER="$ESP32_DIR/.pio/build/esp32s3/bootloader.bin"
    PARTITION_TABLE="$ESP32_DIR/.pio/build/esp32s3/partitions.bin"
    BOOTLOADER_ADDR="0x0"
    PARTITION_TABLE_ADDR="0x8000"
    
    # Use esptool.py directly to flash partitions in one command
    # This keeps the ESP32 in bootloader mode for the entire operation
    # Add --verify to ensure flash succeeded
    # Use --after no_reset to prevent RTS reset (we don't have RTS pin)
    echo -e "${BLUE}Starting flash operation...${NC}"
    
    # Build flash command arguments
    FLASH_ARGS="--chip esp32s3 --port $PORT --baud 921600 --after no_reset write_flash --verify --flash_mode dio --flash_freq 80m --flash_size 8MB"
    
    # Include bootloader if it exists (at 0x0) - REQUIRED for ESP32-S3
    BOOTLOADER="$ESP32_DIR/.pio/build/esp32s3/bootloader.bin"
    if [ -f "$BOOTLOADER" ]; then
        echo -e "${BLUE}Including bootloader at 0x0...${NC}"
        FLASH_ARGS="$FLASH_ARGS 0x0 $BOOTLOADER"
    else
        echo -e "${YELLOW}⚠ Warning: Bootloader not found at $BOOTLOADER${NC}"
        echo -e "${YELLOW}  This may cause boot issues. Continuing anyway...${NC}"
    fi
    
    # Include partition table if it exists (at 0x8000)
    if [ -f "$PARTITION_TABLE" ]; then
        echo -e "${BLUE}Including partition table at $PARTITION_TABLE_ADDR...${NC}"
        FLASH_ARGS="$FLASH_ARGS $PARTITION_TABLE_ADDR $PARTITION_TABLE"
    else
        echo -e "${YELLOW}⚠ Warning: Partition table not found at $PARTITION_TABLE${NC}"
        echo -e "${YELLOW}  This may cause boot issues. Continuing anyway...${NC}"
    fi
    
    # Add firmware and filesystem
    FLASH_ARGS="$FLASH_ARGS 0x10000 $FIRMWARE $LITTLEFS_ADDR $LITTLEFS_IMAGE"
    
    # Use pipefail (set at top of script) to ensure we catch esptool failures even when using tee
    FLASH_SUCCESS=false
    if "$PYTHON_CMD" "$ESPTOOL_PY" $FLASH_ARGS 2>&1 | tee /tmp/flash_output.log; then
        # Double-check for fatal errors in the output (esptool sometimes returns 0 even on failure)
        # Look for specific fatal error patterns, not just any "error" word
        if grep -qiE "(fatal error|Failed to connect|No serial data received|A fatal error occurred)" /tmp/flash_output.log; then
            echo ""
            echo -e "${RED}✗ Flash failed (fatal errors detected in output)!${NC}"
            echo -e "${YELLOW}Check the output above for errors.${NC}"
            FLASH_SUCCESS=false
        else
            echo ""
            echo -e "${GREEN}✓ Firmware and filesystem flashed and verified successfully!${NC}"
            echo ""
            echo -e "${YELLOW}⚠ IMPORTANT: Manually reset the ESP32 now!${NC}"
            echo -e "${CYAN}Press and release the RESET button on the ESP32 to boot.${NC}"
            echo ""
            FLASH_SUCCESS=true
        fi
    else
        FLASH_SUCCESS=false
    fi
    
    if [ "$FLASH_SUCCESS" = false ]; then
        echo ""
        echo -e "${RED}✗ Flash failed!${NC}"
        echo -e "${YELLOW}Check the output above for errors.${NC}"
        echo -e "${YELLOW}Common issues:${NC}"
        echo "  - ESP32 not in bootloader mode (put it in bootloader mode and try again)"
        echo "  - Wrong serial port"
        echo "  - USB cable issues"
        echo ""
        echo -e "${CYAN}To put ESP32 in bootloader mode:${NC}"
        echo "  1. Hold the BOOT button"
        echo "  2. Press and release the RESET button"
        echo "  3. Release the BOOT button"
        echo "  4. Run the flash script again"
        exit 1
    fi
else
    # Flash firmware only (either firmware-only flag or LittleFS not available)
    # Use PlatformIO which handles erase automatically
    echo -e "${YELLOW}⚡ Flashing firmware (with auto-erase)...${NC}"
    if pio run -e esp32s3 -t upload --upload-port "$PORT"; then
        echo ""
        echo -e "${GREEN}✓ Firmware flashed successfully!${NC}"
        FLASH_SUCCESS=true
        
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
        FLASH_SUCCESS=false
        exit 1
    fi
fi

# Only print success message if we actually succeeded
# (This prevents false success messages if flash failed earlier)
if [ "$FLASH_SUCCESS" = true ]; then
    echo ""
    echo -e "${GREEN}✓ Flash complete!${NC}"
    echo ""
    echo "Monitor serial output with:"
    echo -e "  ${CYAN}cd src/esp32 && pio device monitor -p $PORT${NC}"
else
    # This shouldn't be reached if exit 1 was called, but just in case
    echo ""
    echo -e "${RED}✗ Flash did not complete successfully${NC}"
    exit 1
fi

