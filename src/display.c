/*
 * display.c
 * ---------
 * Display/backlight hardware abstraction implementation
 *
 * Handles all sysfs interaction for backlight control
 * Implements write caching to minimize SD card wear
 */

#include "display.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>

/* Display context structure */
struct display_ctx {
    int brightness_fd;          /* File descriptor for brightness control */
    int max_brightness;         /* Maximum brightness from hardware */
    int current_brightness;     /* Cached brightness value */
    int min_brightness;         /* Minimum allowed brightness */
};

/*
 * Read integer value from sysfs file
 */
static int read_sysfs_int(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;

    char buf[16];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (n <= 0)
        return -1;

    buf[n] = '\0';
    return atoi(buf);
}

/*
 * Open display/backlight device
 */
display_t *display_open(const char *backlight_name) {
    if (backlight_name == NULL) {
        syslog(LOG_ERR, "display_open: NULL backlight_name");
        return NULL;
    }

    display_t *display = calloc(1, sizeof(display_t));
    if (display == NULL) {
        syslog(LOG_ERR, "display_open: malloc failed");
        return NULL;
    }

    /* Build paths */
    char brightness_path[256];
    char max_brightness_path[256];

    snprintf(brightness_path, sizeof(brightness_path),
             "/sys/class/backlight/%s/brightness", backlight_name);
    snprintf(max_brightness_path, sizeof(max_brightness_path),
             "/sys/class/backlight/%s/max_brightness", backlight_name);

    /* Read max_brightness */
    display->max_brightness = read_sysfs_int(max_brightness_path);
    if (display->max_brightness <= 0) {
        syslog(LOG_WARNING, "Cannot read max_brightness from %s, assuming %d",
               max_brightness_path, CONFIG_MAX_BRIGHTNESS);
        display->max_brightness = CONFIG_MAX_BRIGHTNESS;
    }

    /* Clamp to valid range */
    if (display->max_brightness > CONFIG_MAX_BRIGHTNESS)
        display->max_brightness = CONFIG_MAX_BRIGHTNESS;

    /* Open brightness control file */
    display->brightness_fd = open(brightness_path, O_RDWR);
    if (display->brightness_fd < 0) {
        syslog(LOG_ERR, "Cannot open %s: %s", brightness_path, strerror(errno));
        free(display);
        return NULL;
    }

    /* Read current brightness */
    char buf[16];
    ssize_t n = read(display->brightness_fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        display->current_brightness = atoi(buf);
    } else {
        display->current_brightness = -1;
    }

    display->min_brightness = CONFIG_MIN_BRIGHTNESS;

    syslog(LOG_INFO, "Display opened: %s (max=%d, current=%d)",
           backlight_name, display->max_brightness, display->current_brightness);

    return display;
}

/*
 * Close display device
 */
void display_close(display_t *display) {
    if (display == NULL)
        return;

    if (display->brightness_fd > 0)
        close(display->brightness_fd);

    free(display);
}

/*
 * Set display brightness
 */
int display_set_brightness(display_t *display, int brightness) {
    /* Validate parameters (CERT ERR06-C) */
    if (display == NULL) {
        syslog(LOG_ERR, "display_set_brightness: NULL display");
        return -1;
    }

    if (brightness < 0 || brightness > CONFIG_MAX_BRIGHTNESS) {
        syslog(LOG_ERR, "display_set_brightness: brightness %d out of range", brightness);
        return -1;
    }

    if (display->brightness_fd <= 0) {
        syslog(LOG_ERR, "display_set_brightness: invalid fd");
        return -1;
    }

    /* Use cache - skip write if unchanged */
    if (brightness == display->current_brightness)
        return 0;

    /* Enforce minimum brightness (except for screen off) */
    if (brightness > 0 && brightness < display->min_brightness)
        brightness = display->min_brightness;

    /* Write to sysfs */
    char buf[16];
    int len = snprintf(buf, sizeof(buf), "%d", brightness);

    /* Reset file position for repeated writes */
    if (lseek(display->brightness_fd, 0, SEEK_SET) == -1) {
        syslog(LOG_ERR, "display_set_brightness: lseek failed: %s", strerror(errno));
        return -1;
    }

    ssize_t ret = write(display->brightness_fd, buf, len);
    if (ret != len) {
        syslog(LOG_ERR, "display_set_brightness: write failed: %s", strerror(errno));
        return -1;
    }

    /* Update cache */
    display->current_brightness = brightness;
    return 0;
}

/*
 * Get current cached brightness
 */
int display_get_brightness(display_t *display) {
    if (display == NULL)
        return -1;

    return display->current_brightness;
}

/*
 * Get maximum brightness
 */
int display_get_max_brightness(display_t *display) {
    if (display == NULL)
        return -1;

    return display->max_brightness;
}

/*
 * Get minimum brightness
 */
int display_get_min_brightness(void) {
    return CONFIG_MIN_BRIGHTNESS;
}
