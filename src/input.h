/*
 * input.h
 * -------
 * Touch input device abstraction layer
 *
 * Provides clean interface for touchscreen event monitoring
 * Isolates Linux input subsystem for portability and testing
 */

#ifndef TOUCH_TIMEOUT_INPUT_H
#define TOUCH_TIMEOUT_INPUT_H

#include <stdbool.h>

/* Input handle - opaque structure */
typedef struct input_ctx input_t;

/*
 * Open input device
 *
 * Opens /dev/input/{device_name} for reading
 * Sets O_NONBLOCK for poll-based event loop
 * Validates device is readable
 *
 * Parameters:
 *   device_name: Name of input device (e.g., "event0")
 *
 * Returns: Input handle on success, NULL on error
 */
input_t *input_open(const char *device_name);

/*
 * Close input device
 *
 * Closes file descriptor and frees resources
 * Safe to call with NULL handle
 *
 * Parameters:
 *   input: Input handle
 */
void input_close(input_t *input);

/*
 * Get file descriptor for polling
 *
 * Returns file descriptor for use with poll() or select()
 *
 * Parameters:
 *   input: Input handle
 *
 * Returns: File descriptor or -1 on error
 */
int input_get_fd(input_t *input);

/*
 * Check if input device has touch event
 *
 * Reads available input events from kernel buffer
 * Returns true if any EV_KEY or EV_ABS events detected
 * Drains all pending events to prevent backlog
 *
 * Parameters:
 *   input: Input handle
 *
 * Returns: true if touch event detected, false otherwise
 */
bool input_has_touch_event(input_t *input);

#endif /* TOUCH_TIMEOUT_INPUT_H */
