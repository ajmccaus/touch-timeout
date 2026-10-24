# touch-timeout v0.9 Design Specification (Delta)

Prescriptive spec for the v0.9.0 change set. Describes what to BUILD, as changes
against the v0.8.0 baseline on `main` (commit 2ad5769). For how the code DOES
work, see ARCHITECTURE.md (descriptive). After v0.9.0 ships, set Status to
Implemented and keep this file as the design record.

- Status: Approved for implementation
- Target version: 0.9.0
- Baseline: `main` @ 2ad5769, v0.8.0, 48/48 unit tests passing
- Size of change: roughly 150 lines of C net, plus scripts and docs. This is a
  delta, NOT a rewrite.

---

## 0. How to Use This Document (for Claude Code)

1. Read sections 1-2 for scope, then section 12 (phases) for execution order.
2. Work one phase at a time. In each phase: write the failing tests listed for
   that phase FIRST (red), implement until green, refactor, then commit.
3. When this spec is silent, preserve existing v0.8.0 behavior. Do not invent
   features.
4. When this spec conflicts with existing code comments or docs, this spec wins;
   fix the comment or doc in the same commit.
5. Stop and ask the human only for items in section 14.2. Everything else is
   decided.
6. Section 15 lists what NOT to do. Read it before starting.

---

## 1. Baseline: What Stays

The v0.8.0 architecture is correct and is kept. Do not restructure it.

| Element | v0.8.0 | v0.9 |
|---|---|---|
| Module split | `state.c/h` pure FSM (no I/O, no time calls); `main.c` I/O, CLI, loop | Unchanged |
| States | FULL, DIMMED, OFF | Unchanged |
| FSM API | `state_init/touch/timeout/get_timeout_sec/get_brightness/get_current` | Unchanged |
| Timer | `poll()` timeout computed from `state_get_timeout_sec()`; no timerfd | Unchanged |
| Time base | `CLOCK_MONOTONIC`, uint32 seconds, wraparound-safe | Unchanged |
| Auto-detect | sysfs backlight scan; ioctl scan for ABS_MT_POSITION_X/Y | Unchanged |
| Activity | Any input event counts | Unchanged |
| Brightness clamp | Clamp to hardware `max_brightness` | Unchanged |
| Unit tests | `tests/test_state.c` includes `main.c` under `UNIT_TEST`, no mocks | Unchanged approach, new tests added |
| Static ARM builds | `-static`, gcc-arm-linux-gnueabihf / gcc-aarch64-linux-gnu | Unchanged |
| Deploy | Stage to `/run/touch-timeout-staging/`, versioned install, rollback | Unchanged |

Rationale for keeping `poll()` timeout over timerfd: one fewer fd, and the FSM
already owns all time arithmetic. Every event source other than the timer is a
file descriptor. The timer stays as the poll timeout on purpose.

---

## 2. Change Summary

| ID | Change | Why |
|---|---|---|
| C1 | Signals via `signalfd` (SIGTERM, SIGINT, SIGHUP, SIGUSR1) | Fixes the lost-SIGUSR1 race on `main`; removes async handlers and `volatile` globals; makes every non-timer event an fd |
| C2 | FIFO wake at `/run/touch-timeout/wake` | The only wake path that crosses the container boundary without a helper process |
| C3 | Retire `http-wake.py`, its installer, and its unit | Replaced by C2; removes a ~10 MB Python process and a TCP port |
| C4 | Hardware-truth brightness: single apply point reconciles FSM target with last-written value | Spec principle "state reflects hardware"; replaces scattered compare-and-write |
| C5 | Separate `--dim-level` (brightness %) from `-d` (time %) | `-d` currently drives both, which is undocumented and surprising |
| C6 | Reject invalid or out-of-range CLI values: exit 2 | CERT input validation; failure visible in systemd instead of silent defaults |
| C7 | POLLHUP/POLLERR on the input fd: exit 1 | Current loop spins at 100% CPU if the input device disappears |
| C8 | Remove `HAVE_SYSTEMD`/`sd_notify` scaffolding | Dead code for `Type=simple`; cross builds never link libsystemd anyway |
| C9 | Adopt build and unit hardening from the security branch; drop its loop changes | Keep the good parts; C1 supersedes its `ppoll()` fix |
| C10 | Memory metric: report Pss and Rss | 200 kB is an aspiration; make measurements comparable |
| D1 | Documentation cleanup | See section 11 |

---

## 3. Behavioral Specification

### 3.1 States

| State | Brightness written | Next timeout |
|---|---|---|
| FULL | `brightness` (`-b`, clamped to hw max) | `dim_sec` after last activity |
| DIMMED | `dim_brightness` (section 6.2) | `off_sec` after last activity |
| OFF | 0 | None (poll blocks indefinitely) |

### 3.2 Events

All four wake sources are equivalent to a touch: they call `state_touch(now)`.
They reset the idle timer even if already FULL. The FSM does not know which
source woke it.

| Event | Source | FSM call |
|---|---|---|
| Touch | input fd POLLIN, any event drained | `state_touch` |
| SIGUSR1 | signalfd | `state_touch` |
| FIFO write | FIFO fd POLLIN, bytes drained and discarded | `state_touch` |
| Timeout | `poll()` returns 0 | `state_timeout` |
| SIGTERM, SIGINT, SIGHUP | signalfd | Leave loop, graceful shutdown |

SIGHUP is new. Rationale: an ssh session drop during `-v` development sends
SIGHUP; without handling, the default action kills the daemon and can leave the
screen off.

### 3.3 Transitions

| From | Event | To | Write |
|---|---|---|---|
| FULL | Timeout (idle >= dim_sec) | DIMMED | dim_brightness |
| DIMMED | Timeout (idle >= off_sec) | OFF | 0 |
| DIMMED, OFF | Any wake event | FULL | brightness |
| FULL | Any wake event | FULL | none (idle timer reset only) |
| Any | SIGTERM/SIGINT/SIGHUP | exit 0 | brightness (restore) |

### 3.4 Verbose Log Contract

Integration tests grep these exact strings. Do not reword them.

| Situation | Level | Exact text |
|---|---|---|
| Wake by touch causing transition | DEBUG | `Touch -> FULL (brightness N)` |
| Wake by SIGUSR1 causing transition | DEBUG | `SIGUSR1 -> FULL (brightness N)` |
| Wake by FIFO causing transition | DEBUG | `FIFO -> FULL (brightness N)` |
| Timeout to DIMMED | DEBUG | `Timeout -> DIMMED (brightness N)` |
| Timeout to OFF | DEBUG | `Timeout -> OFF (brightness 0)` |
| Shutdown restore | INFO | `Brightness restored to N, shutting down` |
| FIFO unavailable | WARN | `FIFO wake disabled: <reason>` |
| Input device lost | ERROR | `Input device lost, exiting` |

If several wake sources are ready in the same poll cycle, the transition is
logged once with the first present source in the order Touch, SIGUSR1, FIFO.

The startup INFO line keeps its current fields and appends
`fifo=/run/touch-timeout/wake` or `fifo=disabled`.

---

## 4. Architecture Changes

### 4.1 File Descriptors

| fd | Open flags | poll events | Required |
|---|---|---|---|
| Input `/dev/input/eventN` | `O_RDONLY, O_NONBLOCK, O_CLOEXEC` | POLLIN | Yes (exit 1 if open fails) |
| signalfd | `SFD_NONBLOCK, SFD_CLOEXEC` | POLLIN | Yes (exit 1 if creation fails) |
| FIFO | `O_RDWR, O_NONBLOCK, O_CLOEXEC, O_NOFOLLOW` | POLLIN | No (warn and continue; set pollfd fd to -1) |
| Backlight `brightness` | `O_RDWR, O_CLOEXEC` | not polled | Yes |

Expected fd delta vs v0.8.0: +2 (signalfd, FIFO). `test-performance.sh` leak
check compares against itself, so no threshold change is needed.

### 4.2 Startup Sequence

1. Parse and validate CLI (section 6). Usage errors exit 2 before any device is
   touched.
2. Block SIGTERM, SIGINT, SIGHUP, SIGUSR1 with `sigprocmask`. This is done
   before anything else so a signal during startup is queued, not lost or
   fatal. Ignore SIGPIPE (kept from v0.8.0).
3. Create signalfd for the same set.
4. Auto-detect devices (unchanged).
5. Open backlight and input; read hw max; clamp; derive dim brightness and
   timeouts.
6. Set up FIFO (section 5.3). Failure here is a warning, not fatal.
7. Write initial brightness. Failure is fatal (exit 1), as in v0.8.0.
8. Log startup line. Enter loop.

### 4.3 Event Loop (one cycle)

1. Compute poll timeout from `state_get_timeout_sec()` (unchanged math).
2. `poll()` on input, signalfd, FIFO. Retry on EINTR (signals are blocked, so
   EINTR should not occur, but handle it).
3. signalfd ready: drain all pending `signalfd_siginfo` records until EAGAIN.
   Record shutdown if any of TERM/INT/HUP was seen; record wake if USR1 was
   seen.
4. If shutdown recorded: leave the loop immediately. Shutdown beats wake.
5. Input `revents` has POLLHUP or POLLERR: log `Input device lost, exiting`,
   exit path with status 1 (C7). Else POLLIN: drain; record wake.
6. FIFO `revents` has POLLHUP or POLLERR: log `FIFO wake disabled: poll error`,
   close it, set its pollfd fd to -1, continue. Else POLLIN: read into a small
   stack buffer until EAGAIN; record wake.
7. If any wake recorded: `state_touch(now)` once.
8. If `poll()` returned 0: `state_timeout(now)`.
9. Apply (section 4.4).

### 4.4 Apply: Hardware-Truth Brightness (C4)

One variable, `hw_brightness`, holds the value last successfully written to
sysfs. It replaces `cached_brightness` and is the only record of hardware state.
The FSM holds the desired state.

At the end of every loop cycle: if `state_get_brightness()` differs from
`hw_brightness`, write it. On success, update `hw_brightness`. On failure, leave
`hw_brightness` unchanged; the next cycle retries.

Engineering analogy: a PLC output scan. Logic computes the desired output; the
scan writes the output only when it differs from what was last driven.

Write-failure logging is edge-triggered: log ERROR on the first failure after a
success, and INFO `Brightness write recovered` on the first success after a
failure. This prevents a log flood while touch events arrive many times a second.

Known limitation (accepted): in OFF there is no timeout, so a failed write is
retried only on the next event.

### 4.5 Exit Path

All exits after the initial brightness write go through one cleanup path:
restore `brightness` (log result), close all fds. Never unlink the FIFO or its
directory (section 5.3, item 6).

| Exit code | Meaning | systemd behavior |
|---|---|---|
| 0 | Graceful shutdown (signal), `--help`, `--version` | No restart |
| 1 | Runtime failure: device open, initial write, signalfd, input lost, poll error | Restart after 5 s, re-runs auto-detect |
| 2 | Usage error: unknown option, unparseable or out-of-range value | No restart (`RestartPreventExitStatus=2`) |

---

## 5. Interfaces

### 5.1 Backlight (unchanged)

`/sys/class/backlight/<name>/brightness`, `max_brightness`. Writes use lseek to
0 then write the decimal value.

### 5.2 Input (one change)

`/dev/input/eventN`. Change: POLLHUP/POLLERR is fatal (exit 1). Reason: when an
evdev device is removed, the kernel reports HUP/ERR on every poll, so the v0.8.0
loop spins without ever reading or exiting.

### 5.3 FIFO Wake (C2)

Path: `/run/touch-timeout/wake`. Fixed; no CLI option. `/run` is tmpfs, so
nothing here touches the SD card.

Setup procedure. Implement as a function that takes the directory path as a
parameter (production passes `/run/touch-timeout`) so unit tests can run it
against a temporary directory.

1. Create the directory with mode 0755. EEXIST is fine if it is a directory.
2. `lstat` the FIFO path. If it exists and is not a FIFO (regular file left by
   an `echo` while the daemon was down, a symlink, anything else), unlink it.
3. If absent, create it with `mkfifo`.
4. Open with `O_RDWR | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW`.
5. `fstat` the opened fd and confirm it is a FIFO (guards against a swap between
   steps 2 and 4). Then `fchmod` the fd to 0622. The explicit chmod is required:
   `mkfifo` applies the umask, so a requested 0622 becomes 0600 under the usual
   umask 022, and the container could not write.
6. Never unlink the FIFO or directory at exit. The container bind-mounts the
   directory; leaving both in place keeps its view stable across restarts.

Any failure in steps 1-5: log `FIFO wake disabled: <reason>`, continue without
the FIFO. Dimming, touch wake, and SIGUSR1 still work.

Why `O_RDWR`: the daemon then holds its own write end, so the FIFO never reports
EOF when an external writer closes. With `O_RDONLY`, every writer close leaves
the fd permanently readable at EOF and the loop spins. This was the nastiest
bug in the original greenfield spec; `O_RDWR` removes the reopen logic entirely.

Why mode 0622: any local user, including a container user with a different
UID, can wake the screen. Waking is idempotent and harmless; strict
permissions would only create UID-mapping problems.

Why no `RuntimeDirectory=` in the unit: systemd deletes a RuntimeDirectory when
the service stops, which would pull the directory out from under the
container's bind mount. The daemon owns the directory instead.

Draining: read up to a small stack buffer per call until EAGAIN. Content is
ignored. Any number of bytes in one cycle is one wake.

### 5.4 Signals (C1)

| Signal | Action |
|---|---|
| SIGUSR1 | Wake (host-side integrations and manual testing) |
| SIGTERM, SIGINT, SIGHUP | Graceful shutdown, exit 0 |
| SIGPIPE | Ignored (unchanged) |

SIGUSR1 is kept for host processes and backward compatibility with existing
`pkill -USR1 touch-timeout` usage. It cannot be sent from inside the shairport
container (separate PID namespace); the FIFO is the container path.

### 5.5 shairport-sync Integration (HifiBerryOS, Docker)

Two device-side changes, documented in INSTALLATION.md (performed by the human,
section 14.2):

1. Bind-mount the directory, not the file, into the container. In
   `/data/extensions/shairport/docker-compose.yaml` volumes:
   `/run/touch-timeout:/run/touch-timeout`. Mounting the file would pin the
   inode, and a recreated FIFO would become invisible to the container.
2. In the hook script `pause-others.sh`, replace the `curl ... :8765/wake` line
   with: `timeout 1 sh -c 'echo > /run/touch-timeout/wake'`.
   The `timeout` matters: if the daemon is down, the FIFO has no reader and a
   plain `echo` blocks forever, which could stall playback start if the hook
   waits for completion. Fallback if `timeout` is missing in the container
   image: `echo > /run/touch-timeout/wake &`.

If the container starts before the daemon, Docker creates the directory on the
host; the daemon then adopts it (step 1 accepts EEXIST) and creates the FIFO.

Host-side alternatives, both supported: `echo > /run/touch-timeout/wake` or
`pkill -USR1 touch-timeout`.

### 5.6 systemd Unit (`systemd/touch-timeout.service`)

- Adopt the hardening directives from branch
  `claude/security-privacy-review-l1v2sy` (ProtectHome, ProtectKernelModules,
  ProtectControlGroups, RestrictAddressFamilies=AF_UNIX, RestrictNamespaces,
  RestrictRealtime, LockPersonality, MemoryDenyWriteExecute,
  SystemCallFilter=@system-service, SystemCallErrorNumber=EPERM), including its
  comment explaining why ProtectKernelTunables is NOT set.
- Add `RestartPreventExitStatus=2`. Without it, `StartLimitIntervalSec=0` plus
  `Restart=on-failure` restarts a misconfigured service every 5 s forever.
- Remove `systemd-backlight@.service` from `After=`. A template name without an
  instance matches nothing.
- Keep `Type=simple`. No `RuntimeDirectory=` (section 5.3).
- Must verify on device (phase 3 test): FIFO creation, `fchmod`, and signalfd
  work under `SystemCallFilter=@system-service`. If anything returns EPERM, add
  the specific syscall to the filter; do not remove the filter.

---

## 6. CLI

### 6.1 Options

| Option | Meaning | Range | Default | Change |
|---|---|---|---|---|
| `-b, --brightness=N` | Full brightness | 15-255, then clamped to hw max | 150 | Out of range: exit 2 |
| `-t, --timeout=N` | Seconds idle until OFF | 10-86400 | 300 | Out of range: exit 2 |
| `-d, --dim-percent=N` | Dim at N% of timeout | 1-100 | 10 | Now affects timing ONLY; out of range: exit 2 |
| `-D, --dim-level=N` | Dim brightness as N% of full | 1-100 | 10 | NEW |
| `-l, --backlight=NAME` | Backlight device | name, no `/` or `..`, < 64 chars | auto | Invalid: exit 2 |
| `-i, --input=NAME` | Input device | same as above | auto | Invalid: exit 2 |
| `-v, --verbose` | DEBUG logging to stderr | | off | |
| `-V, --version` | Print version, exit 0 | | | |
| `-h, --help` | Print usage, exit 0 | | | Update text: FIFO and SIGUSR1 wake |

Defaults reproduce v0.8.0 behavior exactly: dim at 30 s to brightness 15, off
at 300 s. Running with no arguments is the primary use case.

### 6.2 Derived Values

- `dim_brightness = brightness * dim_level / 100`, floored at 10 (existing
  `MIN_DIM_BRIGHTNESS`). The existing `calculate_dim_brightness()` already has
  this shape; change only its caller to pass `dim_level`.
- `dim_sec`, `off_sec`: unchanged `calculate_timeouts()` using `dim_percent`.

### 6.3 Validation Structure

Separate parsing from range checking so range rules are unit-testable without
`exit()`: a validation function takes the parsed config and returns success or
an error identifying the bad field. `parse_args()` prints the error and usage
hint, then exits 2. Unknown options and non-numeric values also exit 2.

---

## 7. Error Handling Summary

| Condition | Response |
|---|---|
| Invalid CLI | stderr message, exit 2 |
| Backlight or input open fails | ERROR, exit 1 (systemd retries; covers DSI not ready at boot) |
| `max_brightness` unreadable | WARN, assume 255 (unchanged) |
| Initial brightness write fails | ERROR, exit 1 |
| signalfd or sigprocmask fails | ERROR, exit 1 |
| FIFO setup fails | WARN, continue without FIFO |
| FIFO POLLHUP/POLLERR | WARN, disable FIFO, continue |
| Input POLLHUP/POLLERR | ERROR, restore brightness, exit 1 |
| Brightness write fails in loop | Edge-triggered ERROR, retry next cycle (section 4.4) |
| `poll()` fails (not EINTR) | ERROR, restore brightness, exit 1 |
| Input read returns error other than EAGAIN | Treat as no activity; POLLHUP handling covers device loss |

---

## 8. Timers

Unchanged. Granularity is 1 s: timestamps are whole seconds from
`CLOCK_MONOTONIC`, so transitions occur within 1 s of the nominal time. This is
acceptable for a screen timeout; do not add millisecond precision.

---

## 9. Security Branch Disposition (C9)

Branch `claude/security-privacy-review-l1v2sy` (e064b64) is NOT merged as a
whole.

| Branch content | Disposition |
|---|---|
| Makefile `BASE_CFLAGS`: `-std=c11`, `-Wformat=2`, `-fstack-protector-strong`, `-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2`, shared by native and cross targets | Adopt |
| `tests/Makefile` `-std=c11` | Adopt |
| `touch-timeout.service` hardening | Adopt (section 5.6) |
| `main.c` `ppoll()` + blocked-signal loop | Drop; superseded by C1 |
| `http-wake.py` and `http-wake.service` changes | Drop; files are deleted (C3) |
| CHANGELOG "Unreleased" text | Drop; write a fresh 0.9.0 entry |

After v0.9.0 is merged, the remote branch is deleted (human action).

---

## 10. Retire http-wake (C3)

Delete: `scripts/http-wake.py`, `scripts/install-http-wake.sh`,
`systemd/http-wake.service`. Remove every reference (section 11 lists them).

Do not add migration code to `install.sh`. Instead, CHANGELOG 0.9.0 and
INSTALLATION.md carry a one-time upgrade note telling the user to stop and
disable `http-wake.service`, then delete `/etc/systemd/system/http-wake.service`
and `/usr/bin/http-wake`. One device is affected; permanent migration code
would be cruft.

---

## 11. Documentation Cleanup Directive (D1)

Rules that apply to every doc and header comment touched in v0.9:

- R1. ARCHITECTURE.md, README.md, INSTALLATION.md, and source headers describe
  implemented behavior only. Plans live in this file.
- R2. No hardcoded counts that drift (lines of code, test counts, memory). Either
  point to the command that produces the number (`make test`,
  `scripts/test-performance.sh`) or state the number with its measurement
  method and date.
- R3. Every documented command and example must be run once against the v0.9
  build before the commit that adds it. If it can't be run (device-only), the
  phase 3 integration run covers it.
- R4. Documentation lives at point of use (5S): file header comments say what
  the file does and why, then point to related files. No orphan references.
- R5. Historical documents (PROJECT-HISTORY.md, past CHANGELOG entries) keep
  their narrative. Fix only factual errors and dead links; do not rewrite
  history.

Specific items found in the review:

| File | Fix |
|---|---|
| `src/main.c` header | Remove `CLAUDE.md` reference (file intentionally not in repo) and the libsystemd/sd_notify dependency; rewrite EVENT LOOP DESIGN for signalfd and FIFO |
| `src/main.c` usage text | Document FIFO and SIGUSR1 wake, `--dim-level` |
| `Makefile` header | Remove "Optional: pkg-config libsystemd"; remove the `SYSTEMD_PKG` detection block |
| `doc/ARCHITECTURE.md` | Update FR5 wording, block diagram (three polled fds plus timeout), event loop steps, key design choices; remove "Optional features (libsystemd)"; replace "48 tests" per R2; add exit codes and FIFO interface |
| `README.md` | Replace External Wake section (FIFO for containers, SIGUSR1 for host); remove http-wake and port 8765; add `--dim-level` to options table; replace "~360 KB memory" per R2; "poll-based" stays accurate |
| `doc/INSTALLATION.md` | Replace the shairport section: the `/etc/shairport-sync.conf` `pkill` example does not work from inside the container. Document section 5.5 steps and the http-wake upgrade note |
| `CHANGELOG.md` | Add 0.9.0 entry. Fix 0.7.0 date `2024-12-19` to `2025-12-19` (it follows 0.6.0 of 2025-12-19). Flag, do not guess: 0.3.0 (2024-11-12) is dated before 0.2.0 (2024-11-14); see section 14.2 |
| `CHANGELOG.md`, `doc/PROJECT-HISTORY.md` | v0.7 size is stated as ~620 lines in one and ~370 in the other. Measured with `wc -l src/*.c src/*.h` at tag v0.7.0: 709 lines. Replace both with "709 lines (wc -l, all of src/)" |
| `doc/PROJECT-HISTORY.md` | Reference to `doc/DESIGN.md` now resolves to this file; change its description to "Design specification (v0.9 delta)". Line 78 historical mention stays (R5) |
| `scripts/test-integration.sh` | Header comment: add FIFO and SIGHUP tests; fix the "~5s" DIMMED comment (with `-t 10` and default `-d 10`, dim happens at 1 s) |
| `scripts/test-performance.sh` | Header and output: report Pss and Rss (C10) |
| `systemd/touch-timeout.service` | Comments for new directives (section 5.6) |

Done criterion for D1: `grep -rn -i "http-wake\|8765\|sd_notify\|HAVE_SYSTEMD\|libsystemd\|CLAUDE.md"`
outside `.git/`, `CHANGELOG.md` history entries, and this file returns nothing.

---

## 12. Build and Test Strategy

### 12.1 Principles

- Red, green, refactor per phase. Tests for a phase are committed failing (or
  shown failing in the session) before the implementation.
- No mocks. Unit tests use real files in a temp directory: a regular file stands
  in for the sysfs `brightness` attribute (read back only the bytes written), and
  real FIFOs test the FIFO setup.
- Scripts and Makefiles are code: `bash -n` every script (already in
  `test-integration.sh`), and every new script gets a descriptive header.
- On-device test runs write logs to `/run/touch-timeout-staging/`, not `/tmp`,
  so testing never writes to the SD card.

### 12.2 Test Inventory

Unit tests (`tests/test_state.c`, native, `make test`):

| ID | Change | Test |
|---|---|---|
| U1 | C6 | Validation rejects each field at min-1 and max+1, accepts min and max |
| U2 | C5 | `--dim-level` 10 with brightness 150 gives 15; dim level 1 floors to 10; dim level is independent of dim percent |
| U3 | C2 | FIFO setup in a temp dir creates dir and FIFO; mode is exactly 0622 even with umask 022 set during the test |
| U4 | C2 | FIFO setup replaces a pre-existing regular file and a symlink at the FIFO path |
| U5 | C2 | FIFO setup reuses an existing FIFO (same inode before and after) |
| U6 | C2 | No EOF spin: after setup, open a separate writer, write, close it; drain; then `poll()` with zero timeout reports no POLLIN and no POLLHUP |
| U7 | C2 | Drain consumes a write larger than the read buffer in one call sequence and leaves the fd empty |
| U8 | C4 | Apply against a temp file: write occurs when desired differs; no write when equal; after a forced failure (read-only fd), `hw_brightness` is unchanged |
| U9 | C1 | signalfd drain: block SIGUSR1 and SIGTERM, `raise()` both, drain reports wake and shutdown |

Integration tests (`scripts/test-integration.sh`):

| ID | Where | Test |
|---|---|---|
| I1 | Native | `-t 5`, `-b 999`, `-D 0`, `--bogus` each exit with status 2 |
| I2 | Native | `--help` and `--version` exit 0; help text mentions the FIFO path and SIGUSR1 |
| I3 | Device | FULL to DIMMED to OFF via timeout (existing, log path moved to `/run`) |
| I4 | Device | SIGUSR1 from OFF logs `SIGUSR1 -> FULL` (existing) |
| I5 | Device | `echo > /run/touch-timeout/wake` from OFF logs `FIFO -> FULL` |
| I6 | Device | No spin after FIFO writes: write to FIFO 5 times, wait 5 s, utime+stime from `/proc/PID/stat` increases by at most 1 tick |
| I7 | Device | SIGTERM restores brightness: after kill, sysfs `brightness` equals the `-b` value; same for SIGHUP |
| I8 | Device | With daemon stopped, `timeout 1 sh -c 'echo > /run/touch-timeout/wake'` returns within 2 s |
| I9 | Device | FIFO mode is 0622 and directory survives daemon stop |
| I10 | Device | Under the installed systemd unit (with SystemCallFilter): service active, FIFO present, I5 passes against the service instance |

Test-run hygiene: on-device tests stop the systemd service first (record whether
it was active) and restart it at the end if it was. Two instances must never
share the FIFO.

Not automated, verified by review: C7 (input device removal is impractical to
trigger on a DSI panel). Manual tests that remain manual: touch wake, container
hook end-to-end (play a track over AirPlay with the screen OFF).

Performance (`scripts/test-performance.sh`, device): CPU about 0% idle, fd delta
0, SD writes 0, Pss and Rss reported. Pss at or under 200 kB is an aspiration:
report it, never fail on it. Do not change libc or toolchain to chase it.

---

## 13. Implementation Phases

Each phase ends with `make test` green, `make arm64` building cleanly with the
hardened flags and no new warnings, and one commit.

| Phase | Content | Tests first | Done when |
|---|---|---|---|
| 1 | C9 build flags (c11, hardening), C8 remove sd_notify/libsystemd | Existing 48 pass under new flags | Clean build native and arm64; no `HAVE_SYSTEMD` left |
| 2 | C6 validation, C5 `--dim-level` | U1, U2, I1, I2 | Defaults unchanged; bad args exit 2 |
| 3 | C1 signalfd, C2 FIFO, C7 input HUP, C4 apply, unit file (5.6) | U3-U9, I3-I10 | All integration tests pass on device, including I10 under the unit |
| 4 | C3 delete http-wake files | grep check from section 11 | Files gone |
| 5 | D1 docs, C10 perf metric, version 0.9.0, CHANGELOG | R3 run-through | Section 11 done criterion met; perf run recorded in CHANGELOG with date |

Phase 3 is the largest. If it gets unwieldy, split signalfd+apply (3a) from
FIFO (3b); tests U9 and U8 belong to 3a.

---

## 14. Decisions

### 14.1 Decided (with rationale)

| Decision | Choice | Rationale |
|---|---|---|
| Wake mechanisms | FIFO plus SIGUSR1 | FIFO crosses the container boundary; SIGUSR1 costs one line once signalfd exists and preserves host-side usage |
| http-wake | Retire | Its only purpose is now served by the FIFO |
| Signal handling | signalfd, not the branch's ppoll | Uniform fd-based loop; no async handlers |
| FIFO path | Fixed `/run/touch-timeout/wake` | Stable contract for the container; no option to misconfigure |
| FIFO failure | Non-fatal | Core function (dimming) must not depend on an optional integration |
| FIFO lifetime | Persist across daemon exit | Stable bind mount; `timeout` in the hook covers the no-reader case |
| FIFO mode | 0622 | Wake is harmless; avoids container UID mapping problems |
| Dim brightness control | New `-D/--dim-level`, default 10% | Decouples it from timing without changing defaults; keeps defaults as computed relationships |
| Out-of-range CLI | Exit 2, no restart | Visible failure instead of silently running something else |
| Input device lost | Exit 1, systemd restart | Self-heals and re-detects; no hot-plug code |
| Timer | Keep poll timeout | Simpler than timerfd; documented exception to "all sources are fds" |
| Memory 200 kB | Aspiration, reported as Pss | Static glibc likely can't reach it; not worth a toolchain change |
| Spec file | `doc/DESIGN.md` | Matches existing descriptive/prescriptive doc split; fixes a dead link |

### 14.2 Human Actions and Open Questions

| Item | Owner |
|---|---|
| Fix CHANGELOG: 0.3.0 is dated 2024-11-12, before 0.2.0 at 2024-11-14. Which date is wrong? | Human answers; Claude Code applies |
| On the device: add the bind mount and update `pause-others.sh` (section 5.5); restart the shairport container | Human |
| On the device: remove the installed http-wake service and binary (section 10) | Human |
| Delete remote branch `claude/security-privacy-review-l1v2sy` after v0.9.0 merges | Human |
| Confirm `timeout` exists in the shairport container image; if not, use the background `&` fallback | Human, during device setup |

---

## 15. Guardrails (Do Not)

- Do not rewrite or restructure `state.c/h` or merge modules. The FSM API does
  not change.
- Do not introduce timerfd, epoll, threads, dynamic allocation, a config file,
  D-Bus, or a FIFO command protocol. FIFO content is ignored.
- Do not add a CLI option for the FIFO path.
- Do not add brightness ramping, multi-display, or hot-plug re-detection.
- Do not add migration logic to `install.sh`.
- Do not merge the security branch wholesale (section 9).
- Do not reword the log strings in section 3.4.
- Do not use emoji anywhere: code, comments, logs, docs, commit messages.
- Do not write to the SD card during tests; use `/run`.
- If a change seems to need any of the above, stop and ask.
