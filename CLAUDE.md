# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**touch-timeout** is a lightweight touchscreen backlight manager daemon for Raspberry Pi 7" displays. It automatically dims and powers off the display during inactivity and instantly restores brightness on touch.

**Current Version:** v2.0.0 (released 2025-12-11)

Modular architecture with event-driven I/O, comprehensive testing, and CERT C security compliance. See [CHANGELOG.md](CHANGELOG.md) for release notes and [ROADMAP.md](doc/ROADMAP.md) for future plans.

## Design Principle

See [DESIGN.md](doc/DESIGN.md) for design intent, philosophy, and principles.
See [ARCHITECTURE.md](doc/ARCHITECTURE.md) for current implementation state.

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

### Security Standards

Follow CERT C, POSIX, and MISRA C guidelines. When writing/modifying code:
- INT32-C: Overflow protection in timeout calculations
- FIO32-C: Validate all filesystem paths
- ERR06-C: Graceful error handling (no assertions in production)
- Signal handlers: async-signal-safe only (set flags, no I/O)
- No dynamic allocation
- Explicit error checking on all system calls


## Build & Testing

See [README.md](README.md) for complete build instructions and [ARCHITECTURE.md - Test Infrastructure](doc/ARCHITECTURE.md#test-infrastructure) for test details.

**Quick reference:**
- `make` - Build native binary
- `make test` - Run all unit tests
- `make arm64` - Cross-compile for RPi4
- `make coverage` - Generate coverage report

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

See [DESIGN.md - SD card write optimization](doc/DESIGN.md#decision-sd-card-write-optimization) for the three-layer strategy.

## SSH Key Setup for Remote Deployment

See [INSTALLATION.md - SSH Key Setup](doc/INSTALLATION.md#ssh-key-setup-optional-but-recommended) for comprehensive SSH key configuration instructions.

Quick reference:
```bash
ssh-copy-id <USER>@<IP_ADDRESS>
ssh <USER>@<IP_ADDRESS> "echo OK"  # Verify passwordless login
```


## Project Structure

```
touch-timeout/
├── README.md           # User entry point, feature overview
├── CHANGELOG.md        # Release history
├── CLAUDE.md           # AI instructions (this file)
├── doc/
│   ├── DESIGN.md       # Design intent (prescriptive, stable)
│   ├── ARCHITECTURE.md # Current state (descriptive, per-release)
│   ├── INSTALLATION.md # User install guide
│   ├── ROADMAP.md      # Future plans
│   └── plans/          # Version-specific implementation plans
│       └── archive/    # Completed plans (record keeping)
├── src/                # Source code
├── tests/              # C unit test source files
├── scripts/            # Shell scripts (deployment, testing)
└── systemd/            # Service configuration
```

**Separation of Concerns:**
- **Root**: Entry points only (README, CHANGELOG, CLAUDE.md)
- **doc/**: All documentation (user and developer)
- **DESIGN.md**: How it SHOULD work (prescriptive) - stable across versions
- **ARCHITECTURE.md**: How it DOES work (descriptive) - updated each release
- **plans/**: Ephemeral implementation guidance, archived after release

## Architecture

See [ARCHITECTURE.md](doc/ARCHITECTURE.md) for current implementation and [DESIGN.md](doc/DESIGN.md) for design intent.

**Key patterns when modifying code:**
- **Event-Driven I/O**: poll() on input + timer fds, zero CPU while idle
- **Pure state machine**: state.c has zero I/O dependencies (enables unit testing)
- **HAL modules**: display/input/timer abstract hardware for portability
- **Table-Driven Config**: Adding parameter = one table entry + test

**State transitions:** FULL → DIMMED (at dim_percent timeout) → OFF (at off_timeout)

## Configuration

Default config: `/etc/touch-timeout.conf` (see [INSTALLATION.md - Configuration](doc/INSTALLATION.md#configuration) for example)

**Configuration parameters:** See [README.md - Configuration](README.md#configuration) for complete parameter reference.

## Code Organization

See [ARCHITECTURE.md - Module Interfaces](doc/ARCHITECTURE.md#module-interfaces) for complete module documentation.

**When reading code, start with:**

1. **main.c** - Event loop showing module orchestration
2. **state.h** - State machine interface
3. **config.h** - Configuration structure and defaults
4. **tests/test_state.c** - State machine unit tests (reference)
5. **Makefile** - Build system and cross-compilation targets

## Documentation Standards

**SSoT (Single Source of Truth) for this project:**
- **Design intent/philosophy**: DESIGN.md (in doc/)
- **Current implementation**: ARCHITECTURE.md (in doc/)
- **Configuration parameters**: README.md
- **Installation/deployment**: INSTALLATION.md (in doc/)
- **Future plans**: ROADMAP.md (in doc/)
- **Build commands**: Makefile
- **Code defaults**: src/config.h

When documenting, reference the SSoT instead of duplicating.

**Plan files:**
- Active plans live in `doc/plans/`
- Move to `doc/plans/archive/` when version ships
- Plans guide implementation, then become historical record

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
state_t state;
state_init(&state, 150, 15, 270, 300);  // user_brightness, dim_brightness, dim_timeout, off_timeout
int brightness;
state_handle_event(&state, STATE_EVENT_TOUCH, &brightness);
assert(state_get_brightness(&state) == 150);  // FULL state
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

## Known Limitations

- Linux-only (timerfd, sysfs, /dev/input)
- Single display only
- Fixed device paths (/sys/class/backlight, /dev/input)
- No multi-device input support

## Future Roadmap (v2.1+)

See [ROADMAP.md](doc/ROADMAP.md) for planned features.

## References

- DESIGN.md: Design intent, philosophy, and principles
- ARCHITECTURE.md: Current implementation state
- README.md: Feature overview and configuration
- INSTALLATION.md: Installation and deployment
- ROADMAP.md: Future plans
- [CERT C Coding Standard](https://wiki.sei.cmu.edu/confluence/display/c/SEI+CERT+C+Coding+Standard)
- [timerfd API](https://man7.org/linux/man-pages/man2/timerfd_create.2.html)