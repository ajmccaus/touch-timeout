/*
 * state.h - Pure state machine for display power management
 *
 * Three-state Moore machine: FULL -> DIMMED -> OFF
 * Pure logic only - no I/O, no time calls
 * Caller provides timestamps in seconds from CLOCK_MONOTONIC
 */

#ifndef TOUCH_TIMEOUT_STATE_H
#define TOUCH_TIMEOUT_STATE_H

#include <stdint.h>

/* Display power states */
typedef enum {
    STATE_FULL = 0,    /* Full brightness - active use */
    STATE_DIMMED = 1,  /* Dimmed - user inactive */
    STATE_OFF = 2      /* Screen off - power saving */
} state_e;

/* State machine context */
typedef struct {
    state_e state;              /* Current state */
    uint32_t last_touch_sec;    /* Timestamp of last touch (monotonic sec) */
    int brightness_full;        /* Brightness for FULL state */
    int brightness_dim;         /* Brightness for DIMMED state */
    uint32_t dim_timeout_sec;   /* Seconds before FULL -> DIMMED */
    uint32_t off_timeout_sec;   /* Seconds before DIMMED -> OFF */
} state_s;

/*
 * Initialize state machine
 *
 * Sets state to STATE_FULL with last_touch_sec = 0
 * Caller should call state_touch() immediately with current time
 *
 * Preconditions (caller must ensure):
 *   - brightness_full >= 0, brightness_dim >= 0
 *   - dim_timeout_sec < off_timeout_sec
 *   - off_timeout_sec <= INT_MAX / 1000 (for poll() compatibility)
 */
void state_init(state_s *st, int brightness_full, int brightness_dim,
                uint32_t dim_timeout_sec, uint32_t off_timeout_sec);

/*
 * Handle touch event
 *
 * Updates last_touch_sec to now_sec
 * Transitions to STATE_FULL if not already there
 *
 * Returns: new brightness value, or -1 if no change
 */
int state_touch(state_s *st, uint32_t now_sec);

/*
 * Check for timeout transition
 *
 * Checks if idle time has exceeded threshold for current state
 * Transitions: FULL -> DIMMED -> OFF
 *
 * Returns: new brightness value, or -1 if no change
 */
int state_timeout(state_s *st, uint32_t now_sec);

/*
 * Get seconds until next transition
 *
 * Returns: seconds until next state change, 0 if already due, -1 if none (OFF state)
 * Note: Caller multiplies by 1000 for poll() timeout
 */
int state_get_timeout_sec(const state_s *st, uint32_t now_sec);

/*
 * Get current brightness for state
 *
 * Returns: brightness value for current state
 */
int state_get_brightness(const state_s *st);

/*
 * Get current state
 *
 * Returns: current state enum value
 */
state_e state_get_current(const state_s *st);

#endif /* TOUCH_TIMEOUT_STATE_H */
