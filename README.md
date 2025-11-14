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

**Idle CPU**: <0.1% (100ms poll interval)  
**Wake latency**: <100ms from touch to full brightness

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

## To build and deploy:
see installation instructions (Installation.md)

### Quick Install

```bash
# Clone repository
git clone https://github.com/ajmccaus/touch-timeout.git
cd touch-timeout

# Build and install
gcc -O2 -Wall -Wextra -o touch-timeout touch-timeout.c
sudo install -m 755 touch-timeout /usr/bin/
sudo install -m 644 touch-timeout.service /etc/systemd/system/
sudo install -m 644 touch-timeout.conf /etc/

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

### Identify Your Touchscreen Device

If the default `device=event0` doesn't work:

```bash
# List input devices
ls -l /dev/input/by-path/

# Find touchscreen (usually contains "event-touch" or "touchscreen")
# Example output: platform-gpu-event → ../../event0

# Update config with correct event number
sudo nano /etc/touch-timeout.conf  # Change device=event0 to your device
sudo systemctl restart touch-timeout.service
```

### Uninstall

```bash
sudo systemctl stop touch-timeout.service
sudo systemctl disable touch-timeout.service
sudo rm /usr/bin/touch-timeout /etc/systemd/system/touch-timeout.service
sudo systemctl daemon-reload
```
