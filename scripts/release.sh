#!/bin/bash

# BrewOS Release Management Script
# Usage:
#   ./scripts/release.sh beta 1.0.0-beta.2    # Create beta release
#   ./scripts/release.sh stable 1.0.0         # Create stable release
#   ./scripts/release.sh bump patch           # Bump patch version
#   ./scripts/release.sh bump minor           # Bump minor version
#   ./scripts/release.sh bump major           # Bump major version

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
VERSION_FILE="$ROOT_DIR/version.json"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get current version
get_current_version() {
    if command -v jq &> /dev/null; then
        jq -r '.version' "$VERSION_FILE"
    else
        grep '"version"' "$VERSION_FILE" | cut -d'"' -f4
    fi
}

# Update version in all files
update_version() {
    local new_version=$1
    local channel=$2
    local build_date=$(date +%Y-%m-%d)
    
    echo -e "${YELLOW}Updating version to $new_version (channel: $channel)${NC}"
    
    # Update version.json
    if command -v jq &> /dev/null; then
        jq --arg v "$new_version" --arg c "$channel" --arg d "$build_date" \
           '.version = $v | .channel = $c | .buildDate = $d | .components.esp32 = $v | .components.web = $v | .components.cloud = $v' \
           "$VERSION_FILE" > "$VERSION_FILE.tmp" && mv "$VERSION_FILE.tmp" "$VERSION_FILE"
    else
        # Fallback: use sed
        sed -i.bak "s/\"version\": \"[^\"]*\"/\"version\": \"$new_version\"/" "$VERSION_FILE"
        sed -i.bak "s/\"channel\": \"[^\"]*\"/\"channel\": \"$channel\"/" "$VERSION_FILE"
        sed -i.bak "s/\"buildDate\": \"[^\"]*\"/\"buildDate\": \"$build_date\"/" "$VERSION_FILE"
        rm -f "$VERSION_FILE.bak"
    fi
    
    # Update ESP32 platformio.ini
    if [ -f "$ROOT_DIR/src/esp32/platformio.ini" ]; then
        sed -i.bak "s/version = .*/version = $new_version/" "$ROOT_DIR/src/esp32/platformio.ini" 2>/dev/null || true
        rm -f "$ROOT_DIR/src/esp32/platformio.ini.bak"
    fi
    
    # Update web package.json
    if [ -f "$ROOT_DIR/src/web/package.json" ]; then
        if command -v jq &> /dev/null; then
            jq --arg v "$new_version" '.version = $v' "$ROOT_DIR/src/web/package.json" > "$ROOT_DIR/src/web/package.json.tmp"
            mv "$ROOT_DIR/src/web/package.json.tmp" "$ROOT_DIR/src/web/package.json"
        fi
    fi
    
    # Update cloud package.json
    if [ -f "$ROOT_DIR/src/cloud/package.json" ]; then
        if command -v jq &> /dev/null; then
            jq --arg v "$new_version" '.version = $v' "$ROOT_DIR/src/cloud/package.json" > "$ROOT_DIR/src/cloud/package.json.tmp"
            mv "$ROOT_DIR/src/cloud/package.json.tmp" "$ROOT_DIR/src/cloud/package.json"
        fi
    fi
    
    echo -e "${GREEN}✓ Version updated to $new_version${NC}"
}

# Create git tag and push
create_release() {
    local version=$1
    local channel=$2
    local tag="v$version"
    
    echo -e "${YELLOW}Creating release $tag...${NC}"
    
    # Stage version changes
    git add -A
    
    # Commit if there are changes
    if ! git diff --cached --quiet; then
        git commit -m "chore: release $version"
    fi
    
    # Create annotated tag
    if [ "$channel" = "stable" ]; then
        git tag -a "$tag" -m "Release $version"
    else
        git tag -a "$tag" -m "Pre-release $version"
    fi
    
    echo -e "${GREEN}✓ Created tag $tag${NC}"
    echo ""
    echo -e "To publish this release:"
    echo -e "  ${YELLOW}git push origin main --tags${NC}"
    echo ""
    echo -e "Then create a GitHub Release:"
    echo -e "  - Go to: https://github.com/YOUR_REPO/releases/new"
    echo -e "  - Select tag: $tag"
    if [ "$channel" != "stable" ]; then
        echo -e "  - ${YELLOW}Check 'Set as a pre-release'${NC}"
    fi
}

# Parse semantic version
parse_version() {
    local version=$1
    # Remove leading 'v' if present
    version="${version#v}"
    # Remove pre-release suffix for parsing
    local base_version="${version%%-*}"
    
    IFS='.' read -r major minor patch <<< "$base_version"
    echo "$major $minor $patch"
}

# Bump version
bump_version() {
    local bump_type=$1
    local current=$(get_current_version)
    local base_version="${current%%-*}"
    
    read major minor patch <<< $(parse_version "$base_version")
    
    case $bump_type in
        major)
            major=$((major + 1))
            minor=0
            patch=0
            ;;
        minor)
            minor=$((minor + 1))
            patch=0
            ;;
        patch)
            patch=$((patch + 1))
            ;;
        *)
            echo -e "${RED}Invalid bump type: $bump_type${NC}"
            echo "Use: major, minor, or patch"
            exit 1
            ;;
    esac
    
    echo "$major.$minor.$patch"
}

# Main
case $1 in
    beta)
        if [ -z "$2" ]; then
            echo "Usage: $0 beta <version>"
            echo "Example: $0 beta 1.0.0-beta.1"
            exit 1
        fi
        update_version "$2" "beta"
        create_release "$2" "beta"
        ;;
    
    stable)
        if [ -z "$2" ]; then
            echo "Usage: $0 stable <version>"
            echo "Example: $0 stable 1.0.0"
            exit 1
        fi
        update_version "$2" "stable"
        create_release "$2" "stable"
        ;;
    
    bump)
        if [ -z "$2" ]; then
            echo "Usage: $0 bump <major|minor|patch>"
            exit 1
        fi
        new_version=$(bump_version "$2")
        echo "New version would be: $new_version"
        echo "Run: $0 beta ${new_version}-beta.1  OR  $0 stable $new_version"
        ;;
    
    current)
        echo "Current version: $(get_current_version)"
        ;;
    
    *)
        echo "BrewOS Release Management"
        echo ""
        echo "Usage:"
        echo "  $0 beta <version>      Create beta release (e.g., 1.0.0-beta.1)"
        echo "  $0 stable <version>    Create stable release (e.g., 1.0.0)"
        echo "  $0 bump <type>         Calculate next version (major/minor/patch)"
        echo "  $0 current             Show current version"
        echo ""
        echo "Current version: $(get_current_version)"
        ;;
esac

