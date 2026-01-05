#!/usr/bin/env python3
"""
Update wifi_setup_page.h from wifi_setup_page_dev.html
This keeps the HTML file as the single source of truth.
"""

import os
import re
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent
HEADER_FILE = SCRIPT_DIR / "include" / "wifi_setup_page.h"
HTML_FILE = SCRIPT_DIR / "wifi_setup_page_dev.html"

if not HTML_FILE.exists():
    print(f"Error: {HTML_FILE} not found")
    exit(1)

print(f"Reading {HTML_FILE}...")
with open(HTML_FILE, 'r', encoding='utf-8') as f:
    html_content = f.read()

# Remove the mock API section (between the comment and the closing of that script section)
# Find the script tag and remove mock API code
script_pattern = r'(<script>.*?)(// Mock API endpoints for local development.*?)(// Intercept fetch calls.*?const originalFetch = window\.fetch;.*?return originalFetch\.apply\(this, arguments\);.*?)(\s+function showStatus)'
html_content = re.sub(script_pattern, r'\1\4', html_content, flags=re.DOTALL)

# Also remove the MOCK_NETWORKS constant if it exists separately
html_content = re.sub(r'const MOCK_NETWORKS = \[.*?\];\s*', '', html_content, flags=re.DOTALL)

# Create header content
header_content = '''#ifndef WIFI_SETUP_PAGE_H
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
 * 2. Run: python3 update_wifi_setup_header.py
 * 3. This will update this header file from the dev HTML file
 */
const char WIFI_SETUP_PAGE_HTML[] PROGMEM = R"rawliteral(
''' + html_content + '''
)rawliteral";

#endif // WIFI_SETUP_PAGE_H
'''

print(f"Writing {HEADER_FILE}...")
with open(HEADER_FILE, 'w', encoding='utf-8') as f:
    f.write(header_content)

print(f"✓ Updated {HEADER_FILE} from {HTML_FILE}")
print("\nNote: Verify that mock API code was properly removed from the header file")

