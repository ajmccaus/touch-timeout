/*
 * state.h
 * -------
 * Display power state machine - pure logic, no I/O
 *
 * Three-state machine: FULL -> DIMMED -> OFF
 * Handles timeout calculations and state transitions
 * Completely testable without hardware
 */

#ifndef TOUCH_TIMEOUT_STATE_H
#define TOUCH_TIMEOUT_STATE_H

#include <time.h>
#include <stdbool.h>

/* Display power states */
typedef enum {
    STATE_FULL = 0,      /* Full brightness - active use */
    STATE_DIMMED = 1,    /* Dimmed - user inactive */
    STATE_OFF = 2        /* Screen off - power saving */
} state_type_t;

/* State machine events */
typedef enum {
    STATE_EVENT_TOUCH,   /* Touch input detected */
    STATE_EVENT_TIMEOUT  /* Timer expired - check for state change */
} state_event_t;

/* State machine context */
typedef struct {
    state_type_t current_state;     /* Current state */
    int user_brightness;            /* Full brightness value */
    int dim_brightness;             /* Dimmed brightness value */
    int dim_timeout_sec;            /* Seconds before dimming */
    int off_timeout_sec;            /* Seconds before turning off */
    time_t last_input_time;         /* Timestamp of last touch (CLOCK_REALTIME for compatibility) */
} state_t;

/*
 * Initialize state machine
 *
 * Sets initial state to STATE_FULL
 * Records current time as last input
 *
 * Parameters:
 *   state:            State structure to initialize
 *   user_brightness:  Brightness for FULL state
 *   dim_brightness:   Brightness for DIMMED state
 *   dim_timeout_sec:  Seconds before dimming
 *   off_timeout_sec:  Seconds before turning off
 *
 * Returns: 0 on success, -1 on error (invalid parameters)
 */
int state_init(state_t *state, int user_brightness, int dim_brightness,
               int dim_timeout_sec, int off_timeout_sec);

/*
 * Handle state machine event
 *
 * Processes touch events and timeout checks
 * Returns new brightness if state changed
 *
 * Parameters:
 *   state:            State machine context
 *   event:            Event type (TOUCH or TIMEOUT)
 *   new_brightness:   Output - new brightness value if state changed
 *
 * Returns: true if brightness should change, false if no change
 */
bool state_handle_event(state_t *state, state_event_t event, int *new_brightness);

/*
 * Get current state
 *
 * Parameters:
 *   state: State machine context
 *
 * Returns: Current state
 */
state_type_t state_get_current(const state_t *state);

/*
 * Get current target brightness
 *
 * Returns brightness value for current state
 *
 * Parameters:
 *   state: State machine context
 *
 * Returns: Brightness value
 */
int state_get_brightness(const state_t *state);

/*
 * Get seconds until next state transition
 *
 * Calculates time remaining until next dim or off event
 * Returns 0 if already past timeout (state change needed)
 *
 * Parameters:
 *   state: State machine context
 *
 * Returns: Seconds until next state change, or -1 if no timeout
 */
int state_get_next_timeout(const state_t *state);

#endif /* TOUCH_TIMEOUT_STATE_H */
