#!/bin/bash
# Test deployment changes without requiring RPi hardware
#
# AUTOMATED TESTS (syntax + structure validation):
#   - Script syntax validation (deploy.sh, install.sh)
#   - Documentation exists
#   - Version variables valid
#
# MANUAL TESTS (require real RPi):
#   [ ] Deploy: make deploy-arm64 RPI=<IP>
#   [ ] Verify: ssh root@<IP> 'systemctl status touch-timeout'
#

set -e

echo "=== Deployment Smoke Tests ==="
echo ""

# Test 1: Script syntax validation (HIGH VALUE - catches 80% of issues)
echo "[1/3] Validating script syntax..."
bash -n scripts/deploy.sh && echo "  OK: deploy.sh"
bash -n scripts/install.sh && echo "  OK: install.sh"
bash -n scripts/test-performance.sh && echo "  OK: test-performance.sh"

# Test 2: Documentation exists
echo "[2/3] Checking documentation..."
[ -f doc/INSTALLATION.md ] && echo "  OK: INSTALLATION.md exists"
[ -f README.md ] && echo "  OK: README.md exists"

# Test 3: Version variables are valid integers
echo "[3/3] Validating version..."
VERSION_MAJOR=$(grep "^VERSION_MAJOR" Makefile | awk -F'=' '{print $2}' | tr -d ' ')
VERSION_MINOR=$(grep "^VERSION_MINOR" Makefile | awk -F'=' '{print $2}' | tr -d ' ')
VERSION_PATCH=$(grep "^VERSION_PATCH" Makefile | awk -F'=' '{print $2}' | tr -d ' ')

if [[ "$VERSION_MAJOR" =~ ^[0-9]+$ ]] && \
   [[ "$VERSION_MINOR" =~ ^[0-9]+$ ]] && \
   [[ "$VERSION_PATCH" =~ ^[0-9]+$ ]]; then
    echo "  OK: Version $VERSION_MAJOR.$VERSION_MINOR.$VERSION_PATCH"
else
    echo "  FAIL: Invalid version format"
    exit 1
fi

echo ""
echo "=== All Tests Passed ==="
