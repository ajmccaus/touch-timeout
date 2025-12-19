---
version: "2.0.0"
updated: 2025-12-19
---

# Touch-Timeout Architecture

Current implementation state. This document is descriptive (how it DOES work).

For design intent and principles, see [DESIGN.md](DESIGN.md).

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

**state.h** - Pure state machine (caller provides time):
- `state_init()` - Initialize with brightness and timeout values
- `state_touch(now_ms)` - Handle touch, return new brightness or -1
- `state_timeout(now_ms)` - Check timeout, return new brightness or -1
- `state_get_timeout_ms(now_ms)` - Return ms until next transition
- `state_get_brightness()` - Return current brightness
- `state_get_current()` - Return current state enum

**main.c** - Inlined functions:
- `parse_args()` - getopt_long CLI parsing
- `open_backlight()`, `open_input()` - Device file access
- `set_brightness()` - Write to sysfs
- `drain_touch_events()` - Read and discard input events
- `now_ms()` - CLOCK_MONOTONIC timestamp

## Event Loop

```c
while (g_running) {
    int timeout_ms = state_get_timeout_ms(&state, now_ms());
    int ret = poll(&pfd, 1, timeout_ms);

    if (ret > 0 && (pfd.revents & POLLIN)) {
        // Touch event
        if (drain_touch_events(input_fd)) {
            new_bright = state_touch(&state, now_ms());
        }
    } else if (ret == 0) {
        // Timeout
        new_bright = state_timeout(&state, now_ms());
    }

    // Update brightness if changed
    if (new_bright >= 0 && new_bright != cached_brightness) {
        set_brightness(bl_fd, new_bright);
        cached_brightness = new_bright;
    }
}
```

**Key design choices:**
- Single `poll()` with timeout (no timerfd)
- Pure state machine - caller owns time via `CLOCK_MONOTONIC`
- Brightness caching - avoid redundant sysfs writes
- SIGUSR1 wake support for external integration

## Configuration

**No config file** - CLI-only configuration:

```bash
touch-timeout -b 200 -t 600 -d 20
```

**Customization via systemd:**

```bash
sudo systemctl edit touch-timeout
# Add: [Service]
#      ExecStart=
#      ExecStart=/usr/bin/touch-timeout -b 200 -t 600
```

See [README.md - Configuration](../README.md#configuration) for all options.

## Build System

**Makefile targets:**

| Target | Description |
|--------|-------------|
| `make` | Native build |
| `make arm64` | Cross-compile ARM 64-bit |
| `make arm32` | Cross-compile ARM 32-bit |
| `make deploy-arm64 RPI=<ip>` | Build + deploy + install |
| `make test` | Run unit tests |
| `make coverage` | Generate coverage report |

**Compiler flags:**
- `-std=c99 -D_GNU_SOURCE`
- `-Wall -Wextra`

## Test Infrastructure

**Test executables:**
- `tests/test_state` - State machine tests (22 tests)

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

## Performance Metrics

| Metric | Value |
|--------|-------|
| CPU (idle) | < 0.05% |
| Memory (RSS) | ~200 KB |
| Touch latency | ~45 ms |
| File descriptors | 2 |

## Known Limitations

1. **Linux-only** - Requires input subsystem, sysfs
2. **Single display** - No multi-display support
3. **Fixed device paths** - `/sys/class/backlight`, `/dev/input`
4. **Touchscreen only** - Keyboard/mouse out of scope

## References

- [DESIGN.md](DESIGN.md) - Design intent and principles
- [ROADMAP.md](ROADMAP.md) - Future plans
- [README.md](../README.md) - User documentation
- [INSTALLATION.md](INSTALLATION.md) - Deployment guide
