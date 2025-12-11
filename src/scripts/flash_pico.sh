#!/bin/bash
# Flash Pico firmware via USB (BOOTSEL mode)
# Usage: ./scripts/flash_pico.sh [machine_type] [firmware_path] [mount_point]
#   machine_type: DUAL_BOILER, SINGLE_BOILER, HEAT_EXCHANGER (default: DUAL_BOILER)
#   firmware_path: Path to .uf2 file (optional, auto-detects if not provided)
#   mount_point: Manual mount point path (optional, auto-detects if not provided)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PICO_DIR="$SCRIPT_DIR/../pico"
BUILD_DIR="$PICO_DIR/build"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BLUE='\033[0;34m'
NC='\033[0m'

# Machine types
MACHINE_TYPES=("DUAL_BOILER" "SINGLE_BOILER" "HEAT_EXCHANGER")
MACHINE_TYPE="${1:-DUAL_BOILER}"

# Validate machine type
if [[ ! " ${MACHINE_TYPES[@]} " =~ " ${MACHINE_TYPE} " ]]; then
    echo -e "${RED}✗ Invalid machine type: $MACHINE_TYPE${NC}"
    echo ""
    echo "Valid machine types:"
    for type in "${MACHINE_TYPES[@]}"; do
        echo "  - $type"
    done
    exit 1
fi

# Convert to lowercase for filename
MACHINE_LOWER=$(echo "$MACHINE_TYPE" | tr '[:upper:]' '[:lower:]')
FIRMWARE_NAME="brewos_${MACHINE_LOWER}.uf2"

echo -e "${CYAN}"
echo "========================================"
echo "  BrewOS Pico Flasher"
echo "========================================"
echo -e "${NC}"

# Check if mount point provided manually (3rd argument)
MANUAL_MOUNT=""
if [ -n "$3" ]; then
    MANUAL_MOUNT="$3"
    if [ ! -d "$MANUAL_MOUNT" ]; then
        echo -e "${RED}✗ Mount point not found: $MANUAL_MOUNT${NC}"
        exit 1
    fi
    echo -e "${BLUE}Using manual mount point: $MANUAL_MOUNT${NC}"
fi

# Check if firmware path provided manually
if [ -n "$2" ]; then
    FIRMWARE="$2"
    if [ ! -f "$FIRMWARE" ]; then
        echo -e "${RED}✗ Firmware file not found: $FIRMWARE${NC}"
        exit 1
    fi
    echo -e "${YELLOW}⚠ Using manually specified firmware: $FIRMWARE${NC}"
    echo -e "${YELLOW}⚠ Note: This firmware may not match the current source code${NC}"
    echo ""
else
    # Always build firmware before flashing
    echo -e "${BLUE}Building firmware for $MACHINE_TYPE...${NC}"
    
    # Check if Pico SDK is set
    if [ -z "$PICO_SDK_PATH" ]; then
        if [ -d "$HOME/pico-sdk" ]; then
            export PICO_SDK_PATH="$HOME/pico-sdk"
            echo -e "${BLUE}Using PICO_SDK_PATH=$PICO_SDK_PATH${NC}"
        else
            echo -e "${RED}✗ PICO_SDK_PATH not set and ~/pico-sdk not found${NC}"
            echo "Please set PICO_SDK_PATH or install Pico SDK to ~/pico-sdk"
            exit 1
        fi
    fi
    
    # Create build directory if it doesn't exist
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    # Configure CMake if not already configured or if machine type changed
    if [ ! -f "CMakeCache.txt" ] || ! grep -q "MACHINE_TYPE:STRING=$MACHINE_TYPE" CMakeCache.txt 2>/dev/null; then
        echo -e "${BLUE}Configuring CMake for $MACHINE_TYPE...${NC}"
        cmake .. -DMACHINE_TYPE="$MACHINE_TYPE"
    fi
    
    # Always rebuild to ensure we flash the latest version
    echo -e "${BLUE}Building firmware...${NC}"
    if make -j$(sysctl -n hw.ncpu 2>/dev/null || echo 4); then
        echo -e "${GREEN}✓ Build successful${NC}"
    else
        echo -e "${RED}✗ Build failed${NC}"
        exit 1
    fi
    
    # Set firmware path to built version
    FIRMWARE="$BUILD_DIR/$FIRMWARE_NAME"
    echo ""
fi

# Verify firmware exists
if [ ! -f "$FIRMWARE" ]; then
    echo -e "${RED}✗ Firmware file not found: $FIRMWARE${NC}"
    exit 1
fi

echo -e "📦 Machine Type: ${GREEN}$MACHINE_TYPE${NC}"
echo -e "📦 Firmware: ${GREEN}$(basename $FIRMWARE)${NC} ($(du -h "$FIRMWARE" | cut -f1))"
echo ""

# Detect RPI-RP2 mount point
echo -e "${YELLOW}Looking for Pico in BOOTSEL mode...${NC}"
echo ""
echo -e "${CYAN}Instructions:${NC}"
echo "  1. Disconnect Pico from power"
echo "  2. Hold the BOOTSEL button"
echo "  3. Connect USB cable (while holding BOOTSEL)"
echo "  4. Release BOOTSEL"
echo "  5. Pico should appear as 'RPI-RP2' (Pico 1) or 'RP2350' (Pico 2) drive"
echo ""

# Use manual mount point if provided, otherwise auto-detect
if [ -n "$MANUAL_MOUNT" ]; then
    MOUNT_POINT="$MANUAL_MOUNT"
else
    # Wait for RPI-RP2 mount (or RP2350 for Pico 2)
    MOUNT_POINT=""
    MAX_WAIT=30
    WAIT_COUNT=0

    # Function to detect Pico mount point
    detect_pico_mount() {
        if [[ "$OSTYPE" == "darwin"* ]]; then
            # macOS - check for both RPI-RP2 (Pico 1) and RP2350 (Pico 2)
            for vol_name in "RPI-RP2" "RP2350"; do
                local test_mount="/Volumes/$vol_name"
                if [ -d "$test_mount" ]; then
                    echo "$test_mount"
                    return 0
                fi
            done
            
            # Also check all volumes for any that might be a Pico
            for vol in /Volumes/*; do
                if [ -d "$vol" ] && [ -f "$vol/INDEX.HTM" ] && [ -f "$vol/INFO_UF2.TXT" ]; then
                    # This looks like a Pico in BOOTSEL mode
                    echo "$vol"
                    return 0
                fi
            done
        else
            # Linux - check multiple possible locations
            # Try findmnt first (more reliable)
            if command -v findmnt >/dev/null 2>&1; then
                local found=$(findmnt -n -o TARGET -l | grep -iE "RPI-RP2|RP2350" | head -1)
                if [ -n "$found" ] && [ -d "$found" ]; then
                    echo "$found"
                    return 0
                fi
            fi
            
            # Fallback to mount command
            local found=$(mount | grep -iE "RPI-RP2|RP2350" | awk '{print $3}' | head -1)
            if [ -n "$found" ] && [ -d "$found" ]; then
                echo "$found"
                return 0
            fi
            
            # Also check common mount locations for both names
            for vol_name in "RPI-RP2" "RP2350"; do
                for dir in /media/*/$vol_name /mnt/$vol_name /run/media/*/$vol_name; do
                    if [ -d "$dir" ]; then
                        echo "$dir"
                        return 0
                    fi
                done
            done
            
            # Last resort: check all mounted filesystems for Pico indicators
            for mount_dir in /media/* /mnt /run/media/*; do
                if [ -d "$mount_dir" ]; then
                    for vol in "$mount_dir"/*; do
                        if [ -d "$vol" ] && [ -f "$vol/INDEX.HTM" ] && [ -f "$vol/INFO_UF2.TXT" ]; then
                            echo "$vol"
                            return 0
                        fi
                    done
                fi
            done
        fi
        return 1
    }

    # Check immediately first (in case already mounted)
    MOUNT_POINT=$(detect_pico_mount)
    
    # If not found, wait and retry
    while [ -z "$MOUNT_POINT" ] && [ $WAIT_COUNT -lt $MAX_WAIT ]; do
        MOUNT_POINT=$(detect_pico_mount)
        if [ -n "$MOUNT_POINT" ]; then
            break
        fi
    
        echo -ne "\r${YELLOW}Waiting for Pico (RPI-RP2/RP2350)... (${WAIT_COUNT}s/${MAX_WAIT}s)${NC}"
        sleep 1
        WAIT_COUNT=$((WAIT_COUNT + 1))
    done

    echo ""
fi

if [ -z "$MOUNT_POINT" ] || [ ! -d "$MOUNT_POINT" ]; then
    echo -e "${RED}✗ Pico drive not found!${NC}"
    echo ""
    echo "Troubleshooting:"
    echo "  - Make sure Pico is in BOOTSEL mode (hold BOOTSEL while connecting USB)"
    echo "  - Check USB cable connection"
    echo "  - Try disconnecting and reconnecting"
    echo ""
    if [[ "$OSTYPE" == "darwin"* ]]; then
        echo "Available volumes:"
        ls -1 /Volumes/ 2>/dev/null || echo "  (none)"
        echo ""
        echo "If you see a volume that looks like a Pico, you can manually specify it:"
        echo "  ${CYAN}./flash_pico.sh $MACHINE_TYPE /Volumes/[VOLUME_NAME]${NC}"
    else
        echo "Available USB devices:"
        lsblk -o NAME,SIZE,TYPE,MOUNTPOINT | grep -E "sd|mmc" || echo "  (none)"
        echo ""
        echo "If you see a mount point that looks like a Pico, you can manually specify it:"
        echo "  ${CYAN}./flash_pico.sh $MACHINE_TYPE /path/to/mount${NC}"
    fi
    exit 1
fi

echo -e "📍 Mount Point: ${GREEN}$MOUNT_POINT${NC}"

# Verify this is actually a Pico device
if [ ! -f "$MOUNT_POINT/INFO_UF2.TXT" ]; then
    echo -e "${YELLOW}⚠ Warning: INFO_UF2.TXT not found. This might not be a Pico device.${NC}"
    echo -e "${YELLOW}Continuing anyway...${NC}"
fi

echo ""

# Copy firmware
echo -e "${YELLOW}⚡ Copying firmware to Pico...${NC}"
if cp "$FIRMWARE" "$MOUNT_POINT/"; then
    # Wait a moment for the copy to complete
    sleep 1
    
    # Note: The Pico will automatically reboot and unmount the drive after receiving the .uf2 file
    # This is expected behavior - we don't check if the file still exists because the drive unmounts
    
    echo ""
    echo -e "${GREEN}✓ Firmware copied successfully!${NC}"
    echo ""
    echo -e "${CYAN}Pico is rebooting with the new firmware...${NC}"
    echo -e "${YELLOW}(The drive will unmount automatically - this is normal)${NC}"
    echo ""
    echo -e "Monitor serial output with:"
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # Wait a moment for serial port to appear after reboot
        sleep 2
        SERIAL_PORT=$(ls /dev/cu.usbmodem* /dev/cu.usbserial* 2>/dev/null | head -1)
    else
        sleep 2
        SERIAL_PORT=$(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | head -1)
    fi
    
    if [ -n "$SERIAL_PORT" ]; then
        echo -e "  ${CYAN}screen $SERIAL_PORT 115200${NC}"
        echo -e "  ${CYAN}or${NC}"
        echo -e "  ${CYAN}minicom -D $SERIAL_PORT -b 115200${NC}"
    else
        echo -e "  ${YELLOW}(Serial port will be available after Pico reboots)${NC}"
        echo -e "  ${YELLOW}Try: screen /dev/cu.usbmodem* 115200${NC}"
    fi
else
    echo ""
    echo -e "${RED}✗ Failed to copy firmware${NC}"
    echo ""
    echo "Possible issues:"
    echo "  - Pico drive is read-only or locked"
    echo "  - Insufficient disk space"
    echo "  - File system error"
    exit 1
fi

