# touch-timeout Roadmap  
*Detailed feature specification and implementation guidelines*

This roadmap balances stability, refactoring safety, and future extensibility while maintaining performance targets and strict compatibility constraints.

---

# Version 1.0.1 — Zero-Wear & Safety Patch (In Progress)

## Goals  
Stabilize the daemon, reduce SD wear, and improve observability without architectural changes.

## Features  
- **Validated `snprintf()` everywhere** (replaces all unsafe/ambiguous `strncpy()` usage)
- **Configurable Logging:** `log_level=0/1/2`  
- **Debug Flag:** `-d/--debug` for verbose output  
- **Foreground Mode:** `-f` runs in foreground using stderr  
- **Startup Log Batching:** reduce boot-time syslog writes (3 → 1)  
- **Silent Production Mode:** default `log_level=0` = zero runtime writes  
- **NTP Safety:** tolerant to backward/forward jumps (no asserts in hot paths)  
- **SD Write Reduction:** 1 write (boot only) → zero writes during operation  

### Performance Targets (apply to all later versions)
- **CPU (idle):** <0.3%  
- **Touch-to-restore latency:** <100 ms  
- **Hotplug detection:** <250 ms  
- **RAM:** <0.5 MB RSS  
- **SD runtime writes:** 0 (unless non-default logging enabled)

### Compatibility Constraints
- Kernel ≥ 4.14 (for stable evdev behavior)  
- Sysfs backlight semantics for Raspberry Pi official 7" display  
- ARMv7 / ARM64 supported (musl + glibc)  

---

# Version 1.0.2 — Struct Consolidation & Refactor Preparation

## Goals  
Prepare the codebase for the 1.1.0 architecture split by consolidating state, grouping variables, and introducing consistent typedef'd structures.

## Key Improvements  
- Replace raw enums with explicit `typedef enum`  
- Replace implicit struct declarations with `typedef struct`  
- Consolidate global state into `display_state_t`  
- Centralize function pointers into grouped operator structures (e.g., `log_ops_t`, `io_ops_t`)  
- Replace scattered globals with a single `app_ctx_t` root context  
- Improve naming consistency, replace magic numbers, and formalize constant groups  

## Data Structure Summary  

| Item | Before | After | Benefit |
|------|--------|--------|---------|
| **Display State Enum** | `enum display_state_enum { STATE_FULL, ... }` | ```typedef enum display_state_e {    DISPLAY_STATE_FULL = 0,    DISPLAY_STATE_DIMMED = 1,    DISPLAY_STATE_OFF = 2} display_state_e;``` | Namespaced, explicit type, less risk of collisions |
| **Display State Struct** | `struct display_state { ... }` | ```typedef struct display_state {    int                bright_fd;    int                user_brightness;    int                dim_brightness;    int                current_brightness;    int                dim_timeout;    int                off_timeout;    time_t             last_input;    display_state_e     state;} display_state_t;``` | Strong typing, consistent suffix `_t`, prepares for modularization |
| **Logging Functions** | 3 loose function pointers | `typedef struct log_ops { log_func_t critical; log_func_t info; log_func_t debug; } log_ops_t;` | Easier injection, smaller signatures, mocking for tests |
| **I/O Operations** | Hard-coded sysfs + input I/O | `typedef struct io_ops { int (*set_brightness)(); int (*get_brightness)(); int (*read_event)(); } io_ops_t;` | Enables fake providers for tests |
| **Global State** | Many spread-out globals | `typedef struct app_ctx { display_state_t disp; log_ops_t log; io_ops_t io; int poll_interval; int running; } app_ctx_t;` | Provides a clean path to 1.1.0 modular design |

---

# Version 1.1.0 — Architecture & Test Foundation

## Goals  
Split code into independent modules, introduce a test framework, and enable safe extensibility.

## Features  
### Modular File Layout
- `logic.c/h` — timeout logic + state machine  
- `io.c/h` — brightness + event device operations  
- `main.c` — CLI, daemon loop, initialization  

### Unit Testing  
- Fake time provider  
- Fake brightness provider  
- Fake event source  
- Makefile targets: `make test`, `make coverage`  

### Config Enhancements  
- Precise dimming control (absolute and %-based)  
- Extended dim timeout (1–100% of off timeout)

---

# Version 1.1.1 — Hardening & Stability Micro-Release

## Goals  
Reduce risk before multi-device support.

## Features  
- Strict integer overflow checks for timeout arithmetic  
- Mandatory error path for sysfs write failures  
- Optional “safe brightness mode” (no write outside `[0, max]`)  
- clang-tidy + cppcheck integrated into CI  
- Opaque pointer model for internal state data  

---

# Version 1.2.0 — Multi-Input & Hotplug Foundation

## Goals  
Support multiple input sources and automatic hotplug behavior.

## Features  
- Auto-scan `/dev/input/by-path/` with capability filtering (`EV_KEY`, `EV_REL`, `EV_ABS`)  
- Hotplug monitoring via `inotify`  
- Polling up to ~10 devices  
- Event source abstraction using the new `event_source_t`  

### Testing Strategy  
- Hotplug stress (20× connect/disconnect)  
- Mixed device storms (touch + keyboard)  
- Permission failures (EACCES)  
- Timewarp simulation (NTP jumps)  

---

# Version 1.3.0 — Advanced Input Classification

## Goals  
Enable correct zero-config wake-on-input behavior for mixed setups.

## Features  
- Classify devices: touchscreen, mouse, keyboard  
- Auto-enable only “activity-relevant” devices  
- Persistent rules for by-path devices  

---

# Version 1.4.0 — Optional Activity Sources

## Goals  
Extend timeout logic to non-input activity.

## Features  
- ALSA / PulseAudio “playback active” prevents timeout  
- SSH session presence prevents timeout  
- Toggleable in config  

---

# Support & Maintenance Policy
This is a hobby project maintained voluntarily.  
Bug reports and well-structured PRs are welcome.

---