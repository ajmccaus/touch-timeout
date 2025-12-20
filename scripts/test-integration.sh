#!/bin/bash
# Integration tests - validates deployment infrastructure and CLI behavior
#
# Run from project root: ./scripts/test-integration.sh
#
# AUTOMATED TESTS:
#   - Script syntax validation
#   - Documentation exists
#   - Version consistency
#   - CLI argument handling
#
# MANUAL TESTS (require real RPi):
#   [ ] Deploy: make deploy-arm64 RPI=<IP>
#   [ ] Verify: ssh root@<IP> 'systemctl status touch-timeout'
#   [ ] Touch screen, verify wake from DIMMED/OFF states
#

set -e

# Change to project root
cd "$(dirname "$0")/.."

PASS=0
FAIL=0

pass() { echo "  OK: $1"; PASS=$((PASS + 1)); }
fail() { echo "  FAIL: $1"; FAIL=$((FAIL + 1)); }

echo ""
echo "=== Integration Tests ==="
echo ""

# ==================== DEPLOYMENT INFRASTRUCTURE ====================

echo "[1/5] Validating script syntax..."
if bash -n scripts/deploy.sh; then pass "deploy.sh"; else fail "deploy.sh syntax"; fi
if bash -n scripts/install.sh; then pass "install.sh"; else fail "install.sh syntax"; fi
if bash -n scripts/test-performance.sh; then pass "test-performance.sh"; else fail "test-performance.sh syntax"; fi

echo "[2/5] Checking documentation..."
if [ -f doc/INSTALLATION.md ]; then pass "INSTALLATION.md exists"; else fail "INSTALLATION.md missing"; fi
if [ -f README.md ]; then pass "README.md exists"; else fail "README.md missing"; fi

echo "[3/5] Validating version consistency..."
VERSION_MAJOR=$(grep "^VERSION_MAJOR" Makefile | awk -F'=' '{print $2}' | tr -d ' ')
VERSION_MINOR=$(grep "^VERSION_MINOR" Makefile | awk -F'=' '{print $2}' | tr -d ' ')
VERSION_PATCH=$(grep "^VERSION_PATCH" Makefile | awk -F'=' '{print $2}' | tr -d ' ')
VERSION="$VERSION_MAJOR.$VERSION_MINOR.$VERSION_PATCH"

if [[ "$VERSION_MAJOR" =~ ^[0-9]+$ ]] && \
   [[ "$VERSION_MINOR" =~ ^[0-9]+$ ]] && \
   [[ "$VERSION_PATCH" =~ ^[0-9]+$ ]]; then
    pass "Version $VERSION"
else
    fail "Invalid version format"
fi

# ==================== CLI INTEGRATION ====================

echo "[4/5] Building native binary..."
if make -s 2>/dev/null; then
    pass "Build succeeded"
    BINARY="build/touch-timeout-${VERSION}-native"
else
    fail "Build failed"
    BINARY=""
fi

echo "[5/5] Testing CLI behavior..."
if [ -n "$BINARY" ] && [ -x "$BINARY" ]; then
    # --version returns correct version
    if $BINARY --version 2>&1 | grep -q "$VERSION"; then
        pass "--version shows $VERSION"
    else
        fail "--version output incorrect"
    fi

    # --help exits successfully
    if $BINARY --help >/dev/null 2>&1; then
        pass "--help exits 0"
    else
        fail "--help failed"
    fi

    # Invalid brightness rejected
    if ! $BINARY --brightness=abc 2>&1 | grep -qi "invalid"; then
        fail "--brightness=abc should be rejected"
    else
        pass "--brightness=abc rejected"
    fi

    # Invalid device name rejected (path traversal)
    if ! $BINARY --input="../etc/passwd" 2>&1 | grep -qi "invalid"; then
        fail "--input=../etc/passwd should be rejected"
    else
        pass "--input=../etc/passwd rejected"
    fi
else
    fail "Binary not found or not executable"
fi

# ==================== SUMMARY ====================

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="

if [ $FAIL -gt 0 ]; then
    exit 1
fi
