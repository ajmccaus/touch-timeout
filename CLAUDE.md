# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**touch-timeout** is a lightweight touchscreen backlight manager daemon for Raspberry Pi 7" displays. It automatically dims and powers off the display during inactivity and instantly restores brightness on touch.

Currently on the `refactoring-v2` branch (v2.0.0) implementing a modular architecture. The `main` branch contains the v1.x monolithic implementation.

**v2.0.0 Status:** Pre-release planning complete
- Comprehensive 20-30 hour release plan prepared (see `V2_RELEASE_PLAN.md` symlink in project root)
- Key tasks: config hardening, security review, hardware testing, documentation
- Breaking changes: `poll_interval` removed (v1.0 artifact), `dim_percent` default 50→10
- New features: graceful config fallback, ROADMAP.md documenting v2.1-v2.4+ plans
- **Awaiting:** Hardware validation before production release

## Design Principle

See [ARCHITECTURE.md - Design Philosophy](ARCHITECTURE.md#design-philosophy) for project design principles.

## Development Guidelines
- Use clear variable names
- Comment complex logic
- Test before committing
- Explain all system API calls
```

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
make test         # Run 50 unit tests (config + state modules)
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
./tests/test_state      # State machine tests (21 tests)
./tests/test_config     # Configuration parsing tests (29 tests)

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
- `make test`: 50 unit tests (state + config modules)
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

Default config: `/etc/touch-timeout.conf` (see `config/touch-timeout.conf` for example)

Key parameters:
- `brightness`: Active brightness (15-255, recommend ≤200)
- `off_timeout`: Seconds until screen off (min: 10, default: 300)
- `dim_percent`: When to dim (10-100% of off_timeout, default: 50)
- `device`: Touch input device in `/dev/input/` (default: event0)
- `backlight`: Device name in `/sys/class/backlight/` (default: rpi_backlight)

## Testing Infrastructure

### Unit Tests (50 total)

**test_state.c** (21 tests):
- State transitions on touch/timeout
- Clock handling and timer resets
- Getter functions and initialization

**test_config.c** (29 tests):
- Default initialization
- Config file parsing (int, string parameters)
- Range validation and overflow protection
- Safe integer parsing (`safe_atoi`)
- Security: path traversal prevention

### Test Categories

- **Pure Logic Tests**: State machine tests require no hardware mocking (zero I/O)
- **Integration Tests**: Config module tests mock file I/O via test fixtures
- **Coverage**: Use `make coverage` to measure code coverage

### Performance Testing

Run on device: `scp scripts/test-performance.sh root@[IP_ADDRESS]:/tmp/ && ssh root@[IP_ADDRESS] "bash /tmp/test-performance.sh"`

Measures: CPU usage, memory (RSS), SD card write activity, file descriptor leaks.

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

### Module Interfaces

**state.h** - Pure state machine:
```c
state_t *state_create(int on_timeout, int dim_timeout);
void state_handle_touch(state_t *state);
void state_handle_timeout(state_t *state);
int state_get_brightness(state_t *state);
```

**config.h** - Configuration management:
```c
int config_load_from_file(const char *path, config_t *config);
void config_set_defaults(config_t *config);
```

**display.h** - Backlight HAL:
```c
display_t *display_open(const char *backlight_name);
int display_set_brightness(display_t *display, int brightness);
void display_close(display_t *display);
```

**timer.h** - POSIX timer wrapper:
```c
timer_t *timer_create(int fd_to_track);
int timer_arm(timer_t *timer, int milliseconds);
void timer_disarm(timer_t *timer);
```

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

- **v2.1.0**: Configurable log levels, silent production mode
- **v2.2.0**: USB hotplug, multi-device input
- **v2.3.0**: Input device auto-classification
- **v2.4.0**: Optional activity sources (ALSA, SSH detection)

See ARCHITECTURE.md for detailed feature roadmap.

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