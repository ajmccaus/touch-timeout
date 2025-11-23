# Changelog
All notable changes to touch-timeout will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.1] - 2025-11-23

### Added
- Configurable logging system (`log_level=0/1/2` in config)
- Debug flag `-d` for verbose logging
- Foreground flag `-f` to prevent daemonization
- Overflow protection for timeout arithmetic
- Comprehensive input validation with `safe_atoi()`
- Partial sysfs read detection and handling
- Brightness bounds enforcement (dim ≤ max)
- Pre-initialization error logging to stderr

### Changed
- **BREAKING**: Replaced all `assert()` with explicit error handling
- Batched startup logs (5→1 syslog write, 80% reduction)
- Default log level changed to 0 (silent) for SD card longevity
- Config parser warnings now output to stderr (before syslog init)
- All `atoi()` calls replaced with `safe_atoi()` for safety
- All `snprintf()` calls now validate return values

### Fixed
- Daemon crashes on clock adjustments (NTP jumps)
- Duplicate `openlog()` call during initialization
- Negative `poll_interval` values not rejected
- Missing daemonization guard for `-f` flag
- Potential signed integer overflow in timeout calculations
- Malformed sysfs content causing silent failures

### Security
- Eliminated undefined behavior from unchecked `atoi()` conversions
- Added bounds checking for all buffer operations
- Validated all file descriptor operations before use

### Performance
- Zero SD writes during runtime with `log_level=0`
- Reduced boot-time syslog writes by 80%

## [1.0.0] - 2024-XX-XX

### Added
- Initial release
- Automatic display dimming and power-off
- Configurable brightness and timeouts
- Configuration file support (`/etc/touch-timeout.conf`)
- Systemd integration
- Poll-based event loop (<0.1% CPU)
- Brightness caching to prevent redundant writes
- Time-based timeout logic (handles missed poll cycles)
- Graceful shutdown on SIGTERM/SIGINT

### Known Issues (Fixed in 1.0.1)
- `assert()` statements can crash daemon under rare conditions
- No overflow protection for large timeout values
- Direct `syslog()` calls bypass logging configuration
- Excessive SD writes during normal operation

## [0.2.0] - 2024-XX-XX

### Added
- Configuration file parsing
- Input validation with logging
- Enhanced error handling

## [0.1.0] - 2024-XX-XX

### Added
- Basic touchscreen activity monitoring
- Brightness control via sysfs
- Simple timeout mechanism