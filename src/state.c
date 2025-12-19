/*
 * state.c - Pure state machine implementation
 *
 * No I/O dependencies - fully testable
 * Caller provides timestamps, we do pure logic
 */

#include "state.h"

void state_init(state_s *st, int brightness_full, int brightness_dim,
                uint32_t dim_timeout_ms, uint32_t off_timeout_ms) {
    st->state = STATE_FULL;
    st->last_touch_ms = 0;
    st->brightness_full = brightness_full;
    st->brightness_dim = brightness_dim;
    st->dim_timeout_ms = dim_timeout_ms;
    st->off_timeout_ms = off_timeout_ms;
}

int state_touch(state_s *st, uint32_t now_ms) {
    st->last_touch_ms = now_ms;

    if (st->state != STATE_FULL) {
        st->state = STATE_FULL;
        return st->brightness_full;
    }
    return -1;  /* Already full, no change */
}

int state_timeout(state_s *st, uint32_t now_ms) {
    uint32_t idle = now_ms - st->last_touch_ms;

    if (st->state == STATE_FULL && idle >= st->dim_timeout_ms) {
        st->state = STATE_DIMMED;
        return st->brightness_dim;
    }

    if (st->state == STATE_DIMMED && idle >= st->off_timeout_ms) {
        st->state = STATE_OFF;
        return 0;
    }

    return -1;  /* No transition */
}

int state_get_timeout_ms(const state_s *st, uint32_t now_ms) {
    uint32_t idle = now_ms - st->last_touch_ms;

    switch (st->state) {
        case STATE_FULL:
            if (idle >= st->dim_timeout_ms)
                return 0;
            return (int)(st->dim_timeout_ms - idle);

        case STATE_DIMMED:
            if (idle >= st->off_timeout_ms)
                return 0;
            return (int)(st->off_timeout_ms - idle);

        case STATE_OFF:
            return -1;  /* No timeout, wait for touch */
    }
    return -1;
}

int state_get_brightness(const state_s *st) {
    switch (st->state) {
        case STATE_FULL:
            return st->brightness_full;
        case STATE_DIMMED:
            return st->brightness_dim;
        case STATE_OFF:
            return 0;
    }
    return -1;
}

state_e state_get_current(const state_s *st) {
    return st->state;
}
