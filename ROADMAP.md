# touch-timeout Roadmap

Planned features for touch-timeout v2.x releases.

**Current Release:** v2.0.0 (2025-12-11)

---

## Guiding Principles

1. **User experience first** - Features must solve real problems
2. **Zero-config by default** - Works out of the box, customization optional
3. **Simplicity** - Minimum complexity for maximum value
4. **SD card longevity** - Minimize writes
5. **Testability** - New features require automated tests

---

## v2.1.0 - Debugging & Robustness

**Goal:** Easier debugging and better error recovery.

**Foreground mode (`-f` flag):**
- Run daemon in foreground with stderr output (no daemonization)
- Useful for debugging on device: `sudo /usr/bin/touch-timeout -f`
- All log messages visible (bypasses systemd LogLevelMax)

**Debug mode (`-d` flag):**
- Enable LOG_DEBUG messages at runtime
- Combine with `-f` for full visibility: `touch-timeout -df`

**Device disconnection handling:**
- Graceful handling when input device disappears (USB unplug)
- Auto-recovery when device reappears
- Log warning instead of crash

**Codebase review:**
- Review against MVP/Lean principles for over-engineering
- Identify simplification opportunities

---

## v2.2.0 - Multi-Device Input

**Goal:** Support multiple input devices.

**Multi-device configuration:**
```ini
device=event0,event1    # Explicit list
device=auto             # Auto-detect (future)
```

**USB hotplug monitoring:**
- `inotify` watch on `/dev/input/`
- Add/remove devices dynamically
- Limit: 10 devices maximum

---

## v2.3.0+ - Future Possibilities

Ideas under consideration (no commitment):

- **Input device classification** - Auto-detect touchscreen vs keyboard vs mouse
- **Activity sources** - Prevent timeout during audio playback or SSH sessions
- **Adaptive brightness** - Ambient light sensor integration
- **Web API** - REST interface for remote control

These require careful evaluation against complexity cost.

---

## Deferred / Rejected

**Code-level log_level configuration:**
- Originally planned for v2.1.0
- Deferred: v2.0.0's `LOG_DEBUG` + systemd `LogLevelMax=info` achieves the goal
- May revisit if users need fine-grained control without systemd

---

## Contributing

Bug reports and feature requests: [GitHub Issues](https://github.com/...)

Before proposing new features, consider:
- Does this solve a real problem users have?
- Can it be done with existing configuration?
- Is the complexity justified?

---

## Version History

See [CHANGELOG.md](CHANGELOG.md) for detailed release notes.
