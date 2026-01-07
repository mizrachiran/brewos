#!/bin/bash
# Flash ESP32 firmware and web files via USB
# Usage: ./scripts/flash_esp32.sh [port] [--skip-prompt] [--firmware-only] [--no-clean] [--factory-reset] [--quiet] [--noscreen]
#   port: Serial port (optional, auto-detects if not provided)
#   --skip-prompt: Skip the bootloader mode prompt (for automated scripts)
#   --firmware-only: Only flash firmware, skip web files (default: flash both)
#   --no-clean: Skip clean rebuild (faster, but may use cached code)
#   --factory-reset: Erase entire flash before flashing (complete clean slate)
#   --quiet, -q: Suppress non-essential output (faster execution)
#   --noscreen: Build and flash headless variant (N16R8 - 16MB Flash, 8MB Octal PSRAM)

set -e
set -o pipefail  # Make pipes fail if any command in the pipeline fails

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ESP32_DIR="$SCRIPT_DIR/../esp32"
FIRMWARE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
APP_DIR="$FIRMWARE_DIR/../app"
WEB_DATA_DIR="$ESP32_DIR/data"
BUILD_APP_SCRIPT="$SCRIPT_DIR/build_app_for_esp32.sh"

# Variant selection (default to screen, will be updated by --noscreen flag)
VARIANT="screen"
ENV_NAME="esp32s3"

# Function to update firmware paths based on variant
update_firmware_paths() {
    FIRMWARE="$ESP32_DIR/.pio/build/$ENV_NAME/firmware.bin"
    LITTLEFS_IMAGE="$ESP32_DIR/.pio/build/$ENV_NAME/littlefs.bin"
}

# Initialize paths with default variant
update_firmware_paths

# Configuration
MAX_FILE_AGE_SECONDS=60  # Warn if web files are older than this

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BLUE='\033[0;34m'
NC='\033[0m'

# Initialize QUIET flag (will be set by argument parsing)
QUIET=false

# Helper function for conditional output (must be defined before first use)
quiet_echo() {
    if [ "$QUIET" = false ]; then
        echo -e "$@"
    fi
}

quiet_echo_nc() {
    if [ "$QUIET" = false ]; then
        echo "$@"
    fi
}

# Status log function - always prints (even in quiet mode) for key progress updates
status_log() {
    echo -e "${CYAN}[STATUS]${NC} $@"
}

quiet_echo "${CYAN}"
quiet_echo "========================================"
quiet_echo "  BrewOS ESP32 Flasher"
quiet_echo "========================================"
quiet_echo "${NC}"

# Check flags
FIRMWARE_ONLY=false
SKIP_PROMPT=false
DO_CLEAN=true  # Default: always clean build to ensure latest code
FACTORY_RESET=false
for arg in "$@"; do
    case "$arg" in
        --firmware-only)
            FIRMWARE_ONLY=true
            ;;
        --skip-prompt)
            SKIP_PROMPT=true
            ;;
        --no-clean)
            DO_CLEAN=false
            ;;
        --factory-reset)
            FACTORY_RESET=true
            ;;
        --quiet|-q)
            QUIET=true
            SKIP_PROMPT=true  # Auto-skip prompt in quiet mode
            ;;
        --noscreen)
            VARIANT="noscreen"
            ENV_NAME="esp32s3-noscreen"
            update_firmware_paths
            ;;
    esac
done

source ~/.pio-py313-venv/bin/activate

# Note: Web files will be built during firmware build if needed

# Always build firmware to ensure latest code is flashed
status_log "Build started..."
quiet_echo "${BLUE}Building firmware (always rebuild to ensure latest code)...${NC}"
cd "$ESP32_DIR"

# Build web UI if not firmware-only (always clean and rebuild for consistency)
if [ "$FIRMWARE_ONLY" = false ]; then
    if [ -f "$BUILD_APP_SCRIPT" ]; then
        status_log "Building web UI..."
        quiet_echo "${BLUE}Building web UI for ESP32...${NC}"
        
        # Completely clear data directory before building (prevent ANY stale files)
        quiet_echo "${BLUE}Clearing data directory (removing all old web files)...${NC}"
        if [ -d "$WEB_DATA_DIR" ]; then
            # Remove entire directory and recreate to ensure ALL files are gone (including hidden)
            rm -rf "$WEB_DATA_DIR"
            mkdir -p "$WEB_DATA_DIR"
            quiet_echo "${GREEN}✓ Data directory cleared${NC}"
        else
            mkdir -p "$WEB_DATA_DIR"
            quiet_echo "${GREEN}✓ Data directory created${NC}"
        fi
        
        # Use the build_app_for_esp32.sh script to build and sync web files
        if [ "$QUIET" = true ]; then
            if ! "$BUILD_APP_SCRIPT" --build-only > /dev/null 2>&1; then
                echo -e "${RED}✗ Web build failed!${NC}" >&2
                exit 1
            fi
        else
            if ! "$BUILD_APP_SCRIPT" --build-only; then
                echo -e "${RED}✗ Web build failed!${NC}"
                exit 1
            fi
        fi
        status_log "Web UI build complete"
        
        # Verify web files were built and copied to data directory
        quiet_echo "${BLUE}Verifying web files in data directory...${NC}"
        if [ ! -d "$WEB_DATA_DIR" ]; then
            echo -e "${RED}✗ Data directory not found: $WEB_DATA_DIR${NC}" >&2
            quiet_echo "${YELLOW}Web build may have failed or output path is incorrect${NC}"
            exit 1
        fi
        
        # Check for essential files
        MISSING_FILES=()
        if [ ! -f "$WEB_DATA_DIR/index.html" ]; then
            MISSING_FILES+=("index.html")
        fi
        if [ ! -d "$WEB_DATA_DIR/assets" ] || [ -z "$(ls -A "$WEB_DATA_DIR/assets" 2>/dev/null)" ]; then
            MISSING_FILES+=("assets/")
        fi
        
        if [ ${#MISSING_FILES[@]} -gt 0 ]; then
            echo -e "${RED}✗ Missing required web files:${NC}" >&2
            for file in "${MISSING_FILES[@]}"; do
                echo -e "  ${RED}- $file${NC}" >&2
            done
            quiet_echo "${YELLOW}Web build may have failed or files were not copied correctly${NC}"
            if [ "$QUIET" = false ]; then
                quiet_echo "${CYAN}Data directory contents:${NC}"
                ls -la "$WEB_DATA_DIR" || true
            fi
            exit 1
        fi
        
        # Only do expensive file checks if not in quiet mode
        if [ "$QUIET" = false ]; then
            WEB_FILE_COUNT=$(find "$WEB_DATA_DIR" -type f | wc -l | tr -d ' ')
            WEB_DATA_SIZE=$(du -sh "$WEB_DATA_DIR" | cut -f1)
            
            # Check that files are fresh (modified within last 60 seconds)
            NEWEST_FILE=$(find "$WEB_DATA_DIR" -type f -exec stat -f "%m %N" {} \; 2>/dev/null | sort -rn | head -1 | cut -d' ' -f2-)
            if [ -n "$NEWEST_FILE" ]; then
                NEWEST_AGE=$(($(date +%s) - $(stat -f "%m" "$NEWEST_FILE" 2>/dev/null || echo 0)))
                if [ "$NEWEST_AGE" -gt "$MAX_FILE_AGE_SECONDS" ]; then
                    quiet_echo "${YELLOW}⚠ Warning: Web files may be stale (newest file is ${NEWEST_AGE}s old)${NC}"
                fi
            fi
            
            quiet_echo "${GREEN}✓ Web files verified: $WEB_FILE_COUNT files ($WEB_DATA_SIZE) in $WEB_DATA_DIR${NC}"
        fi
    else
        quiet_echo "${YELLOW}⚠ Build script not found: $BUILD_APP_SCRIPT${NC}"
        if [ ! -d "$WEB_DATA_DIR" ] || [ -z "$(ls -A "$WEB_DATA_DIR" 2>/dev/null)" ]; then
            echo -e "${RED}✗ No web files found in $WEB_DATA_DIR${NC}" >&2
            quiet_echo "${YELLOW}Please run: ${CYAN}./scripts/build_app_for_esp32.sh${NC}"
            exit 1
        fi
    fi
fi

# Clean build by default (ensures fresh compile of all files)
if [ "$DO_CLEAN" = true ]; then
    status_log "Cleaning previous build..."
    quiet_echo "${BLUE}Cleaning previous build (use --no-clean to skip)...${NC}"
    if [ "$QUIET" = true ]; then
        pio run -e "$ENV_NAME" -t clean > /dev/null 2>&1
    else
        pio run -e "$ENV_NAME" -t clean
    fi
fi

# Build firmware (PlatformIO will rebuild only changed files unless --clean was used)
if [ "$VARIANT" = "noscreen" ]; then
    status_log "Compiling firmware (headless variant - N16R8)..."
    quiet_echo "${BLUE}Building ESP32 firmware (headless - no screen, N16R8: 16MB Flash, 8MB Octal PSRAM)...${NC}"
else
    status_log "Compiling firmware..."
    quiet_echo "${BLUE}Running PlatformIO build...${NC}"
fi
if [ "$QUIET" = true ]; then
    if ! pio run -e "$ENV_NAME" > /dev/null 2>&1; then
        echo -e "${RED}✗ Build failed!${NC}" >&2
        exit 1
    fi
else
    if ! pio run -e "$ENV_NAME"; then
        echo -e "${RED}✗ Build failed!${NC}"
        exit 1
    fi
fi

# Show firmware hash for verification
FIRMWARE_HASH=$(shasum -a 256 "$FIRMWARE" 2>/dev/null | head -c 16)
status_log "Firmware build complete (hash: $FIRMWARE_HASH)"
quiet_echo "${GREEN}✓ Build complete - Firmware hash: ${CYAN}$FIRMWARE_HASH${NC}"

# Verify partition table exists
if [ "$VARIANT" = "noscreen" ]; then
    PARTITION_FILE="$ESP32_DIR/partitions_16MB.csv"
    if [ ! -f "$PARTITION_FILE" ]; then
        echo -e "${RED}✗ Partition file not found: $PARTITION_FILE${NC}" >&2
        exit 1
    fi
    quiet_echo "${BLUE}✓ Using 16MB partition table (N16R8)${NC}"
fi

# Verify the build includes our changes by checking for the new code pattern
# This helps catch cases where the build might be using cached/old code
if grep -q "Keepalive status sent to client" "$ESP32_DIR/.pio/build/esp32s3/src/web_server_broadcast.cpp.o" 2>/dev/null || \
   strings "$FIRMWARE" 2>/dev/null | grep -q "Keepalive status sent" > /dev/null 2>&1; then
    quiet_echo "${GREEN}✓ Verified: New WebSocket keepalive code is included in build${NC}"
elif [ "$DO_CLEAN" = false ]; then
    quiet_echo "${YELLOW}⚠ Warning: Could not verify new code in build (may be using cached build)${NC}"
    quiet_echo "${YELLOW}  Consider running without --no-clean to force a fresh build${NC}"
fi

# Build LittleFS image if not firmware-only (both screen and noscreen variants need web files)
if [ "$FIRMWARE_ONLY" = false ]; then
    # Double-check data directory exists and has files before building LittleFS
    if [ ! -d "$WEB_DATA_DIR" ] || [ -z "$(ls -A "$WEB_DATA_DIR" 2>/dev/null)" ]; then
        echo -e "${RED}✗ Data directory is empty or missing: $WEB_DATA_DIR${NC}" >&2
        quiet_echo "${YELLOW}Cannot build LittleFS image without web files${NC}"
        quiet_echo "${CYAN}Please ensure web files are built and synced first:${NC}"
        quiet_echo "  ${CYAN}./scripts/build_app_for_esp32.sh${NC}"
        exit 1
    fi
    
    status_log "Building LittleFS filesystem image..."
    quiet_echo "${BLUE}Building LittleFS image from $WEB_DATA_DIR...${NC}"
    if [ "$QUIET" = false ]; then
        quiet_echo "${BLUE}Files to include: $(find "$WEB_DATA_DIR" -type f | wc -l | tr -d ' ') files${NC}"
    fi
    
    if [ "$QUIET" = true ]; then
        if ! pio run -e "$ENV_NAME" -t buildfs > /dev/null 2>&1; then
            echo -e "${RED}✗ LittleFS build failed!${NC}" >&2
            exit 1
        fi
    else
        if ! pio run -e "$ENV_NAME" -t buildfs; then
            echo -e "${RED}✗ LittleFS build failed!${NC}"
            quiet_echo "${YELLOW}Check that:${NC}"
            quiet_echo "  1. Data directory exists: $WEB_DATA_DIR"
            quiet_echo "  2. Data directory contains files"
            quiet_echo "  3. PlatformIO is configured correctly"
            exit 1
        fi
    fi
    
    # Verify LittleFS image was created
    if [ ! -f "$LITTLEFS_IMAGE" ]; then
        echo -e "${RED}✗ LittleFS image not found after build: $LITTLEFS_IMAGE${NC}" >&2
        quiet_echo "${YELLOW}Build may have failed silently${NC}"
        exit 1
    fi
    
    status_log "LittleFS image built"
    if [ "$QUIET" = false ]; then
        LITTLEFS_SIZE=$(du -h "$LITTLEFS_IMAGE" | cut -f1)
        quiet_echo "${GREEN}✓ LittleFS image built: $LITTLEFS_SIZE${NC}"
    fi
fi
quiet_echo ""

# Note: LittleFS image will be built during firmware build if needed

# Now proceed to port detection and flashing
status_log "Detecting serial port..."
quiet_echo "${BLUE}Detecting serial port...${NC}"

# Detect port (first non-flag argument)
PORT=""
for arg in "$@"; do
    case "$arg" in
        --skip-prompt|--firmware-only|--no-clean|--factory-reset|--quiet|-q|--noscreen)
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
        if [ "$QUIET" = false ]; then
            echo -e "${YELLOW}Multiple serial ports found:${NC}"
            for i in "${!AVAILABLE_PORTS[@]}"; do
                echo -e "  ${CYAN}[$((i+1))]${NC} ${AVAILABLE_PORTS[$i]}"
            done
            echo ""
            echo -e "${YELLOW}Please select a port (1-${#AVAILABLE_PORTS[@]}):${NC}"
        fi
        read -r SELECTION
        if [[ "$SELECTION" =~ ^[0-9]+$ ]] && [ "$SELECTION" -ge 1 ] && [ "$SELECTION" -le ${#AVAILABLE_PORTS[@]} ]; then
            PORT="${AVAILABLE_PORTS[$((SELECTION-1))]}"
            quiet_echo "${GREEN}Selected: $PORT${NC}"
        else
            echo -e "${RED}✗ Invalid selection${NC}" >&2
            exit 1
        fi
    elif [ ${#AVAILABLE_PORTS[@]} -eq 1 ]; then
        PORT="${AVAILABLE_PORTS[0]}"
        status_log "Port detected: $PORT"
        quiet_echo "${BLUE}Auto-detected port: ${GREEN}$PORT${NC}"
    else
        PORT=""
    fi
fi

# Validate port is not the debug console
if [[ "$PORT" == *"debug-console"* ]]; then
    echo -e "${RED}✗ Cannot use debug-console port for flashing!${NC}" >&2
    if [ "$QUIET" = false ]; then
        echo ""
        echo "The ESP32-S3 has two USB ports:"
        echo "  - Programming port (usbmodem/usbserial) - use this for flashing"
        echo "  - Debug console (debug-console) - use this for Serial monitor only"
        echo ""
        echo "Please specify the programming port manually:"
        echo "  $0 /dev/cu.usbserial-00000000"
    fi
    exit 1
fi

if [ -z "$PORT" ]; then
    echo -e "${YELLOW}⚠ No ESP32 device auto-detected${NC}" >&2
    if [ "$QUIET" = false ]; then
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
    fi
    read -r PORT
    
    if [ -z "$PORT" ]; then
        echo -e "${RED}✗ No port specified${NC}" >&2
        exit 1
    fi
    
    if [ ! -e "$PORT" ]; then
        echo -e "${RED}✗ Port does not exist: $PORT${NC}" >&2
        exit 1
    fi
    
    quiet_echo "${GREEN}Using port: $PORT${NC}"
fi

status_log "Preparing to flash to $PORT"
quiet_echo "📍 Port: ${GREEN}$PORT${NC}"
quiet_echo "📦 Variant: ${GREEN}$VARIANT${NC}"
quiet_echo "📦 Environment: ${GREEN}$ENV_NAME${NC}"
quiet_echo "📦 Firmware: ${GREEN}$(basename $FIRMWARE)${NC} ($(du -h "$FIRMWARE" | cut -f1))"
if [ "$FIRMWARE_ONLY" = false ]; then
    if [ -d "$WEB_DATA_DIR" ] && [ -n "$(ls -A "$WEB_DATA_DIR" 2>/dev/null)" ]; then
        if [ "$QUIET" = false ]; then
            WEB_SIZE=$(du -sh "$WEB_DATA_DIR" | cut -f1)
            quiet_echo "📦 Web Files: ${GREEN}$WEB_DATA_DIR${NC} ($WEB_SIZE)"
        fi
    else
        quiet_echo "📦 Web Files: ${YELLOW}(not found)${NC}"
    fi
fi
if [ "$FACTORY_RESET" = true ]; then
    status_log "Factory reset mode enabled"
    quiet_echo "🔄 Mode: ${YELLOW}FACTORY RESET (full flash erase)${NC}"
fi
quiet_echo ""

if [ "$SKIP_PROMPT" = false ]; then
    quiet_echo "${CYAN}Instructions to enter bootloader mode:${NC}"
    quiet_echo "  1. Disconnect ESP32 from power (if connected)"
    quiet_echo "  2. Hold the BOOT button (or IO0 button) on the ESP32"
    quiet_echo "  3. While holding BOOT, press and release the RESET button"
    quiet_echo "  4. Release the BOOT button"
    quiet_echo "  5. ESP32 is now in bootloader mode"
    quiet_echo ""
    quiet_echo "${YELLOW}Press Enter when ready to flash, or Ctrl+C to cancel...${NC}"
    read -r
fi

# Flash using PlatformIO
status_log "Flashing firmware to $PORT..."
quiet_echo "${YELLOW}⚡ Flashing ESP32 firmware...${NC}"
quiet_echo "${BLUE}Using port: $PORT${NC}"
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

# OTA data partition address and size (from partitions_16MB.csv partition table)
# This partition stores which OTA slot to boot from
# IMPORTANT: Must match partition table - partitions_16MB.csv uses 0xe000
OTADATA_ADDR="0xe000"
OTADATA_SIZE="0x2000"

# OTA app1 partition address and size (from partitions_16MB.csv)
# This partition may contain old firmware from previous OTA that causes fallback issues
APP1_ADDR="0x310000"
APP1_SIZE="0x300000"

# Factory reset: erase entire flash first
if [ "$FACTORY_RESET" = true ]; then
    status_log "Erasing entire flash (factory reset)..."
    quiet_echo "${YELLOW}🔄 Factory reset: Erasing entire flash...${NC}"
    quiet_echo "${BLUE}This will take about 30-60 seconds...${NC}"
    
    if [ "$QUIET" = true ]; then
        if ! "$PYTHON_CMD" "$ESPTOOL_PY" --chip esp32s3 --port "$PORT" erase-flash > /dev/null 2>&1; then
            echo -e "${RED}✗ Flash erase failed!${NC}" >&2
            exit 1
        fi
    else
        if ! "$PYTHON_CMD" "$ESPTOOL_PY" --chip esp32s3 --port "$PORT" erase-flash; then
            echo -e "${RED}✗ Flash erase failed!${NC}"
            quiet_echo "${YELLOW}Make sure ESP32 is in bootloader mode and try again${NC}"
            exit 1
        fi
    fi
    status_log "Flash erased successfully"
    quiet_echo "${GREEN}✓ Flash erased successfully${NC}"
    quiet_echo ""
else
    # Erase OTA data partition to clear boot selection
    status_log "Erasing OTA partitions..."
    quiet_echo "${YELLOW}🔄 Erasing OTA data partition (ensures fresh firmware boots)...${NC}"
    quiet_echo "${BLUE}Erasing ${OTADATA_SIZE} bytes at ${OTADATA_ADDR}...${NC}"
    
    if [ "$QUIET" = true ]; then
        if ! "$PYTHON_CMD" "$ESPTOOL_PY" --chip esp32s3 --port "$PORT" erase-region "$OTADATA_ADDR" "$OTADATA_SIZE" > /dev/null 2>&1; then
            # Continue even if erase fails in quiet mode
            :
        fi
    else
        if ! "$PYTHON_CMD" "$ESPTOOL_PY" --chip esp32s3 --port "$PORT" erase-region "$OTADATA_ADDR" "$OTADATA_SIZE" 2>&1; then
            quiet_echo "${YELLOW}⚠ OTA data erase failed (may need bootloader mode), continuing...${NC}"
        else
            quiet_echo "${GREEN}✓ OTA data partition erased${NC}"
        fi
    fi
    
    # ALSO erase app1 partition to remove old OTA firmware
    # This prevents fallback to old firmware if OTA crashes
    quiet_echo "${YELLOW}🔄 Erasing OTA app1 partition (removes old OTA firmware)...${NC}"
    quiet_echo "${BLUE}Erasing ${APP1_SIZE} bytes at ${APP1_ADDR}...${NC}"
    
    if [ "$QUIET" = true ]; then
        if ! "$PYTHON_CMD" "$ESPTOOL_PY" --chip esp32s3 --port "$PORT" erase-region "$APP1_ADDR" "$APP1_SIZE" > /dev/null 2>&1; then
            # Continue even if erase fails in quiet mode
            :
        fi
    else
        if ! "$PYTHON_CMD" "$ESPTOOL_PY" --chip esp32s3 --port "$PORT" erase-region "$APP1_ADDR" "$APP1_SIZE" 2>&1; then
            quiet_echo "${YELLOW}⚠ App1 erase failed, continuing...${NC}"
        else
            quiet_echo "${GREEN}✓ App1 partition erased${NC}"
        fi
    fi
    quiet_echo ""
fi

# Flash firmware and filesystem together if not firmware-only
if [ "$FIRMWARE_ONLY" = false ] && [ -f "$LITTLEFS_IMAGE" ]; then
    # Get LittleFS partition address from PlatformIO (partitions_16MB.csv uses 0x610000)
    # Try to get it from partition table, fallback to default
    LITTLEFS_ADDR="0x610000"
    PARTITION_TABLE="$ESP32_DIR/.pio/build/$ENV_NAME/partitions.bin"
    if [ -f "$PARTITION_TABLE" ]; then
        # Try to extract from partition table (this is a fallback - PlatformIO uses 0x610000 for partitions_16MB.csv)
        # For now, we'll use the known address for partitions_16MB.csv
        LITTLEFS_ADDR="0x610000"
    fi
    
    # Verify Python has pyserial before attempting flash
    if ! "$PYTHON_CMD" -c "import serial" 2>/dev/null; then
        echo -e "${RED}✗ pyserial not available in Python environment${NC}" >&2
        quiet_echo "${YELLOW}Please install: pip install pyserial${NC}"
        exit 1
    fi
    
    # Verify firmware files exist
    if [ ! -f "$FIRMWARE" ]; then
        echo -e "${RED}✗ Firmware file not found: $FIRMWARE${NC}" >&2
        exit 1
    fi
    if [ ! -f "$LITTLEFS_IMAGE" ]; then
        echo -e "${RED}✗ LittleFS image not found: $LITTLEFS_IMAGE${NC}" >&2
        exit 1
    fi
    
    if [ "$QUIET" = false ]; then
        quiet_echo "${BLUE}Firmware size: $(du -h "$FIRMWARE" | cut -f1)${NC}"
        quiet_echo "${BLUE}LittleFS size: $(du -h "$LITTLEFS_IMAGE" | cut -f1)${NC}"
        quiet_echo ""
    fi
    
    # Use esptool.py to flash both partitions in one session
    # Firmware goes to 0x10000 (app partition), LittleFS goes to 0x610000 (for 16MB flash with partitions_16MB.csv)
    # write_flash automatically erases sectors before writing, so no explicit erase needed
    quiet_echo "${BLUE}Flashing firmware to 0x10000 and filesystem to $LITTLEFS_ADDR...${NC}"
    quiet_echo "${YELLOW}(Sectors will be automatically erased before writing)${NC}"
    
    # Check if bootloader, partition table, and other required files exist
    BOOTLOADER="$ESP32_DIR/.pio/build/$ENV_NAME/bootloader.bin"
    PARTITION_TABLE="$ESP32_DIR/.pio/build/$ENV_NAME/partitions.bin"
    BOOTLOADER_ADDR="0x0"
    PARTITION_TABLE_ADDR="0x8000"
    
    # Use esptool.py directly to flash partitions in one command
    # This keeps the ESP32 in bootloader mode for the entire operation
    # Add --verify to ensure flash succeeded
    # Use --after no_reset to prevent RTS reset (we don't have RTS pin)
    quiet_echo "${BLUE}Starting flash operation...${NC}"
    
    # Build flash command arguments as an array for safe expansion
    # Use --after hard-reset to automatically reset ESP32 after flashing (boots firmware)
    # Note: --verify removed in esptool v5.x (verification is automatic)
    # Flash size is 16MB for UEDX48480021-MD80E (ESP32-S3-WROOM-1-N16R8)
    FLASH_ARGS=(
        --chip esp32s3
        --port "$PORT"
        --baud 921600
        --after hard-reset
        write-flash
        --flash-mode dio
        --flash-freq 80m
        --flash-size 16MB
    )
    
    # Include bootloader if it exists (at 0x0) - REQUIRED for ESP32-S3
    BOOTLOADER="$ESP32_DIR/.pio/build/$ENV_NAME/bootloader.bin"
    if [ -f "$BOOTLOADER" ]; then
        quiet_echo "${BLUE}Including bootloader at 0x0...${NC}"
        FLASH_ARGS+=("0x0" "$BOOTLOADER")
    else
        quiet_echo "${YELLOW}⚠ Warning: Bootloader not found at $BOOTLOADER${NC}"
        quiet_echo "${YELLOW}  This may cause boot issues. Continuing anyway...${NC}"
    fi
    
    # Include partition table if it exists (at 0x8000)
    if [ -f "$PARTITION_TABLE" ]; then
        quiet_echo "${BLUE}Including partition table at $PARTITION_TABLE_ADDR...${NC}"
        FLASH_ARGS+=("$PARTITION_TABLE_ADDR" "$PARTITION_TABLE")
    else
        quiet_echo "${YELLOW}⚠ Warning: Partition table not found at $PARTITION_TABLE${NC}"
        quiet_echo "${YELLOW}  This may cause boot issues. Continuing anyway...${NC}"
    fi
    
    # Add firmware and filesystem
    FLASH_ARGS+=("0x10000" "$FIRMWARE" "$LITTLEFS_ADDR" "$LITTLEFS_IMAGE")
    
    # Use pipefail (set at top of script) to ensure we catch esptool failures
    FLASH_SUCCESS=false
    
    # In quiet mode, suppress output and use a more efficient method
    if [ "$QUIET" = true ]; then
        # In quiet mode, capture output to temp file only for error checking, don't display
        TEMP_LOG=$(mktemp)
        status_log "Writing firmware and filesystem to flash..."
        if "$PYTHON_CMD" "$ESPTOOL_PY" "${FLASH_ARGS[@]}" > "$TEMP_LOG" 2>&1; then
            # Check for fatal errors even in quiet mode
            if grep -qiE "(fatal error|Failed to connect|No serial data received|A fatal error occurred)" "$TEMP_LOG"; then
                echo -e "${RED}✗ Flash failed (fatal errors detected)!${NC}" >&2
                cat "$TEMP_LOG" >&2
                rm -f "$TEMP_LOG"
                exit 1
            else
                FLASH_SUCCESS=true
                status_log "Flash complete - verifying..."
            fi
        else
            echo -e "${RED}✗ Flash failed!${NC}" >&2
            cat "$TEMP_LOG" >&2
            rm -f "$TEMP_LOG"
            exit 1
        fi
        rm -f "$TEMP_LOG"
    else
        # In verbose mode, use tee to show output and log it
        if "$PYTHON_CMD" "$ESPTOOL_PY" "${FLASH_ARGS[@]}" 2>&1 | tee /tmp/flash_output.log; then
            # Double-check for fatal errors in the output (esptool sometimes returns 0 even on failure)
            # Look for specific fatal error patterns, not just any "error" word
            if grep -qiE "(fatal error|Failed to connect|No serial data received|A fatal error occurred)" /tmp/flash_output.log; then
                echo ""
                echo -e "${RED}✗ Flash failed (fatal errors detected in output)!${NC}"
                echo -e "${YELLOW}Check the output above for errors.${NC}"
                FLASH_SUCCESS=false
            else
                status_log "Flash complete - verifying..."
                echo ""
                echo -e "${GREEN}✓ Firmware and filesystem flashed and verified successfully!${NC}"
                echo -e "${CYAN}ESP32 will now reset and boot automatically.${NC}"
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
    fi
else
    # Flash firmware only (either firmware-only flag or LittleFS not available)
    # Use PlatformIO which handles erase automatically
    status_log "Flashing firmware only to $PORT..."
    quiet_echo "${YELLOW}⚡ Flashing firmware (with auto-erase)...${NC}"
    if [ "$QUIET" = true ]; then
        if pio run -e "$ENV_NAME" -t upload --upload-port "$PORT" > /dev/null 2>&1; then
            FLASH_SUCCESS=true
            status_log "Firmware flash complete"
        else
            echo -e "${RED}✗ Flash failed!${NC}" >&2
            FLASH_SUCCESS=false
            exit 1
        fi
    else
        if pio run -e "$ENV_NAME" -t upload --upload-port "$PORT"; then
            status_log "Firmware flash complete"
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
fi

# Only print success message if we actually succeeded
# (This prevents false success messages if flash failed earlier)
if [ "$FLASH_SUCCESS" = true ]; then
    status_log "Flash operation completed successfully"
    quiet_echo ""
    quiet_echo "${GREEN}✓ Flash complete!${NC}"
    quiet_echo "${CYAN}Firmware hash: $FIRMWARE_HASH${NC}"
    quiet_echo ""
    
    # # Start serial monitor automatically (unless in quiet mode)
    # if [ "$QUIET" = false ]; then
    #     quiet_echo "${BLUE}Starting serial monitor on $PORT...${NC}"
    #     quiet_echo "${YELLOW}Press Ctrl+C to exit monitor${NC}"
    #     quiet_echo ""
        
    #     # Brief delay to let ESP32 reset and start booting
    #     sleep 1
        
    #     # Start PlatformIO device monitor (exec replaces shell, so Ctrl+C exits cleanly)
    #     cd "$ESP32_DIR"
    #     exec pio device monitor -p "$PORT"
    # fi
else
    # This shouldn't be reached if exit 1 was called, but just in case
    echo ""
    echo -e "${RED}✗ Flash did not complete successfully${NC}" >&2
    exit 1
fi
