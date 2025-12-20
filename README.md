# touch-timeout
Lightweight touchscreen backlight manager for Raspberry Pi 7" displays. Automatically dims and powers off the display during inactivity, instantly restoring on touch. Optimized for minimal Linux distributions like HifiBerry OS.

**Version:** 0.7.0 | [CHANGELOG](CHANGELOG.md) | [ARCHITECTURE](doc/ARCHITECTURE.md)

## Quick Start

**From your Linux/WSL2 development machine:**

```bash
git clone https://github.com/ajmccaus/touch-timeout.git
cd touch-timeout
make deploy-arm64 RPI=<IP_ADDRESS>
```

This cross-compiles and deploys to your Raspberry Pi in one step. See [INSTALLATION.md](doc/INSTALLATION.md) for prerequisites and options.

**Or build directly on Raspberry Pi:**

```bash
git clone https://github.com/ajmccaus/touch-timeout.git
cd touch-timeout
make && sudo make install
```

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
| `-v, --verbose` | Verbose logging | |

**External Wake (shairport-sync):**

```bash
# Wake display programmatically
pkill -USR1 touch-timeout
```

See [INSTALLATION.md - Configuration](doc/INSTALLATION.md#configuration) for more examples.

## Performance

Optimized for 24/7 embedded operation: <0.05% CPU idle, ~200 KB memory, instant touch response.

## Scope & Non-Goals

This daemon manages **touchscreen timeout only**.

**Out of scope:**
- Keyboard/mouse input (use DPMS/xscreensaver)
- Multi-device input monitoring
- Adaptive brightness / ambient light sensing
- Audio activity monitoring (use SIGUSR1 integration instead)
- Web API / D-Bus interface

**Contributing:** Before proposing features, consider: Does this solve a real problem for touchscreen users? Can it be done with SIGUSR1 or existing configuration?

## About This Project

This is a learning project and case study in non-expert AI-assisted software development. It chronicles what happens when you use AI to build software without proper design specifications—and the lessons learned from untangling the result.

See [PROJECT-HISTORY.md](doc/PROJECT-HISTORY.md) for the full story.

**Support:**
- Bug reports welcomed with reproduction steps
- Feature requests considered but not guaranteed
- PRs: No guarantees on response time
