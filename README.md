# touch-timeout
Lightweight touchscreen backlight manager for Raspberry Pi 7" displays. Automatically dims and powers off the display during inactivity, instantly restoring on touch. Optimized for minimal Linux distributions like HifiBerry OS.

**Version:** 2.0.0 | [CHANGELOG](CHANGELOG.md) | [ARCHITECTURE](ARCHITECTURE.md)

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
# One-step deployment (auto-install, recommended)
./scripts/deploy-arm.sh <IP_ADDRESS> arm64
```

→ **[Complete guide: INSTALLATION.md - Method 2](INSTALLATION.md#method-2-remote-deployment-cross-compilation)**

---

## Features

- **Works out-of-box with sensible defaults** - no configuration required!
- **Configurable dimming**: Percentage-based dim timing (1-100% of off timeout)
- **Power efficient**: <0.05% CPU usage during idle
- **Hardware-optimized**: Respects display max brightness, prevents flicker
- **Robust**: Handles missed poll cycles, system suspend/resume
- Systemd integration with graceful shutdown

## Default Behavior

| Event | Action |
|-------|--------|
| **Service start** | Uses hardcoded defaults or `/etc/touch-timeout.conf` if present (see [Configuration](#configuration)) |
| **Touch detected** | Restores full brightness, resets idle timer |
| **Idle (10% of timeout)** | Dims to `user_brightness ÷ 10` (minimum 10) |
| **Idle (100% of timeout)** | Powers off display (brightness = 0) |
| **Invalid config** | Logs warning, falls back to defaults |
| **Systemd stop** | Gracefully closes file descriptors |

## Performance

Benchmarked on Raspberry Pi 4 (1.5GHz ARM Cortex-A72):

| Metric | Value | Notes |
|--------|-------|-------|
| **CPU (idle)** | <0.05% | Poll-based event loop, near-zero overhead |
| **CPU (active)** | <1% | Brief spikes during touch events |
| **Memory (RSS)** | 0.2 MB | Minimal footprint, no leaks after extended runtime |
| **Latency** | <200ms | Touch-to-restore response time |
| **I/O efficiency** | Cached | No redundant sysfs writes |

Tested over 24+ hours continuous operation with no performance degradation.

## Configuration

**Hardcoded Defaults:**
- `brightness=150` - Active screen brightness (15-255, recommend ≤200 for RPi official 7" touchscreen)
- `off_timeout=300` - 5 minutes until screen off (minimum 10 seconds)
- `dim_percent=10` - Dims at 10% of timeout (e.g., 30s for default 300s timeout)
- `backlight=rpi_backlight` - RPi official 7" touchscreen backlight device
- `device=event0` - First input device

**To customize:** See [INSTALLATION.md - Configuration](INSTALLATION.md#configuration) for complete examples:
- Creating `/etc/touch-timeout.conf` (copy/paste template provided)
- Using CLI arguments in systemd service file

## Future Roadmap

See [ROADMAP.md](ROADMAP.md) for planned features.

## Support Policy
This is a learning project maintained in my spare time.
- Bug reports: Welcomed with reproduction steps
- Feature requests: Considered but not guaranteed
- PRs: No guarantees on response time
- Commercial support: Not available
