# Version Management

## Overview

ECM firmware uses **semantic versioning** (MAJOR.MINOR.PATCH) with a centralized version management system. All version information is stored in a single `VERSION` file at the project root, and automatically synchronized to all firmware projects.

## Version Components

### Firmware Version (MAJOR.MINOR.PATCH)

Tracks the overall firmware release version. Both Pico and ESP32 firmware share the same version number.

- **MAJOR**: Breaking changes
  - Protocol version changes
  - Incompatible API changes
  - Requires coordinated update of both devices
  
- **MINOR**: New features (backward compatible)
  - New message types
  - New features that don't break existing functionality
  - Devices can be updated independently
  
- **PATCH**: Bug fixes (backward compatible)
  - Bug fixes
  - Performance improvements
  - Devices can be updated independently

### Protocol Version

Tracked separately in `src/shared/protocol_defs.h` as `ECM_PROTOCOL_VERSION`. Incremented only for breaking protocol changes.

**Relationship:**
- Protocol version changes → Firmware MAJOR version should also increment
- Protocol version stays same → Firmware can increment MINOR or PATCH

## Version File

The `VERSION` file at the project root is the single source of truth:

```
FIRMWARE_VERSION=0.1.0
PROTOCOL_VERSION=1
```

## Version Management Script

Use `scripts/version.py` to manage versions:

### Show Current Version

```bash
python scripts/version.py
# or
python scripts/version.py --show
```

Output:
```
Firmware Version: 0.1.0
Protocol Version: 1

Current version definitions:
  Pico:     0.1.0
  ESP32:    0.1.0
  Protocol: 1
```

### Bump Version

```bash
# Bump patch version (0.1.0 → 0.1.1)
python scripts/version.py --bump patch

# Bump minor version (0.1.0 → 0.2.0)
python scripts/version.py --bump minor

# Bump major version (0.1.0 → 1.0.0)
python scripts/version.py --bump major
```

### Set Specific Version

```bash
python scripts/version.py --set 1.2.3
```

### Update Protocol Version

```bash
python scripts/version.py --protocol 2
```

### Combined Operations

```bash
# Bump minor version and update protocol
python scripts/version.py --bump minor --protocol 2
```

## Automatic Version Injection

The version script automatically updates:

1. **Pico**: `src/pico/include/config.h`
   - `FIRMWARE_VERSION_MAJOR`
   - `FIRMWARE_VERSION_MINOR`
   - `FIRMWARE_VERSION_PATCH`

2. **ESP32**: `src/esp32/include/config.h`
   - `ESP32_VERSION_MAJOR`
   - `ESP32_VERSION_MINOR`
   - `ESP32_VERSION_PATCH`
   - `ESP32_VERSION` (string)

3. **Protocol**: `src/shared/protocol_defs.h`
   - `ECM_PROTOCOL_VERSION`

## GitHub Workflows

### Release Workflow

The `.github/workflows/release.yml` workflow automatically:

1. **Triggers on version tags**: `v1.2.3`
2. **Extracts version** from tag
3. **Updates all version files** using the version script
4. **Builds both firmwares** (Pico and ESP32)
5. **Creates GitHub release** with firmware artifacts

### Creating a Release

#### Method 1: Tag-based Release (Recommended)

```bash
# 1. Update version
python scripts/version.py --bump minor

# 2. Commit version changes
git add VERSION src/pico/include/config.h src/esp32/include/config.h src/shared/protocol_defs.h
git commit -m "Bump version to 0.2.0"

# 3. Create and push tag
git tag -a v0.2.0 -m "Release v0.2.0"
git push origin v0.2.0
```

The GitHub workflow will automatically:
- Build both firmwares
- Create a release with artifacts
- Attach firmware files to the release

#### Method 2: Manual Workflow Dispatch

1. Go to GitHub Actions → "Build and Release" → "Run workflow"
2. Enter version (e.g., `1.2.3`)
3. Workflow builds and uploads artifacts

### Artifacts

Each release includes:
- `ecm_pico.uf2` - Pico firmware (USB flash format)
- `ecm_pico.hex` - Pico firmware (alternative format)
- `firmware.bin` - ESP32 firmware (OTA format)

## Versioning Workflow

### For Bug Fixes (PATCH)

```bash
# 1. Fix bug, commit
git commit -m "Fix temperature reading issue"

# 2. Bump patch version
python scripts/version.py --bump patch

# 3. Commit version change
git add VERSION src/*/include/config.h src/shared/protocol_defs.h
git commit -m "Bump version to 0.1.1"

# 4. Tag and push
git tag v0.1.1 -m "Release v0.1.1"
git push origin main --tags
```

### For New Features (MINOR)

```bash
# 1. Add feature, commit
git commit -m "Add pre-infusion support"

# 2. Bump minor version
python scripts/version.py --bump minor

# 3. Commit and tag
git add VERSION src/*/include/config.h
git commit -m "Bump version to 0.2.0"
git tag v0.2.0 -m "Release v0.2.0"
git push origin main --tags
```

### For Breaking Changes (MAJOR)

```bash
# 1. Make breaking changes, update protocol version
python scripts/version.py --protocol 2

# 2. Bump major version
python scripts/version.py --bump major

# 3. Commit and tag
git add VERSION src/*/include/config.h src/shared/protocol_defs.h
git commit -m "Breaking: Protocol v2, bump to v1.0.0"
git tag v1.0.0 -m "Release v1.0.0 - Protocol v2"
git push origin main --tags
```

## Best Practices

1. **Always use the version script** - Don't manually edit version files
2. **Commit version changes** with the feature/bug fix
3. **Tag releases** using semantic versioning (`vMAJOR.MINOR.PATCH`)
4. **Update protocol version** when making breaking protocol changes
5. **Test builds** after version changes
6. **Document breaking changes** in release notes

## Version in Firmware

Firmware versions are accessible at runtime:

### Pico

```c
// In MSG_BOOT payload
boot_payload_t boot = {
    .version_major = FIRMWARE_VERSION_MAJOR,
    .version_minor = FIRMWARE_VERSION_MINOR,
    .version_patch = FIRMWARE_VERSION_PATCH,
    // ...
};
```

### ESP32

```cpp
// In web UI and logs
LOG_I("ECM Controller ESP32-S3 v%s", ESP32_VERSION);
```

## Troubleshooting

### Version Mismatch

If versions get out of sync:

```bash
# Read current version from VERSION file
python scripts/version.py --show

# Force update all files
python scripts/version.py --set $(python scripts/version.py --show | grep "Firmware Version" | cut -d: -f2 | xargs)
```

### Missing VERSION File

If `VERSION` file is missing, the script will create a default:

```bash
python scripts/version.py --show
# Creates VERSION with 0.1.0 / 1
```

