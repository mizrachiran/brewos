# WiFi Setup Page Development

## Single Source Workflow

The WiFi setup page HTML is maintained in **`src/esp32/wifi_setup_page_dev.html`** as the single source of truth.

### Workflow

1. **Edit locally**: Open `src/esp32/wifi_setup_page_dev.html` in your browser

   - The file includes mock API endpoints so you can test locally
   - Just open the file directly in your browser (no server needed)

2. **Test your changes**: The mock APIs simulate:

   - `/api/wifi/networks` - Returns sample networks
   - `/api/wifi/connect` - Simulates connection

3. **Sync to header**: You have three options:

   **Option A: Automatic Build Sync (Recommended for builds)**
   The HTML file is automatically synced and linted before every build via PlatformIO's pre-build script.
   Just build normally - no manual steps needed!

   ```bash
   pio run
   ```

   **Option B: Live Reload (Recommended for development)**

   ```bash
   cd src/esp32
   python3 watch_wifi_setup.py
   ```

   This monitors the HTML file and automatically syncs on every save. Perfect for active development!

   **Option C: Manual Sync**

   ```bash
   cd src/esp32
   python3 update_wifi_setup_header.py
   ```

   Or use the bash script:

   ```bash
   cd src/esp32
   ./update_wifi_setup_header.sh
   ```

   This updates `src/esp32/include/wifi_setup_page.h` from the dev HTML file.

### Important Notes

- **Automatic sync on build**: The HTML file is automatically synced and linted before every PlatformIO build
- **Works in CI/CD**: The sync runs automatically in GitHub Actions and other CI environments - no special configuration needed
- **Works locally**: The sync runs automatically when you run `pio run` locally
- **Remove mock code before syncing**: The script automatically removes the mock API code during sync
- The mock API section in the dev file is between `// Mock API endpoints` and `// Intercept fetch calls`
- After syncing, the header file will be used by the ESP32 firmware
- **Linting**: The build process runs linting checks that validate:
  - HTML structure and tag matching
  - Required JavaScript functions
  - API endpoint references
  - Common issues that could break the C++ raw string
- **Error handling**: Lint errors show warnings but don't fail the build (graceful degradation)

### File Structure

- `src/esp32/wifi_setup_page_dev.html` - **Source file** (edit this)
- `src/esp32/include/wifi_setup_page.h` - **Generated file** (auto-updated by script)
- `src/esp32/scripts/pre_build.py` - **Build integration** (syncs & lints before build)
- `src/esp32/scripts/lint_wifi_setup.py` - **Linter** (checks HTML for issues)
- `src/esp32/watch_wifi_setup.py` - **Live reload watcher** (monitors and auto-syncs)
- `src/esp32/update_wifi_setup_header.py` - Sync script (Python, manual)
- `src/esp32/update_wifi_setup_header.sh` - Sync script (Bash, manual)

### Quick Start

```bash
# 1. Start the live reload watcher (in one terminal)
cd src/esp32
python3 watch_wifi_setup.py

# 2. Open the dev file in your browser (in another window)
open src/esp32/wifi_setup_page_dev.html

# 3. Make your changes and save - the header file auto-updates!
#    Watch the terminal for sync confirmations

# 4. Build and flash firmware when ready
```

**Note**: The watcher uses the `watchdog` library for efficient file monitoring. If not installed, it falls back to polling mode. Install with:

```bash
pip install watchdog
```
