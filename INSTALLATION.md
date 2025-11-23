# Installation

## Quick Install

```bash
# Clone repository
git clone https://github.com/ajmccaus/touch-timeout.git
cd touch-timeout

# Build and install
gcc -O2 -Wall -Wextra -o touch-timeout touch-timeout.c
sudo install -m 755 touch-timeout /usr/bin/
sudo install -m 644 touch-timeout.service /etc/systemd/system/
sudo install -m 644 touch-timeout.conf /etc/

# Using Makefile if installing on same system as compiler
make
sudo make install

# Configure (edit brightness, timeout, etc.)
sudo nano /etc/touch-timeout.conf

# Enable and start service
sudo systemctl daemon-reload
sudo systemctl enable --now touch-timeout.service
```

## Verify Installation

```bash
# Check service status
sudo systemctl status touch-timeout.service

# View logs
sudo journalctl -u touch-timeout.service -f
```

## Identify Your Touchscreen Device

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

<!-- ADD new section after "Service Management" -->

## Logging Configuration

### Production Mode (Silent)
Recommended for SD card longevity:
```bash
echo "log_level=0" | sudo tee -a /etc/touch-timeout.conf
sudo systemctl restart touch-timeout
```

Verification (should show only startup message):
```bash
journalctl -u touch-timeout -n 50
```

### Debug Mode (Verbose)
For troubleshooting:

**Option 1: Temporary (command line)**
```bash
sudo systemctl stop touch-timeout
sudo touch-timeout -df  # Foreground + debug mode
# Press Ctrl+C to exit
sudo systemctl start touch-timeout
```

**Option 2: Persistent (config file)**
```bash
sudo sed -i 's/^log_level=.*/log_level=2/' /etc/touch-timeout.conf
sudo systemctl restart touch-timeout
journalctl -f -u touch-timeout  # Watch logs in real-time
```

**Restore silent mode:**
```bash
sudo sed -i 's/^log_level=.*/log_level=0/' /etc/touch-timeout.conf
sudo systemctl restart touch-timeout
```

### Log Levels Explained

| Level | Name | Output | Use Case | SD Writes |
|-------|------|--------|----------|-----------|
| 0 | Silent | Startup only | Production (default) | 1 at boot |
| 1 | Info | + state changes | Monitoring brightness behavior | 3-5 per day |
| 2 | Debug | + all touch events | Troubleshooting input issues | 50-200 per day |

### Foreground Mode (`-f`)
Run without daemonizing (useful for systemd Type=simple or manual testing):
```bash
touch-timeout -f  # Uses syslog, stays in foreground
touch-timeout -df # Uses stderr, stays in foreground (best for debugging)
```

**Note:** Systemd users typically don't need `-f` flag as the service unit handles process management.

## Uninstall

```bash
sudo systemctl stop touch-timeout.service
sudo systemctl disable touch-timeout.service
sudo rm /usr/bin/touch-timeout /etc/systemd/system/touch-timeout.service
sudo systemctl daemon-reload
```
