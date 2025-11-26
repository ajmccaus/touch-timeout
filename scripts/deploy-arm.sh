#!/bin/bash
#
# deploy-arm.sh - Deploy touch-timeout cross-compiled binary to RPi4 over network
#
# Usage: scripts/deploy-arm.sh <last_3_ip_octets> [arm32|arm64]
# Example: scripts/deploy-arm.sh 127 arm64
#
# Process:
# 1. Validates arguments
# 2. Cross-compiles binary for specified architecture
# 3. Copies binary and install script to RPi staging location
# 4. Prints instructions for user to SSH and run final installation
#

set -euo pipefail

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'  # No Color

# Configuration
RPI_IP_PREFIX="192.168.1"
RPI_USER="root"
RPI_STAGING_DIR="/tmp/touch-timeout-staging"
ARCH="${2:-arm64}"
BUILD_DIR="build"

# Helper functions
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

# Validate arguments
if [[ $# -lt 1 ]]; then
    log_error "Usage: $0 <last_3_ip_octets> [arm32|arm64]"
    echo "Examples:"
    echo "  $0 127           # Build and deploy arm64 to 192.168.1.127"
    echo "  $0 127 arm32     # Build and deploy arm32 to 192.168.1.127"
    exit 1
fi

IP_OCTETS="$1"
if ! [[ "$IP_OCTETS" =~ ^[0-9]{1,3}$ ]] || [[ "$IP_OCTETS" -gt 255 ]]; then
    log_error "Invalid IP octets: $IP_OCTETS (must be 0-255)"
    exit 1
fi

if [[ "$ARCH" != "arm32" && "$ARCH" != "arm64" ]]; then
    log_error "Invalid architecture: $ARCH (must be arm32 or arm64)"
    exit 1
fi

RPI_IP="${RPI_IP_PREFIX}.${IP_OCTETS}"

log_info "Configuration:"
log_info "  Target IP: ${BLUE}$RPI_IP${NC}"
log_info "  Architecture: ${BLUE}$ARCH${NC}"
log_info "  Staging dir: ${BLUE}$RPI_STAGING_DIR${NC}"
echo

# Check if cross-compilation toolchain is available
case "$ARCH" in
    arm32)
        if ! command -v arm-linux-gnueabihf-gcc &> /dev/null; then
            log_error "Cross-compiler not found: arm-linux-gnueabihf-gcc"
            echo "Install with: sudo apt-get install gcc-arm-linux-gnueabihf"
            exit 1
        fi
        ;;
    arm64)
        if ! command -v aarch64-linux-gnu-gcc &> /dev/null; then
            log_error "Cross-compiler not found: aarch64-linux-gnu-gcc"
            echo "Install with: sudo apt-get install gcc-aarch64-linux-gnu"
            exit 1
        fi
        ;;
esac

# Verify SSH connectivity before building
log_info "Verifying SSH connectivity to $RPI_IP..."
if ! ssh -o ConnectTimeout=5 -o BatchMode=yes "$RPI_USER@$RPI_IP" "echo ok" &> /dev/null; then
    log_error "Cannot connect to $RPI_USER@$RPI_IP"
    echo "Make sure RPi4 is powered on and SSH is accessible"
    exit 1
fi
log_success "SSH connectivity verified"
echo

# Build cross-compiled binary
log_info "Building touch-timeout for ${ARCH}..."
if ! make "$ARCH" &> /dev/null; then
    log_error "Build failed for $ARCH"
    echo "Run: make $ARCH (for full output)"
    exit 1
fi
log_success "Build completed: ${BUILD_DIR}/${ARCH}/touch-timeout"
echo

# Verify binary was created
BINARY_PATH="${BUILD_DIR}/${ARCH}/touch-timeout"
if [[ ! -f "$BINARY_PATH" ]]; then
    log_error "Binary not found: $BINARY_PATH"
    exit 1
fi

# Get version from binary
VERSION=$("$BINARY_PATH" --version 2>/dev/null || echo "unknown")

# Prepare staging directory on RPi
log_info "Preparing staging directory on $RPI_IP..."
ssh "$RPI_USER@$RPI_IP" "mkdir -p $RPI_STAGING_DIR && rm -f $RPI_STAGING_DIR/*"
log_success "Staging directory ready"
echo

# Copy binary to RPi
log_info "Transferring binary ($(du -h "$BINARY_PATH" | cut -f1))..."
scp -q "$BINARY_PATH" "$RPI_USER@$RPI_IP:$RPI_STAGING_DIR/touch-timeout"
log_success "Binary transferred"

# Copy install script to RPi
log_info "Transferring install script..."
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
scp -q "$SCRIPT_DIR/install-on-rpi.sh" "$RPI_USER@$RPI_IP:$RPI_STAGING_DIR/"
ssh "$RPI_USER@$RPI_IP" "chmod +x $RPI_STAGING_DIR/install-on-rpi.sh"
log_success "Install script transferred"

# Copy systemd service file (optional, may not be used on minimal systems)
log_info "Transferring systemd service file..."
scp -q "$PROJECT_ROOT/systemd/touch-timeout.service" "$RPI_USER@$RPI_IP:$RPI_STAGING_DIR/" 2>/dev/null || \
    log_warn "Systemd service file not found (optional)"

# Copy config file
log_info "Transferring config file..."
scp -q "$PROJECT_ROOT/config/touch-timeout.conf" "$RPI_USER@$RPI_IP:$RPI_STAGING_DIR/" || \
    log_warn "Config file not found"
echo

# Summary and next steps
echo -e "${GREEN}========================================${NC}"
log_success "Deployment package ready on RPi4"
echo -e "${GREEN}========================================${NC}"
echo
log_info "Next steps:"
echo "  1. SSH into RPi4:"
echo "     ssh root@$RPI_IP"
echo
echo "  2. Run the installation script:"
echo "     sudo ARCH=$ARCH $RPI_STAGING_DIR/install-on-rpi.sh"
echo
log_warn "The install script will:"
echo "  - Install binary as: /usr/bin/touch-timeout-2.0.0-$ARCH"
echo "  - Create symlink: /usr/bin/touch-timeout → binary"
echo "  - Install config: /etc/touch-timeout.conf (if not exists)"
echo "  - Install systemd service (if systemd available)"
echo "  - Restart service (if running)"
echo
