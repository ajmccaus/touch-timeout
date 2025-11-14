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

## Uninstall

```bash
sudo systemctl stop touch-timeout.service
sudo systemctl disable touch-timeout.service
sudo rm /usr/bin/touch-timeout /etc/systemd/system/touch-timeout.service
sudo systemctl daemon-reload
```
