# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

**Current version:** 0.7.0

## Project Overview

**touch-timeout** is a lightweight touchscreen backlight manager daemon for Raspberry Pi 7" displays. It automatically dims and powers off the display during inactivity and instantly restores brightness on touch.

### Problem
Raspberry Pi 4 with official 7" touchscreen needs automatic backlight dimming.
Screen should dim after inactivity, turn off after further inactivity, and wake on
touch or external signal (e.g., from shairport-sync).

### Target Environment
- Hardware: Raspberry Pi 4, Official 7" Touchscreen (FT5406 controller)
- OS: HifiBerryOS (minimal Linux, no compiler)
- Development: WSL2 Debian, cross-compile to RPi
- Backlight: /sys/class/backlight/rpi_backlight/brightness (0-255)
- Input: Linux input subsystem (/dev/input/eventX)

### Functional Requirements
1. Set initial brightness on startup
2. Dim screen after configurable inactivity timeout
3. Turn screen off after further inactivity timeout  
4. Wake and restore brightness on touch event
5. Wake on external signal (IPC from other programs)
6. Restore brightness on graceful shutdown
7. Integrate with systemd
8. Support verbose mode for development/debugging

### Non-Functional Requirements
- Zero CPU when idle (blocking I/O only, no polling)
- No SD card writes during normal operation (log to stderr/journal only)
- Work with sensible defaults, no config file required
- Minimal memory footprint, no steady-state dynamic allocation

## Coding Standards and Best Practices

This project follows production-ready embedded C daemon standards to ensure robust, maintainable, and robust code. Think DRY and self documenting. 

All coding must follow best coding practices and patterns, not just work by accident:
  - No magic numbers         
  - Compile-time safety checks where possible                                                                  
  - Modern C patterns (EXIT_*, snprintf etc.)                                                               
  - Consistent error handling      
  - Spell-check identifiers 

## Naming Conventions

**Types:**
- Struct typedefs: `module_s` suffix (e.g., `state_s`, `config_s`)
- Enum typedefs: `module_e` suffix (e.g., `state_type_e`, `state_event_e`)
- Avoids POSIX `_t` suffix conflicts

**Functions:**
- Public: `module_verb()` or `module_verb_noun()` (e.g., `state_init()`, `state_touch()`)
- Static: Same pattern for consistency (e.g., `parse_args()`, `set_brightness()`)

**Variables:**
- Globals: `g_` prefix (e.g., `g_running`, `g_config`)
- Constants/macros: `MODULE_UPPER_CASE` (e.g., `CONFIG_DEFAULT_BRIGHTNESS`)
- Locals: `snake_case`

## Security Guidelines

- Platform: Linux-specific (poll, input subsystem, sysfs)
- Security: Follow CERT C principles for input validation and resource cleanup

## Development Workflow
- Cross-compile on WSL2, deploy to /run on RPi over ssh for testing
- Minimize SD card wear during development iteration

## Build & Testing

See [INSTALLATION.md] for complete build instructions and test details.

**Quick reference:**
- `make` - Build native binary
- `make test` - Run all unit tests
- `make arm64` - Cross-compile for RPi4
- `make coverage` - Generate coverage report

## Testing Strategy

**Coverage target:** 95%+ automated, <10 minutes manual testing on device.

**Test categories:**
- Initialization and defaults
- Valid input handling
- Invalid input handling (boundary conditions)
- Security (path traversal, overflow, etc.)

**Edge case focus:**
- Invalid inputs: IP addresses, paths, out-of-range values
- System boundaries: missing files, permission errors, device disconnection
- Timing: wraparound, rapid transitions, clock adjustments

**Commands:**
- `make test` - Run all unit tests
- `scripts/test-deployment.sh` - Validate deployment changes (<5 seconds)

**Manual verification** (on device, minimize):
- Touch responsiveness after deployment
- Log verification via `journalctl`
- State transitions: FULL → DIMMED → OFF

## Out of Scope
- Brightness ramping/animation
- Multiple displays
- Ambient light sensing
- D-Bus interface
- Config file parsing

**Separation of Concerns:**
- **Root**: Entry points only (README, CHANGELOG, CLAUDE.md)
- **doc/**: All documentation (user and developer)
- **ARCHITECTURE.md**: How it DOES work (descriptive) - updated each release
- **plans/**: Ephemeral implementation guidance, archived after release

## Architecture

**Key patterns when modifying code:**
- **Event-Driven I/O**: poll() on input fd with timeout, zero CPU while idle
- **Pure state machine**: state.c has zero I/O dependencies (enables unit testing)

**State transitions:** FULL → DIMMED (at dim_percent timeout) → OFF (at off_timeout)

