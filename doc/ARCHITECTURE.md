---
version: "0.7"
updated: 2025-12-19
---

# Touch-Timeout Architecture

Current implementation state. This document is descriptive (how it DOES work).

## Module Structure

Simplified 2-module architecture:

```
src/
├── main.c          # CLI, device I/O, event loop
└── state.c/h       # Pure state machine (no I/O)
```

## Block Diagram

```
                              main.c
    ┌────────────────────────────────────────────────────────────┐
    │                                                            │
    │  ┌──────────────┐  ┌──────────────┐  ┌─────────────────┐  │
    │  │ CLI Parsing  │  │  Device I/O  │  │   Event Loop    │  │
    │  │              │  │              │  │                 │  │
    │  │ parse_args() │  │ open_*()     │  │ poll() on input │
    │  │ usage()      │  │ set_bright() │  │ ├─ POLLIN: touch│  │
    │  │              │  │ drain_*()    │  │ └─ timeout: dim │  │
    │  └──────────────┘  └──────────────┘  └─────────────────┘  │
    │                                               │            │
    │                                               │ state_*()  │
    │                                               ▼            │
    └───────────────────────────────────────────────────────────┘
                                    │
                                    ▼
    ┌────────────────────────────────────────────────────────────┐
    │                        state.c/h                           │
    │                                                            │
    │  Pure Moore State Machine - No I/O, No Time Calls          │
    │                                                            │
    │  ┌─────────┐      ┌───────────┐      ┌──────────┐         │
    │  │  FULL   │─────▶│  DIMMED   │─────▶│   OFF    │         │
    │  │ (bright)│      │   (dim)   │      │   (0)    │         │
    │  └─────────┘      └───────────┘      └──────────┘         │
    │       ▲                 │                  │               │
    │       └─────────────────┴──────────────────┘               │
    │                    touch event                             │
    └────────────────────────────────────────────────────────────┘
```

## Module Interfaces

**state.h** - Pure state machine (caller provides time in seconds):
- `state_init()` - Initialize with brightness and timeout values
- `state_touch(now_sec)` - Handle touch, return new brightness or -1
- `state_timeout(now_sec)` - Check timeout, return new brightness or -1
- `state_get_timeout_sec(now_sec)` - Return seconds until next transition
- `state_get_brightness()` - Return brightness for current state
- `state_get_current()` - Return current state enum

## Event Loop

The main loop uses blocking I/O for zero CPU idle:

1. **Wait**: poll() blocks on input fd with timeout from state machine
2. **Touch event**: Drain events, notify state machine, apply brightness if changed
3. **Timeout**: Notify state machine, apply brightness if changed
4. **Signal**: SIGUSR1 wakes display; SIGTERM/SIGINT trigger graceful shutdown

Loop exits when `g_running` becomes false (signal received).

**Key design choices:**
- Single `poll()` with timeout (no timerfd)
- Pure state machine - caller owns time via `CLOCK_MONOTONIC`
- Brightness caching - avoid redundant sysfs writes
- SIGUSR1 wake support for external integration

## Build System

See `make help` or Makefile for available targets. Key flags: `-std=c99 -D_GNU_SOURCE -Wall -Wextra`

## Test Infrastructure

**Test executables:**
- `tests/test_state` - State machine tests (48 tests)

**Testing approach:**
- Pure state machine = no mocking needed
- Pass mock timestamps directly to functions
- Coverage target: 95%+

## Systemd Integration

**Service file:** `systemd/touch-timeout.service`

- `Type=simple` - Compatible with minimal systems
- `Restart=on-failure` - Auto-restart on crashes
- `LogLevelMax=info` - Filter DEBUG messages

**Optional features** (requires libsystemd at build time):
- `sd_notify()` for startup confirmation

## Known Limitations

1. **Linux-only** - Requires input subsystem, sysfs
2. **Single display** - No multi-display support
3. **Fixed device paths** - `/sys/class/backlight`, `/dev/input`
4. **Touchscreen only** - Keyboard/mouse out of scope

## References

- [README.md](../README.md) - User documentation
- [INSTALLATION.md](INSTALLATION.md) - Deployment guide
