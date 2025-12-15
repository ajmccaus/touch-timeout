# Touch-Timeout Design

Design intent and principles guiding implementation. This document is prescriptive (how it SHOULD work).

For current implementation state, see [ARCHITECTURE.md](ARCHITECTURE.md).

## Overview

A daemon for Raspberry Pi touchscreens that automatically dims and blanks the display after periods of inactivity, then instantly restores brightness on touch input. Designed for 24/7 embedded kiosk operation with minimal resource usage.

## Design Philosophy

**Simplicity and user experience first.** All decisions optimize for:

- **First-time users succeeding quickly** - Installation works without SSH keys, documentation is scannable
- **Clear, maintainable code** - Obvious over clever, modular over monolithic, testable over tightly-coupled
- **Automated testing** - 95%+ coverage without hardware, test-first development
- **Embedded constraints** - Minimize SD card writes, zero CPU when idle, robust against power loss

## Constraints

| Constraint | Requirement | Rationale |
|------------|-------------|-----------|
| CPU (idle) | < 0.1% | Battery/thermal on always-on display |
| Memory (RSS) | < 5 MB | Embedded system with limited RAM |
| Touch latency | < 200 ms | User-perceptible responsiveness |
| SD card writes | < 50/day | Limited write cycles (~10k-100k) |
| File descriptors | ≤ 4 | input, timer, backlight, signal |
| Dependencies | libc only | Minimal buildroot compatibility |
| Clock source | CLOCK_MONOTONIC | Immune to NTP/suspend drift |
| Graceful shutdown | < 5 seconds | systemd timeout compliance |

## Module Structure

```
┌─────────────────────────────────────────────────────┐
│                    Main Loop                         │
│              (event orchestration)                   │
├──────────┬──────────┬──────────┬───────────────────┤
│  Config  │  Input   │  Timer   │     Display       │
│  Module  │   HAL    │   HAL    │       HAL         │
└──────────┴──────────┴──────────┴───────────────────┘
                 │          │              │
                 ▼          ▼              ▼
            /dev/input  timerfd      /sys/backlight
```

**Responsibilities:**
- **Config** - Parse configuration, validate values, provide defaults
- **Input HAL** - Abstract touch input device, provide pollable fd
- **Timer HAL** - Abstract timerfd, provide CLOCK_MONOTONIC timing
- **Display HAL** - Abstract backlight control, cache brightness
- **Main Loop** - Orchestrate modules, handle events, manage lifecycle

## State Machine

Pure finite state machine with zero I/O, zero time tracking.

```
┌──────┐  timeout   ┌────────┐  timeout   ┌─────┐
│ FULL │ ─────────▶ │ DIMMED │ ─────────▶ │ OFF │
└──────┘            └────────┘            └─────┘
    ▲                   │                     │
    │                   │ touch               │ touch
    └───────────────────┴─────────────────────┘
```

**Transition Rules:**

```
ON TOUCH from any state:
    transition to FULL
    output: user_brightness
    caller arms timer: dim_timeout seconds

ON TIMEOUT from FULL:
    transition to DIMMED
    output: dim_brightness (user_brightness / 10, min 10)
    caller arms timer: off_timeout - dim_timeout seconds

ON TIMEOUT from DIMMED:
    transition to OFF
    output: 0 (display off)
    caller disarms timer (wait for touch)

ON TIMEOUT from OFF:
    no transition (invalid, timer should be disarmed)
```

**State Machine Purity:**
- Zero I/O operations (no file access, no device control)
- Zero time tracking (timer module owns all timing)
- Zero dependencies beyond standard C
- 100% unit testable without mocks

## Event Loop

```
INITIALIZE:
    load configuration from file (use defaults if missing)
    open input device (touch events)
    open display device (backlight control)
    create timer (monotonic clock)
    initialize state machine (FULL state)
    set initial brightness
    arm timer for dim timeout

LOOP while running:
    wait for events on [input_fd, timer_fd] (block indefinitely)

    IF input event:
        drain all pending events
        send TOUCH event to state machine
        apply brightness from state machine
        arm timer based on current state

    IF timer expired:
        read timer to clear event
        send TIMEOUT event to state machine
        apply brightness from state machine
        IF not in OFF state:
            arm timer based on current state

    IF signal received:
        set running = false

CLEANUP:
    restore full brightness
    close all file descriptors
```

## HAL Contracts

### Display HAL

- MUST provide open/close lifecycle with backlight device name
- MUST cache current brightness to avoid redundant writes
- MUST return error codes (not exceptions) for all operations
- MUST validate brightness range before hardware write
- Brightness range: 0-255 (clamped to device max_brightness)

### Input HAL

- MUST provide pollable file descriptor for event loop integration
- MUST drain all pending events on read (non-blocking)
- MUST distinguish touch events from other input types
- File descriptor ownership: module owns fd, caller must not close

### Timer HAL

- MUST use CLOCK_MONOTONIC (immune to system time changes)
- MUST provide pollable file descriptor
- MUST support rearming without recreating
- Timer precision: second-level (not sub-second)

## Error Handling

| Error | Handling | Recovery |
|-------|----------|----------|
| Config file missing | Use defaults | Continue |
| Config value invalid | Use default for that value | Log warning |
| Input device open fail | Exit with error | systemd restarts |
| Display device open fail | Exit with error | systemd restarts |
| Timer create fail | Exit with error | systemd restarts |
| Brightness write fail | Log error | Continue (cached value) |
| Signal received | Set shutdown flag | Graceful exit |

## Design Decisions

### Decision: Monotonic timer vs wall clock

**Chosen:** timerfd with CLOCK_MONOTONIC

**Rationale:**
- Immune to NTP adjustments
- Correct behavior across suspend/resume
- Event-driven (no polling)
- Integrates cleanly with poll() event loop

**Rejected:**
- time() comparisons: Vulnerable to clock changes
- SIGALRM: Signal handler complexity
- sleep(): Can't wait on multiple events

### Decision: Pure state machine

**Chosen:** State machine with zero I/O, zero time tracking

**Rationale:**
- 100% unit testable without mocks
- Single responsibility (state transitions only)
- Timer module owns all timing
- Predictable, verifiable behavior

**Rejected:**
- State machine with embedded timing: Hard to test, clock domain mixing
- State machine with I/O: Requires hardware mocks

### Decision: Hardware abstraction layers

**Chosen:** Separate HAL modules for display, input, timer

**Rationale:**
- Mock implementations for testing
- Platform portability (sysfs today, DRM future)
- Clear contracts and ownership
- Separate compilation units

**Rejected:**
- Direct sysfs calls in main: Not testable
- Single HAL module: Too many responsibilities

### Decision: SD card write optimization

**Chosen:** Three-layer strategy to minimize SD card wear

**Layers:**
1. **Deployment**: Stage to `/run/` (tmpfs) before install
2. **Logging**: `LogLevelMax=info` filters DEBUG (zero runtime journal writes)
3. **Runtime**: Display HAL caches brightness, skips redundant sysfs writes

**Rationale:**
- SD cards have limited write cycles (~10k-100k)
- 24/7 operation would exhaust card in months without optimization
- Target: < 50 writes/day

**Rejected:**
- Write-through (always write): ~360 writes/day
- Log everything to journal: Constant writes during operation

## Naming Conventions

**Types:**
- Struct typedefs: `module_s` suffix (e.g., `state_s`, `config_s`)
- Enum typedefs: `module_e` suffix (e.g., `state_type_e`, `state_event_e`)
- Avoids POSIX `_t` suffix conflicts

**Functions:**
- Public: `module_verb()` or `module_verb_noun()` (e.g., `state_init()`, `config_load()`)
- Static: Same pattern for consistency (e.g., `config_trim()`, `config_find_param()`)
- HAL pattern: `module_open()`, `module_close()`, `module_get_fd()`

**Variables:**
- Globals: `g_` prefix (e.g., `g_running`, `g_config`)
- Constants/macros: `MODULE_UPPER_CASE` (e.g., `CONFIG_DEFAULT_BRIGHTNESS`)
- Locals: `snake_case`

## Security Guidelines

Follow CERT C, POSIX, and MISRA C guidelines:

| Guideline | Requirement |
|-----------|-------------|
| INT31-C | Validate all integer inputs before use |
| INT32-C | Prevent overflow in timeout calculations |
| FIO32-C | Validate all filesystem paths |
| ERR06-C | Graceful error handling (no assertions in production) |
| SIG31-C | Use sigaction(), async-signal-safe handlers only |
| STR31-C | Buffer overflow protection in string handling |

## Testing Strategy

**Pure logic testing:** State machine and config modules testable without hardware mocks.

**HAL isolation:** Hardware abstraction enables future mock implementations.

**Coverage target:** 95%+ automated, <10 minutes manual testing on device.

**Test categories:**
- Initialization and defaults
- Valid input handling
- Invalid input handling (boundary conditions)
- Security (path traversal, overflow, etc.)
