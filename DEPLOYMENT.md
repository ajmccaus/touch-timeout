# Deployment Guide - touch-timeout v2.0.0

Cross-compile and deploy touch-timeout to Raspberry Pi 4 from WSL2 or Linux.

## Quick Start

Deploy to RPi4 at 192.168.1.127:

```bash
# Step 1: Build and transfer from build machine
./scripts/deploy-arm.sh 127 arm64

# Step 2: SSH into RPi4 and install
ssh root@192.168.1.127
sudo /tmp/touch-timeout-staging/install-on-rpi.sh
```

## Prerequisites

### Build Machine (WSL2 or Linux)

Install cross-compilation toolchains:
```bash
sudo apt-get update
sudo apt-get install gcc-arm-linux-gnueabihf gcc-aarch64-linux-gnu
```

Verify installation:
```bash
arm-linux-gnueabihf-gcc --version     # ARM 32-bit
aarch64-linux-gnu-gcc --version       # ARM 64-bit
```

### RPi4 Target

- SSH enabled
- Root or passwordless sudo access
- systemd with touch-timeout.service already installed
- Network connectivity to build machine

## Architecture

### Build Output

```
build/
├── native/touch-timeout     # Native build
├── arm32/touch-timeout      # ARM 32-bit (armv7l)
└── arm64/touch-timeout      # ARM 64-bit (aarch64)
```

### RPi4 Installation

```
/usr/bin/
├── touch-timeout              # Symlink → touch-timeout-2.0.0-arm64
├── touch-timeout-2.0.0-arm64  # Current version
└── touch-timeout-1.x.x-arm64  # Previous versions (for rollback)

/etc/
├── touch-timeout.conf         # Config (preserved across updates)
└── systemd/system/touch-timeout.service
```

## Deployment Process

### Step 1: Build and Transfer

Run on build machine:
```bash
./scripts/deploy-arm.sh <IP_OCTETS> [arm32|arm64]
```

Parameters:
- `<IP_OCTETS>`: Last 3 IP digits (e.g., 127 for 192.168.1.127)
- `[arm32|arm64]`: Target architecture (default: arm64)

Example:
```bash
./scripts/deploy-arm.sh 127 arm64
```

The script will:
1. Validate arguments and SSH connectivity
2. Cross-compile binary with architecture-specific optimizations
3. Create staging directory on RPi4: `/tmp/touch-timeout-staging/`
4. Transfer binary and install script via SCP

### Step 2: Install on RPi4

SSH into RPi4 and run:
```bash
ssh root@192.168.1.127
sudo /tmp/touch-timeout-staging/install-on-rpi.sh
```

Options:
- Default (quiet mode): Minimizes SD card writes
- Verbose: `QUIET_MODE=0 sudo /tmp/touch-timeout-staging/install-on-rpi.sh`

The script will:
1. Detect binary architecture
2. Stop running service
3. Install binary as `/usr/bin/touch-timeout-2.0.0-{arch}`
4. Create/update symlink atomically
5. Reload systemd and restart service

## Build Targets

```bash
make              # Build native binary → build/native/touch-timeout
make test         # Run unit tests
make coverage     # Generate coverage report
make clean        # Clean local artifacts

make arm32        # Build ARM 32-bit → build/arm32/touch-timeout
make arm64        # Build ARM 64-bit → build/arm64/touch-timeout
make clean-all    # Remove all build artifacts
```

## Rollback

List available versions:
```bash
ls -lh /usr/bin/touch-timeout*
```

Switch to previous version:
```bash
sudo ln -sf /usr/bin/touch-timeout-2.0.0-arm64 /usr/bin/touch-timeout
sudo systemctl restart touch-timeout.service
```

Verify:
```bash
journalctl -u touch-timeout.service -n 20
```

## Troubleshooting

**Build fails: "command not found"**
```bash
sudo apt-get install gcc-arm-linux-gnueabihf gcc-aarch64-linux-gnu
```

**Cannot connect to RPi4**
```bash
ping 192.168.1.127
ssh root@192.168.1.127 "echo ok"
```

**Service won't start**
```bash
systemctl status touch-timeout.service
journalctl -u touch-timeout.service -n 50
```

Check that:
- Symlink target binary exists
- `/etc/touch-timeout.conf` is valid
- Binary is executable (`ls -l /usr/bin/touch-timeout`)
- Device `/dev/input/event0` exists (or configured device)

## Performance Notes

- **Deployment**: Happens only during software updates
- **Runtime**: Uses POSIX timers (timerfd) - ~0 SD card writes during operation
- **Quiet Mode**: Reduces journal writes by ~75% compared to verbose mode

## Advanced Usage

Deploy both architectures:
```bash
./scripts/deploy-arm.sh 127 arm32
ssh root@192.168.1.127 "sudo /tmp/touch-timeout-staging/install-on-rpi.sh"

./scripts/deploy-arm.sh 127 arm64
ssh root@192.168.1.127 "sudo /tmp/touch-timeout-staging/install-on-rpi.sh"
```

Automated CI/CD deployment:
```bash
#!/bin/bash
set -e
./scripts/deploy-arm.sh 127 arm64
ssh root@192.168.1.127 "sudo /tmp/touch-timeout-staging/install-on-rpi.sh"
ssh root@192.168.1.127 "systemctl is-active touch-timeout.service && echo OK"
```

Custom IP prefix (edit deploy-arm.sh):
```bash
RPI_IP_PREFIX="10.0.0"
./scripts/deploy-arm.sh 127 arm64  # Now targets 10.0.0.127
```

## Documentation

- INSTALLATION.md: Initial setup and configuration
- README.md: Feature overview
- CLAUDE.md: Architecture and coding standards
- Makefile: Build system reference
