#!/bin/bash
#
# BrewOS UI Theme Simulator
# Builds and runs the LVGL UI simulator for local development
#
# Usage: ./src/scripts/run_simulator.sh
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ESP32_DIR="$SCRIPT_DIR/../esp32"

echo ""
echo "╔══════════════════════════════════════╗"
echo "║    BrewOS UI Theme Simulator         ║"
echo "╚══════════════════════════════════════╝"
echo ""

# Check for SDL2 (macOS)
if command -v brew &>/dev/null; then
    if ! brew list sdl2 &>/dev/null; then
        echo "📦 SDL2 not found. Installing via Homebrew..."
        brew install sdl2
    fi
fi

cd "$ESP32_DIR"

# Build the simulator
echo "🔨 Building simulator..."
if pio run -e simulator; then
    echo ""
    echo "✅ Build successful!"
    echo ""
    echo "🚀 Starting simulator..."
    echo "   • Click to simulate touch input"
    echo "   • Press ESC to close window"
    echo ""
    
    # Run the simulator
    .pio/build/simulator/program
else
    echo ""
    echo "❌ Build failed. Check errors above."
    exit 1
fi

