#!/bin/bash
#
# install.sh - On-device installation with version management
#
# PURPOSE:
#   Installs touch-timeout binary with versioned naming and symlink
#   management for rollback support. Optimized for minimal SD card wear.
#
# RUNS ON:
#   Raspberry Pi (target device) - Called by deploy.sh or manually
#
# WORKFLOW:
#   1. Detect binary in /run/touch-timeout-staging/ (versioned format)
#   2. Parse version and architecture from filename
#   3. Stop running service (if active)
#   4. Install as /usr/bin/touch-timeout-X.Y.Z-{arm32,arm64}
#   5. Create/update symlink: /usr/bin/touch-timeout → versioned binary
#   6. Install systemd service file (if systemd present)
#   7. Reload systemd, enable service, start service
#
# VERSIONING:
#   Keeps multiple versions installed for rollback:
#   /usr/bin/touch-timeout-0.7.0-arm64
#   /usr/bin/touch-timeout-0.8.0-arm64
#   /usr/bin/touch-timeout → touch-timeout-0.8.0-arm64 (symlink)
#
# ENVIRONMENT VARIABLES:
#   QUIET_MODE=0   - Enable verbose output (default: 1 for minimal SD writes)
#
# EXIT CODES:
#   0 - Success (installed and service started)
#   1 - Failure (invalid binary, permissions, or service start failed)
#
# USAGE:
#   sudo /run/touch-timeout-staging/install.sh           # Auto-detect binary
#   QUIET_MODE=0 sudo /run/touch-timeout-staging/install.sh  # Verbose mode
#
# SEE ALSO:
#   - scripts/deploy.sh - Remote deployment that calls this script
#   - Makefile (rollback targets) - Version management commands
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
STAGING_DIR="/run/touch-timeout-staging"
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

# Find versioned binary in staging directory
BINARY_FILE=$(ls "$STAGING_DIR"/touch-timeout-*-* 2>/dev/null | head -1)
if [[ -z "$BINARY_FILE" ]]; then
    log_error "No versioned binary found in $STAGING_DIR/"
    echo "Expected filename format: touch-timeout-VERSION-ARCH (e.g., touch-timeout-2.0.0-arm32)"
    exit 1
fi

# Extract filename without path
BINARY_NAME=$(basename "$BINARY_FILE")

# Parse version and architecture from filename
# Format: touch-timeout-VERSION-ARCH
if [[ ! "$BINARY_NAME" =~ ^touch-timeout-([0-9]+\.[0-9]+\.[0-9]+)-(arm32|arm64|native)$ ]]; then
    log_error "Invalid binary filename format: $BINARY_NAME"
    echo "Expected format: touch-timeout-VERSION-ARCH (e.g., touch-timeout-2.0.0-arm64)"
    exit 1
fi

VERSION="${BASH_REMATCH[1]}"
ARCH="${BASH_REMATCH[2]}"

# Construct target paths
VERSIONED_BINARY="${INSTALL_DIR}/${BINARY_NAME}"
CURRENT_LINK="${INSTALL_DIR}/touch-timeout"

log_info "Installing ${BINARY_NAME}..."

# Stop service if running (minimize systemctl calls)
systemctl stop touch-timeout.service 2>/dev/null || true

# Install versioned binary (without verbose output to save SD writes)
install -m 755 "$BINARY_FILE" "$VERSIONED_BINARY"

# Create or update symlink atomically
ln -sf "$VERSIONED_BINARY" "${CURRENT_LINK}.new"
mv -f "${CURRENT_LINK}.new" "$CURRENT_LINK"

# Verify binary is executable
if [[ ! -x "$CURRENT_LINK" ]]; then
    log_error "Binary is not executable after installation"
    exit 1
fi

# Install systemd service if systemd is available and service file is present
if command -v systemctl >/dev/null 2>&1 && [[ -f "$STAGING_DIR/touch-timeout.service" ]]; then
    log_info "Installing systemd service..."
    install -m 644 "$STAGING_DIR/touch-timeout.service" "/etc/systemd/system/touch-timeout.service"
    log_success "Service installed"
fi

# Reload systemd, enable, and start service (if systemd available)
if command -v systemctl >/dev/null 2>&1; then
    systemctl daemon-reload
    systemctl enable touch-timeout.service 2>/dev/null || true
    if ! systemctl start touch-timeout.service; then
        log_error "Failed to start touch-timeout service"
        exit 1
    fi
    log_success "Installation complete: $(readlink "$CURRENT_LINK")"
    log_info "View logs: journalctl -u touch-timeout.service -f"
    log_info "Rollback: ln -sf /usr/bin/touch-timeout-<VERSION>-<ARCH> /usr/bin/touch-timeout && sudo systemctl restart touch-timeout.service"
else
    log_success "Installation complete: $(readlink "$CURRENT_LINK")"
    log_warn "systemd not available - service not started automatically"
    log_info "Start manually or configure init system to run: $CURRENT_LINK"
fi
