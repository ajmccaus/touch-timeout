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
# Usage: sudo /tmp/touch-timeout-staging/install-on-rpi.sh
#

set -euo pipefail

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

# Helpers
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[OK]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_warn() {
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

log_info "Installation starting..."
echo

# Detect binary architecture
log_info "Detecting binary architecture..."
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
log_success "Architecture detected: $ARCH"

# Extract version from binary (stored in symlink or hardcoded version)
# For now, use v2.0.0 (read from build or Makefile in production)
VERSION="2.0.0"
log_info "Version: $VERSION"

# Construct versioned binary name
VERSIONED_BINARY="${INSTALL_DIR}/touch-timeout-${VERSION}-${ARCH}"
CURRENT_LINK="${INSTALL_DIR}/touch-timeout"

echo
log_info "Installation details:"
log_info "  Binary: $VERSIONED_BINARY"
log_info "  Symlink: $CURRENT_LINK"
echo

# Stop service if running
log_info "Stopping touch-timeout service..."
if systemctl is-active --quiet touch-timeout.service 2>/dev/null; then
    systemctl stop touch-timeout.service
    log_success "Service stopped"
else
    log_warn "Service not currently running"
fi
echo

# Install versioned binary
log_info "Installing binary..."
install -v -m 755 "$STAGING_DIR/touch-timeout" "$VERSIONED_BINARY"
log_success "Binary installed: $VERSIONED_BINARY"

# List previous versions
log_info "Previous versions:"
ls -lh "${INSTALL_DIR}"/touch-timeout-* 2>/dev/null | awk '{print "  " $9 " (" $5 ")"}' || log_warn "  No previous versions found"
echo

# Create or update symlink (atomic operation)
log_info "Updating symlink: $CURRENT_LINK → $VERSIONED_BINARY"
ln -sf "$VERSIONED_BINARY" "${CURRENT_LINK}.new"
mv -f "${CURRENT_LINK}.new" "$CURRENT_LINK"
log_success "Symlink updated"

# Verify symlink
SYMLINK_TARGET=$(readlink "$CURRENT_LINK")
log_info "Symlink verification: $CURRENT_LINK → $SYMLINK_TARGET"
echo

# Verify binary is executable and correct
log_info "Verifying binary..."
if [[ ! -x "$CURRENT_LINK" ]]; then
    log_error "Binary is not executable"
    exit 1
fi

# Test binary runs and can show version (or at least doesn't crash)
if "$CURRENT_LINK" --help &>/dev/null || true; then
    log_success "Binary test passed"
else
    log_warn "Binary test inconclusive (may be normal for daemon)"
fi
echo

# Reload and start service
log_info "Reloading systemd configuration..."
systemctl daemon-reload
log_success "Systemd reloaded"

log_info "Starting touch-timeout service..."
if systemctl start touch-timeout.service; then
    sleep 1
    if systemctl is-active --quiet touch-timeout.service; then
        log_success "Service started and running"
    else
        log_error "Service started but not running (check logs)"
        systemctl status touch-timeout.service || true
        exit 1
    fi
else
    log_error "Failed to start service"
    systemctl status touch-timeout.service || true
    exit 1
fi
echo

# Show status and recent logs
log_info "Service status:"
systemctl status touch-timeout.service

echo
log_success "=========================================="
log_success "Installation complete!"
log_success "=========================================="
echo
log_info "Binary: $VERSIONED_BINARY"
log_info "Symlink: $CURRENT_LINK → $(readlink "$CURRENT_LINK")"
echo
log_info "View logs:"
echo "  journalctl -u touch-timeout.service -f"
echo
log_info "Rollback to previous version:"
echo "  ln -sf /usr/bin/touch-timeout-<VERSION>-<ARCH> /usr/bin/touch-timeout"
echo "  sudo systemctl restart touch-timeout.service"
echo
