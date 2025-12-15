---
version: "2.0.0"
updated: 2025-12-14
---

# Touch-Timeout Architecture

Current implementation state. This document is descriptive (how it DOES work).

For design intent and principles, see [DESIGN.md](DESIGN.md).

## Module Structure

```
src/
├── config.c/h      # Table-driven configuration with validation
├── display.c/h     # Display/backlight hardware abstraction
├── input.c/h       # Touch input device abstraction
├── state.c/h       # State machine logic (FULL→DIMMED→OFF)
├── timer.c/h       # POSIX timerfd abstraction (CLOCK_MONOTONIC)
└── main.c          # Event loop with systemd integration
```

## Data Flow

```
[Input HAL] ─────▶ [Main Loop] ◀───── [Timer HAL]
(src/input.c)         │              (src/timer.c)
                      │
                      ▼
               [State Machine]
               (src/state.c)
                      │
                      ▼
               [Display HAL]
              (src/display.c)
```

## Module Interfaces

See source headers (`.h` files) for complete API signatures.

**state.h:**
- `state_init()` - Initialize with brightness and timeout values
- `state_handle_event()` - Process TOUCH or TIMEOUT event
- `state_get_current()` - Return current state
- `state_get_brightness()` - Return current brightness
- `state_get_next_timeout()` - Return seconds until next transition

**config.h:**
- `config_init()` - Initialize with defaults
- `config_load()` - Load from file
- `config_set_value()` - Override individual values

**display.h:**
- `display_open()` / `display_close()` - Lifecycle
- `display_set_brightness()` - Set brightness (cached)
- `display_get_brightness()` - Read current brightness
- `display_get_max_brightness()` - Read hardware maximum

**input.h:**
- `input_open()` / `input_close()` - Lifecycle
- `input_get_fd()` - Return pollable file descriptor
- `input_has_events()` - Check and drain pending events

**timer.h:**
- `timer_create_ctx()` / `timer_destroy()` - Lifecycle
- `timer_arm()` - Set timeout in seconds
- `timer_get_fd()` - Return pollable file descriptor
- `timer_check_expiration()` - Check and clear timer event

## Event Loop Implementation

Located in `main.c`:

1. **Initialization** (lines ~50-120)
   - Load config, open HAL modules, init state machine
   - Set initial brightness, arm timer

2. **Poll loop** (lines ~130-180)
   - Block on input_fd and timer_fd
   - Process events, update state, apply brightness

3. **Cleanup** (lines ~190-210)
   - Restore brightness, close modules

## Configuration

**File:** `/etc/touch-timeout.conf`

**Parameters:** See [README.md - Configuration](../README.md#configuration)

**Table-driven parsing:**
- Each parameter has a descriptor in `config_params[]` array
- Single parsing function handles all types
- Automatic validation against min/max constraints

## Systemd Integration

**Service file:** `systemd/touch-timeout.service`

- `Type=simple` - Compatible with minimal systems
- `Restart=on-failure` - Auto-restart on crashes
- `LogLevelMax=info` - Filter DEBUG messages (zero runtime writes)

**Optional features** (requires libsystemd at build time):
- sd_notify() for startup confirmation
- Watchdog pinging

## Build System

**Makefile targets:**

| Target | Output | Description |
|--------|--------|-------------|
| `make` | `build/native/touch-timeout` | Native build |
| `make test` | Test executables | Run unit tests |
| `make coverage` | Coverage report | Generate coverage |
| `make arm32` | `build/arm32/touch-timeout` | Cross-compile ARM 32-bit |
| `make arm64` | `build/arm64/touch-timeout` | Cross-compile ARM 64-bit |
| `make clean` | - | Clean native artifacts |
| `make clean-all` | - | Clean all build artifacts |

**Compiler flags:**
- `-std=c17 -D_POSIX_C_SOURCE=200809L`
- `-Wall -Wextra -Werror`

## Test Infrastructure

**Test executables:**
- `build/native/test_config` - Configuration module tests
- `build/native/test_state` - State machine tests

**Test counts:**
- Config: 33 tests (parsing, validation, security)
- State: 21 tests (transitions, edge cases)

**Performance testing:** `scripts/test-performance.sh` (on-device)

## Deployment

**Scripts:**
- `scripts/deploy-arm.sh` - Cross-compile and deploy to RPi
- `scripts/install.sh` - Install on target system

**Staging:** `/run/touch-timeout-staging/` (tmpfs)

See [INSTALLATION.md](INSTALLATION.md) for deployment procedures.

## Performance Metrics

| Metric | Value |
|--------|-------|
| CPU (idle) | < 0.05% |
| Memory (RSS) | ~1.2 MB |
| Touch latency | ~45 ms |
| SD writes/day | ~15 |
| File descriptors | 3 |

## Known Limitations

1. **Linux-only** - Requires timerfd, input subsystem, sysfs
2. **Single display** - No multi-display support
3. **Fixed device paths** - `/sys/class/backlight`, `/dev/input`
4. **Touchscreen only** - Keyboard/mouse out of scope

## References

- [DESIGN.md](DESIGN.md) - Design intent and principles
- [ROADMAP.md](ROADMAP.md) - Future plans
- [README.md](../README.md) - User documentation
- [INSTALLATION.md](INSTALLATION.md) - Deployment guide
