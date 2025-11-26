#!/bin/bash
#
# install-on-rpi.sh - Install touch-timeout binary with versioning and symlink management
#
# This script runs on the RPi4 and handles:
# - Verification of binary and permissions
# - Installation with version and architecture suffix
# - Symlink management for zero-downtime updates
# - Service restart
#
# Optimized for SD card wear: Minimizes logging output and journal writes.
# Usage: sudo /tmp/touch-timeout-staging/install-on-rpi.sh
#

set -euo pipefail

# Set to "1" for verbose output (increases SD card writes)
QUIET_MODE="${QUIET_MODE:-1}"

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Paths
STAGING_DIR="/tmp/touch-timeout-staging"
INSTALL_DIR="/usr/bin"
SYSTEMD_LINK="/etc/systemd/system/touch-timeout.service"

# Helpers (conditional on QUIET_MODE to minimize SD writes)
log_info() {
    [[ $QUIET_MODE -eq 1 ]] && return
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    [[ $QUIET_MODE -eq 1 ]] && return
    echo -e "${GREEN}[OK]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"  # Always log errors
}

log_warn() {
    [[ $QUIET_MODE -eq 1 ]] && return
    echo -e "${YELLOW}[WARN]${NC} $1"
}

# Verify running as root
if [[ $EUID -ne 0 ]]; then
    log_error "This script must be run as root (use: sudo $0)"
    exit 1
fi

# Verify binary exists
if [[ ! -f "$STAGING_DIR/touch-timeout" ]]; then
    log_error "Binary not found: $STAGING_DIR/touch-timeout"
    exit 1
fi

# Detect binary architecture
FILE_OUTPUT=$(file "$STAGING_DIR/touch-timeout")
if echo "$FILE_OUTPUT" | grep -q "ARM aarch64"; then
    ARCH="arm64"
elif echo "$FILE_OUTPUT" | grep -q "ARM.*32-bit"; then
    ARCH="arm32"
elif echo "$FILE_OUTPUT" | grep -q "x86-64"; then
    ARCH="x86_64"
elif echo "$FILE_OUTPUT" | grep -q "Intel 80386"; then
    ARCH="x86_32"
else
    log_warn "Could not automatically detect architecture"
    ARCH="unknown"
fi

# Extract version (hardcoded for v2.0.0)
VERSION="2.0.0"

# Construct versioned binary name
VERSIONED_BINARY="${INSTALL_DIR}/touch-timeout-${VERSION}-${ARCH}"
CURRENT_LINK="${INSTALL_DIR}/touch-timeout"

log_info "Installing touch-timeout-${VERSION}-${ARCH}..."

# Stop service if running (minimize systemctl calls)
systemctl stop touch-timeout.service 2>/dev/null || true

# Install versioned binary (without verbose output to save SD writes)
install -m 755 "$STAGING_DIR/touch-timeout" "$VERSIONED_BINARY"

# Create or update symlink atomically
ln -sf "$VERSIONED_BINARY" "${CURRENT_LINK}.new"
mv -f "${CURRENT_LINK}.new" "$CURRENT_LINK"

# Verify binary is executable
if [[ ! -x "$CURRENT_LINK" ]]; then
    log_error "Binary is not executable after installation"
    exit 1
fi

# Reload systemd and start service
systemctl daemon-reload
if ! systemctl start touch-timeout.service; then
    log_error "Failed to start touch-timeout service"
    exit 1
fi

log_success "Installation complete: $(readlink "$CURRENT_LINK")"
log_info "View logs: journalctl -u touch-timeout.service -f"
log_info "Rollback: ln -sf /usr/bin/touch-timeout-<VERSION>-<ARCH> /usr/bin/touch-timeout && sudo systemctl restart touch-timeout.service"
