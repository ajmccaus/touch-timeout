# Installation Guide

Installation methods for touch-timeout on Raspberry Pi.

## Table of Contents

- [Prerequisites](#prerequisites)
- [Method 1: Direct Installation](#method-1-direct-installation-on-raspberry-pi)
- [Method 2: Remote Deployment](#method-2-remote-deployment-cross-compilation)
- [Configuration](#configuration)
- [Verification](#verification)
- [Performance Data Collection](#performance-data-collection)
- [Troubleshooting](#troubleshooting)
- [Rollback](#rollback)
- [Uninstall](#uninstall)

---

## Prerequisites

**Hardware:**
- Raspberry Pi (3, 4, or Zero) with 7" touchscreen
- SD card with Raspberry Pi OS (Debian-based)

**Software:**
- For Direct Installation: GCC compiler on Raspberry Pi
- For Remote Deployment: Cross-compiler on Linux/WSL2

---

## Method 1: Direct Installation (On Raspberry Pi)

Install directly on your Raspberry Pi (building on the device).

### Quick Install

```bash
# Clone repository
git clone https://github.com/ajmccaus/touch-timeout.git
cd touch-timeout

# Build and install
make
sudo make install

# Enable and start service (uses defaults)
sudo systemctl daemon-reload
sudo systemctl enable --now touch-timeout.service

# Optional: Customize settings (see Configuration section below)
```

---

## Method 2: Remote Deployment (Cross-Compilation)

Build on a development machine (Linux/WSL2) and deploy to Raspberry Pi over network.

### Prerequisites for Remote Deployment

**On build machine (Linux/WSL2):**
```bash
# Install cross-compilation toolchains
sudo apt-get update
sudo apt-get install gcc-arm-linux-gnueabihf gcc-aarch64-linux-gnu

# Verify installation
aarch64-linux-gnu-gcc --version       # ARM 64-bit
arm-linux-gnueabihf-gcc --version     # ARM 32-bit
```

**On Raspberry Pi:**
- SSH enabled
- User access (root or pi with sudo)
- Network connectivity

### One-Step Deployment (Recommended)

**Auto-install (default):**
```bash
# Deploy to RPi4 (ARM 64-bit) - builds, transfers, and installs
make deploy-arm64 RPI=<IP_ADDRESS>

# Deploy as non-root user
make deploy-arm64 RPI=<IP_ADDRESS> RPI_USER=pi

# Deploy to older RPi (ARM 32-bit)
make deploy-arm32 RPI=<IP_ADDRESS>
```

### Two-Step Deployment (Manual)

**Manual mode (transfer only, skip auto-install):**
```bash
# Step 1: Transfer only
make deploy-arm64 RPI=<IP_ADDRESS> MANUAL=1

# Step 2: SSH and install manually
ssh root@<IP_ADDRESS>
/run/touch-timeout-staging/install.sh
```

**Verbose install:**
```bash
QUIET_MODE=0 /run/touch-timeout-staging/install.sh
```

### SSH Key Setup (Optional)

Eliminates password prompts during deployment.

```bash
# Check for existing keys
ls ~/.ssh/id_rsa.pub

# Generate if needed
ssh-keygen -t rsa -b 4096 -C "your_email@example.com"

# Copy to RPi (method 1)
ssh-copy-id <USER>@<IP_ADDRESS>

# Copy to RPi (method 2 - if ssh-copy-id unavailable)
cat ~/.ssh/id_rsa.pub | ssh <USER>@<IP_ADDRESS> \
  "mkdir -p ~/.ssh && cat >> ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys && chmod 700 ~/.ssh"

# Verify
ssh <USER>@<IP_ADDRESS> "echo OK"
```

---

## Configuration

### External Wake (shairport-sync)

Wake display from external programs:

```bash
pkill -USR1 touch-timeout
```

**shairport-sync config (`/etc/shairport-sync.conf`):**
```
sessioncontrol = {
    run_this_before_play_begins = "/usr/bin/pkill -USR1 touch-timeout";
};
```

### Manual Device Override (Rarely Needed)

**Note:** Version 0.8.0+ includes device auto-detection. These instructions are only needed if auto-detection fails.

If you need to manually specify devices:
```bash
# Find touchscreen device (if auto-detection fails)
ls -l /dev/input/by-path/  # Look for device with "touch" in name

# Override via systemd
sudo systemctl edit touch-timeout
# Add: ExecStart=/usr/bin/touch-timeout -i event1 -l backlight_device
```

---

## Verification

**Check service status:**
```bash
sudo systemctl status touch-timeout.service
```

**View real-time logs:**
```bash
sudo journalctl -u touch-timeout.service -f
```

**Test touch responsiveness:**
1. Wait for timeout (screen should dim, then turn off)
2. Touch screen (should restore brightness immediately)
3. Check logs for state transitions

---

## Performance Data Collection

Collect metrics to verify [README.md](../README.md#performance) claims:

**After `make deploy` (script already on device):**
```bash
ssh <USER>@<IP> "bash /run/touch-timeout-staging/test-performance.sh [seconds]"
```

**Manual deployment:**
```bash
scp scripts/test-performance.sh <USER>@<IP>:/run/
ssh <USER>@<IP> "bash /run/test-performance.sh [seconds]"
```

Default duration: 30 seconds. Outputs CPU average, memory, SD writes, FD count.

---

## Troubleshooting

### SSH Connection Fails

```bash
# Verify connectivity
ping <IP_ADDRESS>
ssh <USER>@<IP_ADDRESS>

# First-time connection: accept host key when prompted
```

### Cross-Compiler Not Found

```bash
sudo apt-get install gcc-aarch64-linux-gnu gcc-arm-linux-gnueabihf
```

### Service Won't Start

```bash
journalctl -u touch-timeout.service -n 50
ls -l /usr/bin/touch-timeout
ls /sys/class/backlight/
```

---

## Rollback (Remote Deployment)

**List available versions:**
```bash
make rollback-list RPI=<IP_ADDRESS>
```

**Rollback to a specific version:**
```bash
make rollback RPI=<IP_ADDRESS> TO=0.6.0
```

**Manual rollback (on RPi):**
```bash
ls -lh /usr/bin/touch-timeout*
sudo ln -sf /usr/bin/touch-timeout-<VERSION>-<ARCH> /usr/bin/touch-timeout
sudo systemctl restart touch-timeout.service
```

---

## Uninstall

```bash
sudo systemctl stop touch-timeout.service
sudo systemctl disable touch-timeout.service
sudo rm /usr/bin/touch-timeout* /etc/systemd/system/touch-timeout.service
sudo systemctl daemon-reload
```

