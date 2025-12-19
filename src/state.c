/*
 * state.c - Pure state machine implementation
 *
 * No I/O dependencies - fully testable
 * Caller provides timestamps, we do pure logic
 */

#include "state.h"

void state_init(state_t *s, int brightness_full, int brightness_dim,
                uint32_t dim_timeout_ms, uint32_t off_timeout_ms) {
    s->state = STATE_FULL;
    s->last_touch_ms = 0;
    s->brightness_full = brightness_full;
    s->brightness_dim = brightness_dim;
    s->dim_timeout_ms = dim_timeout_ms;
    s->off_timeout_ms = off_timeout_ms;
}

int state_touch(state_t *s, uint32_t now_ms) {
    s->last_touch_ms = now_ms;

    if (s->state != STATE_FULL) {
        s->state = STATE_FULL;
        return s->brightness_full;
    }
    return -1;  /* Already full, no change */
}

int state_timeout(state_t *s, uint32_t now_ms) {
    uint32_t idle = now_ms - s->last_touch_ms;

    if (s->state == STATE_FULL && idle >= s->dim_timeout_ms) {
        s->state = STATE_DIMMED;
        return s->brightness_dim;
    }

    if (s->state == STATE_DIMMED && idle >= s->off_timeout_ms) {
        s->state = STATE_OFF;
        return 0;
    }

    return -1;  /* No transition */
}

int state_get_timeout_ms(const state_t *s, uint32_t now_ms) {
    uint32_t idle = now_ms - s->last_touch_ms;

    switch (s->state) {
        case STATE_FULL:
            if (idle >= s->dim_timeout_ms)
                return 0;
            return (int)(s->dim_timeout_ms - idle);

        case STATE_DIMMED:
            if (idle >= s->off_timeout_ms)
                return 0;
            return (int)(s->off_timeout_ms - idle);

        case STATE_OFF:
            return -1;  /* No timeout, wait for touch */
    }
    return -1;
}

int state_get_brightness(const state_t *s) {
    switch (s->state) {
        case STATE_FULL:
            return s->brightness_full;
        case STATE_DIMMED:
            return s->brightness_dim;
        case STATE_OFF:
            return 0;
    }
    return -1;
}

state_e state_get_current(const state_t *s) {
    return s->state;
}
