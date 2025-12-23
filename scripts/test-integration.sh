#!/bin/bash
# Integration tests - validates deployment infrastructure and CLI behavior
#
# USAGE:
#   ./scripts/test-integration.sh                    - Automated tests only
#   RPI=<ip> ./scripts/test-integration.sh           - Include on-device tests
#   RPI=<ip> RPI_USER=pi ./scripts/test-integration.sh  - Non-root user
#
# ENVIRONMENT VARIABLES:
#   RPI=<ip>        - RPi IP address (enables on-device tests)
#   RPI_USER=<user> - SSH user (default: root)
#
# AUTOMATED TESTS:
#   - Script syntax validation
#   - Documentation exists
#   - Version consistency
#   - CLI argument handling
#   - State transitions (if RPI set): FULL->DIMMED->OFF via timeout
#   - SIGUSR1 wake (if RPI set): OFF->FULL via signal
#
# MANUAL TESTS (always required):
#   - Touch wake: ssh $RPI_USER@$RPI '/run/.../touch-timeout-*-arm* -t 10 -v'
#                 Wait for OFF, touch screen, verify Touch->FULL in logs
#   - Performance: ssh $RPI_USER@$RPI 'bash /run/.../test-performance.sh'
#                  Targets: CPU ~0%, MEM <0.5MB, SD writes = 0, FD delta = 0
#   - Install: ssh $RPI_USER@$RPI '/run/touch-timeout-staging/install.sh'
#

set -e

# Configuration (matches Makefile/deploy.sh pattern)
RPI_USER="${RPI_USER:-root}"

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

# ==================== ON-DEVICE TESTS ====================

if [ -n "${RPI:-}" ]; then
    echo "[6/7] Testing state transitions on device..."

    # Start daemon with short timeout and capture verbose output
    LOGFILE=$(mktemp)
    ssh $RPI_USER@$RPI 'pkill -f touch-timeout 2>/dev/null || true'  # Clean slate
    ssh $RPI_USER@$RPI 'nohup /run/touch-timeout-staging/touch-timeout-*-arm* -t 10 -v > /tmp/tt.log 2>&1 &'
    sleep 1  # Let daemon start

    # Wait for DIMMED transition (~5s with -t 10)
    sleep 6
    if ssh $RPI_USER@$RPI 'grep -q "Timeout -> DIMMED" /tmp/tt.log 2>/dev/null'; then
        pass "FULL -> DIMMED transition"
    else
        fail "FULL -> DIMMED transition not detected"
    fi

    # Wait for OFF transition (~10s total)
    sleep 6
    if ssh $RPI_USER@$RPI 'grep -q "Timeout -> OFF" /tmp/tt.log 2>/dev/null'; then
        pass "DIMMED -> OFF transition"
    else
        fail "DIMMED -> OFF transition not detected"
    fi

    echo "[7/7] Testing SIGUSR1 wake signal..."

    # Send wake signal
    ssh $RPI_USER@$RPI 'pkill -USR1 -f touch-timeout'
    sleep 1

    if ssh $RPI_USER@$RPI 'grep -q "SIGUSR1 -> FULL" /tmp/tt.log 2>/dev/null'; then
        pass "SIGUSR1 -> FULL wake"
    else
        fail "SIGUSR1 -> FULL wake not detected"
    fi

    # Cleanup
    ssh $RPI_USER@$RPI 'pkill -f touch-timeout 2>/dev/null || true'
    ssh $RPI_USER@$RPI 'rm -f /tmp/tt.log'
    rm -f "$LOGFILE"
else
    echo "[6/7] Skipping on-device tests (RPI not set)"
    echo "[7/7] Skipping SIGUSR1 test (RPI not set)"
fi

# ==================== SUMMARY ====================

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="

if [ $FAIL -gt 0 ]; then
    exit 1
fi
