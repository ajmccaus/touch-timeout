# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**touch-timeout** is a lightweight touchscreen backlight manager daemon for Raspberry Pi 7" displays. It automatically dims and powers off the display during inactivity and instantly restores brightness on touch.

Currently on the `refactoring-v2` branch (v2.0.0) implementing a modular architecture. The `main` branch contains the v1.x monolithic implementation.

## Development Guidelines
- Use clear variable names
- Comment complex logic
- Test on real Mac before committing
- Explain all system API calls

## When I Approve Your Changes
Before I approve, answer:
1. What does this code do?
2. Why this approach vs. simpler alternatives?
3. What could break?
4. How do I test it?

## For New Developers
I'm learning to code. When explaining anything:
- Define technical terms
- Explain WHY we chose this approach
- Show alternatives I could have used
- Break down complex functions step-by-step

## Approval Workflow (CRITICAL)
Before I can approve your changes, you MUST:

1. **Explain the change** in plain language
2. **Show me alternatives** you considered
3. **Identify risks** - what could break?
4. **Provide test steps** - how do I verify this works?

If I don't understand something, I'll ask "ELI5" (Explain Like I'm 5)
and you'll break it down further with analogies.
```

**In your Claude Code prompts:**
```
"Add a notification feature. Before you code:
1. Explain your plan
2. Show me the pros/cons of 2-3 approaches  
3. Recommend one with reasoning
4. Wait for my approval to proceed"


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

## SSH Key Copy Procedure
when setting up key authentication for automated file transfer to RPi system use this 
manual procedure to copy the key directly from your WSL2 machine

1. On WSL2, create a temporary file with just the key:
cat ~/.ssh/id_rsa.pub > /tmp/pubkey.txt
2. Now copy it using SCP [use sudo if not logging in as root]:
scp /tmp/pubkey.txt [username]@[IP_ADDRESS]:~/.ssh/authorized_keys
3. Enter the password when prompted. 
4. Then enter [use sudo if not logging in as root]:
ssh [username]@[IP_ADDRESS] "chmod 600 ~/.ssh/authorized_keys"
Now test passwordless login: ssh [username]@[IP_ADDRESS] "echo OK"
5. This should work without a password now!


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

```bash
./scripts/deploy-arm.sh [IP_ADDRESS] arm64
ssh root@[IP_ADDRESS] "sudo /tmp/touch-timeout-staging/install-on-rpi.sh"
```

See DEPLOYMENT.md for detailed workflow.

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

See REFACTORING.md for detailed feature roadmap.

## Compiler & Flags

- **Compiler**: gcc 7+ with C17 support
- **Standard**: `-std=c17` with `-D_POSIX_C_SOURCE=200809L`
- **Optimization**: `-O2` for release builds
- **Warnings**: `-Wall -Wextra -Wno-unused-parameter`
- **Optional**: `-coverage` for unit test instrumentation

## References

- REFACTORING.md: v2.0 architecture and improvements over v1.x
- README.md: Feature overview and configuration
- DEPLOYMENT.md: Cross-compilation and deployment workflow
- INSTALLATION.md: System setup instructions
- [CERT C Coding Standard](https://wiki.sei.cmu.edu/confluence/display/c/SEI+CERT+C+Coding+Standard)
- [timerfd API](https://man7.org/linux/man-pages/man2/timerfd_create.2.html)