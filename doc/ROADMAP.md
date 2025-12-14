# touch-timeout Roadmap

Planned features for future releases.

**Current Release:** v2.0.0 (2025-12-11)

---

## Scope

This daemon manages **touchscreen timeout only**. Keyboards, mice, and other input devices are out of scope (use DPMS/xscreensaver for those).

Features are evaluated against the [Design Philosophy](ARCHITECTURE.md#design-philosophy).

---

## v2.1.0 - Developer Experience & Integration

**Foreground mode (`-f` flag):**
- Run daemon in foreground with stderr output
- Essential for development and troubleshooting: `sudo ./touch-timeout -f`

**Programmatic wake (SIGUSR1):**
- `pkill -USR1 touch-timeout` triggers same behavior as touch
- Enables integration with music players, kiosks, etc.
- Use case: Wake screen when playback starts (via shairport hooks, audiocontrol, etc.)

**Debug mode (`-d` flag):**
- Enable LOG_DEBUG messages at runtime
- Combine with `-f` for full visibility: `touch-timeout -df`

**Device disconnection handling:**
- Graceful exit on input device error (POLLERR/POLLHUP/POLLNVAL)
- Systemd restarts daemon automatically
- Prevents CPU spin-loop if USB touchscreen disconnected

**CLI redesign (getopt):**
- Positional args replaced with flags: `-b`, `-t`, `-B`, `-D`
- Enables `-f` and `-d` flags
- Breaking change: old syntax rejected with helpful error

---

## Deferred

Reconsidered if user demand emerges:

- **Multi-device input** - Out of scope (touchscreen-only daemon)

---

## Rejected

- **Audio/activity monitoring** - Use SIGUSR1 instead; simpler, no dependencies
- **Input device classification** - Scope creep
- **Adaptive brightness** - Different concern
- **Web API** - Over-engineered

---

## Contributing

Before proposing features, consider:
- Does this solve a real problem for touchscreen users?
- Can it be done with SIGUSR1 or existing configuration?

See [CHANGELOG.md](CHANGELOG.md) for release history.
