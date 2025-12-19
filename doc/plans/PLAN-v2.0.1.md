# v2.0.1 Release Plan (Comprehensive)

**Type:** Patch release
**Scope:** Fix CLOCK_MONOTONIC violation + CERT C compliance + UX improvements
**Philosophy:** Daemon, not library. You own all callers.
**Changes:** ~78 lines net reduction (simplification + security hardening)

**Single pass:** Code changes and doc updates together (avoids rework)

---

## Core Change: Simplify State Machine (Option B)

### Problem

State machine tracks time twice:
```
timer.c:    timerfd (CLOCK_MONOTONIC)     → knows when to fire
state.c:    time(NULL) (CLOCK_REALTIME)   → re-calculates when to fire
```

Documentation promises CLOCK_MONOTONIC immunity. Reality: state.c uses CLOCK_REALTIME.

### Solution

Remove time tracking from state.c entirely. State machine becomes pure FSM:

**Before (state.c tracks time):**
```c
typedef struct {
    state_type_t current_state;
    int user_brightness;
    int dim_brightness;
    int dim_timeout_sec;
    int off_timeout_sec;
    time_t last_input_time;      // DELETE
} state_t;

bool state_handle_event(state_t *state, state_event_t event, int *new_brightness) {
    time_t now = time(NULL);                    // DELETE
    double idle = difftime(now, state->last_input_time);  // DELETE

    if (idle < -5.0) { ... }                    // DELETE (clock adjustment hack)

    if (idle >= state->off_timeout_sec) { ... } // DELETE (time comparison)
}

int state_get_next_timeout(const state_t *state) {  // DELETE ENTIRE FUNCTION
    // 30+ lines of timeout calculation
}
```

**After (pure FSM):**
```c
typedef struct {
    state_type_t current_state;
    int user_brightness;
    int dim_brightness;
    int dim_timeout_sec;
    int off_timeout_sec;
    // No time tracking
} state_t;

// State transitions are deterministic:
// TIMEOUT event in FULL    → DIMMED
// TIMEOUT event in DIMMED  → OFF
// TOUCH event in any state → FULL

bool state_handle_event(state_t *state, state_event_t event, int *new_brightness) {
    switch (event) {
        case STATE_EVENT_TOUCH:
            if (state->current_state != STATE_FULL) {
                state->current_state = STATE_FULL;
                *new_brightness = state->user_brightness;
                return true;
            }
            return false;

        case STATE_EVENT_TIMEOUT:
            switch (state->current_state) {
                case STATE_FULL:
                    state->current_state = STATE_DIMMED;
                    *new_brightness = state->dim_brightness;
                    return true;
                case STATE_DIMMED:
                    state->current_state = STATE_OFF;
                    *new_brightness = 0;
                    return true;
                case STATE_OFF:
                    return false;  // Already off
            }
    }
    return false;
}

// Returns seconds until next transition (for timer arming)
int state_get_timeout(const state_s *state) {
    switch (state->current_state) {
        case STATE_FULL:   return state->dim_timeout_sec;
        case STATE_DIMMED: return state->off_timeout_sec - state->dim_timeout_sec;
        case STATE_OFF:    return -1;  // No timeout
    }
    return -1;
}
```

**main.c change:**
```c
// After any event, rearm timer based on current state
int timeout = state_get_timeout(&state);
if (timeout > 0)
    timer_arm(timer, timeout);
```

### Benefits

| Aspect | Before | After |
|--------|--------|-------|
| Clock domain | REALTIME (wrong) | None (correct) |
| -5.0 hack | Required | Eliminated |
| time() calls | Every event | Zero |
| difftime() | Every event | Zero |
| Lines in state.c | ~180 | ~80 |
| Testability | Needs time mocking | Pure logic |

---

## Remove Internal NULL Checks

### Philosophy

- **Entry points** (open, init, close): Keep NULL checks - external inputs
- **Internal functions** (handle_event, getters): Remove - you own all callers

### What to remove

**state.c:**
```c
// REMOVE (line 55-58):
if (state == NULL || new_brightness == NULL) {
    syslog(LOG_ERR, "state_handle_event: NULL parameter");
    return false;
}

// REMOVE (line 119-120):
if (state == NULL)
    return STATE_OFF;

// REMOVE (line 129-130):
if (state == NULL)
    return -1;
```

**display.c:**
```c
// KEEP (line 51-54) - entry point:
if (backlight_name == NULL) {
    syslog(LOG_ERR, "display_open: NULL backlight_name");
    return NULL;
}

// REMOVE (line 127-130) - internal:
if (display == NULL) {
    syslog(LOG_ERR, "display_set_brightness: NULL display");
    return -1;
}
```

**Same pattern for input.c, timer.c, config.c**

### Lines saved

~60 lines of NULL checks + error messages

---

## Security Fixes (Keep)

### 1. signal() → sigaction() (main.c:84)

```c
// Before:
signal(SIGPIPE, SIG_IGN);

// After:
sa.sa_handler = SIG_IGN;
sigaction(SIGPIPE, &sa, NULL);
```

**Why:** CERT SIG31-C compliance. Mixing APIs is undefined behavior on some systems.

### 2. atoi() → safe conversion (display.c:44,96)

```c
// Before:
return atoi(buf);

// After:
int val;
if (parse_int(buf, &val) < 0)
    return -1;
return val;
```

**Why:** CERT INT31-C. atoi() returns 0 on error (valid brightness).

### 3. Inline read_sysfs_int() (display.c:32-44)

Function only called once, code duplicated inline at lines 92-96. Remove function, inline with safe parsing.

### 4. File descriptor validation (CERT FIO42-C)

**Issue:** Code validates with `fd > 0` before closing, should be `fd >= 0`

**Impact:** FD 0 (stdin) is valid. If stdin closed before device open, fd could be 0 and wouldn't be properly managed.

**Locations:** display.c (2 places), input.c (2 places), timer.c (2 places)

**Fix:** Change comparison operator in all close() and validation checks

**Lines:** +0 (character-level change only)

### 5. snprintf truncation warnings (CERT STR31-C)

**Issue:** snprintf() return value not checked - silent truncation of device paths

**Impact:** "rpi_backlight" → "rpi_backli" causes device open failures without warning

**Locations:** config.c parameter assignment (3 places)

**Fix:** Check return value, log WARNING if truncated

**Lines:** +15 (validation and logging)

### 6. Path validation enhancement (CERT FIO32-C defense-in-depth)

**Issue:** Path validation missing edge cases

**Current:** Rejects absolute paths, `..` traversal, path separators

**Missing:** Trailing slashes, backslashes, shell metacharacters

**Impact:** Defense-in-depth for embedded security

**Location:** config.c validate_path_component()

**Fix:**
```
PSEUDOCODE:
add checks for:
    - trailing slashes or backslashes
    - shell metacharacters: ; & | < > $ `
reject with error if found
```

**Lines:** +5

### 7. Restore brightness on shutdown

**Issue:** DESIGN.md documents brightness restoration but code skips it

**Impact:** Screen stays off/dimmed when daemon stops (poor UX)

**Location:** main.c cleanup section before display_close()

**Fix:**
```
PSEUDOCODE:
before closing display:
    get user_brightness from state
    set display to user_brightness
    log restoration
```

**Lines:** +5

---

## Type Suffix Standardization

Per original code standards:
- `_s` suffix for struct typedefs (avoids POSIX `_t` conflicts)
- `_e` suffix for enum typedefs

| Current | Correct | Type |
|---------|---------|------|
| `display_t` | `display_s` | struct |
| `input_t` | `input_s` | struct |
| `config_t` | `config_s` | struct |
| `state_t` | `state_s` | struct |
| `timer_ctx_s` | `timer_s` | struct |
| `state_type_t` | `state_type_e` | enum |
| `state_event_t` | `state_event_e` | enum |

---

## Function Naming Standardization

### Static functions need module prefix (config.c)

| Current | Correct |
|---------|---------|
| `trim()` | `config_trim()` |
| `validate_path_component()` | `config_validate_path()` |
| `find_param()` | `config_find_param()` |
| `validate_param()` | `config_validate_param()` |
| `parse_config_line()` | `config_parse_line()` |

### Timer module HAL pattern

Align with display/input pattern (`*_open`/`*_close` not `*_create`/`*_destroy`):

| Current | Correct |
|---------|---------|
| `timer_create_ctx()` | `timer_open()` |
| `timer_destroy()` | `timer_close()` |

---

## Poll Array Enum

```c
enum { FD_INPUT, FD_TIMER, FD_COUNT };
struct pollfd fds[FD_COUNT];
```

Self-documenting, enables cleaner poll error handling in v2.1.0.

---

## Remove Redundant Comments

Comments that repeat what code says:

```c
// REMOVE:
/* Touch always restores to FULL state */
if (state->current_state != STATE_FULL) {

// REMOVE:
/* Main event loop */
while (g_running) {

// REMOVE:
/* Disarm timer */
spec.it_value.tv_sec = 0;
```

~15 lines saved.

---

## Documentation Updates (same commit)

These docs are updated WITH the code to avoid rework:

| Doc | Change |
|-----|--------|
| ARCHITECTURE.md:363-379 | Module interfaces - `state_get_timeout_for_current()` |
| ARCHITECTURE.md:204-220 | Test counts - update after test rewrite |
| ARCHITECTURE.md:448-452 | LOC metrics - update line counts |
| CLAUDE.md:297-302 | State machine example - correct API |
| CHANGELOG.md | Add v2.0.1 entry |

**NOT updated (already fixed in pre-v2.0.1 commit):**
- Timer API docs
- Dead config reference
- Roadmap v2.2.0

---

## What's NOT in v2.0.1

| Item | Reason |
|------|--------|
| Integration tests for main.c | Complex harness, low ROI (deferred to manual testing) |
| Makefile cross-compile improvements | Convenience, not a bug |
| Header doc comments cleanup | Useful for understanding, keep them |
| Systemd stubs removal | Keep portability option |

---

## Test Updates Required

State machine tests change significantly:

**Before:**
```c
// Tests manipulate time:
state.last_input_time -= 6;  // Simulate 6 seconds passing
state_handle_event(&state, STATE_EVENT_TIMEOUT, &brightness);
```

**After:**
```c
// Tests verify state transitions directly:
state_handle_event(&state, STATE_EVENT_TIMEOUT, &brightness);
ASSERT_EQ(state.current_state, STATE_DIMMED);

state_handle_event(&state, STATE_EVENT_TIMEOUT, &brightness);
ASSERT_EQ(state.current_state, STATE_OFF);
```

Tests become simpler because they don't need to simulate time.

---

## Summary

| Change | Lines |
|--------|-------|
| Simplify state.c (Option B) | -50 |
| Remove internal NULL checks | -60 |
| Remove redundant comments | -15 |
| sigaction fix | +2 |
| safe atoi | +10 |
| FD validation fix | +0 |
| snprintf truncation checks | +15 |
| Path validation enhancement | +5 |
| Restore brightness on shutdown | +5 |
| **Net** | **~-78** |

**Result:** 850 → ~772 lines (9% reduction)

**Note:** Performance metrics verified accurate (RSS ~196 KB after 3+ days runtime)

---

## Pre-Commit Review

Before committing, run full code review:

1. **code-reviewer agent**: Verify all changes match plan, no regressions
2. **doc-reviewer agent**: Verify docs updated, SSoT maintained
3. **make test**: All tests pass
4. **Manual verification**: Build succeeds for native and ARM targets

---

## Changelog Entry

```markdown
## [2.0.1] - YYYY-MM-DD

### Fixed

- **CLOCK_MONOTONIC compliance**: State machine no longer tracks time internally
  (eliminates CLOCK_REALTIME dependency that contradicted documentation)
- **CERT SIG31-C**: Use sigaction() consistently (was mixing with signal())
- **CERT INT31-C**: Safe integer parsing in display module (was using atoi())
- **CERT FIO42-C**: File descriptor validation now handles FD 0 correctly
- **CERT STR31-C**: Detect and warn on snprintf truncation
- **CERT FIO32-C**: Enhanced path validation (defense-in-depth)
- **Brightness restoration**: Screen brightness restored to full on daemon shutdown

### Changed

- **State machine simplified**: Pure FSM without time tracking (~50 lines removed)
- **Defensive code reduced**: Internal NULL checks removed (daemon, not library)
- **Cleaner code**: Removed comments that repeated code

### Removed

- Clock adjustment workaround (-5.0 second backward jump detection) - no longer needed
```
