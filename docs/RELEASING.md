# BrewOS Release Management

This guide explains how to manage versions and releases for BrewOS.

## Version Scheme

BrewOS uses [Semantic Versioning](https://semver.org/):

```
MAJOR.MINOR.PATCH[-PRERELEASE]

Examples:
- 1.0.0           → Stable release
- 1.1.0-beta.1    → First beta of v1.1.0
- 1.1.0-beta.2    → Second beta
- 1.1.0-rc.1      → Release candidate
- 1.1.0           → Stable release
```

## Release Channels

### Beta Channel
- Contains pre-release versions (`-beta.X`, `-rc.X`)
- For testers and early adopters
- Built from `main` branch
- Tagged with `v1.0.0-beta.1` format

### Stable Channel
- Contains only stable releases (no pre-release suffix)
- For regular users
- Tagged with `v1.0.0` format
- Created from tested beta versions

## Creating Releases

### Using the Release Script

```bash
# Show current version
./scripts/release.sh current

# Calculate next version
./scripts/release.sh bump patch    # 1.0.0 → 1.0.1
./scripts/release.sh bump minor    # 1.0.0 → 1.1.0
./scripts/release.sh bump major    # 1.0.0 → 2.0.0

# Create beta release
./scripts/release.sh beta 1.1.0-beta.1

# Create stable release
./scripts/release.sh stable 1.1.0

# Push to GitHub
git push origin main --tags
```

### Manual Release Process

1. **Update version files:**
   - `version.json` (root)
   - `src/esp32/include/config.h`
   - `src/web/package.json`
   - `src/cloud/package.json`

2. **Commit and tag:**
   ```bash
   git add -A
   git commit -m "chore: release 1.0.0-beta.1"
   git tag -a v1.0.0-beta.1 -m "Pre-release 1.0.0-beta.1"
   git push origin main --tags
   ```

3. **Create GitHub Release:**
   - Go to: https://github.com/YOUR_REPO/releases/new
   - Select the tag
   - For beta: ✓ Check "Set as a pre-release"
   - Upload firmware binaries
   - Publish

## Typical Release Workflow

### 1. Development Phase
```bash
# Work on features in main branch
git checkout main
git pull origin main
# ... develop features ...
git commit -m "feat: new feature"
git push origin main
```

### 2. Beta Release
```bash
# When ready for testing
./scripts/release.sh beta 1.1.0-beta.1
git push origin main --tags
```

### 3. Bug Fixes
```bash
# Fix bugs found in beta
git commit -m "fix: bug fix"
./scripts/release.sh beta 1.1.0-beta.2
git push origin main --tags
```

### 4. Stable Release
```bash
# When beta is stable
./scripts/release.sh stable 1.1.0
git push origin main --tags
```

## OTA Update Channels

Users can choose their update channel in Settings → System → Update Channel:

- **Stable** (default): Only receives stable releases
- **Beta**: Receives all releases including pre-releases

### How It Works

1. Device checks for updates at configured URL
2. Server returns available versions based on device's channel:
   - Stable channel → Only versions without pre-release suffix
   - Beta channel → All versions
3. Device compares version and shows update notification

### Update URL Format

```
GET /api/updates/check?current=1.0.0&channel=beta

Response:
{
  "available": true,
  "version": "1.1.0-beta.2",
  "channel": "beta",
  "releaseNotes": "...",
  "downloadUrl": "..."
}
```

## Files Modified During Release

| File | Contains |
|------|----------|
| `version.json` | Central version info for all components |
| `src/esp32/include/config.h` | ESP32 firmware version |
| `src/web/package.json` | Web app version |
| `src/cloud/package.json` | Cloud service version |

## GitHub Actions (CI/CD)

The CI pipeline automatically:

1. **On push to main:**
   - Runs tests
   - Builds all components

2. **On tag push (`v*`):**
   - Builds release artifacts
   - Creates GitHub Release (draft)
   - Uploads firmware binaries

### Release Artifacts

- `brewos-esp32-vX.X.X.bin` - ESP32 firmware
- `brewos-pico-vX.X.X.uf2` - Pico firmware  
- `brewos-web-vX.X.X.zip` - Web app bundle

## Version in Code

### ESP32 (C++)
```cpp
#include "config.h"

// Access version
LOG_I("Version: %s", ESP32_VERSION);

// Check if beta
#if defined(ESP32_VERSION_PRERELEASE) && ESP32_VERSION_PRERELEASE[0] != '\0'
  LOG_I("Running beta version");
#endif
```

### Web (TypeScript)
```typescript
// Import from package.json or fetch from API
const version = import.meta.env.VITE_APP_VERSION;
```

### Cloud (Node.js)
```typescript
import pkg from '../package.json';
console.log(`Version: ${pkg.version}`);
```

## Best Practices

1. **Always test beta before stable release**
2. **Write clear release notes** for each version
3. **Never modify a published release** - create a new version instead
4. **Keep version numbers synchronized** across all components
5. **Use the release script** to avoid manual errors

