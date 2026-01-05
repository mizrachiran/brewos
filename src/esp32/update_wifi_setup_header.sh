#!/bin/bash
# Script to update wifi_setup_page.h from wifi_setup_page_dev.html
# This keeps the HTML file as the single source of truth

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HEADER_FILE="$SCRIPT_DIR/include/wifi_setup_page.h"
HTML_FILE="$SCRIPT_DIR/wifi_setup_page_dev.html"

if [ ! -f "$HTML_FILE" ]; then
    echo "Error: $HTML_FILE not found"
    exit 1
fi

echo "Extracting HTML from $HTML_FILE..."

# Extract HTML content (everything between <!DOCTYPE and </html>)
# Remove the mock API code section
HTML_CONTENT=$(awk '
    /<!DOCTYPE html>/ { start=1 }
    start && /\/\/ Mock API endpoints for local development/ { 
        # Skip until we find the closing of the mock section
        skip=1
    }
    skip && /<\/script>/ && !/\/\/ Auto-scan on load/ { 
        skip=0
        next
    }
    !skip && start { print }
    /<\/html>/ { exit }
' "$HTML_FILE")

# Create the header file
cat > "$HEADER_FILE" << 'HEADER_EOF'
#ifndef WIFI_SETUP_PAGE_H
#define WIFI_SETUP_PAGE_H

#include <Arduino.h>

/**
 * WiFi Setup Page HTML
 * 
 * Self-contained HTML page for WiFi network selection and configuration.
 * Stored in PROGMEM (flash) to avoid RAM/PSRAM usage.
 * 
 * This page is served at /setup route when the device is in AP mode.
 * 
 * To edit this file:
 * 1. Edit wifi_setup_page_dev.html (with mock APIs for local testing)
 * 2. Run: ./update_wifi_setup_header.sh
 * 3. This will update this header file from the dev HTML file
 */
const char WIFI_SETUP_PAGE_HTML[] PROGMEM = R"rawliteral(
HEADER_EOF

# Add the HTML content
echo "$HTML_CONTENT" >> "$HEADER_FILE"

# Close the raw literal and header
cat >> "$HEADER_FILE" << 'FOOTER_EOF'
)rawliteral";

#endif // WIFI_SETUP_PAGE_H
FOOTER_EOF

echo "✓ Updated $HEADER_FILE from $HTML_FILE"
echo ""
echo "Note: Make sure to remove the mock API code section from wifi_setup_page_dev.html"
echo "      before running this script (lines with 'Mock API endpoints' and fetch interception)"
