/*
 * timer.h
 * -------
 * POSIX timer abstraction using timerfd
 *
 * Provides monotonic clock timer for timeout handling
 * Integrates with poll() event loop
 * Robust against system suspend/resume
 */

#ifndef TOUCH_TIMEOUT_TIMER_H
#define TOUCH_TIMEOUT_TIMER_H

#include <stdbool.h>

/* Timer handle - opaque structure */
typedef struct timer_ctx timer_ctx_s;

/*
 * Create timer
 *
 * Creates timerfd with CLOCK_MONOTONIC
 * Timer starts disarmed
 *
 * Returns: Timer handle on success, NULL on error
 */
timer_ctx_s *timer_create_ctx(void);

/*
 * Destroy timer
 *
 * Closes timerfd and frees resources
 * Safe to call with NULL handle
 *
 * Parameters:
 *   timer: Timer handle
 */
void timer_destroy(timer_ctx_s *timer);

/*
 * Get file descriptor for polling
 *
 * Returns timerfd file descriptor for use with poll()
 *
 * Parameters:
 *   timer: Timer handle
 *
 * Returns: File descriptor or -1 on error
 */
int timer_get_fd(timer_ctx_s *timer);

/*
 * Arm timer (one-shot)
 *
 * Sets timer to expire after specified seconds
 * Uses CLOCK_MONOTONIC for robustness
 *
 * Parameters:
 *   timer:   Timer handle
 *   seconds: Timeout in seconds (0 to disarm)
 *
 * Returns: 0 on success, -1 on error
 */
int timer_arm(timer_ctx_s *timer, int seconds);

/*
 * Check if timer expired
 *
 * Reads timerfd to check expiration
 * Resets expiration flag
 *
 * Parameters:
 *   timer: Timer handle
 *
 * Returns: true if timer expired, false otherwise
 */
bool timer_check_expiration(timer_ctx_s *timer);

#endif /* TOUCH_TIMEOUT_TIMER_H */
