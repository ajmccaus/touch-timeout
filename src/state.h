/*
 * state.h - Pure state machine for display power management
 *
 * Three-state Moore machine: FULL -> DIMMED -> OFF
 * Pure logic only - no I/O, no time calls
 * Caller provides timestamps from CLOCK_MONOTONIC
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
    uint32_t last_touch_ms;     /* Timestamp of last touch (monotonic ms) */
    int brightness_full;        /* Brightness for FULL state */
    int brightness_dim;         /* Brightness for DIMMED state */
    uint32_t dim_timeout_ms;    /* Ms before FULL -> DIMMED */
    uint32_t off_timeout_ms;    /* Ms before DIMMED -> OFF */
} state_t;

/*
 * Initialize state machine
 *
 * Sets state to STATE_FULL with last_touch_ms = 0
 * Caller should call state_touch() immediately with current time
 */
void state_init(state_t *s, int brightness_full, int brightness_dim,
                uint32_t dim_timeout_ms, uint32_t off_timeout_ms);

/*
 * Handle touch event
 *
 * Updates last_touch_ms to now_ms
 * Transitions to STATE_FULL if not already there
 *
 * Returns: new brightness value, or -1 if no change
 */
int state_touch(state_t *s, uint32_t now_ms);

/*
 * Check for timeout transition
 *
 * Checks if idle time has exceeded threshold for current state
 * Transitions: FULL -> DIMMED -> OFF
 *
 * Returns: new brightness value, or -1 if no change
 */
int state_timeout(state_t *s, uint32_t now_ms);

/*
 * Get ms until next transition
 *
 * Returns: ms until next state change, 0 if already due, -1 if none (OFF state)
 */
int state_get_timeout_ms(const state_t *s, uint32_t now_ms);

/*
 * Get current brightness for state
 *
 * Returns: brightness value for current state
 */
int state_get_brightness(const state_t *s);

/*
 * Get current state
 *
 * Returns: current state enum value
 */
state_e state_get_current(const state_t *s);

#endif /* TOUCH_TIMEOUT_STATE_H */
