/*
 * input.c
 * -------
 * Touch input device abstraction implementation
 *
 * Handles Linux input subsystem interaction
 */

#include "input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include <linux/input.h>

/* Input context structure */
struct input_ctx {
    int fd;                     /* File descriptor for input device */
    char device_name[64];       /* Device name for logging */
};

/*
 * Open input device
 */
input_t *input_open(const char *device_name) {
    if (device_name == NULL) {
        syslog(LOG_ERR, "input_open: NULL device_name");
        return NULL;
    }

    input_t *input = calloc(1, sizeof(input_t));
    if (input == NULL) {
        syslog(LOG_ERR, "input_open: malloc failed");
        return NULL;
    }

    /* Build device path */
    char dev_path[128];
    snprintf(dev_path, sizeof(dev_path), "/dev/input/%s", device_name);

    /* Open device with non-blocking mode */
    input->fd = open(dev_path, O_RDONLY | O_NONBLOCK);
    if (input->fd < 0) {
        syslog(LOG_ERR, "Cannot open %s: %s", dev_path, strerror(errno));
        free(input);
        return NULL;
    }

    snprintf(input->device_name, sizeof(input->device_name), "%s", device_name);

    syslog(LOG_INFO, "Input device opened: %s (fd=%d)", dev_path, input->fd);

    return input;
}

/*
 * Close input device
 */
void input_close(input_t *input) {
    if (input == NULL)
        return;

    if (input->fd > 0)
        close(input->fd);

    free(input);
}

/*
 * Get file descriptor for polling
 */
int input_get_fd(input_t *input) {
    if (input == NULL)
        return -1;

    return input->fd;
}

/*
 * Check if input device has touch event
 */
bool input_has_touch_event(input_t *input) {
    if (input == NULL || input->fd <= 0)
        return false;

    struct input_event event;
    bool touch_detected = false;

    /* Drain all pending events */
    while (read(input->fd, &event, sizeof(event)) == sizeof(event)) {
        /* Check for touch-related events */
        if (event.type == EV_KEY || event.type == EV_ABS) {
            touch_detected = true;
            /* Continue reading to drain all events */
        }
    }

    return touch_detected;
}
