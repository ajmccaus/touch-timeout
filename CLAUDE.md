# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**touch-timeout** is a lightweight touchscreen backlight manager daemon for Raspberry Pi 7" displays. It automatically dims and powers off the display during inactivity and instantly restores brightness on touch.

**Current Version:** v2.0.0 (released 2025-12-11)

Modular architecture with event-driven I/O, comprehensive testing, and CERT C security compliance. See [CHANGELOG.md](CHANGELOG.md) for release notes and [ROADMAP.md](ROADMAP.md) for future plans.

## Design Principle

See [ARCHITECTURE.md - Design Philosophy](ARCHITECTURE.md#design-philosophy) for project design principles.

## Coding Standards and Best Practices

This project follows production-ready embedded C daemon standards to ensure robust, maintainable, and secure code. The following section documents the standards and conventions used.

### Language Standard

**C17 (ISO/IEC 9899:2018)** with POSIX compliance flags:
```bash
gcc -std=c17 -D_POSIX_C_SOURCE=200809L ...
```

**Why C17?**
- Modern standard with bug fixes and clarifications over C11
- Broad compiler support across embedded toolchains (GCC, Clang)
- Provides stability without requiring cutting-edge features
- Backward compatible with C11 code

### Naming Conventions

**Functions:**
- Public: `module_verb()` or `module_verb_noun()` - e.g., `state_init()`, `config_load()`
- Static: Same pattern as public for consistency - e.g., `config_trim()`, `config_find_param()`
- main.c helpers are exceptions (not a reusable module)

**Types:**
- Struct typedefs: `module_s` suffix - e.g., `state_s`, `config_s`
- Enum typedefs: `module_e` suffix - e.g., `state_type_e`, `state_event_e`
- Avoids POSIX `_t` suffix conflicts

**HAL Module Pattern:**
- `module_open()` / `module_close()` - create/destroy with fd
- `module_get_fd()` - return fd for poll()
- Consistent across display, input, timer modules

**Variables:**
- Globals: `g_` prefix - e.g., `g_running`, `g_config`
- Constants/macros: `MODULE_UPPER_CASE` - e.g., `CONFIG_DEFAULT_BRIGHTNESS`
- Locals: `snake_case`

**Parameters:**
- Handle/context first: `module_func(handle, ...)`
- Output parameters last: `module_func(handle, input, *output)`

### Security Standards Compliance

This project implements best practices from multiple security standards:

**CERT C Coding Standard** (SEI Carnegie Mellon)
- **SIG31-C**: Signal handlers use `volatile sig_atomic_t` (✓ implemented)
- **INT32-C**: Integer overflow protection in timeout arithmetic (✓ implemented)
- **FIO32-C**: Path traversal protection for device/backlight paths (✓ implemented)
- **ERR06-C**: Graceful error handling instead of assertions in production paths (✓ implemented)

**POSIX Compliance**
- Signal handlers follow async-signal-safety rules (only set flags, no complex operations)
- systemd-compatible daemon initialization (no double-fork, SIGTERM handling)
- `sigaction()` used instead of deprecated `signal()`

**CWE/OWASP Embedded Security**
- Buffer overflow prevention via bounds checking
- Path validation (no directory traversal)
- Safe string handling

**MISRA C (Optional)**
- Code follows many MISRA C:2012 guidelines (safety-critical systems subset)
- No dynamic memory allocation (malloc/free) - uses static allocation
- Explicit error checking on all system calls
- No use of implicit type conversions


## Build Commands

```bash
# Native build (x86_64)
make              # Build daemon → build/native/touch-timeout
make test         # Run all unit tests (config + state modules)
make coverage     # Generate lcov coverage report
make clean        # Remove build artifacts

# Cross-compilation (ARM)
make arm32        # Build for ARMv7 → build/arm32/touch-timeout
make arm64        # Build for ARM64 (RPi4) → build/arm64/touch-timeout
make clean-all    # Remove all build artifacts

# Deployment
make install      # Install to system (requires sudo)
```

## Testing

```bash
# Run all tests
make test

# Run specific test executable
./tests/test_state      # State machine tests
./tests/test_config     # Configuration tests

# Generate coverage report
make coverage
# View: lcov -l coverage.info | head -30
```

## Rapid Development Workflow

```bash
# Quick build + test cycle
make clean && make test

# Single test module
./tests/test_config 2>&1 | grep -A5 "FAIL"

# Coverage check for specific file
gcov src/state.c
```

## Testing Strategy

**Goal**: 95%+ automated, <10 minutes total manual testing.

**Automated Tests** (no hardware required):
- `scripts/test-deployment.sh`: Validates deployment changes in <5 seconds (syntax, structure, references)
- `make test`: All unit tests (state + config modules)
- All tests run on every commit

**Manual Testing** (on device - minimize this):
- Touch responsiveness after deployment
- Log verification via `journalctl`
- Basic state transitions (FULL → DIMMED → OFF)

**Edge Case Focus**:
- Invalid IP addresses, missing cross-compilers, SSH failures
- Config out-of-range values, path traversal attempts
- Timer edge cases (wraparound, rapid transitions)

**Workflow**: Write automated test FIRST whenever possible. If manual testing identifies a bug, add automated regression test.

## SD Card Write Optimization

See [ARCHITECTURE.md - SD Card Write Optimization](ARCHITECTURE.md#8-sd-card-write-optimization) for the three-layer optimization strategy (deployment, logging, runtime) and performance metrics.

## SSH Key Setup for Remote Deployment

See [INSTALLATION.md - SSH Key Setup](INSTALLATION.md#ssh-key-setup-optional-but-recommended) for comprehensive SSH key configuration instructions.

Quick reference:
```bash
ssh-copy-id <USER>@<IP_ADDRESS>
ssh <USER>@<IP_ADDRESS> "echo OK"  # Verify passwordless login
```


## Architecture

### Modular Design (6 Independent Modules)

```
src/
├── main.c         - Event loop orchestrator (poll-based)
├── state.c/h      - Pure state machine (FULL → DIMMED → OFF)
├── config.c/h     - Table-driven config parser
├── display.c/h    - Backlight hardware abstraction
├── input.c/h      - Touch input device abstraction
└── timer.c/h      - POSIX timerfd wrapper
```

### Key Design Patterns

**Event-Driven I/O**: `poll()` waits on two file descriptors (input + timer). When touch is detected or timer expires, the state machine updates, triggering display HAL calls. Zero CPU usage while idle.

**Hardware Abstraction Layers**: Display and input modules isolate Linux-specific APIs (sysfs, `/dev/input`) for portability and testing. Pure state machine has zero I/O dependencies, enabling unit testing without hardware.

**Table-Driven Configuration**: `config.c` uses descriptor tables for all parameter types. Adding a parameter requires one table entry + test.

**Brightness Caching**: `display.c` caches current brightness to skip redundant sysfs writes (~90% write reduction vs v1.x).

### Data Flow

```
Input Event → Input HAL → State Machine → Display HAL → /sys/class/backlight/
Timer Expiry → Timer HAL → State Machine → Display HAL → /sys/class/backlight/
```

### State Machine States

- **FULL**: Display at configured brightness
- **DIMMED**: Brightness reduced to `brightness / 10` (min 10)
- **OFF**: Brightness = 0

Transitions triggered by:
- Touch input → restore FULL, reset timers
- Timeout at `dim_percent` of `off_timeout` → transition to DIMMED
- Timeout at `off_timeout` → transition to OFF

## Configuration

Default config: `/etc/touch-timeout.conf` (see [INSTALLATION.md - Configuration](INSTALLATION.md#configuration) for example)

**Configuration parameters:** See [README.md - Configuration](README.md#configuration) for complete parameter reference.

## Testing Infrastructure

See [ARCHITECTURE.md - Testing Infrastructure](ARCHITECTURE.md#7-testing-infrastructure) for test coverage details and categories.

## Security & Compliance

**CERT C Compliance:**
- INT31-C: Range validation on all integer inputs
- INT32-C: Overflow prevention in calculations
- FIO32-C: Path traversal protection
- STR31-C: Buffer overflow prevention
- ERR06-C: Graceful error handling (no assertions in hot paths)

**Defensive Programming:**
- All public functions validate parameters
- No assumptions about device max_brightness
- Safe string parsing via `safe_atoi()`

## Cross-Compilation

### Prerequisites

```bash
sudo apt-get install gcc-arm-linux-gnueabihf gcc-aarch64-linux-gnu
```

### Build for RPi4 (ARM64)

```bash
make arm64  # → build/arm64/touch-timeout
```

### Deployment to RPi4 over TCP-IP

See [INSTALLATION.md - Method 2](INSTALLATION.md#method-2-remote-deployment-cross-compilation) for comprehensive deployment guide including SSH setup, troubleshooting, and CI/CD automation.

## Code Organization

### Module Structure

See [ARCHITECTURE.md - Module Interfaces](ARCHITECTURE.md#module-interfaces) for complete API documentation and usage examples.

**Six independent modules:**
- **state.c/h** - Pure state machine (FULL → DIMMED → OFF), zero I/O dependencies
- **config.c/h** - Table-driven configuration with graceful fallback
- **display.c/h** - Backlight hardware abstraction (sysfs interface)
- **input.c/h** - Touch input device abstraction (/dev/input)
- **timer.c/h** - POSIX timerfd wrapper for event-driven timeouts
- **main.c** - Event loop orchestrator (poll-based)

### Key Files to Read

1. **main.c** - Event loop showing module orchestration
2. **state.h** - State machine interface
3. **config.h** - Configuration structure and defaults
4. **tests/test_state.c** - State machine unit tests (reference)
5. **Makefile** - Build system and cross-compilation targets

## Performance Characteristics

- **CPU (idle)**: <0.05%
- **Memory (RSS)**: ~0.2 MB
- **Latency**: <200ms touch-to-restore
- **SD Card I/O**: ~90% reduction via brightness caching

Benchmarked on RPi4 (1.5GHz ARM Cortex-A72) over 24+ hours continuous operation.

## Documentation Standards

**User-First Organization**: Structure docs by user journey, not implementation details
- **INSTALLATION.md**: Two methods (Direct vs Remote), not "for developers" vs "for users"
- **README.md**: Quick examples with links to comprehensive guides
- **ARCHITECTURE.md**: Technical details for contributors, not mixed with user docs

**Appropriate Detail Level**:
- Quick start: 5-10 lines showing the most common case
- Comprehensive: Full options, edge cases, troubleshooting
- Balance: Don't hide important details, but make common case discoverable in 10 seconds

**Signal-to-Noise**: Every line should add value
- Skip obvious explanations (e.g., "Install the compiler before compiling")
- Include non-obvious guidance (e.g., "Why /run vs /tmp", "SSH keys eliminate 3-4 password prompts")

**Single Source of Truth (SSoT)**: This project's canonical locations:
- **Configuration parameters**: README.md (Configuration section)
- **Installation procedures**: INSTALLATION.md
- **Architecture/internals**: ARCHITECTURE.md
- **Build commands**: CLAUDE.md (Build Commands section)
- **Defaults and constants**: src/config.h (code is the ultimate SSoT)

When documenting, reference the SSoT instead of duplicating. code-reviewer should flag SSoT violations.

## Development Patterns

### Adding a Configuration Parameter

1. Add to `config_t` struct in config.h
2. Add descriptor entry in `config_params[]` table in config.c
3. Add test case in tests/test_config.c (parsing + validation)

### Adding State Machine Logic

Keep state.c pure (no I/O). All device operations go in main.c through HAL modules.

### Testing New Code

State machine tests can use pure C without hardware:
```c
state_t *state = state_create(300, 150);  // 300s timeout, 150s dim
state_handle_touch(state);
assert(state_get_brightness(state) == 100);  // FULL state
```

### Script Organization

Test scripts (`test-*.sh`) live in `scripts/` not `tests/`:
- `tests/` = C unit test source files (compiled executables)
- `scripts/` = Shell scripts (deployment, testing, automation)

## Systemd Integration

Service file: `systemd/touch-timeout.service`

**Optional systemd features** (requires libsystemd):
- sd_notify() for startup confirmation
- Watchdog pinging for health monitoring
- Graceful shutdown handling

**Fallback**: Builds without libsystemd using stub functions. Type=simple service works on minimal systems.

View logs:
```bash
journalctl -u touch-timeout.service -f
systemctl status touch-timeout.service
```

## Important Constraints

- **Brightness values:** 15-255 acceptable, recommend ≤200 for RPi 7" official touchscreen
  - Hardware limitation: >200 may cause unexpected PWM behavior (per hardware errata)
  - Minimum brightness: 15 (avoids flicker)
  - Minimum dim brightness: 10
- **Off timeout:** Must be ≥10 seconds (design decision for user experience)
- **Default dim_percent:** 10 (v2.0.0+), changed from 50 in v1.0.x
  - Rationale: Keeps display at full brightness longer (90% of timeout vs 50%)
  - Dim serves as brief warning before screen off
  - Dims at 10% of off_timeout (e.g., 30s if off_timeout=300s)
- **Error handling:** Graceful degradation, no fatal exits on config errors (v2.0.0+)
  - Out-of-range config values → log warning, use defaults
  - Daemon continues operation even with invalid configuration

## Known Limitations

- Linux-only (timerfd, sysfs, /dev/input)
- Single display only
- Fixed device paths (/sys/class/backlight, /dev/input)
- No multi-device input support

## Future Roadmap (v2.1+)

See [ROADMAP.md](ROADMAP.md) for planned features:
- **v2.1.0**: Foreground mode (-f), debug mode (-d), programmatic wake (SIGUSR1)

## Compiler & Flags

- **Compiler**: gcc 7+ with C17 support
- **Standard**: `-std=c17` with `-D_POSIX_C_SOURCE=200809L`
- **Optimization**: `-O2` for release builds
- **Warnings**: `-Wall -Wextra -Wno-unused-parameter`
- **Optional**: `-coverage` for unit test instrumentation

## References

- ARCHITECTURE.md: v2.0 architecture and improvements over v1.x
- README.md: Feature overview and configuration
- INSTALLATION.md: Installation and deployment (both direct and remote methods)
- [CERT C Coding Standard](https://wiki.sei.cmu.edu/confluence/display/c/SEI+CERT+C+Coding+Standard)
- [timerfd API](https://man7.org/linux/man-pages/man2/timerfd_create.2.html)