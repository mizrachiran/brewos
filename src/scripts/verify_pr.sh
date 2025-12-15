#!/bin/bash
#
# BrewOS Pull Request Verification Script
#
# This script mirrors the CI workflow (ci.yml) and runs all checks required 
# before merging a PR:
#
# 1. Lint Web UI
# 2. Type check Web UI
# 3. Lint Cloud Service
# 4. Type check Cloud Service
# 5. Lint Admin UI
# 6. Type check Admin UI
# 7. Build Web UI
# 8. Build Firmware (ESP32 + Pico)
# 9. Build Cloud Service + Admin UI
# 10. Run Pico Unit Tests
#
# Usage:
#   ./verify_pr.sh [--skip-firmware] [--skip-tests] [--fast]
#
# Options:
#   --skip-firmware  Skip firmware builds (ESP32 + Pico)
#   --skip-tests     Skip running unit tests
#   --fast           Skip firmware builds and tests (lint + build only)
#   --help, -h       Show this help message
#

set -e  # Exit on first error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Timing
START_TIME=$(date +%s)
STEP_START_TIME=$START_TIME

# Parse arguments
SKIP_FIRMWARE=false
SKIP_TESTS=false

for arg in "$@"; do
    case $arg in
        --skip-firmware)
            SKIP_FIRMWARE=true
            shift
            ;;
        --skip-tests)
            SKIP_TESTS=true
            shift
            ;;
        --fast)
            SKIP_FIRMWARE=true
            SKIP_TESTS=true
            shift
            ;;
        --help|-h)
            head -n 15 "$0" | tail -n +2
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $arg${NC}"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Functions
print_header() {
    echo ""
    echo -e "${CYAN}╔════════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║${NC} $1"
    echo -e "${CYAN}╚════════════════════════════════════════════════════════════════════╝${NC}"
}

print_step() {
    STEP_START_TIME=$(date +%s)
    echo ""
    echo -e "${BLUE}▶ $1${NC}"
}

print_success() {
    local duration=$(($(date +%s) - STEP_START_TIME))
    echo -e "${GREEN}✓ $1${NC} ${YELLOW}(${duration}s)${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

print_info() {
    echo -e "${MAGENTA}→ $1${NC}"
}

# Error handler
on_error() {
    echo ""
    print_error "Verification failed at: ${BASH_COMMAND}"
    echo ""
    print_info "To fix:"
    print_info "  1. Review the error message above"
    print_info "  2. Make necessary changes"
    print_info "  3. Run this script again"
    echo ""
    exit 1
}

trap on_error ERR

# ============================================================================
# Main Verification Steps
# ============================================================================

print_header "BrewOS Pull Request Verification"
echo ""
echo -e "  ${CYAN}Repository:${NC} $PROJECT_ROOT"
echo -e "  ${CYAN}Mode:${NC}       $([ "$SKIP_FIRMWARE" = true ] && echo "Fast (no firmware)" || echo "Full verification")"
echo ""

# ----------------------------------------------------------------------------
# Step 1: Lint Web UI
# ----------------------------------------------------------------------------
print_step "1/10 Linting Web UI..."
cd "$PROJECT_ROOT/src/web"

if [ ! -d "node_modules" ]; then
    print_info "Installing web dependencies..."
    npm ci > /dev/null
fi

npm run lint
print_success "Web UI linting passed"

# ----------------------------------------------------------------------------
# Step 2: Type Check Web UI
# ----------------------------------------------------------------------------
print_step "2/10 Type checking Web UI..."
cd "$PROJECT_ROOT/src/web"
npx tsc --noEmit
print_success "Web UI type check passed"

# ----------------------------------------------------------------------------
# Step 3: Lint Cloud Service
# ----------------------------------------------------------------------------
print_step "3/10 Linting Cloud Service..."
cd "$PROJECT_ROOT/src/cloud"

if [ ! -d "node_modules" ]; then
    print_info "Installing cloud dependencies..."
    npm ci > /dev/null
fi

npm run lint
print_success "Cloud service linting passed"

# ----------------------------------------------------------------------------
# Step 4: Type Check Cloud Service
# ----------------------------------------------------------------------------
print_step "4/10 Type checking Cloud Service..."
cd "$PROJECT_ROOT/src/cloud"
npx tsc --noEmit
print_success "Cloud service type check passed"

# ----------------------------------------------------------------------------
# Step 5: Lint Admin UI
# ----------------------------------------------------------------------------
print_step "5/10 Linting Admin UI..."
cd "$PROJECT_ROOT/src/cloud/admin"

if [ ! -d "node_modules" ]; then
    print_info "Installing admin dependencies..."
    npm ci > /dev/null
fi

# Admin doesn't have lint script yet, just do type check
print_success "Admin UI linting passed (no lint rules)"

# ----------------------------------------------------------------------------
# Step 6: Type Check Admin UI
# ----------------------------------------------------------------------------
print_step "6/10 Type checking Admin UI..."
cd "$PROJECT_ROOT/src/cloud/admin"
npx tsc --noEmit
print_success "Admin UI type check passed"

# ----------------------------------------------------------------------------
# Step 7: Build Web UI
# ----------------------------------------------------------------------------
print_step "7/10 Building Web UI (ESP32 + Cloud)..."
cd "$SCRIPT_DIR"
./build_app.sh all > /dev/null
print_success "Web UI built successfully"

# ----------------------------------------------------------------------------
# Step 8: Build Firmware (optional)
# ----------------------------------------------------------------------------
if [ "$SKIP_FIRMWARE" = false ]; then
    print_step "8/10 Building Firmware (ESP32 + Pico)..."
    cd "$SCRIPT_DIR"
    # Run full build and let errors propagate (set -e will catch them)
    ./build_firmware.sh all
    print_success "Firmware built successfully"
else
    print_step "8/10 Building Firmware (ESP32 + Pico)..."
    print_warning "Skipped (--skip-firmware or --fast)"
fi

# ----------------------------------------------------------------------------
# Step 9: Build Cloud Service + Admin UI
# ----------------------------------------------------------------------------
print_step "9/10 Building Cloud Service + Admin UI..."
cd "$PROJECT_ROOT/src/cloud"
npm run build > /dev/null
cd "$PROJECT_ROOT/src/cloud/admin"
npm run build > /dev/null
print_success "Cloud service and Admin UI built successfully"

# ----------------------------------------------------------------------------
# Step 10: Run Unit Tests (optional)
# ----------------------------------------------------------------------------
if [ "$SKIP_TESTS" = false ]; then
    print_step "10/10 Running Pico Unit Tests..."
    cd "$SCRIPT_DIR"
    # Run tests and let errors propagate (set -e will catch them)
    ./run_pico_tests.sh
    print_success "All unit tests passed"
else
    print_step "10/10 Running Pico Unit Tests..."
    print_warning "Skipped (--skip-tests or --fast)"
fi

# ============================================================================
# Summary
# ============================================================================

TOTAL_DURATION=$(($(date +%s) - START_TIME))
MINUTES=$((TOTAL_DURATION / 60))
SECONDS=$((TOTAL_DURATION % 60))

echo ""
print_header "✅ Pull Request Verification Complete"
echo ""
echo -e "  ${GREEN}All checks passed!${NC}"
echo -e "  ${CYAN}Total time:${NC} ${MINUTES}m ${SECONDS}s"
echo ""
echo -e "  ${MAGENTA}Ready to create/merge pull request${NC}"
echo ""

# Optional suggestions based on mode
if [ "$SKIP_FIRMWARE" = true ]; then
    echo -e "  ${YELLOW}Note:${NC} Firmware builds were skipped. Run without --fast for full verification."
    echo ""
fi

exit 0

