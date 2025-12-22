# Changelog

All notable changes to touch-timeout will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## Project History

This project serves as a case study in AI-assisted software development.

**The experiment:** A non-expert developer using AI (Claude) to build embedded C software, learning software engineering principles along the way.

**What happened:**
- **v0.1–v0.3**: Early development using AI web interfaces (Claude, DeepSeek, Grok, ChatGPT) and GitHub direct editing. Produced working but messy code with no clear architecture.
- **v0.4**: First attempt using Claude Code to refactor. With minimal direction and a messy codebase as context, Claude produced an over-engineered 6-module architecture (~900 lines) that worked but was difficult to maintain and full of inconsistencies.
- **v0.5–v0.6**: Attempts to document and stabilize the mess. Documentation was written based on intended behavior, not verified implementation—leading to doc/implementation mismatches.
- **v0.7**: Clean-slate rewrite prompting Claude Code to start from first principles and a new design specification. Simplified to 2 modules (~620 lines) with clear separation of concerns.

**Lesson learned:** AI-assisted development without proper context engineering ("flying the plane while building it") produces technical debt faster than a human could alone. The fix: design specifications and implementation guides *before* delegating to AI.

See [PROJECT-HISTORY.md](doc/PROJECT-HISTORY.md) for the full case study.

> **Note:** Version scheme was corrected in December 2025. Previous tags v1.0.0 and v2.0.0 were renamed to v0.3.0 and v0.4.0 respectively. v1.0.0 is reserved for the first stable production release.

---

## [0.8.0] - 2025-12-21

Device auto-detection and documentation improvements.

### Added

- **Device auto-detection**: Automatic discovery of backlight and touchscreen devices
  - Scans /sys/class/backlight/ for backlight device
  - Scans /dev/input/event* for multitouch-capable devices
  - Fallback to defaults if auto-detection fails
  - Manual override via `-l` and `-i` CLI options

### Changed

- **Documentation**: Enhanced source file headers with architectural context
  - Clear purpose, design constraints, and dependencies
  - Navigation aids with "SEE ALSO" references
  - Testing locations and strategies documented
- **INSTALLATION.md**: Updated for auto-detection (manual device selection rarely needed)
- **Deployment scripts**: Improved output with verification commands and testing instructions
- **Makefile**: Added complete target reference in header
- **test-performance.sh**: Added version and arch to output header

### Performance (arm64)

- CPU: 0.0% avg, Memory: 0.58 MB, FD leaks: 0, SD writes: 0

---

## [0.7.0] - 2024-12-19

Architecture simplification and feature additions.

### Changed

- **Architecture**: Simplified from 6 modules (~1,700 lines) to 2 modules (~620 lines)
  - `main.c`: CLI, device I/O, event loop
  - `state.c`: Pure state machine (no I/O, no time calls)
- **CLI**: Named options via `getopt_long` (`-b`, `-t`, `-d`, etc.)
- **Timer**: Single `poll()` timeout replaces `timerfd`
- **State machine**: Pure implementation—caller passes timestamps

### Added

- **SIGUSR1 wake**: External programs can wake display
- **Verbose mode**: `-v` flag for state transition logging

### Removed

- **Config file support**: CLI-only configuration
- **Modules**: `config.c/h`, `display.c/h`, `input.c/h`, `timer.c/h`

---

## [0.6.0] - 2025-12-19

Documentation reorganization and deployment workflow improvements.

### Changed

- Reorganized documentation into `doc/` directory
- Split ARCHITECTURE.md (descriptive) from DESIGN.md (prescriptive)
- Simplified build/deploy workflow

### Fixed

- Broken documentation links
- Metric inconsistencies in documentation

---

## [0.5.0] - 2025-12-12

Performance verification UX and documentation cleanup.

### Changed

- Simplified test scripts with better UX (progress indicators, clearer output)
- Fixed documentation inconsistencies

---

## [0.4.0] - 2025-12-11

Major refactoring attempt. (Previously tagged as v2.0.0)
Started 2025-11-26

**Note:** This version was over-engineered due to insufficient design guidance during AI-assisted refactoring. Documentation may not accurately reflect the implementation. See Project History above.

### Changed

- Refactored from single-file to 6-module architecture
- Switched from polling to event-driven I/O (`timerfd` + `poll`)
- Added unit test framework

### Added

- Cross-compilation support (ARM32/ARM64)
- Remote deployment scripts
- Configuration file support

---

## [0.3.0] - 2024-11-12

Initial feature-complete release. (Previously tagged as v1.0.0)

### Features

- Automatic touchscreen backlight dimming and power-off
- Touch-to-wake functionality
- Configurable timeouts and brightness
- Systemd service integration

---

## [0.2.0] - 2024-11-14

Early development release.
