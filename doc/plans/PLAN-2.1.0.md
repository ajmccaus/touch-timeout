# v2.1.0 Release Plan

**Type:** Minor release (new features, backwards compatible)
**Scope:** Developer experience, integration, robustness
**Breaking changes:** CLI syntax (positional args → getopt flags)
**Depends on:** v2.0.1 released first

---

## Summary

Implements ROADMAP.md v2.1.0 features: foreground mode, debug mode, programmatic wake. Plus device disconnection handling and CLI redesign.

---

## Features

### 1. Foreground Mode (-f flag)

**ROADMAP requirement:**
> Run daemon in foreground with stderr output. Essential for development and troubleshooting.

**Implementation:**
```c
if (foreground) {
    openlog("touch-timeout", LOG_PID | LOG_PERROR, LOG_DAEMON);  // LOG_PERROR → stderr
} else {
    openlog("touch-timeout", LOG_PID | LOG_CONS, LOG_DAEMON);
}
```

**Usage:** `sudo ./touch-timeout -f`

---

### 2. Debug Mode (-d flag)

**ROADMAP requirement:**
> Enable LOG_DEBUG messages at runtime.

**Implementation:**
```c
if (debug) {
    setlogmask(LOG_UPTO(LOG_DEBUG));
}
```

**Usage:** `sudo ./touch-timeout -df` (foreground + debug, most useful for development)

---

### 3. Programmatic Wake (SIGUSR1)

**ROADMAP requirement:**
> `pkill -USR1 touch-timeout` triggers same behavior as touch.

**Use cases:**
- Wake screen when music starts (shairport-sync hook)
- Kiosk mode: wake on external event
- Integration with home automation

**Implementation:**
```c
static volatile sig_atomic_t g_wake_signal = 0;

static void sigusr1_handler(int signum) {
    (void)signum;
    g_wake_signal = 1;
}

// In event loop, after poll():
if (g_wake_signal) {
    g_wake_signal = 0;
    // Same as touch event
    state_handle_event(&state, STATE_EVENT_TOUCH, &new_brightness);
    display_set_brightness(display, new_brightness);
    timer_arm(timer, state_get_timeout(&state));
}
```

---

### 4. Poll Error Handling (Device Disconnect)

**From code review:** CPU spin-loop if USB touchscreen disconnected.

**Problem:**
1. USB touchscreen unplugged → poll() returns with POLLHUP
2. Code only checks POLLIN, finds nothing
3. Loop continues immediately → 100% CPU

**Fix:**
```c
enum { FD_INPUT = 0, FD_TIMER = 1 };  // Named indices for clarity

// In event loop:
if (fds[FD_INPUT].revents & (POLLERR | POLLHUP | POLLNVAL)) {
    syslog(LOG_ERR, "Input device disconnected");
    break;  // Exit cleanly, systemd restarts us
}

if (fds[FD_TIMER].revents & (POLLERR | POLLHUP | POLLNVAL)) {
    syslog(LOG_ERR, "Timer error");
    break;
}
```

**Why v2.1.0 not v2.0.1:**
- ROADMAP lists "device disconnection handling" for v2.1.0
- Primary target (built-in RPi touchscreen) doesn't disconnect
- Graceful handling is a feature enhancement, not a bug fix

---

### 5. CLI Redesign with getopt()

**Why current approach doesn't work:**

Current (positional):
```c
static const char *cli_keys[] = {"brightness", "off_timeout", "backlight", "device"};
for (int i = 1; i < argc && i <= 4; i++) {
    config_set_value(config, cli_keys[i - 1], argv[i]);
}
```

Cannot handle `-f` or `-d` flags. Mixing flags with positional args is ambiguous.

**Why getopt() wasn't used originally:**
1. **Simplicity** - Positional is simpler for v1.0/v2.0
2. **Reuse** - Leverages `config_set_value()` validation
3. **Config file priority** - CLI was secondary mechanism
4. **No flags needed** - v2.0.0 had only value overrides

**Why getopt() is needed now:**
- `-f` and `-d` are boolean flags
- getopt() is POSIX standard
- Enables `-df` combined flags
- Future extensibility

**New CLI:**
```bash
# v2.0.x (old - positional):
touch-timeout 150 300 rpi_backlight event0

# v2.1.0 (new - flags):
touch-timeout -b 150 -t 300 -B rpi_backlight -D event0
touch-timeout -f                    # Foreground
touch-timeout -d                    # Debug
touch-timeout -df                   # Both
touch-timeout -df -b 200            # Both + brightness override
touch-timeout -h                    # Help
```

**Implementation:**
```c
int opt;
while ((opt = getopt(argc, argv, "fdb:t:B:D:h")) != -1) {
    switch (opt) {
        case 'f': foreground = true; break;
        case 'd': debug = true; break;
        case 'b': config_set_value(config, "brightness", optarg); break;
        case 't': config_set_value(config, "off_timeout", optarg); break;
        case 'B': config_set_value(config, "backlight", optarg); break;
        case 'D': config_set_value(config, "device", optarg); break;
        case 'h': print_usage(argv[0]); exit(0);
        default:  print_usage(argv[0]); exit(1);
    }
}
```

**Breaking change:** Old positional syntax rejected with error message explaining new syntax.

---

## Why Not in This Release

### Integration Tests for main.c

**What's missing:** No automated test for poll() + timer + input + display orchestration.

**Why valuable:**
- Unit tests verify modules in isolation
- Integration tests verify modules work together
- Bugs can hide in "glue" code

**Why deferred:**
- Requires mocking timerfd, /dev/input, sysfs
- Complex test harness
- Unit tests + manual device testing catch most issues
- Low ROI for effort required

**Alternative:** Manual acceptance test script on real hardware.

---

## SIGUSR1 Integration Documentation

v2.1.0 includes integration guides for using SIGUSR1 with common audio players.

### Architecture Decision: Don't Fork HifiBerry OS

**Decision:** Layer touch-timeout on stock HifiBerry OS, don't fork.

**Rationale:**
- Legacy Buildroot-based HifiBerryOS is EOL (April 2025)
- Maintenance burden: 20-40 hrs/release (Buildroot), 10-20 hrs (Debian-based hbosng)
- No thriving community forks exist
- Testing matrix explosion: 4 Pi models x HATs x players

**Approach:** Provide integration via:
1. Native player hooks (shairport-sync, spotifyd)
2. audiocontrol2 plugin (unified, recommended for HifiBerry OS)
3. MPD wrapper script

### Integration Layer 1: Native Player Hooks

**shairport-sync (AirPlay):**

Config in `/etc/shairport-sync.conf`:
```ini
sessioncontrol = {
    run_this_before_entering_active_state = "/usr/local/bin/wake-touch-timeout.sh";
    active_state_timeout = 30;
    wait_for_completion = "no";
};
```

Script `/usr/local/bin/wake-touch-timeout.sh`:
```bash
#!/bin/bash
/usr/bin/pkill -USR1 touch-timeout 2>/dev/null || true
```

**Gotchas:**
- Runs as user `shairport-sync`, not root - use full paths
- Use `active_state` hooks (debounced) not `play_begins` (fires on every pause/resume)
- Script must be executable (`chmod +x`)

**spotifyd (Spotify Connect):**

Config in `/etc/spotifyd.conf`:
```toml
[global]
on_song_change_hook = "/usr/local/bin/spotifyd-wake.sh"
```

Script `/usr/local/bin/spotifyd-wake.sh`:
```bash
#!/bin/bash
case "$PLAYER_EVENT" in
    start|play|change)
        /usr/bin/pkill -USR1 touch-timeout 2>/dev/null || true
        ;;
esac
```

**Gotchas:**
- Single hook for ALL events - must check `$PLAYER_EVENT` env var
- HifiBerry uses custom fork but same hook API
- Events: start, play, pause, stop, change, endoftrack, volumeset

**MPD:**

No native hooks. Requires wrapper service.

Script `/usr/local/bin/mpd-wake-monitor.sh`:
```bash
#!/bin/bash
while true; do
    /usr/bin/mpc idleloop player
    /usr/bin/pkill -USR1 touch-timeout 2>/dev/null || true
done
```

### Integration Layer 2: audiocontrol2 Plugin (Recommended)

For HifiBerry OS with audiocontrol2 - single integration point for ALL players.

Plugin `/data/ac2plugins/touch_timeout.py`:
```python
"""touch-timeout wake integration for audiocontrol2."""
import subprocess
from ac2.metadata import MetadataDisplay

class TouchTimeoutWake(MetadataDisplay):
    def __init__(self, params=None):
        super().__init__()
        self.last_state = None

    def notify(self, metadata):
        state = metadata.get('state')
        if state == 'playing' and self.last_state != 'playing':
            try:
                subprocess.run(['/usr/bin/pkill', '-USR1', 'touch-timeout'],
                              timeout=1, check=False, capture_output=True)
            except Exception:
                pass
        self.last_state = state
```

Config in `/etc/audiocontrol2.conf`:
```ini
[metadata:touch_timeout.TouchTimeoutWake]
```

**Advantages:**
- Catches ALL players (Spotify, AirPlay, MPD, etc.) with one config
- No per-player hook configuration needed
- Survives player backend changes

### Documentation Deliverables

Add to README.md or separate INTEGRATION.md:
1. SIGUSR1 usage examples
2. shairport-sync hook configuration
3. spotifyd hook configuration
4. audiocontrol2 plugin installation
5. MPD wrapper service setup

---

## Test Plan

1. **Unit tests:** Existing pass + new tests for SIGUSR1 handler, getopt parsing
2. **Manual:**
   - `-f`: Verify stderr output, Ctrl+C exits cleanly
   - `-d`: Verify DEBUG messages visible
   - `-df`: Both work together
   - SIGUSR1: `pkill -USR1 touch-timeout` wakes screen
   - Device disconnect: Unplug USB touchscreen, verify clean exit + systemd restart
3. **Migration:** Old positional args show helpful error message

---

## Pre-Commit Review

Before committing, run full code review:

1. **code-reviewer agent**: Verify all changes match plan, no regressions
2. **doc-reviewer agent**: Verify docs updated, SSoT maintained
3. **make test**: All tests pass (including new getopt and SIGUSR1 tests)
4. **Manual verification**: Test -f, -d, -df flags and SIGUSR1 on device

---

## Changelog Entry

```markdown
## [2.1.0] - YYYY-MM-DD

### Added

- **Foreground mode (-f)**: Run in foreground with stderr output
- **Debug mode (-d)**: Enable LOG_DEBUG at runtime
- **Programmatic wake (SIGUSR1)**: Signal-based wake for integration
- **Device disconnect handling**: Graceful exit on input device error

### Changed

- **CLI redesign**: Positional args → getopt() flags
  - Old: `touch-timeout 150 300 rpi_backlight event0`
  - New: `touch-timeout -b 150 -t 300 -B rpi_backlight -D event0`
  - **Migration required** for systemd overrides using positional args
```
