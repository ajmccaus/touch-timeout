#!/bin/bash
#
# deploy.sh - Remote deployment automation for Raspberry Pi
#
# PURPOSE:
#   Transfers cross-compiled binary and scripts to RPi staging area,
#   then optionally auto-installs. Minimizes password prompts via
#   SSH ControlMaster connection reuse.
#
# CALLED BY:
#   make deploy-arm32 RPI=ip   - Deploy 32-bit build
#   make deploy-arm64 RPI=ip   - Deploy 64-bit build
#
# ARGUMENTS:
#   $1: RPI_IP - Target Raspberry Pi IP address
#   $2: BINARY_PATH - Path to cross-compiled binary
#
# ENVIRONMENT VARIABLES:
#   RPI_USER=user   - SSH user (default: root)
#   MANUAL=1        - Transfer only, skip auto-install
#
# WORKFLOW:
#   1. Establish persistent SSH connection (single password prompt)
#   2. Create staging directory: /run/touch-timeout-staging/
#   3. Transfer: binary, install.sh, test-performance.sh, .service file
#   4. Auto-run install.sh (unless MANUAL=1)
#   5. Display verification commands
#
# EXIT CODES:
#   0 - Success (deployed and installed, or staged if MANUAL=1)
#   1 - Failure (SSH connection failed, transfer failed, or install failed)
#
# SEE ALSO:
#   - doc/INSTALLATION.md - Complete deployment guide
#   - scripts/install.sh - On-device installation script
#   - Makefile - Build and deployment targets
#

set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

err() { echo -e "${RED}[ERROR]${NC} $1" >&2; }
ok() { echo -e "${GREEN}[OK]${NC} $1"; }

# Arguments
if [[ $# -lt 2 ]]; then
    err "Usage: deploy.sh <ip> <binary_path>"
    exit 1
fi

RPI_IP="$1"
BINARY_PATH="$2"
RPI_USER="${RPI_USER:-root}"
MANUAL="${MANUAL:-0}"
STAGING="/run/touch-timeout-staging"

# Validate binary exists
if [[ ! -f "$BINARY_PATH" ]]; then
    err "Binary not found: $BINARY_PATH"
    exit 1
fi

BINARY_NAME=$(basename "$BINARY_PATH")

# SSH ControlMaster for connection reuse (single password prompt)
SOCKET="/tmp/deploy-ssh-$$"
cleanup() {
    ssh -S "$SOCKET" -O exit "$RPI_USER@$RPI_IP" 2>/dev/null || true
}
trap cleanup EXIT

# Establish persistent SSH connection
echo "Connecting to $RPI_USER@$RPI_IP..."
if ! ssh -M -S "$SOCKET" -o ControlPersist=60 -o ConnectTimeout=5 "$RPI_USER@$RPI_IP" "echo ok" >/dev/null; then
    err "SSH connection failed"
    echo ""
    echo "Troubleshooting:"
    echo "  1. Verify RPi is on: ping $RPI_IP"
    echo "  2. Test SSH: ssh $RPI_USER@$RPI_IP"
    echo "  3. For passwordless deploy, set up SSH keys (see INSTALLATION.md)"
    exit 1
fi
ok "Connected"

# Prepare staging directory
ssh -S "$SOCKET" "$RPI_USER@$RPI_IP" "mkdir -p $STAGING && rm -f $STAGING/*"

# Transfer files
echo "Transferring $BINARY_NAME..."
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

scp -o ControlPath="$SOCKET" -q "$BINARY_PATH" "$RPI_USER@$RPI_IP:$STAGING/"
scp -o ControlPath="$SOCKET" -q "$SCRIPT_DIR/install.sh" "$RPI_USER@$RPI_IP:$STAGING/"
scp -o ControlPath="$SOCKET" -q "$SCRIPT_DIR/test-performance.sh" "$RPI_USER@$RPI_IP:$STAGING/"
scp -o ControlPath="$SOCKET" -q "$PROJECT_ROOT/systemd/touch-timeout.service" "$RPI_USER@$RPI_IP:$STAGING/" 2>/dev/null || true
ok "Files transferred to $STAGING/"

# Install (unless MANUAL=1)
if [[ "$MANUAL" == "1" ]]; then
    echo ""
    echo "Manual mode - files staged at $STAGING/"
    echo ""
    echo "To install, SSH in and run:"
    if [[ "$RPI_USER" == "root" ]]; then
        echo "  $STAGING/install.sh"
    else
        echo "  sudo $STAGING/install.sh"
    fi
    echo ""
    echo "To test performance before installing:"
    echo "  ssh $RPI_USER@$RPI_IP 'bash $STAGING/test-performance.sh 60'"
    exit 0
fi

# Auto-install
echo "Installing..."
if [[ "$RPI_USER" == "root" ]]; then
    INSTALL_CMD="$STAGING/install.sh"
else
    INSTALL_CMD="sudo $STAGING/install.sh"
fi

if ssh -S "$SOCKET" "$RPI_USER@$RPI_IP" "$INSTALL_CMD"; then
    echo ""
    ok "Deployment complete"
    echo ""
    echo "Verify installation:"
    echo "  Status:       ssh $RPI_USER@$RPI_IP 'systemctl status touch-timeout'"
    echo "  Logs:         ssh $RPI_USER@$RPI_IP 'journalctl -u touch-timeout -f'"
    echo "  Performance:  ssh $RPI_USER@$RPI_IP 'bash $STAGING/test-performance.sh 30'"
    echo "  Version:      ssh $RPI_USER@$RPI_IP 'touch-timeout --version'"
else
    err "Installation failed"
    echo ""
    echo "Debug: ssh $RPI_USER@$RPI_IP 'journalctl -u touch-timeout -n 50'"
    exit 1
fi
