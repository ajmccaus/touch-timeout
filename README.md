# touch-timeout
Lightweight touchscreen backlight manager for Raspberry Pi 7" displays. Automatically dims and powers off the display during inactivity, instantly restoring on touch. Optimized for minimal Linux distributions like HifiBerry OS.

**Branch Context:** You are on the `refactoring-v2` branch (v2.0.0 modular architecture). The roadmap below describes the `main` branch (v1.x monolithic). See ARCHITECTURE.md for v2.0 details.

## Getting Started

Choose your installation method:

### For Raspberry Pi Users

**Installing directly on your Raspberry Pi:**

```bash
git clone https://github.com/ajmccaus/touch-timeout.git
cd touch-timeout
make && sudo make install
```

→ **[Complete guide: INSTALLATION.md - Method 1](INSTALLATION.md#method-1-direct-installation-on-raspberry-pi)**

### For Developers

**Cross-compile on Linux/WSL2 and deploy to Raspberry Pi:**

```bash
# On build machine
./scripts/deploy-arm.sh <IP_ADDRESS> arm64

# On Raspberry Pi (after deployment)
sudo /run/touch-timeout-staging/install.sh
```

→ **[Complete guide: INSTALLATION.md - Method 2](INSTALLATION.md#method-2-remote-deployment-cross-compilation)**

---

## Features

- **Zero-configuration defaults**: Works out-of-box with sensible settings
- **Configurable dimming**: Percentage-based dim timing (1-100% of off timeout)
- **Power efficient**: <0.1% CPU usage during idle
- **Hardware-optimized**: Respects display max brightness, prevents flicker
- **Robust**: Handles missed poll cycles, system suspend/resume
- Systemd integration with graceful shutdown

## Behavior

| Event | Action |
|-------|--------|
| **Service start** | Reads `/etc/touch-timeout.conf`, applies brightness settings |
| **Touch detected** | Restores full brightness, resets idle timer |
| **Idle (10% of timeout)** | Dims to `user_brightness ÷ 10` (minimum 10) |
| **Idle (100% of timeout)** | Powers off display (brightness = 0) |
| **No config file** | Uses defaults: 150 brightness, 300s timeout, 10% dim time (30s for default 300s timeout) |
| **Invalid config** | Logs warning, falls back to defaults |
| **Systemd stop** | Gracefully closes file descriptors |

## Performance

Benchmarked on Raspberry Pi 4 (1.5GHz ARM Cortex-A72):

| Metric | Value | Notes |
|--------|-------|-------|
| **CPU (idle)** | 0.0% | Poll-based event loop with zero overhead |
| **CPU (active)** | <1% | Brief spikes during touch events |
| **Memory (RSS)** | 0.2 MB | Minimal footprint, no leaks after extended runtime |
| **Latency** | <200ms | Touch-to-restore response time |
| **I/O efficiency** | Cached | No redundant sysfs writes |

Tested over 24+ hours continuous operation with no performance degradation.

## Performance Testing

Quick manual tests on the RPi:

```bash
# Get process ID
PID=$(pgrep touch-timeout)

# CPU and memory usage
ps aux | grep touch-timeout

# Detailed resource usage
top -b -n 1 -p $PID

# SD card writes (run twice with delay to measure delta)
grep write_bytes /proc/$PID/io

# File descriptor count (should stay constant)
ls /proc/$PID/fd | wc -l

# System call count over 10 seconds
timeout 10 strace -c -p $PID 2>&1 | tail -20
```

For comprehensive automated testing, use the performance test script:

```bash
# Transfer script to RPi
scp scripts/test-performance.sh root@192.168.1.XXX:/tmp/

# Run on RPi
ssh root@192.168.1.XXX "bash /tmp/test-performance.sh"
```

## Configuration

Edit `/etc/touch-timeout.conf`:

```ini
brightness=150            # Active brightness (15-255, recommend ≤200 for RPi display)
off_timeout=300           # Seconds until screen off (minimum 10)
dim_percent=10            # When to dim (1-100% of off_timeout, default: 10)
backlight=rpi_backlight   # Device name in /sys/class/backlight/
device=event0             # Touchscreen in /dev/input/
```
**Note**: For RPi official 7" touchscreen, brightness >200 reduces brightness and current draw (see https://forums.raspberrypi.com/viewtopic.php?t=216821). Recommend `brightness=200` or lower.

## Product Roadmap

### v1.0.1: Zero-Wear Patch (In Progress)
- [ ] **Configurable Logging**: `log_level=0/1/2` in config (0=silent, 1=info, 2=debug)
- [ ] **Debug Flag**: `-d/--debug` enables verbose logging for testing
- [ ] **Foreground Mode**: `-f` flag for development (uses stderr, no daemonize)
- [ ] **Reduced Boot Writes**: Batched startup logs (3→1 syslog call, 67% reduction)
- [ ] **Quiet Production**: Default `log_level=0` eliminates SD writes from logging
- [ ] **NTP Stability**: Replaced hot-path asserts with graceful error handling (prevents crashes on clock adjustments)
- [ ] **SD Write Impact**: 1 write/boot + 0 runtime events (vs. 10-100/day in v1.0.0)

**Migration Note**: Add `log_level=0` to `/etc/touch-timeout.conf` for silent operation.  
**Dev Tip**: Use `touch-timeout -df` for foreground debugging without config changes.

### v1.1.0: Clean Foundation (Planned)
- [ ] **Modular Architecture**: 3-file split (`logic.c/h`, `io.c/h`, `main.c`)
- [ ] **Unit Tests**: Makefile test target with edge-case coverage
- [ ] **Config-Based Logging**: `log_level=none/info/debug` in `/etc/touch-timeout.conf` (replaces `-d` flag)
- [ ] **Enhanced Dim Control**: Configurable `dim_brightness` (% of brightness, 5%-100%, min 10)
- [ ] **Extended Dim Timeout**: Support 1%-100% of `off_timeout` (1s minimum)

### v1.2.0: Hotplug Foundation (Planned)
- [ ] **USB Hotplug**: `inotify` monitoring for plug-and-play device detection
- [ ] **Multi-Device Polling**: Support up to 10 input devices (static config list)
- [ ] **Robust Event Loop**: Handle device add/remove during runtime

### v1.3.0: Universal Input (Planned)
- [ ] **Keyboard/Mouse Support**: Monitor all input device types (not just touch)
- [ ] **Auto-Discovery**: Scans `/dev/input/by-path/` on startup (zero config)
- [ ] **Device Classification**: Filters by capability flags (touch vs keyboard vs mouse)

### v1.4.0: Audio Integration (Proposed)
- [ ] **Playback Detection**: Optional ALSA/PulseAudio activity resets timeout
- [ ] **SSH Detection**: Prevent screen-off during remote sessions

## To build and deploy:
see installation instructions (INSTALLATION.md)

## Support Policy
This is a hobby project maintained in my spare time.
- Bug reports: Welcomed with reproduction steps
- Feature requests: Considered but not guaranteed
- PRs: We'll see
- Commercial support: Not available
