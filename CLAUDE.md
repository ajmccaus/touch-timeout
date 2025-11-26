# CLAUDE.md

## Purpose

This file serves as Claude Code's project memory, automatically loaded to understand touch-timeout's architecture, standards, and conventions. It documents stable principles that guide consistent code across sessions and versions.

## Audience

**Claude Code**: Learns project standards, constraints, patterns, and anti-patterns when generating or reviewing code.

**Human Developers**: Reference for coding conventions, architectural principles, build/test commands, and project constraints.

**Code Reviewers**: Compliance checklist and rationale for design decisions.

---

## Project Overview

touch-timeout is a lightweight touchscreen backlight manager for Raspberry Pi 7" displays. It automatically dims and powers off the display during inactivity, instantly restoring brightness on touch. Optimized for minimal Linux distributions like HifiBerry OS.

## Build Commands

```bash
# Build
make

# Or manually with gcc
gcc -O2 -Wall -Wextra -o touch-timeout touch-timeout.c

# Install (requires sudo)
sudo make install

# Clean build artifacts
make clean

# Run unit tests
make test

# Run tests with coverage report
make coverage
```

## Architecture

### Current State

**v1.0.0 - Stable Release** (main branch, hardware tested)
- Single-file: `touch-timeout.c` (~500 lines monolithic)
- Production-ready on Raspberry Pi 4 with 7" touchscreen

**v1.0.2 - In Development** (main branch, unit tested only)
- CERT C security hardening
- Awaiting hardware validation before release

**v2.0.0 - In Development** (refactoring-v2 branch)
- Modular architecture: `src/main.c`, `src/state.c`, `src/config.c`, `src/input.c`, `src/display.c`, `src/timer.c`
- POSIX timers (timerfd + CLOCK_MONOTONIC)
- systemd integration (sd_notify, watchdog)

### Architectural Principles

**State Machine**
- Three states: FULL brightness → DIMMED → OFF
- Transitions on: user input, timeout expiration
- Signal handlers only set flags; main loop processes state changes

**Error Handling**
- All system calls checked (return value + errno)
- Graceful degradation, no production assertions
- Errors logged, daemon continues operation

**Memory Safety**
- Zero dynamic allocation (no malloc/free)
- Static/stack buffers only
- Predictable embedded behavior

## Configuration

Runtime config: `/etc/touch-timeout.conf` (see `config/touch-timeout.conf` for template)

Key settings: `brightness`, `off_timeout`, `dim_percent`, `poll_interval`, `backlight`, `device`

## Testing

**Unit tests** (`tests/`): Tests pure functions (`trim`, `safe_atoi`, `load_config`) and validates constants/calculations. Uses `#include "touch-timeout.c"` pattern to access static functions. Run with `make test` or `make coverage`.

**Hardware testing**: This is a Raspberry Pi-specific daemon. To test on target:
1. Cross-compile or build directly on RPi
2. Install systemd service from `systemd/touch-timeout.service`
3. Check logs: `sudo journalctl -u touch-timeout.service -f`

## Important Constraints

- Brightness values >200 are acceptable but will reduce brightness on RPi 7" official touchscreen
- Minimum brightness is 15 (avoids flicker), minimum dim brightness is 10
- Off timeout must be >= 10 seconds (design decision for user experience)
- Graceful error handling (no asserts in production paths)

---

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
- Integer overflow protection
- Buffer overflow prevention via bounds checking
- Path validation (no directory traversal)
- Safe string handling

**MISRA C (Optional)**
- Code follows many MISRA C:2012 guidelines (safety-critical systems subset)
- No dynamic memory allocation (malloc/free) - uses static allocation
- Explicit error checking on all system calls
- No use of implicit type conversions

### Naming Conventions

**Type Definitions** (Structs, Enums, Unions)
- **Default**: Use typedef for all custom types
- **Structs**: End with `_s` suffix (e.g., `typedef struct display_state_s { ... } display_state_s;`)
- **Enums**: End with `_e` suffix (e.g., `typedef enum display_state_e { ... } display_state_e;`)
- **Rationale**: Avoids POSIX `_t` namespace conflicts while maintaining type safety and self-documenting code
- **Enum values**: `SCREAMING_SNAKE_CASE` with context prefix (e.g., `STATE_FULL`, `STATE_DIMMED`)

**Function Names**
- Format: `snake_case` (lowercase with underscores)
- Static functions: Same as public, encapsulation via static keyword
- Module prefixes: Use for public APIs (e.g., `display_set_brightness()`)

**Variable Names**
- Local variables: `snake_case` (e.g., `current_brightness`, `timeout_seconds`)
- Static file-scope variables: `snake_case` (e.g., `static int display_fd;`)
- **Avoid**: Global variables; use static file-scope instead
- Rationale: Single-source-of-truth principle, encapsulation, testability

**Constants and Macros**
- Format: `SCREAMING_SNAKE_CASE` (e.g., `MIN_BRIGHTNESS`, `CONFIG_PATH`)
- Definition location: Top of file or header, grouped logically
- Documentation: Include brief comment explaining the constant's purpose and constraints

### Self-Documenting Code

**Magic Number Elimination**
- Every constant value must be named and defined once
- Include rationale in comment (e.g., why value is 15, not 10)
- Example:
  ```c
  #define MIN_BRIGHTNESS  15  // Avoids flicker on RPi 7" display (per datasheet)
  #define MAX_BRIGHTNESS  200 // Hardware limitation, see errata Section 4.2
  ```

**Variable Naming That Conveys Intent**
- Names should answer: "What is this?" and "Why does it exist?"
- Use full words, not abbreviations (e.g., `current_brightness` not `br`)
- Suffixes convey meaning: `_fd` for file descriptors, `_count`, `_timeout`, `_seconds`

**Function Purpose in Signature**
- Function name and parameters should be self-explanatory
- Avoid cryptic abbreviations in public APIs
- Example: `int set_brightness(struct display_state *state, int value);` is clear

### Documentation Standards

**Comment Style (Hybrid C++/C)**
- `//` for inline code comments and brief explanations
- `/* */` for multi-line block comments and complex logic descriptions
- `/** */` for Doxygen function documentation
- Rationale: C++ style comments are C99+ compliant, concise for single-line use

**Function Documentation (Doxygen)**
- Location: Immediately before function definition
- Required tags: `@brief`, `@param`, `@return`, `@note` (if applicable)

Example:
```c
/**
 * @brief Set backlight brightness with caching to prevent redundant hardware writes
 *
 * Only writes to sysfs if brightness differs from cached value.
 * Prevents excessive I/O on battery-powered devices.
 *
 * @param[in] state   Display state structure (must not be NULL)
 * @param[in] value   Brightness value 0-255, subject to min/max constraints
 * @return 0 on success, -1 on write error (check errno)
 * @note Thread-unsafe: must be called from single-threaded context
 */
```

**Code Comments**
- Explain the "why", not the "what"
- Document design decisions, trade-offs, non-obvious constraints
- Reference specifications, standards, or bug numbers
- Avoid obvious comments

Example:
```c
// Poll interval tuned for responsive touch while minimizing CPU wake-ups
#define POLL_INTERVAL_MS  100

/* Brightness >200 causes unexpected PWM behavior on RPi 7" official touchscreen
   due to hardware limitation (errata Section 4.2). Empirically validated. */
#define MAX_SAFE_BRIGHTNESS  200
```

### Code Readability and Maintainability

**Line Length**
- Soft limit: 80 characters (traditional terminal editors)
- Hard limit: 120 characters (modern widescreen displays acceptable)
- Rationale: Balance between old tools and modern development practices

**Function Complexity**
- Target cyclomatic complexity: < 10-15 decision points per function
- Functions exceeding 30-50 lines should be split into helpers
- Deep nesting (> 3 levels) indicates need for extraction
- Most functions in this codebase are well-sized (10-50 lines) ✓

**Header File Organization**
- Include guards: `#ifndef FILENAME_H` / `#define FILENAME_H` / `#endif`
- Order: System includes first, then local includes
- Forward declarations: Use for struct pointers to minimize dependencies
- API design: Public functions clearly separated from internal helpers

**Example Header Layout**
```c
#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <time.h>
#include <stdint.h>

// Forward declarations
struct input_event;

// Constants
#define DISPLAY_MAX_BRIGHTNESS  255

// Type definitions
struct display_state {
    int brightness;
    time_t last_input;
};

// Public API
int display_init(struct display_state *state, const char *backlight_path);
int display_set_brightness(struct display_state *state, int value);

#endif  // DISPLAY_MANAGER_H
```

### Memory Safety

**Stack vs. Heap**
- **Preference**: Stack allocation (automatic, no fragmentation)
- **Avoidance**: Dynamic allocation (malloc/free) - use static buffers instead
- **Rationale**: Embedded systems need predictable memory behavior
- **Current implementation**: No dynamic allocation ✓

**Buffer Overflow Prevention**
- Always check array bounds before access
- Use safe string functions with size parameters
- Validate all external input (config files, sysfs reads)
- Example: Safe strcpy
  ```c
  size_t len = strlen(src);
  if (len >= sizeof(dest)) {
      syslog(LOG_ERR, "Buffer too small: need %zu bytes, have %zu",
             len, sizeof(dest));
      return -1;
  }
  memcpy(dest, src, len + 1);
  ```

### Error Handling

**Return Value Checking**
- **Every** system call and function return value must be checked
- Ignore errors only if deliberately implemented (with explicit comment)
- Pattern: Check return value, test errno if needed, propagate or handle

**Error Code Pattern**
```c
// Check return, then errno for specific error type
int fd = open(path, O_RDONLY);
if (fd < 0) {
    syslog(LOG_ERR, "Failed to open %s: %s", path, strerror(errno));
    if (errno == ENOENT) {
        // Handle file not found
    }
    return -1;
}
```

**Signal Handler Safety**
- Only set `volatile sig_atomic_t` flags in signal handlers
- Check flags in main event loop (not in handlers)
- Never call `printf()`, `syslog()`, `malloc()` from signal handler

### Testing Standards

**Unit Testing**
- Framework: Unity-style (included in tests/)
- Test pattern: Include `.c` file to access static functions
- Coverage target: 70-80% minimum, 90%+ for critical paths
- Run: `make test` for simple tests, `make coverage` for coverage report

**Test Areas**
- Input validation (config parsing, numeric bounds)
- State machine transitions
- Time-based calculations (handle clock adjustments)
- Error paths (failed opens, permission denied)
- Edge cases (INT_MAX, boundary values)

**Mock/Stub Pattern**
- Fake time provider for testing timeout logic
- Fake brightness provider for testing sysfs writes
- Fake event source for testing input handling

### Version Management

**VERSION Definition**
- Define in single header file: `include/version.h`
- Inject at build-time via Makefile to avoid manual updates
- Match version in systemd service file

Example:
```c
#define VERSION_MAJOR 1
#define VERSION_MINOR 2
#define VERSION_PATCH 3
#define VERSION_STRING "1.2.3"
```

Makefile integration:
```makefile
VERSION_MAJOR = 1
VERSION_MINOR = 2
VERSION_PATCH = 3

version.h:
	echo "#define VERSION_MAJOR $(VERSION_MAJOR)" > include/version.h
	echo "#define VERSION_MINOR $(VERSION_MINOR)" >> include/version.h
	echo "#define VERSION_PATCH $(VERSION_PATCH)" >> include/version.h
	echo "#define VERSION_STRING \"$(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)\"" >> include/version.h
```

**Rationale**: Single source of truth for versioning; eliminates manual synchronization between code and systemd service files.

---

## Standards Summary Reference

| Category | Standard | Why | Status |
|----------|----------|-----|--------|
| Language | C17 with POSIX.1-2008 | Modern stability, broad embedded toolchain support, backward compatible | Implemented |
| Security | CERT C (SIG31-C, INT32-C, FIO32-C, ERR06-C) | Daemon runs with privileges; must resist memory corruption and injection attacks | Implemented |
| Naming | typedef _s/_e suffixes, snake_case functions | Avoids POSIX _t conflicts; self-documenting; type-safe | Implemented |
| Comments | Hybrid C++/C style, explain why not what | C99+ compliant; concise inline comments; rationale preservation | Implemented |
| Error Handling | Check all returns, errno propagation | Daemons run unattended; graceful degradation prevents silent failures | Implemented |
| Memory | Zero dynamic allocation, static/stack only | Predictable behavior; no fragmentation; embedded safety | Implemented |
| Testing | Unit tests, 70-80% coverage minimum | Validates logic without hardware; regression detection | Implemented |
| Documentation | Self-documenting names, Doxygen | Reduces cognitive load; generates API docs; long-term maintainability | Partial |
