# Installation Guide - touch-timeout v2.0

Comprehensive guide for installing touch-timeout on Raspberry Pi.

## Table of Contents

- [Prerequisites](#prerequisites)
- [Method 1: Direct Installation](#method-1-direct-installation-on-raspberry-pi)
- [Method 2: Remote Deployment](#method-2-remote-deployment-cross-compilation)
- [Configuration](#configuration)
- [Verification](#verification)
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

# Configure (edit brightness, timeout, etc.)
sudo nano /etc/touch-timeout.conf

# Enable and start service
sudo systemctl daemon-reload
sudo systemctl enable --now touch-timeout.service
```

### Verify Installation

```bash
# Check service status
sudo systemctl status touch-timeout.service

# View logs
sudo journalctl -u touch-timeout.service -f
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

**Checklist before deploying:**
- [ ] Cross-compiler installed on build machine
- [ ] Raspberry Pi is powered on and network-accessible
- [ ] SSH access works: `ssh <USER>@<IP_ADDRESS>`
- [ ] (Optional) SSH keys configured for passwordless deployment

### SSH Key Setup (Optional but Recommended)

**Why use SSH keys?**
Without SSH keys, you'll be prompted for your password 3-4 times during deployment. SSH keys eliminate password prompts and enable automation.

**Quick SSH Key Setup:**

**Step 1: Check for existing keys**
```bash
ls ~/.ssh/id_rsa.pub
```

**Step 2: Generate key if needed**
```bash
ssh-keygen -t rsa -b 4096 -C "your_email@example.com"
# Press Enter to accept defaults
# Optionally set a passphrase (or leave empty for automation)
```

**Step 3: Copy key to Raspberry Pi**

**Method 1: ssh-copy-id (Linux/macOS/WSL2)**
```bash
ssh-copy-id <USER>@<IP_ADDRESS>
# Replace <USER> with 'root' or 'pi'
# Enter password when prompted
```

**Method 2: Manual copy (when ssh-copy-id unavailable)**
```bash
cat ~/.ssh/id_rsa.pub | ssh <USER>@<IP_ADDRESS> \
  "mkdir -p ~/.ssh && cat >> ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys && chmod 700 ~/.ssh"
```

**Step 4: Verify passwordless login**
```bash
ssh <USER>@<IP_ADDRESS> "echo OK"
# Should print "OK" without password prompt
```

### Deployment Steps

**Step 1: Build and transfer from build machine**
```bash
# Default: Deploy arm64 as root user
./scripts/deploy-arm.sh <IP_ADDRESS> arm64

# Deploy as custom user (e.g., 'pi')
./scripts/deploy-arm.sh <IP_ADDRESS> arm64 --user pi

# Deploy arm32 architecture
./scripts/deploy-arm.sh <IP_ADDRESS> arm32

# Batch mode for CI/CD (requires SSH keys)
./scripts/deploy-arm.sh <IP_ADDRESS> arm64 --batch
```

**The script will:**
1. Validate IP address and architecture
2. Check SSH connectivity (will prompt for password if keys not configured)
3. Cross-compile binary for specified architecture
4. Create staging directory on RPi: `/run/touch-timeout-staging/`
5. Transfer binary, install script, config, and systemd service
6. Display next steps

**Step 2: Install on Raspberry Pi**

SSH into the Raspberry Pi and run the installation script:
```bash
# SSH into RPi
ssh <USER>@<IP_ADDRESS>

# Run installation
sudo /run/touch-timeout-staging/install.sh
```

**Installation options:**
- **Default (quiet mode)**: Minimizes SD card writes
- **Verbose mode**: `QUIET_MODE=0 sudo /run/touch-timeout-staging/install.sh`

**The install script will:**
1. Detect binary architecture
2. Stop running service (if active)
3. Install binary as `/usr/bin/touch-timeout-{version}-{arch}`
4. Create/update symlink: `/usr/bin/touch-timeout`
5. Install config (preserves existing `/etc/touch-timeout.conf`)
6. Install systemd service (if systemd available)
7. Reload systemd and restart service

### CI/CD Automation

For automated pipelines (GitHub Actions, Jenkins, etc.), use `--batch` flag:

```bash
#!/bin/bash
set -e

# Deploy with batch mode (requires passwordless SSH)
./scripts/deploy-arm.sh <IP_ADDRESS> arm64 --user pi --batch
ssh pi@<IP_ADDRESS> "sudo /run/touch-timeout-staging/install.sh"
ssh pi@<IP_ADDRESS> "systemctl is-active touch-timeout.service && echo OK"
```

**What is CI/CD?**
- **CI** = Continuous Integration (automatically build and test code)
- **CD** = Continuous Deployment (automatically deploy tested code)
- Examples: GitHub Actions, Jenkins, GitLab CI/CD

**Note**: Batch mode uses `BatchMode=yes`, which disables password prompts. Ensure SSH keys are configured before using in automation.

---

## Configuration

Edit the configuration file:
```bash
sudo nano /etc/touch-timeout.conf
```

See [README.md - Configuration](README.md#configuration) for parameter reference.

**Identify your touchscreen device** (if default doesn't work):
```bash
# List input devices
ls -l /dev/input/by-path/

# Find touchscreen (usually contains "event-touch" or "touchscreen")
# Example output: platform-gpu-event → ../../event0

# Update config with correct event number
sudo nano /etc/touch-timeout.conf  # Change device=event0 to your device
sudo systemctl restart touch-timeout.service
```

After changing config:
```bash
sudo systemctl restart touch-timeout.service
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

## Troubleshooting

### SSH Connection Fails (Remote Deployment)

**Symptom**: `ERROR: Cannot connect to <USER>@<IP_ADDRESS>`

**Diagnosis steps:**
```bash
# 1. Verify network connectivity
ping <IP_ADDRESS>

# 2. Test SSH manually
ssh <USER>@<IP_ADDRESS>
# Should prompt for password or connect via keys

# 3. Check SSH service on RPi (if you can access it)
sudo systemctl status ssh
```

**Common causes:**
- RPi not powered on or not on network
- Wrong IP address
- SSH service not running on RPi
- Firewall blocking port 22
- First-time connection (see below)

### First-Time SSH Host Key Verification

On first connection to a new device:
```
The authenticity of host '<IP_ADDRESS>' can't be established.
ECDSA key fingerprint is SHA256:...
Are you sure you want to continue connecting (yes/no)?
```

**Action**: Type `yes` and press Enter to add host to `~/.ssh/known_hosts`

### Password Prompts During Deployment

If you're prompted for password 3-4 times during deployment:
- This is normal if SSH keys aren't configured
- Consider setting up SSH keys (see "SSH Setup" above) for faster deployments
- Or use the `--batch` flag to enforce key-only authentication

### Permission Denied (publickey)

If you get this error with `--batch` flag:
```
Permission denied (publickey,password).
```

**Cause**: Batch mode is enabled but SSH keys aren't configured

**Solutions:**
1. Remove `--batch` flag to allow password authentication
2. Set up SSH keys (see "SSH Setup" section)
3. Verify key is added: `ssh-add -l`

### Cross-Compiler Not Found

```
ERROR: Cross-compiler not found: aarch64-linux-gnu-gcc
```

**Fix:**
```bash
sudo apt-get update
sudo apt-get install gcc-aarch64-linux-gnu gcc-arm-linux-gnueabihf
```

### Service Won't Start

```bash
systemctl status touch-timeout.service
journalctl -u touch-timeout.service -n 50
```

**Check that:**
- Symlink target binary exists: `ls -l /usr/bin/touch-timeout`
- `/etc/touch-timeout.conf` is valid
- Binary is executable
- Device `/dev/input/event0` exists (or configured device)
- Backlight device exists: `ls /sys/class/backlight/`

### Build Fails

**Symptom**: `make` command fails

**Check:**
- GCC installed: `gcc --version`
- For cross-compilation: correct toolchain installed
- Sufficient disk space: `df -h`

---

## Rollback (Remote Deployment)

List available versions:
```bash
ls -lh /usr/bin/touch-timeout*
```

Switch to previous version:
```bash
sudo ln -sf /usr/bin/touch-timeout-1.0.0-arm64 /usr/bin/touch-timeout
sudo systemctl restart touch-timeout.service
```

Verify:
```bash
journalctl -u touch-timeout.service -n 20
```

---

## Uninstall

```bash
sudo systemctl stop touch-timeout.service
sudo systemctl disable touch-timeout.service
sudo rm /usr/bin/touch-timeout* /etc/systemd/system/touch-timeout.service
sudo systemctl daemon-reload
```

Optionally remove config:
```bash
sudo rm /etc/touch-timeout.conf
```

---

## Additional Resources

- **README.md**: Feature overview and project description
- **ARCHITECTURE.md**: v2.0 architecture and design decisions
- **Makefile**: Build system reference and targets
