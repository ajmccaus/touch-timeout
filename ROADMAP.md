# touch-timeout Roadmap  
*Detailed feature specification and implementation guidelines*

This roadmap balances stability, refactoring safety, and future extensibility while maintaining performance targets and strict compatibility constraints.

---

<!-- REPLACE Section "Version 1.0.1" starting at line 13 -->

# Version 1.0.1 – Zero-Wear & Safety Patch (COMPLETED)

## Goals  
Stabilize the daemon, reduce SD wear, and improve observability without architectural changes.

## Completed Features  
- **Validated `snprintf()` everywhere** with mandatory return checks
- **Configurable Logging:** `log_level=0/1/2` via config and `-d` flag
- **Foreground Mode:** `-f` runs in foreground using stderr (no daemonization)
- **Startup Log Batching:** 5→1 syslog call (80% reduction vs. target 67%)
- **Silent Production Mode:** default `log_level=0` = zero runtime writes
- **NTP Safety:** tolerant to backward/forward jumps (no asserts in hot paths)
- **Assert Elimination:** All assert() replaced with explicit checks + log_critical()
- **Full safe_atoi() Adoption:** Replaced all atoi() in sysfs readers and parsers
- **Overflow Guards:** Timeout arithmetic protected against signed integer overflow
- **Sysfs Hardening:** Partial read detection, malformed content validation
- **Brightness Bounds:** Enforced dim_brightness ≤ max_brightness uniformly
- **Pre-init Logging:** Config warnings use fprintf(stderr) before logging_init()
- **Daemonization Guard:** Proper fork/setsid only when NOT using `-f` flag
- **SD Write Reduction:** 1 write at boot (startup banner) + 0 runtime writes with log_level=0

## Verification Results
-  24h runtime test with log_level=0 (zero SD writes confirmed)
-  Foreground mode testing: `./touch-timeout -df` (no fork, stderr output)
-  Clock adjustment simulation (backward jump handled gracefully)
-  Valgrind memory leak check (no leaks detected)
-  Config parser error handling (malformed inputs logged to stderr)
-  Overflow protection (INT_MAX timeout rejected)

## Migration Guide
For existing v1.0.0 users:

1. **Add to `/etc/touch-timeout.conf`:**
```ini
   log_level=0  # Silent production mode (recommended)
```

2. **Restart service:**
```bash
   sudo systemctl restart touch-timeout
```

3. **Verify silent operation:**
```bash
   journalctl -u touch-timeout --since "5 minutes ago"
   # Should show only: "Started v1.0.1 | ..." (single line)
```

4. **Development/debugging:**
```bash
   sudo systemctl stop touch-timeout
   sudo touch-timeout -df  # Foreground + debug mode
```

## Breaking Changes
- **Assert Behavior**: Production builds no longer crash on unexpected state (log + safe recovery instead)
- **Config Warnings**: Now output to stderr during startup (captured by systemd journal)
- **Daemonization**: Now conditional on `-f` flag (systemd users unaffected)

## Performance Impact
- **CPU**: No change (<0.1% idle)
- **Memory**: No change (0.2 MB RSS)
- **Disk I/O**: 80-100% reduction in syslog writes (production mode)

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
| **Display State Enum** | `enum display_state_enum { STATE_FULL, ... }` | ```typedef enum display_state_e { DISPLAY_STATE_FULL = 0, ... } display_state_e;``` | Namespaced, explicit type, less risk of collisions |
| **Display State Struct** | `struct display_state { ... }` | ```typedef struct display_state { ...    display_state_e     state;} display_state_t;``` | Strong typing, consistent suffix `_t`, prepares for modularization |
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