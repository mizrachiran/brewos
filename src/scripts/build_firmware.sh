#!/bin/bash

# BrewOS - Build Script
# Builds both Pico and ESP32 firmware, or each separately

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script directory (src/scripts/)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Project root is one level up from scripts/
SRC_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PICO_DIR="$SRC_DIR/pico"
ESP32_DIR="$SRC_DIR/esp32"
WEB_DIR="$SRC_DIR/web"

# Functions
print_header() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_info() {
    echo -e "${YELLOW}→ $1${NC}"
}

# Check if Pico SDK is set
check_pico_sdk() {
    if [ -z "$PICO_SDK_PATH" ]; then
        if [ -d "$HOME/pico-sdk" ]; then
            export PICO_SDK_PATH="$HOME/pico-sdk"
            print_info "Using PICO_SDK_PATH=$PICO_SDK_PATH"
        else
            print_error "PICO_SDK_PATH not set and ~/pico-sdk not found"
            echo "Please set PICO_SDK_PATH or install Pico SDK to ~/pico-sdk"
            exit 1
        fi
    fi
}

# Check if ARM toolchain is in PATH
check_arm_toolchain() {
    if ! command -v arm-none-eabi-gcc &> /dev/null; then
        # Try to find and add official ARM toolchain
        if [ -d "/Applications/ArmGNUToolchain" ]; then
            TOOLCHAIN_PATH=$(find /Applications/ArmGNUToolchain -name "arm-none-eabi-gcc" -type f 2>/dev/null | head -1 | xargs dirname)
            if [ -n "$TOOLCHAIN_PATH" ]; then
                export PATH="$TOOLCHAIN_PATH:$PATH"
                print_info "Added ARM toolchain to PATH: $TOOLCHAIN_PATH"
            fi
        fi
        
        if ! command -v arm-none-eabi-gcc &> /dev/null; then
            print_error "arm-none-eabi-gcc not found in PATH"
            echo "Please install ARM GNU Toolchain or add it to PATH"
            exit 1
        fi
    fi
}

# Build Pico firmware
build_pico() {
    print_header "Building BrewOS Pico Firmware"
    
    check_pico_sdk
    check_arm_toolchain
    
    cd "$PICO_DIR"
    
    # Create build directory if it doesn't exist
    mkdir -p build
    cd build
    
    # Configure CMake - build all machine types
    print_info "Configuring CMake..."
    cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_ALL_MACHINES=ON .. || {
        print_error "CMake configuration failed"
        exit 1
    }
    
    # Build
    print_info "Building firmware..."
    make -j$(sysctl -n hw.ncpu 2>/dev/null || echo 4) || {
        print_error "Build failed"
        exit 1
    }
    
    # Copy compile_commands.json for IDE
    if [ -f "compile_commands.json" ]; then
        cp compile_commands.json .. 2>/dev/null || true
    fi
    
    # Check for output files (multi-machine build)
    UF2_COUNT=$(ls -1 brewos_*.uf2 2>/dev/null | wc -l | tr -d ' ')
    
    if [ "$UF2_COUNT" -gt 0 ]; then
        print_success "Pico firmware built successfully!"
        echo ""
        echo "  Output files:"
        for uf2 in brewos_*.uf2; do
            SIZE=$(ls -lh "$uf2" | awk '{print $5}')
            echo "    - $uf2 ($SIZE)"
        done
    else
        print_error "No UF2 files found after build"
        exit 1
    fi
}

# Build Web UI for ESP32
build_web() {
    print_header "Building Web UI for ESP32"
    
    cd "$WEB_DIR"
    
    # Check if npm is available
    if ! command -v npm &> /dev/null; then
        print_error "npm not found"
        echo "Please install Node.js: https://nodejs.org/"
        exit 1
    fi
    
    # Install dependencies if needed
    if [ ! -d "node_modules" ]; then
        print_info "Installing dependencies..."
        npm ci || npm install || {
            print_error "npm install failed"
            exit 1
        }
    fi
    
    # Build for ESP32
    print_info "Building web UI..."
    npm run build:esp32 || {
        print_error "Web build failed"
        exit 1
    }
    
    print_success "Web UI built successfully"
    du -sh "$ESP32_DIR/data" | awk '{print "  Size: " $1}'
}

# Build ESP32 firmware
build_esp32() {
    print_header "Building BrewOS ESP32 Firmware"
    
    # Build web UI first
    build_web
    echo ""
    
    cd "$ESP32_DIR"
    
    # Check if PlatformIO is available
    if ! command -v pio &> /dev/null; then
        print_error "PlatformIO (pio) not found"
        echo "Please install PlatformIO: https://platformio.org/install/cli"
        exit 1
    fi
    
    # Build
    print_info "Building with PlatformIO..."
    pio run || {
        print_error "Build failed"
        exit 1
    }
    
    # Build LittleFS image
    print_info "Building LittleFS image..."
    pio run -t buildfs || {
        print_error "LittleFS build failed"
        exit 1
    }
    
    # Generate compile_commands.json for IDE
    print_info "Generating compile database for IDE..."
    pio run --target compiledb 2>/dev/null || true
    
    print_success "ESP32 firmware built successfully"
    if [ -f ".pio/build/esp32s3/firmware.bin" ]; then
        ls -lh .pio/build/esp32s3/firmware.bin | awk '{print "  Firmware: " $9 " (" $5 ")"}'
    fi
    if [ -f ".pio/build/esp32s3/littlefs.bin" ]; then
        ls -lh .pio/build/esp32s3/littlefs.bin | awk '{print "  LittleFS: " $9 " (" $5 ")"}'
    fi
}

# Clean build artifacts
clean_pico() {
    print_info "Cleaning Pico build..."
    cd "$PICO_DIR"
    rm -rf build
    rm -f compile_commands.json compile_commands.json.bak
    print_success "Pico build cleaned"
}

clean_esp32() {
    print_info "Cleaning ESP32 build..."
    cd "$ESP32_DIR"
    pio run --target clean 2>/dev/null || rm -rf .pio
    rm -f compile_commands.json
    print_success "ESP32 build cleaned"
}

# Show usage
show_usage() {
    echo "Usage: $0 [pico|esp32|all|clean|clean-pico|clean-esp32]"
    echo ""
    echo "Commands:"
    echo "  pico        Build Pico firmware (all machine types)"
    echo "  esp32       Build ESP32 firmware only"
    echo "  all         Build both firmwares (default)"
    echo "  clean       Clean all build artifacts"
    echo "  clean-pico  Clean Pico build only"
    echo "  clean-esp32 Clean ESP32 build only"
    echo ""
    echo "Pico builds generate firmware for all machine types:"
    echo "  - brewos_dual_boiler.uf2"
    echo "  - brewos_single_boiler.uf2"
    echo "  - brewos_heat_exchanger.uf2"
    echo ""
    echo "Examples:"
    echo "  $0           # Build both"
    echo "  $0 pico      # Build Pico only"
    echo "  $0 esp32     # Build ESP32 only"
    echo "  $0 clean     # Clean everything"
}

# Main
main() {
    case "${1:-all}" in
        pico)
            build_pico
            ;;
        esp32)
            build_esp32
            ;;
        all)
            build_pico
            echo ""
            build_esp32
            echo ""
            print_header "Build Complete"
            print_success "Both firmwares built successfully!"
            ;;
        clean)
            clean_pico
            clean_esp32
            ;;
        clean-pico)
            clean_pico
            ;;
        clean-esp32)
            clean_esp32
            ;;
        help|--help|-h)
            show_usage
            ;;
        *)
            print_error "Unknown command: $1"
            show_usage
            exit 1
            ;;
    esac
}

main "$@"

