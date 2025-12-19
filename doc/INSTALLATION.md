# Installation Guide - touch-timeout v2.0

Comprehensive guide for installing touch-timeout on Raspberry Pi.

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

**The deployment will:**
1. Cross-compile binary for specified architecture
2. Check SSH connectivity (prompts for password once if keys not configured)
3. Create staging directory on RPi: `/run/touch-timeout-staging/`
4. Transfer binary, install script, and systemd service
5. **Automatically install on RPi** (installs binary, service, restarts daemon)
6. Display completion status

### Two-Step Deployment (Manual)

**Manual mode (transfer only, skip auto-install):**
```bash
# Step 1: Transfer only
make deploy-arm64 RPI=<IP_ADDRESS> MANUAL=1

# Step 2: SSH and install manually
ssh root@<IP_ADDRESS>
/run/touch-timeout-staging/install.sh
```

**Installation options:**
- **Default (quiet mode)**: Minimizes SD card writes
- **Verbose mode**: `QUIET_MODE=0 /run/touch-timeout-staging/install.sh`

**The install script will:**
1. Detect binary architecture from filename
2. Stop running service (if active)
3. Install binary as `/usr/bin/touch-timeout-{version}-{arch}`
4. Create/update symlink: `/usr/bin/touch-timeout`
5. Install systemd service (if systemd available)
6. Reload systemd and restart service

### SSH Key Setup (Optional but Recommended)

**Why use SSH keys?**
Without SSH keys, you'll be prompted for your password once per deployment. SSH keys eliminate this prompt entirely and enable automation.

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

---

## Configuration

**The daemon works out-of-box** - no configuration required! See [README.md - Configuration](../README.md#configuration) for default values.

To customize, choose one of the options below:

### Option 1: Config File

Create `/etc/touch-timeout.conf`:

```ini
# All values optional - only specify what you want to change
brightness=160        # 15-255, default 150
off_timeout=600       # seconds, default 300
dim_percent=20        # 1-100, default 10
backlight=rpi_backlight
device=event0
```

Then restart: `sudo systemctl restart touch-timeout.service`

### Option 2: CLI Arguments

Override via systemd:
```bash
sudo systemctl edit touch-timeout.service
```

```ini
[Service]
ExecStart=
ExecStart=/usr/bin/touch-timeout 200 600 rpi_backlight event0
```

Then: `sudo systemctl daemon-reload && sudo systemctl restart touch-timeout.service`

### Finding Your Touchscreen Device

If default `event0` doesn't work:
```bash
ls -l /dev/input/by-path/  # Find device with "touch" in name
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

**Quick health check:**
```bash
top -bn1 -p $(pgrep touch-timeout) | tail -1
# Expected: <1% CPU, ~0.2MB RSS
```

**Test touch responsiveness:**
1. Wait for timeout (screen should dim, then turn off)
2. Touch screen (should restore brightness immediately)
3. Check logs for state transitions

---

## Performance Data Collection

Collect metrics to verify [README.md](../README.md#performance) claims:

```bash
scp scripts/test-performance.sh <USER>@<IP>:/run/
ssh <USER>@<IP> "bash /run/test-performance.sh [seconds]"
```

Default 30 seconds. Outputs CPU average, memory, SD writes, FD count.

---

## Troubleshooting

### SSH Connection Fails (Remote Deployment)

**Symptom**: `[ERROR] SSH connection failed`

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

### Cross-Compiler Not Found

If `make arm64` fails with compiler not found:

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

List available versions and switch:
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

Optionally remove config:
```bash
sudo rm /etc/touch-timeout.conf
```

---

## Additional Resources

- **README.md**: Feature overview and project description
- **ARCHITECTURE.md**: v2.0 architecture and design decisions
- **Makefile**: Build system reference and targets
