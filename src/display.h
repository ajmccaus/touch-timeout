/*
 * display.h
 * ---------
 * Display/backlight hardware abstraction layer
 *
 * Provides clean interface for brightness control
 * Implements caching to minimize SD card writes
 * All sysfs operations isolated for testing
 */

#ifndef TOUCH_TIMEOUT_DISPLAY_H
#define TOUCH_TIMEOUT_DISPLAY_H

#include <stdbool.h>

/* Display handle - opaque structure */
typedef struct display_ctx display_t;

/*
 * Open display/backlight device
 *
 * Opens /sys/class/backlight/{name}/brightness for control
 * Reads max_brightness and current brightness
 * Validates hardware state
 *
 * Parameters:
 *   backlight_name: Name of backlight device (e.g., "rpi_backlight")
 *
 * Returns: Display handle on success, NULL on error
 */
display_t *display_open(const char *backlight_name);

/*
 * Close display device
 *
 * Closes file descriptors and frees resources
 * Safe to call with NULL handle
 *
 * Parameters:
 *   display: Display handle
 */
void display_close(display_t *display);

/*
 * Set display brightness
 *
 * Writes brightness to hardware via sysfs
 * Uses caching - skips write if brightness unchanged
 * Enforces minimum brightness (except for 0 = screen off)
 *
 * CERT compliant: validates parameters, checks write success
 *
 * Parameters:
 *   display:    Display handle
 *   brightness: Brightness value (0 or min_brightness - max_brightness)
 *
 * Returns: 0 on success, -1 on error
 */
int display_set_brightness(display_t *display, int brightness);

/*
 * Get current cached brightness
 *
 * Returns last successfully set brightness value
 * Does NOT read from hardware (uses cache)
 *
 * Parameters:
 *   display: Display handle
 *
 * Returns: Current brightness or -1 on error
 */
int display_get_brightness(display_t *display);

/*
 * Get maximum brightness supported by hardware
 *
 * Reads max_brightness from sysfs at initialization
 *
 * Parameters:
 *   display: Display handle
 *
 * Returns: Maximum brightness or -1 on error
 */
int display_get_max_brightness(display_t *display);

/*
 * Get minimum allowed brightness (compile-time constant)
 *
 * Returns: Minimum brightness value (typically 15 to avoid flicker)
 */
int display_get_min_brightness(void);

#endif /* TOUCH_TIMEOUT_DISPLAY_H */
