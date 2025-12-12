#!/bin/bash
# Test deployment changes without requiring RPi hardware
#
# AUTOMATED TESTS (run with this script):
#   [1/5] Script syntax validation
#   [2/5] Help text validation
#   [3/5] Documentation structure
#   [4/5] File reference updates
#   [5/5] Version consistency
#
# MANUAL TESTS (require real RPi):
#   [ ] Deploy with password (no SSH keys)
#   [ ] Deploy with --user pi flag
#   [ ] Deploy with --batch flag
#   [ ] Verify install.sh on RPi
#

set -e

echo "=== Deployment Testing Suite ==="
echo ""

# Test 1: Script syntax validation
echo "[1/5] Validating script syntax..."
if bash -n scripts/deploy-arm.sh 2>/dev/null; then
    echo "  ✓ deploy-arm.sh syntax valid"
else
    echo "  ✗ deploy-arm.sh has syntax errors"
    exit 1
fi

if bash -n scripts/install.sh 2>/dev/null; then
    echo "  ✓ install.sh syntax valid"
else
    echo "  ✗ install.sh has syntax errors"
    exit 1
fi

# Test 2: Help text works
echo "[2/5] Testing help output..."
if ./scripts/deploy-arm.sh 2>&1 | grep -q "Options:"; then
    echo "  ✓ Help text displays correctly"
else
    echo "  ✗ Help text missing 'Options:' section"
    exit 1
fi

# Test 3: Documentation structure
echo "[3/5] Checking documentation structure..."
if [ -f INSTALLATION.md ]; then
    echo "  ✓ INSTALLATION.md exists"
else
    echo "  ✗ INSTALLATION.md not found"
    exit 1
fi

if [ ! -f DEPLOYMENT.md ]; then
    echo "  ✓ DEPLOYMENT.md deleted (merged)"
else
    echo "  ⚠ DEPLOYMENT.md still exists (should be deleted)"
fi

if grep -q "Method 1" INSTALLATION.md && grep -q "Method 2" INSTALLATION.md; then
    echo "  ✓ INSTALLATION.md has Method 1 and Method 2 sections"
else
    echo "  ✗ INSTALLATION.md missing Method 1/2 structure"
    exit 1
fi

# Test 4: File references updated
echo "[4/5] Verifying file references..."
if grep -q "install.sh" scripts/deploy-arm.sh; then
    echo "  ✓ deploy-arm.sh references install.sh"
else
    echo "  ✗ deploy-arm.sh still references install-on-rpi.sh"
    exit 1
fi

if grep -q "install.sh" README.md; then
    echo "  ✓ README.md references install.sh"
else
    echo "  ⚠ README.md doesn't reference install.sh"
fi

if grep -q "/run/touch-timeout-staging" scripts/deploy-arm.sh; then
    echo "  ✓ deploy-arm.sh uses /run for staging"
else
    echo "  ✗ deploy-arm.sh still uses /tmp for staging"
    exit 1
fi

# Test 5: Version consistency
echo "[5/5] Checking version consistency..."
if grep -q "VERSION_MAJOR" Makefile && grep -q "VERSION_MINOR" Makefile && grep -q "VERSION_PATCH" Makefile; then
    echo "  ✓ Version variables present in Makefile"
else
    echo "  ✗ Version variables missing from Makefile"
    exit 1
fi

# Summary
echo ""
echo "=== All Automated Tests Passed ==="
echo ""
echo "Manual testing checklist:"
echo "  [ ] Deploy with password auth: ./scripts/deploy-arm.sh <IP> arm64"
echo "  [ ] Deploy with custom user: ./scripts/deploy-arm.sh <IP> arm64 --user pi"
echo "  [ ] Deploy with batch mode: ./scripts/deploy-arm.sh <IP> arm64 --batch"
echo "  [ ] Verify install on RPi: ssh <USER>@<IP> 'sudo /run/touch-timeout-staging/install.sh'"
echo "  [ ] Check service status: ssh <USER>@<IP> 'systemctl status touch-timeout'"
echo ""
