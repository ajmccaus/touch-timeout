# Documentation Issues - Implementation vs Documentation Analysis

**Generated:** 2025-12-15
**Analysis Type:** Comprehensive codebase exploration with 3 parallel agents
**Version:** v2.0.0

## Summary

This document tracks discrepancies found between the touch-timeout implementation and its documentation during a systematic codebase exploration.

**Total Issues:** 9
**Critical:** 1
**High:** 3
**Medium:** 4
**Low:** 1

---

## Issue List

| # | Severity | Type | Location | Issue | Current State | Correct State | Impact |
|---|----------|------|----------|-------|---------------|---------------|--------|
| 1 | Medium | API Name | ARCHITECTURE.md:64 | Function name mismatch | Documents `input_has_events()` | Should be `input_has_touch_event()` | Contributors reading ARCHITECTURE.md will not find the documented function |
| 2 | Medium | Test Count | ARCHITECTURE.md:136 | Outdated test count | States "State: 21 tests" | Should be "State: 31 tests" | Misleading test coverage metrics |
| 3 | High | Design Rationale | state.h:37 | Clock source comment unclear | Comment: "CLOCK_REALTIME for compatibility" | Explain: "CLOCK_REALTIME despite MONOTONIC timer; clock adjustment detection (state.c:83-87) handles mismatch" | Confusing design decision without rationale; contributors may "fix" intentional behavior |
| 4 | Critical | API Documentation | ARCHITECTURE.md:58 | Non-existent API documented | Documents `display_get_brightness()` | Remove - API doesn't exist. Only `display_get_max_brightness()` implemented. Brightness tracked via internal caching. | Contributors will try to use non-existent API |
| 5 | Low | Config Reference | README.md:64 | Minimum brightness not in quick reference | Quick ref shows "15-255" without emphasizing minimum | Add note: "minimum 15 prevents flicker" in quick reference | Users might try brightness <15 without understanding constraint |
| 6 | Medium | Line Numbers | ARCHITECTURE.md:80 | Outdated line references | States "Poll loop (lines ~130-180)" | Actual: lines 203-248 in main.c | Line references break after code changes; consider removing or using relative references |
| 7 | Medium | Build Requirements | ARCHITECTURE.md:106 | Stub implementation not mentioned | States systemd features "requires libsystemd at build time" | Add: "Stub implementations provided (main.c:40-48) - builds work without libsystemd" | Users might think libsystemd is required for building |
| 8 | High | Error Handling | config.h + ARCHITECTURE.md | Dual error handling strategy unclear | Two strategies exist but not clearly distinguished | Add section explaining: (1) File-based config: fallback to defaults, (2) CLI args: fail fast. Rationale documented. | Contributors confused about when to use graceful vs strict validation |
| 9 | Low | Code Reference | Explorer agent report | Brightness caching line number uncertain | Agent reported "display.c line ~115" | Verify actual line number in display.c or use function name reference | Minor - agent may have reported approximate line |

---

## Detailed Issue Descriptions

### Issue 1: API Name Mismatch (input_has_events)

**File:** `doc/ARCHITECTURE.md:64`

**Current Documentation:**
```markdown
**input.h:**
- `input_has_events()` - Check and drain pending events
```

**Actual Implementation:**
- Function: `input_has_touch_event()` (input.h, main.c:216, main.c:231)
- Consistently used throughout codebase

**Fix Required:**
```markdown
**input.h:**
- `input_has_touch_event()` - Check and drain pending touch events
```

**Verification:**
```bash
grep -n "input_has_" src/input.h src/main.c
```

---

### Issue 2: Outdated Test Count

**File:** `doc/ARCHITECTURE.md:136`

**Current Documentation:**
```markdown
**Test counts:**
- Config: 44 tests (parsing, validation, security)
- State: 21 tests (transitions, edge cases)
```

**Actual Test Counts:**
- Config: 44 tests ✓ (correct)
- State: **31 tests** (not 21)

**Fix Required:**
```markdown
**Test counts:**
- Config: 44 tests (parsing, validation, security)
- State: 31 tests (transitions, edge cases, clock adjustment)
```

**Verification:**
```bash
./tests/test_state 2>&1 | grep -E "Tests (run|passed)"
```

---

### Issue 3: Clock Source Design Rationale Missing

**File:** `src/state.h:37`

**Current Comment:**
```c
time_t last_input_time; /* Timestamp of last touch (CLOCK_REALTIME for compatibility) */
```

**Problem:**
- State machine uses `time(NULL)` (CLOCK_REALTIME)
- Timer uses `timerfd_create(CLOCK_MONOTONIC, ...)` (timer.c:42)
- **Intentional mismatch** - handled by clock adjustment detection (state.c:83-87)
- Comment doesn't explain *why* or *what* compatibility means

**Better Comment:**
```c
time_t last_input_time; /* Timestamp of last touch (CLOCK_REALTIME).
                         * Despite timer using CLOCK_MONOTONIC, REALTIME chosen for:
                         * 1. Simplicity (time(NULL) is portable)
                         * 2. Clock adjustment detection (state.c:83-87) handles NTP changes
                         * 3. Mismatch is intentional and tested
                         */
```

**Additional Fix:** Add design rationale section to ARCHITECTURE.md or DESIGN.md

---

### Issue 4: Non-Existent API Documented

**File:** `doc/ARCHITECTURE.md:58`

**Current Documentation:**
```markdown
**display.h:**
- `display_open()` / `display_close()` - Lifecycle
- `display_set_brightness()` - Set brightness (cached)
- `display_get_brightness()` - Read current brightness  ← DOESN'T EXIST
- `display_get_max_brightness()` - Read hardware maximum
```

**Actual display.h API:**
- `display_open()` ✓
- `display_close()` ✓
- `display_set_brightness()` ✓
- `display_get_max_brightness()` ✓
- `display_get_brightness()` ✗ (NOT IMPLEMENTED)

**Fix Required:**
Remove the non-existent API from documentation:
```markdown
**display.h:**
- `display_open()` / `display_close()` - Lifecycle
- `display_set_brightness()` - Set brightness (cached internally)
- `display_get_max_brightness()` - Read hardware maximum
```

**Note:** Current brightness is tracked via `display->current_brightness` internal caching, not exposed via getter.

**Verification:**
```bash
grep -n "display_get_brightness" src/display.h src/display.c
```

---

### Issue 5: Brightness Minimum Not in Quick Reference

**File:** `README.md:64`

**Current Quick Reference:**
```markdown
- `brightness=150` - Active screen brightness (15-255, recommend ≤200 for RPi official 7" touchscreen)
```

**Problem:**
- Shows range "15-255" but doesn't explain **why** minimum is 15
- config.h:24 defines `CONFIG_MIN_BRIGHTNESS 15 /* Avoid flicker */`
- CLAUDE.md explains flicker prevention but not in user-facing README

**Better Quick Reference:**
```markdown
- `brightness=150` - Active screen brightness (15-255, min 15 prevents flicker; recommend ≤200 for RPi official 7" touchscreen)
```

**Impact:** Minor - users experimenting with values <15 will be clamped, but won't understand why

---

### Issue 6: Outdated Line Number References

**File:** `doc/ARCHITECTURE.md:80`

**Current Documentation:**
```markdown
2. **Poll loop** (lines ~130-180)
   - Block on input_fd and timer_fd
   - Process events, update state, apply brightness
```

**Actual main.c:**
- Initialization: lines ~99-194
- Poll loop: lines 203-248
- Cleanup: lines 253-266

**Problem:** Line numbers drift as code evolves

**Fix Options:**

**Option A:** Update to current line numbers
```markdown
2. **Poll loop** (lines 203-248)
```

**Option B:** Use relative references (recommended)
```markdown
2. **Poll loop** (see `main.c` poll loop section)
```

**Option C:** Remove line numbers entirely (most maintainable)
```markdown
2. **Poll loop**
   - Block on input_fd and timer_fd (using `poll()`)
   - Process events, update state, apply brightness
```

**Recommendation:** Option C - line numbers are fragile

---

### Issue 7: Systemd Stub Implementation Not Mentioned

**File:** `doc/ARCHITECTURE.md:106`

**Current Documentation:**
```markdown
**Optional features** (requires libsystemd at build time):
- sd_notify() for startup confirmation
- Watchdog pinging
```

**Actual Implementation (main.c:36-48):**
```c
#ifdef HAVE_SYSTEMD
#include <systemd/sd-daemon.h>
#else
/* Stub implementations if systemd not available */
static inline int sd_notify(int unset_environment, const char *state) {
    (void)unset_environment; (void)state;
    return 0;
}
...
#endif
```

**Problem:** Documentation implies libsystemd is required for building

**Better Documentation:**
```markdown
**Optional systemd features** (libsystemd at build time enables full support):
- sd_notify() for startup confirmation
- Watchdog pinging
- **Note:** Stub implementations provided (main.c:36-48) - daemon builds without libsystemd
```

**Impact:** Users on minimal systems might think they can't build

---

### Issue 8: Configuration Error Handling Strategy Not Clearly Explained

**Files:** `src/config.c`, `src/config.h`, `doc/ARCHITECTURE.md`

**Current State:**
- `config.h:57` says: "Invalid values are logged and defaults preserved"
- `config.h:100` says: "Invalid values are rejected (config unchanged)"
- Implementation has TWO strategies but not clearly distinguished

**Actual Implementation:**

**Strategy 1:** `parse_config_line()` (config.c:191-221) - File-based config
```c
if (validate_param(param, value, &int_val) != 0) {
    syslog(LOG_WARNING, "Invalid %s='%s' at line %d, keeping default %d", ...);
    return 0;  // Fallback to default - not a fatal error
}
```

**Strategy 2:** `config_set_value()` (config.c:226-257) - CLI arguments
```c
if (validate_param(param, value, &int_val) != 0) {
    syslog(LOG_ERR, "Invalid %s='%s' (valid range: %d-%d)", ...);
    return -1;  // Signal failure
}
```

**Design Rationale:**
- **File-based:** Daemon shouldn't fail for invalid config file (graceful degradation)
- **CLI args:** Explicit command-line errors should fail fast (fail-safe)

**Fix Required:**

Add to ARCHITECTURE.md:
```markdown
## Configuration Error Handling

The configuration system uses **dual error handling strategies**:

| Source | Strategy | Rationale |
|--------|----------|-----------|
| Config file (`/etc/touch-timeout.conf`) | **Graceful fallback** - invalid values logged, defaults preserved | Daemon shouldn't fail to start due to config file errors |
| CLI arguments | **Fail fast** - invalid values rejected, daemon aborts | Explicit user errors should be visible immediately |

**Implementation:**
- `config_load()` → `parse_config_line()` - fallback strategy
- `config_set_value()` (CLI args in main.c) - strict validation
```

---

### Issue 9: Brightness Caching Line Number Uncertain

**Source:** Explorer agent report

**Agent Report:**
```c
// From display.c (line ~115):
if (brightness == display->current_brightness) {
    return 0;  // Skip write - no SD card wear
}
```

**Problem:** Agent reported approximate line number ("~115")

**Fix Required:**
1. Verify actual line number in display.c
2. Update references to use function name instead: "See `display_set_brightness()` in display.c"

**Low priority** - doesn't affect functionality

---

## Recommendations

### Immediate Actions (High/Critical Severity)

1. **Fix Issue #4:** Remove non-existent `display_get_brightness()` API from ARCHITECTURE.md
2. **Fix Issue #3:** Add clear comment explaining CLOCK_REALTIME vs MONOTONIC design decision
3. **Fix Issue #8:** Document dual error handling strategies in ARCHITECTURE.md

### Short-Term (Medium Severity)

4. **Fix Issue #1:** Correct function name `input_has_events` → `input_has_touch_event`
5. **Fix Issue #2:** Update test count (21 → 31 state tests)
6. **Fix Issue #6:** Remove line number references or use relative references
7. **Fix Issue #7:** Mention systemd stub implementations

### Long-Term (Low Severity)

8. **Fix Issue #5:** Add flicker explanation to README quick reference
9. **Fix Issue #9:** Verify and update brightness caching line references

---

## Validation Commands

Run these commands to verify fixes:

```bash
# Issue #1: Verify input API name
grep -rn "input_has_" src/ tests/

# Issue #2: Count state tests
./tests/test_state 2>&1 | tail -5

# Issue #4: Verify display API
grep -rn "display_get_brightness" src/

# Issue #6: Check main.c line numbers
grep -n "poll(fds" src/main.c
grep -n "while (g_running)" src/main.c

# Issue #7: Verify systemd stubs
grep -A10 "ifndef HAVE_SYSTEMD" src/main.c
```

---

## Contributing

When fixing these issues:

1. **Update all references:** If changing API names, update ARCHITECTURE.md, comments, and tests
2. **Verify with grep:** Use grep to find all occurrences before claiming fix is complete
3. **Run tests:** Ensure `make test` passes after documentation changes
4. **Check SSoT:** Ensure Single Source of Truth is maintained (don't duplicate information)

---

## Status Tracking

| Issue # | Status | Assignee | Target Date | Notes |
|---------|--------|----------|-------------|-------|
| 1 | Open | - | - | |
| 2 | Open | - | - | |
| 3 | Open | - | - | |
| 4 | Open | - | - | |
| 5 | Open | - | - | |
| 6 | Open | - | - | |
| 7 | Open | - | - | |
| 8 | Open | - | - | |
| 9 | Open | - | - | |

---

## Document History

- **2025-12-15:** Initial creation based on Phase 2 codebase exploration
