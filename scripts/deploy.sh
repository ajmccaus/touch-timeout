#!/bin/bash
#
# deploy.sh - Transfer and install pre-built binary to Raspberry Pi
#
# Called by: make deploy-arm64 RPI=<ip>
# Arguments: <ip> <binary_path>
# Environment:
#   RPI_USER - SSH user (default: root)
#   MANUAL   - Set to 1 to skip auto-install (default: 0)
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
scp -o ControlPath="$SOCKET" -q "$PROJECT_ROOT/systemd/touch-timeout.service" "$RPI_USER@$RPI_IP:$STAGING/" 2>/dev/null || true
ok "Files transferred to $STAGING/"

# Install (unless MANUAL=1)
if [[ "$MANUAL" == "1" ]]; then
    echo ""
    echo "Manual mode - files staged at $STAGING/"
    echo "To install, SSH in and run:"
    if [[ "$RPI_USER" == "root" ]]; then
        echo "  $STAGING/install.sh"
    else
        echo "  sudo $STAGING/install.sh"
    fi
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
    echo "Commands:"
    echo "  Logs:   ssh $RPI_USER@$RPI_IP 'journalctl -u touch-timeout -f'"
    echo "  Status: ssh $RPI_USER@$RPI_IP 'systemctl status touch-timeout'"
else
    err "Installation failed"
    echo ""
    echo "Debug: ssh $RPI_USER@$RPI_IP 'journalctl -u touch-timeout -n 50'"
    exit 1
fi
