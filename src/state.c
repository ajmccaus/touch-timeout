/*
 * state.c - Pure state machine implementation
 *
 * No I/O dependencies - fully testable
 * Caller provides timestamps in seconds, we do pure logic
 */

#include "state.h"

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
    return -1;  /* Already full, no change */
}

int state_timeout(state_s *st, uint32_t now_sec) {
    uint32_t idle = now_sec - st->last_touch_sec;

    if (st->state == STATE_FULL && idle >= st->dim_timeout_sec) {
        st->state = STATE_DIMMED;
        return st->brightness_dim;
    }

    if (st->state == STATE_DIMMED && idle >= st->off_timeout_sec) {
        st->state = STATE_OFF;
        return 0;
    }

    return -1;  /* No transition */
}

int state_get_timeout_sec(const state_s *st, uint32_t now_sec) {
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
            return -1;  /* No timeout, wait for touch */

        default:
            return -1;  /* Invalid state */
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
            return -1;  /* Invalid state */
    }
}

state_e state_get_current(const state_s *st) {
    return st->state;
}
