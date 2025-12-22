/*
 * state.c - Pure state machine implementation
 *
 * ARCHITECTURE ROLE:
 *   Pure business logic for 3-state Moore machine (FULL → DIMMED → OFF).
 *   Deliberately isolated from all I/O to enable unit testing without mocks.
 *
 * DESIGN CONSTRAINTS:
 *   - No I/O: No file access, no time calls, no system calls
 *   - Pure functions: All state passed as parameters
 *   - Wraparound-safe: Arithmetic handles CLOCK_MONOTONIC wraparound correctly
 *   - Caller owns time: All timestamps provided by caller in seconds
 *
 * STATE TRANSITIONS:
 *   FULL → DIMMED → OFF (on idle timeout)
 *   Any state → FULL (on touch event)
 *   See doc/ARCHITECTURE.md for state diagram.
 *
 * TESTING STRATEGY:
 *   No mocking needed - pass mock timestamps directly to functions.
 *   See tests/test_state.c for comprehensive test suite.
 *
 * USED BY:
 *   - main.c event loop (production)
 *   - tests/test_state.c (unit tests)
 *
 * SEE ALSO:
 *   - state.h - Public API and preconditions
 *   - doc/ARCHITECTURE.md - State machine diagram
 */

#include "state.h"

#include <limits.h>

/* Verify timeout values can be safely converted to int for poll() */
_Static_assert(UINT32_MAX <= (unsigned long)INT_MAX * 2 + 1,
               "uint32_t range exceeds expected bounds");

void state_init(state_s *st, int brightness_full, int brightness_dim,
                uint32_t dim_timeout_sec, uint32_t off_timeout_sec) {
    st->state = STATE_FULL;
    st->last_touch_sec = 0;
    st->brightness_full = brightness_full;
    st->brightness_dim = brightness_dim;
    st->dim_timeout_sec = dim_timeout_sec;
    st->off_timeout_sec = off_timeout_sec;
}

int state_touch(state_s *st, uint32_t now_sec) {
    st->last_touch_sec = now_sec;

    if (st->state != STATE_FULL) {
        st->state = STATE_FULL;
        return st->brightness_full;
    }
    return STATE_NO_CHANGE;  /* Already full */
}

int state_timeout(state_s *st, uint32_t now_sec) {
    /* Unsigned subtraction handles wraparound correctly */
    uint32_t idle = now_sec - st->last_touch_sec;

    if (st->state == STATE_FULL && idle >= st->dim_timeout_sec) {
        st->state = STATE_DIMMED;
        return st->brightness_dim;
    }

    if (st->state == STATE_DIMMED && idle >= st->off_timeout_sec) {
        st->state = STATE_OFF;
        return 0;
    }

    return STATE_NO_CHANGE;
}

int state_get_timeout_sec(const state_s *st, uint32_t now_sec) {
    /* Unsigned subtraction handles wraparound correctly */
    uint32_t idle = now_sec - st->last_touch_sec;

    switch (st->state) {
        case STATE_FULL:
            if (idle >= st->dim_timeout_sec)
                return 0;
            return (int)(st->dim_timeout_sec - idle);

        case STATE_DIMMED:
            if (idle >= st->off_timeout_sec)
                return 0;
            return (int)(st->off_timeout_sec - idle);

        case STATE_OFF:
            return STATE_NO_CHANGE;  /* No timeout, wait for touch */

        default:
            return STATE_NO_CHANGE;  /* Invalid state */
    }
}

int state_get_brightness(const state_s *st) {
    switch (st->state) {
        case STATE_FULL:
            return st->brightness_full;
        case STATE_DIMMED:
            return st->brightness_dim;
        case STATE_OFF:
            return 0;
        default:
            return STATE_NO_CHANGE;  /* Invalid state */
    }
}

state_e state_get_current(const state_s *st) {
    return st->state;
}
