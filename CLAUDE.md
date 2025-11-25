# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

touch-timeout is a lightweight touchscreen backlight manager for Raspberry Pi 7" displays. It automatically dims and powers off the display during inactivity, instantly restoring brightness on touch. Optimized for minimal Linux distributions like HifiBerry OS.

## Build Commands

```bash
# Build
make

# Or manually with gcc
gcc -O2 -Wall -Wextra -o touch-timeout touch-timeout.c

# Install (requires sudo)
sudo make install

# Clean build artifacts
make clean

# Run unit tests
make test

# Run tests with coverage report
make coverage
```

## Architecture

**Single-file C application** (`touch-timeout.c`) - the main application is a monolithic C file (~500 lines) containing:
- State machine with three states: `STATE_FULL`, `STATE_DIMMED`, `STATE_OFF`
- `struct display_state` tracks brightness, timeouts, and current state
- Poll-based event loop monitoring `/dev/input/eventX` for touch events
- Sysfs interface for backlight control via `/sys/class/backlight/`
- Signal handlers for graceful systemd shutdown (SIGTERM/SIGINT)

**Key functions:**
- `load_config()` - parses `/etc/touch-timeout.conf`
- `set_brightness()` - cached sysfs writes (prevents redundant hardware access)
- `check_timeouts()` - time-based dim/off logic with clock adjustment handling
- `restore_brightness()` - instant brightness restore on touch

**Planned modular split** (v1.1.0): `src/` contains stub files for future refactor into `logic.c/h`, `io.c/h`, `main.c`.

## Configuration

Runtime config: `/etc/touch-timeout.conf` (see `config/touch-timeout.conf` for template)

Key settings: `brightness`, `off_timeout`, `dim_percent`, `poll_interval`, `backlight`, `device`

## Testing

**Unit tests** (`tests/`): Tests pure functions (`trim`, `safe_atoi`, `load_config`) and validates constants/calculations. Uses `#include "touch-timeout.c"` pattern to access static functions. Run with `make test` or `make coverage`.

**Hardware testing**: This is a Raspberry Pi-specific daemon. To test on target:
1. Cross-compile or build directly on RPi
2. Install systemd service from `systemd/touch-timeout.service`
3. Check logs: `sudo journalctl -u touch-timeout.service -f`

## Important Constraints

- Brightness values >200 may cause unexpected behavior on RPi 7" official touchscreen
- Minimum brightness is 15 (avoids flicker), minimum dim brightness is 10
- Off timeout must be >= 10 seconds
- Uses `assert()` for internal consistency checks - these will crash on violation
