# Changelog

All notable changes to touch-timeout will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.0.0] - 2025-12-11

Complete architectural refactoring from monolithic to modular design with enhanced security, testing, and user experience improvements.

### Breaking Changes

**Configuration Parameters:**

1. **`poll_interval` removed** - No longer needed with event-driven architecture
   - v1.0.0: Used 100ms polling by default
   - v2.0.0: Uses POSIX `timerfd` with `poll()` for event-driven I/O (zero CPU when idle)
   - **Migration:** Remove `poll_interval` from config files

2. **`dim_percent` default changed: 50 → 10**
   - v1.0.0: Screen dimmed at 50% of timeout (e.g., 150s if `off_timeout=300`)
   - v2.0.0: Screen dims at 10% of timeout (e.g., 30s if `off_timeout=300`)
   - **Rationale:** Keeps display at full brightness longer; dim serves as brief warning before screen-off
   - **Migration:** Add `dim_percent=50` to config file to preserve v1.0.0 behavior

3. **`brightness` default changed: 100 → 150**
   - v1.0.0: Default brightness 100
   - v2.0.0: Default brightness 150 (brighter default for better visibility)
   - **Migration:** Add `brightness=100` to config file to preserve v1.0.0 behavior

4. **`dim_percent` minimum changed: 10% → 1%**
   - v1.0.0: Minimum `dim_percent=10` (screen dims at 10% of timeout)
   - v2.0.0: Minimum `dim_percent=1` (allows finer control)
   - **Migration:** No action required unless using edge-case values

**Installation Changes:**

5. **No default config file installed**
   - v1.0.0: Installed `/etc/touch-timeout.conf` with defaults
   - v2.0.0: Zero-config operation (uses compiled-in defaults, graceful fallback if config invalid)
   - **Migration:** Config file is optional; daemon runs without it

**Code Structure (Contributors Only):**

6. **Modular architecture**
   - v1.0.0: Single 594-line `touch-timeout.c`
   - v2.0.0: 6 independent modules (`src/main.c`, `src/state.c`, `src/config.c`, etc.)
   - **Migration:** Update build scripts to use new Makefile targets

### Added

**New Features:**
- Event-driven I/O using `timerfd_create()` with `CLOCK_MONOTONIC` (immune to NTP/suspend issues)
- Graceful configuration fallback (invalid values → log warning + use defaults, daemon continues)
- `config_set_value()` API for runtime configuration with validation
- Brightness caching in display HAL (~90% reduction in sysfs writes)
- Optional systemd notify/watchdog support (requires libsystemd at build time)
- Comprehensive unit tests: 65 tests across state machine and config modules
- Hardware performance testing script (`scripts/test-performance.sh`)
- Cross-compilation support for ARM32/ARM64 targets
- Remote deployment workflow with automatic service installation

**Security Enhancements:**
- CERT C compliance: INT31-C, INT32-C, FIO32-C, SIG31-C, STR31-C, ERR06-C
- CLI argument validation now uses table-driven config parser (prevents bypass)
- Path traversal protection for device paths
- Integer overflow protection in timeout calculations
- Signal-safe handler implementation

**Documentation:**
- [ARCHITECTURE.md](ARCHITECTURE.md) - Complete technical reference
- [INSTALLATION.md](INSTALLATION.md) - Comprehensive installation guide (direct + remote methods)
- Improved [README.md](README.md) with quick-start examples
- Project-specific [CLAUDE.md](CLAUDE.md) for AI-assisted development

### Changed

**Performance:**
- CPU usage (idle): 0.08% → <0.05%
- Memory footprint: ~0.2 MB RSS (similar to v1.0.0)
- Touch response latency: <200ms (improved from v1.0.0)
- SD card I/O: ~90% reduction via brightness caching + `LogLevelMax=info` (filters DEBUG)

**Deployment:**
- Staging directory: `/tmp/` → `/run/touch-timeout-staging/` (guaranteed tmpfs)
- Service auto-install enabled by default (`--manual` flag to opt-out)
- Install script enables systemd service on boot

**Build System:**
- Build directory: `build/native/`, `build/arm32/`, `build/arm64/`
- C standard: C11 → C17 with POSIX.1-2008
- Test coverage: `make coverage` generates lcov reports
- Separate test executables per module

### Fixed

- System time changes no longer affect timeouts (uses `CLOCK_MONOTONIC`)
- Suspend/resume cycles handled correctly
- Config validation bypass via CLI arguments (now validated)
- Makefile install path bug (staging directory mismatch)
- Service not enabled on boot after installation
- Runtime log messages now use `LOG_DEBUG` (filtered by default via systemd `LogLevelMax=info`)

### Removed

- `poll_interval` configuration parameter (obsolete with event-driven architecture)
- Default config file installation (zero-config operation)
- `assert()` calls in production code paths (graceful error handling)
- Unnecessary `fsync()` on sysfs writes

---

## [1.0.0] - 2024-12-06

Initial release.

### Features

- Automatic touchscreen backlight dimming and power-off
- Touch-to-wake functionality
- Configurable timeouts and brightness levels
- Systemd service integration
- RPi 7" touchscreen support
- Single-file monolithic implementation (594 lines)
- Configuration via `/etc/touch-timeout.conf`
- Poll-based event loop (100ms default interval)

---

## Migration Guide: v1.0.x → v2.0.0

### Quick Migration (Most Users)

**No action required** - v2.0.0 runs with zero configuration. Defaults are sensible for most use cases.

### Preserving v1.0.0 Behavior

If you want v1.0.0 behavior, create `/etc/touch-timeout.conf`:

```ini
# Restore v1.0.0 defaults
brightness=100
dim_percent=50
```

**Note:** Do NOT add `poll_interval` - this parameter is obsolete in v2.0.0.

### For Contributors/Developers

**Build System Changes:**
```bash
# v1.0.0
make                    # Build touch-timeout
make test               # Run tests

# v2.0.0
make                    # Build build/native/touch-timeout
make test               # Run all unit tests
make coverage           # Generate coverage report
make arm32              # Cross-compile for ARM32
make arm64              # Cross-compile for ARM64
```

**Testing Changes:**
- v1.0.0: 1 test file (48 tests)
- v2.0.0: Modular tests (65 tests), `tests/test_config`, `tests/test_state`

**Deployment Changes:**
```bash
# v1.0.0
scp touch-timeout root@IP:/usr/local/bin/
scp touch-timeout.service root@IP:/etc/systemd/system/

# v2.0.0 (automated)
make arm32 && scripts/deploy-arm.sh root@IP
```

See [INSTALLATION.md](INSTALLATION.md) for complete deployment documentation.

---

## Version History

- **v2.0.0** (2025-12-11): Modular architecture, event-driven I/O, enhanced security
- **v1.0.0** (2024-12-06): Initial release
