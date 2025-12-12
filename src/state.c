/*
 * state.c
 * -------
 * Display power state machine implementation - pure logic
 *
 * Three-state machine: FULL -> DIMMED -> OFF
 * No I/O dependencies - fully testable
 */

#include "state.h"
#include <stdlib.h>
#include <syslog.h>

/*
 * Initialize state machine
 */
int state_init(state_t *state, int user_brightness, int dim_brightness,
               int dim_timeout_sec, int off_timeout_sec) {
    /* Validate parameters */
    if (state == NULL) {
        syslog(LOG_ERR, "state_init: NULL state");
        return -1;
    }

    if (user_brightness <= 0 || dim_brightness < 0) {
        syslog(LOG_ERR, "state_init: invalid brightness values");
        return -1;
    }

    if (dim_timeout_sec <= 0 || off_timeout_sec <= 0) {
        syslog(LOG_ERR, "state_init: invalid timeout values");
        return -1;
    }

    if (dim_timeout_sec >= off_timeout_sec) {
        syslog(LOG_ERR, "state_init: dim_timeout >= off_timeout");
        return -1;
    }

    /* Initialize state */
    state->current_state = STATE_FULL;
    state->user_brightness = user_brightness;
    state->dim_brightness = dim_brightness;
    state->dim_timeout_sec = dim_timeout_sec;
    state->off_timeout_sec = off_timeout_sec;
    state->last_input_time = time(NULL);

    return 0;
}

/*
 * Handle state machine event
 */
bool state_handle_event(state_t *state, state_event_t event, int *new_brightness) {
    if (state == NULL || new_brightness == NULL) {
        syslog(LOG_ERR, "state_handle_event: NULL parameter");
        return false;
    }

    bool brightness_changed = false;
    time_t now = time(NULL);
    double idle;  /* Declare here for C11 compatibility */

    switch (event) {
        case STATE_EVENT_TOUCH:
            /* Touch always restores to FULL state */
            if (state->current_state != STATE_FULL) {
                state->current_state = STATE_FULL;
                *new_brightness = state->user_brightness;
                brightness_changed = true;
                syslog(LOG_DEBUG, "Touch detected - restored to FULL brightness %d",
                       *new_brightness);
            }
            /* Update last input time */
            state->last_input_time = now;
            break;

        case STATE_EVENT_TIMEOUT:
            /* Calculate idle time */
            idle = difftime(now, state->last_input_time);

            /* Handle clock adjustment (NTP, manual time change) */
            if (idle < -5.0) {
                syslog(LOG_WARNING, "Clock adjusted backwards by %.1fs - resetting timer", -idle);
                state->last_input_time = now;
                break;
            }

            /* Check for state transitions based on idle time */
            if (idle >= state->off_timeout_sec && state->current_state != STATE_OFF) {
                /* Transition to OFF */
                state->current_state = STATE_OFF;
                *new_brightness = 0;
                brightness_changed = true;
                syslog(LOG_DEBUG, "Display OFF (idle=%.0fs)", idle);

            } else if (idle >= state->dim_timeout_sec && state->current_state == STATE_FULL) {
                /* Transition to DIMMED (only from FULL) */
                state->current_state = STATE_DIMMED;
                *new_brightness = state->dim_brightness;
                brightness_changed = true;
                syslog(LOG_DEBUG, "Display DIMMED to %d (idle=%.0fs)",
                       *new_brightness, idle);
            }
            break;

        default:
            syslog(LOG_WARNING, "state_handle_event: unknown event %d", event);
            return false;
    }

    return brightness_changed;
}

/*
 * Get current state
 */
state_type_t state_get_current(const state_t *state) {
    if (state == NULL)
        return STATE_OFF;  /* Safe default */

    return state->current_state;
}

/*
 * Get current target brightness
 */
int state_get_brightness(const state_t *state) {
    if (state == NULL)
        return -1;

    switch (state->current_state) {
        case STATE_FULL:
            return state->user_brightness;
        case STATE_DIMMED:
            return state->dim_brightness;
        case STATE_OFF:
            return 0;
        default:
            return -1;
    }
}

/*
 * Get seconds until next state transition
 */
int state_get_next_timeout(const state_t *state) {
    if (state == NULL)
        return -1;

    time_t now = time(NULL);
    double idle = difftime(now, state->last_input_time);

    /* Handle clock going backwards */
    if (idle < -5.0)
        return 1;  /* Check again soon */

    switch (state->current_state) {
        case STATE_FULL:
            /* Next event is DIM */
            {
                int remaining = state->dim_timeout_sec - (int)idle;
                return (remaining > 0) ? remaining : 0;
            }

        case STATE_DIMMED:
            /* Next event is OFF */
            {
                int remaining = state->off_timeout_sec - (int)idle;
                return (remaining > 0) ? remaining : 0;
            }

        case STATE_OFF:
            /* No timeout - waiting for touch */
            return -1;

        default:
            return -1;
    }
}
