#!/bin/bash
#
# deploy-arm.sh - Deploy touch-timeout cross-compiled binary to RPi4 over network
#
# Usage: scripts/deploy-arm.sh <IP_ADDRESS> [arm32|arm64] [OPTIONS]
# Example: scripts/deploy-arm.sh 192.168.1.127 arm64 --user pi
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
RPI_USER="root"
RPI_STAGING_DIR="/run/touch-timeout-staging"
ARCH="${2:-arm64}"
BUILD_DIR="build"
BATCH_MODE=""
MANUAL_MODE=0  # Default: auto-install

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
    log_error "Usage: $0 <IP_ADDRESS> [arm32|arm64] [OPTIONS]"
    echo ""
    echo "Arguments:"
    echo "  <IP_ADDRESS>    IP address of Raspberry Pi target"
    echo "  [arm32|arm64]   Target architecture (default: arm64)"
    echo ""
    echo "Options:"
    echo "  --user USER     SSH user for deployment (default: root)"
    echo "  --batch         Batch mode - requires passwordless SSH (for CI/CD)"
    echo "  --manual        Manual install - transfer only, skip auto-install"
    echo ""
    echo "Examples:"
    echo "  $0 192.168.1.127                          # Deploy + auto-install (default)"
    echo "  $0 192.168.1.127 arm64 --manual           # Transfer only, manual install"
    echo "  $0 192.168.1.127 arm64 --user pi          # Deploy as user 'pi'"
    echo "  $0 192.168.1.127 arm64 --batch            # CI/CD mode (auto-install)"
    exit 1
fi

RPI_IP="$1"

# Parse optional flags (after IP and ARCH)
shift 2 2>/dev/null || shift 1  # Remove IP and optionally ARCH
while [[ $# -gt 0 ]]; do
    case "$1" in
        --user)
            RPI_USER="$2"
            shift 2
            ;;
        --batch)
            BATCH_MODE="-o BatchMode=yes"
            shift
            ;;
        --manual)
            MANUAL_MODE=1
            shift
            ;;
        *)
            log_error "Unknown option: $1"
            exit 1
            ;;
    esac
done
# Validate IP address format (basic check for x.x.x.x pattern)
if ! [[ "$RPI_IP" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    log_error "Invalid IP address format: $RPI_IP"
    echo "Expected format: x.x.x.x (e.g., 192.168.1.127)"
    exit 1
fi

if [[ "$ARCH" != "arm32" && "$ARCH" != "arm64" ]]; then
    log_error "Invalid architecture: $ARCH (must be arm32 or arm64)"
    exit 1
fi

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
log_info "Verifying SSH connectivity to $RPI_USER@$RPI_IP..."
if ! ssh -o ConnectTimeout=5 $BATCH_MODE "$RPI_USER@$RPI_IP" "echo ok" 2>/dev/null; then
    log_error "Cannot connect to $RPI_USER@$RPI_IP"
    echo ""
    echo "Troubleshooting steps:"
    echo "  1. Verify RPi is powered on: ping $RPI_IP"
    echo "  2. Test SSH manually: ssh $RPI_USER@$RPI_IP"
    echo "  3. Ensure SSH service is running on RPi"
    echo ""
    log_warn "For passwordless deployment (recommended):"
    echo "  - Set up SSH keys (see INSTALLATION.md - SSH Setup section)"
    echo "  - Eliminates password prompts during deployment"
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

# Get version from Makefile
VERSION_MAJOR=$(grep "^VERSION_MAJOR" Makefile | awk '{print $3}')
VERSION_MINOR=$(grep "^VERSION_MINOR" Makefile | awk '{print $3}')
VERSION_PATCH=$(grep "^VERSION_PATCH" Makefile | awk '{print $3}')
VERSION="${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}"

# Copy binary to RPi with versioned name
VERSIONED_BINARY="touch-timeout-${VERSION}-${ARCH}"
log_info "Transferring binary as $VERSIONED_BINARY ($(du -h "$BINARY_PATH" | cut -f1))..."
scp -q "$BINARY_PATH" "$RPI_USER@$RPI_IP:$RPI_STAGING_DIR/$VERSIONED_BINARY"
log_success "Binary transferred"

# Copy install script to RPi
log_info "Transferring install script..."
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
scp -q "$SCRIPT_DIR/install.sh" "$RPI_USER@$RPI_IP:$RPI_STAGING_DIR/"
ssh "$RPI_USER@$RPI_IP" "chmod +x $RPI_STAGING_DIR/install.sh"
log_success "Install script transferred"

# Copy systemd service file (optional, may not be used on minimal systems)
log_info "Transferring systemd service file..."
scp -q "$PROJECT_ROOT/systemd/touch-timeout.service" "$RPI_USER@$RPI_IP:$RPI_STAGING_DIR/" 2>/dev/null || \
    log_warn "Systemd service file not found (optional)"
echo

# Summary and next steps
echo -e "${GREEN}========================================${NC}"
log_success "Deployment package ready on RPi4"
echo -e "${GREEN}========================================${NC}"
echo

# Auto-install by default, unless --manual flag used
if [[ "$MANUAL_MODE" == "0" ]]; then
    log_info "Running installation automatically..."
    echo

    # Determine if sudo is needed (root user doesn't need sudo)
    if [[ "$RPI_USER" == "root" ]]; then
        INSTALL_CMD="$RPI_STAGING_DIR/install.sh"
    else
        INSTALL_CMD="sudo $RPI_STAGING_DIR/install.sh"
    fi

    # Run install script remotely
    if ssh $BATCH_MODE "$RPI_USER@$RPI_IP" "$INSTALL_CMD"; then
        echo
        echo -e "${GREEN}========================================${NC}"
        log_success "Deployment and installation complete!"
        echo -e "${GREEN}========================================${NC}"
        echo
        log_info "View logs: ssh $RPI_USER@$RPI_IP 'journalctl -u touch-timeout.service -f'"
        log_info "Check status: ssh $RPI_USER@$RPI_IP 'systemctl status touch-timeout.service'"
    else
        log_error "Installation failed"
        echo
        log_warn "Manual recovery steps:"
        echo "  1. SSH into RPi: ssh $RPI_USER@$RPI_IP"
        echo "  2. Check logs: journalctl -u touch-timeout.service -n 50"
        echo "  3. Retry install: $INSTALL_CMD"
        exit 1
    fi
else
    # Manual mode - show next steps
    log_info "Manual mode - installation skipped"
    echo
    log_info "Next steps:"
    echo "  1. SSH into RPi4:"
    echo "     ssh $RPI_USER@$RPI_IP"
    echo
    echo "  2. Run the installation script:"
    if [[ "$RPI_USER" == "root" ]]; then
        echo "     $RPI_STAGING_DIR/install.sh"
    else
        echo "     sudo $RPI_STAGING_DIR/install.sh"
    fi
    echo
    log_warn "The install script will:"
    echo "  - Install binary: /usr/bin/$VERSIONED_BINARY"
    echo "  - Create symlink: /usr/bin/touch-timeout → $VERSIONED_BINARY"
    echo "  - Install systemd service (if systemd available)"
    echo "  - Restart service (if running)"
    echo
fi
