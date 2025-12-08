# BrewOS OTA (Over-The-Air) Updates

This document describes how firmware updates work in BrewOS, including the architecture, update process, and available commands.

## Overview

BrewOS runs on two microcontrollers:

- **ESP32-S3**: Handles WiFi, web server, cloud connection, and UI
- **Raspberry Pi Pico**: Handles real-time machine control, sensors, and safety

Both modules must run matching firmware versions to ensure protocol compatibility. The ESP32 orchestrates all OTA updates, including updates to the Pico.

## Update Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        GitHub Releases                          │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐  │
│  │ brewos_esp32.bin│  │brewos_dual_     │  │brewos_single_   │  │
│  │                 │  │boiler.uf2       │  │boiler.uf2       │  │
│  └────────┬────────┘  └────────┬────────┘  └────────┬────────┘  │
└───────────┼─────────────────────┼─────────────────────┼─────────┘
            │                     │                     │
            └──────────┬──────────┴──────────┬──────────┘
                       │                     │
                       ▼                     ▼
              ┌────────────────────────────────────┐
              │              ESP32                  │
              │  ┌──────────────────────────────┐  │
              │  │     OTA Update Manager       │  │
              │  │  - Download from GitHub      │  │
              │  │  - Flash ESP32 (self)        │  │
              │  │  - Stream to Pico via UART   │  │
              │  └──────────────────────────────┘  │
              └────────────────┬───────────────────┘
                               │ UART
                               ▼
              ┌────────────────────────────────────┐
              │              Pico                   │
              │  ┌──────────────────────────────┐  │
              │  │     Serial Bootloader        │  │
              │  │  - Receive firmware chunks   │  │
              │  │  - Verify CRC32              │  │
              │  │  - Write to flash            │  │
              │  └──────────────────────────────┘  │
              └────────────────────────────────────┘
```

## Update Channels

| Channel    | Source                           | Audience        | Tag Format      |
| ---------- | -------------------------------- | --------------- | --------------- |
| **Stable** | GitHub releases (non-prerelease) | All users       | `v1.0.0`        |
| **Beta**   | GitHub releases (prerelease)     | Testers         | `v1.1.0-beta.1` |
| **Dev**    | Rolling `dev-latest` tag         | Developers only | `dev-latest`    |

The update channel is selected in the web app's System Settings. Dev channel is only visible when Dev Mode is enabled.

## Cloud Environments

BrewOS Cloud provides remote access and monitoring. The ESP32 connects to a cloud server via WebSocket.

| Environment    | URL                       | Audience        | Purpose               |
| -------------- | ------------------------- | --------------- | --------------------- |
| **Production** | `wss://cloud.brewos.io`   | All users       | Main cloud service    |
| **Staging**    | `wss://staging.brewos.io` | Developers only | Testing cloud changes |
| **Custom**     | User-defined              | Self-hosters    | Private cloud server  |

### Why Environments Are Separate from Channels

Firmware channels and cloud environments are **independent settings**:

- **Firmware channels** determine which firmware version to install
- **Cloud environments** determine which cloud server to connect to

They are kept separate because:

1. **User accounts live in Production** - Users can't log into Staging (no account there)
2. **Testing flexibility** - Developers may need to test:
   - Dev firmware against Production cloud (test firmware changes)
   - Stable firmware against Staging cloud (test cloud changes)
   - Dev firmware against Staging cloud (test both together)
3. **Self-hosters** - Users running their own cloud server use Custom environment with any firmware channel

### Typical Configurations

| User Type    | Firmware Channel | Cloud Environment | Use Case                      |
| ------------ | ---------------- | ----------------- | ----------------------------- |
| Regular user | Stable           | Production        | Normal usage                  |
| Beta tester  | Beta             | Production        | Test new features             |
| Developer    | Dev              | Staging           | Test firmware + cloud changes |
| Developer    | Dev              | Production        | Test firmware only            |
| Self-hoster  | Stable           | Custom            | Private cloud server          |

### Enabling Staging Environment

Staging is only visible when **Dev Mode** is enabled:

1. Go to Settings → System
2. Tap the version number 7 times to enable Dev Mode
3. Go to Settings → Cloud
4. Staging environment option appears

> **Note:** Your user account and devices only exist in Production. Staging is a separate database for development testing.

## Firmware Assets

Each release includes firmware for all supported machine types:

| Asset Name                  | Target   | Description                                              |
| --------------------------- | -------- | -------------------------------------------------------- |
| `brewos_esp32.bin`          | ESP32-S3 | Main controller firmware                                 |
| `brewos_dual_boiler.uf2`    | Pico     | Dual boiler machines (ECM Synchronika, Profitec Pro 700) |
| `brewos_single_boiler.uf2`  | Pico     | Single boiler machines (Rancilio Silvia, Gaggia Classic) |
| `brewos_heat_exchanger.uf2` | Pico     | Heat exchanger machines                                  |

The ESP32 automatically selects the correct Pico firmware based on the machine type reported by the connected Pico.

## Combined Update Process

When a user triggers an update, BrewOS performs a **combined update** that updates both modules:

### Update Order: Pico First, Then ESP32

```
1. User triggers "ota_start" command with version
2. ESP32 checks machine type is known
3. Download Pico firmware from GitHub → Save to LittleFS
4. Send MSG_CMD_BOOTLOADER to Pico
5. Wait for bootloader ACK (0xAA 0x55)
6. Stream firmware to Pico in 200-byte chunks
7. Reset Pico → Wait for Pico to boot
8. Download ESP32 firmware from GitHub
9. Stream to ESP32 flash partition
10. Reboot ESP32
```

### Why Pico First?

1. **ESP32 orchestrates updates** - If ESP32 reboots first, it loses context to update Pico
2. **Version matching** - Ensures Pico is updated before ESP32 expects new protocol features
3. **Fail-safe** - If Pico update fails, ESP32 is unchanged and user can retry

## WebSocket Commands

### Check for Updates

```json
{ "type": "command", "command": "check_update" }
```

Response:

```json
{
  "type": "update_check_result",
  "updateAvailable": true,
  "combinedUpdateAvailable": true,
  "currentVersion": "0.4.4",
  "currentPicoVersion": "0.4.4",
  "latestVersion": "0.5.0",
  "releaseName": "BrewOS v0.5.0",
  "prerelease": false,
  "publishedAt": "2024-12-08T10:00:00Z",
  "esp32AssetSize": 1234567,
  "esp32AssetFound": true,
  "picoAssetSize": 567890,
  "picoAssetFound": true,
  "picoAssetName": "brewos_dual_boiler.uf2",
  "machineType": 1,
  "changelog": "## What's New\n- Feature X\n- Bug fix Y"
}
```

### Start Update (Combined - Recommended)

```json
{ "type": "command", "command": "ota_start", "version": "0.5.0" }
```

This triggers the combined update (Pico → ESP32).

### Advanced: Individual Updates

For recovery or testing purposes only:

```json
// ESP32 only
{ "type": "command", "command": "esp32_ota_start", "version": "0.5.0" }

// Pico only
{ "type": "command", "command": "pico_ota_start", "version": "0.5.0" }
```

## Progress Reporting

During updates, the ESP32 broadcasts progress via WebSocket:

```json
{
  "type": "ota_progress",
  "stage": "download",
  "progress": 35,
  "message": "Downloading..."
}
```

### Progress Stages

| Stage      | Progress Range | Description                |
| ---------- | -------------- | -------------------------- |
| `download` | 0-40%          | Downloading Pico firmware  |
| `flash`    | 40-60%         | Flashing Pico              |
| `download` | 65-95%         | Downloading ESP32 firmware |
| `flash`    | 95-98%         | Finalizing ESP32 flash     |
| `complete` | 100%           | Update complete, rebooting |
| `error`    | 0%             | Error occurred             |

### User-Facing Messages

Messages are intentionally non-technical:

| Internal Action           | User Message                          |
| ------------------------- | ------------------------------------- |
| Starting combined OTA     | "Starting BrewOS update to v0.5.0..." |
| Downloading Pico firmware | "Downloading..."                      |
| Flashing Pico             | "Installing..."                       |
| Completing ESP32          | "Completing update..."                |
| Success                   | "BrewOS updated successfully!"        |
| Error                     | "Update error: [reason]"              |

## Version Mismatch Detection

On Pico boot, the ESP32 compares versions:

```c
if (strcmp(picoVersion, ESP32_VERSION) != 0) {
    // Broadcast warning to clients
    broadcastLog("Firmware update recommended", "warning");
}
```

This prompts users to update without exposing internal architecture details.

## Error Handling

### Common Errors

| Error                 | Cause                                     | Resolution                   |
| --------------------- | ----------------------------------------- | ---------------------------- |
| "Device not ready"    | Machine type unknown (Pico not connected) | Ensure machine is powered on |
| "Update not found"    | Version doesn't exist on GitHub           | Check version number         |
| "Connection failed"   | No internet or GitHub unreachable         | Check WiFi connection        |
| "Installation failed" | Flash write error                         | Retry update                 |

### Recovery

If an update fails mid-process:

1. **Pico fails, ESP32 unchanged** → Retry combined update
2. **Pico succeeds, ESP32 fails** → Use `esp32_ota_start` to retry ESP32 only
3. **Pico bricked** → Use USB bootloader (hold BOOTSEL while powering on)

## Pico Serial Bootloader Protocol

The Pico uses a custom serial bootloader for OTA:

### Handshake

1. ESP32 sends `MSG_CMD_BOOTLOADER` command
2. Pico enters bootloader mode
3. Pico sends ACK: `0xAA 0x55`

### Firmware Transfer

1. ESP32 sends chunks (up to 256 bytes each)
2. Each chunk includes: magic bytes, chunk number, data, checksum
3. Pico ACKs each chunk

### Completion

1. ESP32 sends end marker: `0xAA 0x55` with chunk number `0xFFFFFFFF`
2. Pico verifies CRC32 of entire firmware
3. Pico writes to flash and reboots

## Security Considerations

1. **HTTPS only** - Firmware downloaded over HTTPS from GitHub
2. **CRC32 verification** - Pico verifies firmware integrity before flashing
3. **No downgrade protection** - Users can install any version (for recovery)
4. **Cloud commands** - OTA can be triggered remotely via cloud connection

## Release Process

### How Releases Are Created

Releases are created via GitHub Actions when a version tag is pushed:

```bash
# Create a stable release
node src/scripts/version.js --set 1.0.0 --release

# Create a beta release
node src/scripts/version.js --set 1.1.0-beta.1 --release
```

The version script:

1. Updates version in all source files
2. Commits and tags the release
3. Pushes to GitHub

GitHub Actions then:

1. Builds ESP32 firmware (`brewos_esp32.bin`)
2. Builds Pico firmware for all machine types (`.uf2` files)
3. Creates a GitHub Release with all assets

### Dev Channel Automation

The `dev-latest` release is automatically updated on every push to `main` that touches:

- `src/esp32/`
- `src/pico/`
- `src/web/`
- `src/shared/`

This provides developers with always-current builds without manual releases.

### Release Assets Structure

```
GitHub Release (v1.0.0)
├── brewos_esp32.bin           # ESP32 firmware
├── brewos_dual_boiler.uf2     # Pico - dual boiler
├── brewos_single_boiler.uf2   # Pico - single boiler
├── brewos_heat_exchanger.uf2  # Pico - heat exchanger
└── Source code (zip/tar.gz)   # Auto-generated by GitHub
```

## Related Documentation

- [RELEASING.md](../RELEASING.md) - Release management and versioning
- [Communication Protocol](../shared/Communication_Protocol.md) - ESP32 ↔ Pico protocol
- [WebSocket Protocol](../web/WebSocket_Protocol.md) - Web app communication
