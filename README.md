# touch-timeout
Lightweight touchscreen backlight manager for Raspberry Pi 7" displays. Automatically dims and powers off the display during inactivity, instantly restoring on touch. Optimized for minimal Linux distributions like HifiBerry OS.

## Features

- **Zero-configuration defaults**: Works out-of-box with sensible settings
- **Configurable dimming**: Percentage-based dim timing (10-100% of off timeout)
- **Power efficient**: <0.1% CPU usage during idle
- **Hardware-optimized**: Respects display max brightness, prevents flicker
- **Robust**: Handles missed poll cycles, system suspend/resume
- **Production-ready**: Systemd integration with graceful shutdown

## Behavior

| Event | Action |
|-------|--------|
| **Service start** | Reads `/etc/touch-timeout.conf`, applies brightness settings |
| **Touch detected** | Restores full brightness, resets idle timer |
| **Idle (50% of timeout)** | Dims to `user_brightness ÷ 10` (minimum 10) |
| **Idle (100% of timeout)** | Powers off display (brightness = 0) |
| **No config file** | Uses defaults: 100 brightness, 300s timeout, 50% dim time (150s for default 300s timeout) |
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

Tested over 9+ minutes continuous operation with no performance degradation.

## Configuration

Edit `/etc/touch-timeout.conf`:

```ini
brightness=150            # Active brightness (15-254, recommend ≤200 for RPi display)
off_timeout=300           # Seconds until screen off (minimum 10)
dim_percent=50            # When to dim (10-100% of off_timeout)
poll_interval=100         # Polling rate in ms (10-2000, recommend 50-1000)
backlight=rpi_backlight   # Device name in /sys/class/backlight/
device=event0             # Touchscreen in /dev/input/
```
**Note**: For RPi official 7" touchscreen, brightness >200 reduces brightness and current draw (see https://forums.raspberrypi.com/viewtopic.php?t=216821). Recommend `brightness=200` or lower.

## v1.1.0 Goals
- [ ] Configurable dim_brightness (calculation provides default of user_brightness ÷ 10, config file provides user override)

## To build and deploy:
see installation instructions (INSTALLATION.md)

## Support Policy
This is a hobby project maintained in my spare time.
- Bug reports: Welcomed with reproduction steps
- Feature requests: Considered but not guaranteed
- PRs: We'll see
- Commercial support: Not available
