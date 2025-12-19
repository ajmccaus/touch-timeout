# touch-timeout
Lightweight touchscreen backlight manager for Raspberry Pi 7" displays. Automatically dims and powers off the display during inactivity, instantly restoring on touch. Optimized for minimal Linux distributions like HifiBerry OS.

**Version:** 2.0.0 | [CHANGELOG](CHANGELOG.md) | [ARCHITECTURE](doc/ARCHITECTURE.md)

## Getting Started

Choose your installation method:

### For Raspberry Pi Users

**Installing directly on your Raspberry Pi:**

```bash
git clone https://github.com/ajmccaus/touch-timeout.git
cd touch-timeout
make && sudo make install
```

See [INSTALLATION.md](doc/INSTALLATION.md) for complete guide.

### For Developers

**Cross-compile on Linux/WSL2 and deploy to Raspberry Pi:**

```bash
make deploy-arm64 RPI=<IP_ADDRESS>
```

See [INSTALLATION.md - Remote Deployment](doc/INSTALLATION.md#method-2-remote-deployment-cross-compilation) for details.

---

## Features

- **Works out-of-box** - sensible defaults, no configuration required
- **Configurable via CLI** - all options available as command-line arguments
- **Power efficient** - <0.05% CPU usage during idle (poll-based, zero CPU when waiting)
- **External wake support** - SIGUSR1 for shairport-sync integration
- **Hardware-aware** - respects display max brightness, prevents flicker
- Systemd integration with graceful shutdown

## Default Behavior

| Event | Action |
|-------|--------|
| **Service start** | Full brightness (150), ready for touch |
| **Touch detected** | Restores full brightness, resets idle timer |
| **Idle 30s** | Dims to 10% brightness (minimum 10) |
| **Idle 5 min** | Powers off display (brightness = 0) |
| **SIGUSR1** | Wakes display (for shairport-sync integration) |
| **Systemd stop** | Restores brightness, graceful shutdown |

## Configuration

**Defaults work for most setups.** To customize, use systemd override:

```bash
sudo systemctl edit touch-timeout
```

Add your options:

```ini
[Service]
ExecStart=
ExecStart=/usr/bin/touch-timeout -b 200 -t 600
```

**CLI Options:**

| Option | Description | Default |
|--------|-------------|---------|
| `-b, --brightness=N` | Full brightness (15-255) | 150 |
| `-t, --timeout=N` | Off timeout in seconds (10-86400) | 300 |
| `-d, --dim-percent=N` | Dim at N% of timeout (1-100) | 10 |
| `-l, --backlight=NAME` | Backlight device | rpi_backlight |
| `-i, --input=NAME` | Input device | event0 |
| `-f, --foreground` | Run in foreground, log to stderr | |
| `-v, --verbose` | Verbose logging | |

**External Wake (shairport-sync):**

```bash
# Wake display programmatically
pkill -USR1 touch-timeout
```

See [INSTALLATION.md - Configuration](doc/INSTALLATION.md#configuration) for more examples.

## Performance

Optimized for 24/7 embedded operation: <0.05% CPU idle, ~200 KB memory, instant touch response.

## Future Roadmap

See [ROADMAP.md](doc/ROADMAP.md) for planned features.

## Support Policy
This is a learning project maintained in my spare time.
- Bug reports: Welcomed with reproduction steps
- Feature requests: Considered but not guaranteed
- PRs: No guarantees on response time
