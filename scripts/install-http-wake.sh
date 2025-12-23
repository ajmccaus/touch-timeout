#!/bin/bash
#
# install-http-wake.sh - Install HTTP wake endpoint for touch-timeout
#
# Installs HTTP wake service for external integration. Enables containerized
# services (shairport-sync, spotifyd) and other systems to wake the display.
#
# Usage:
#   sudo scripts/install-http-wake.sh          # Install
#   sudo scripts/install-http-wake.sh remove   # Uninstall
#
# After install, integrate with audio scripts:
#   curl -sS -X POST http://127.0.0.1:8765/wake
#
# See scripts/http-wake.py for detailed examples.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

if [[ $EUID -ne 0 ]]; then
    echo "Error: This script must be run as root"
    exit 1
fi

if [[ "$1" == "remove" ]]; then
    systemctl stop http-wake.service 2>/dev/null || true
    systemctl disable http-wake.service 2>/dev/null || true
    rm -f /etc/systemd/system/http-wake.service
    rm -f /usr/bin/http-wake
    systemctl daemon-reload
    echo "HTTP wake service removed"
else
    # Check dependencies
    command -v python3 >/dev/null || { echo "Error: Python 3 required"; exit 1; }
    command -v systemctl >/dev/null || { echo "Error: systemd required"; exit 1; }

    # Install files
    cp "$SCRIPT_DIR/http-wake.py" /usr/bin/http-wake
    chmod +x /usr/bin/http-wake
    cp "$PROJECT_ROOT/systemd/http-wake.service" /etc/systemd/system/

    # Enable and start service
    systemctl daemon-reload
    systemctl enable http-wake.service
    systemctl restart http-wake.service

    echo "HTTP wake service installed"
    echo "Test: curl -X POST http://127.0.0.1:8765/wake"
    echo "Integration: See scripts/http-wake.py for examples"
fi
