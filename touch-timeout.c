/*
 * touch-timeout.c
 * ----------------
 * A lightweight touchscreen activity monitor for Raspberry Pi 7" displays.
 *
 * Features:
 *  - Automatically dims and turns off the backlight after user-defined inactivity
 *  - Restores brightness instantly on any touchscreen input
 *  - Reads config from /etc/touch-timeout.conf (with command-line overrides)
 *  - Near-zero CPU usage (poll-based event loop)
 *  - Safe sysfs writes (lseek, fsync)
 *  - Graceful shutdown via SIGTERM/SIGINT (systemd compatible)
 *
 * Author: Andrew McCausland (optimized & hardened version)
 * License: GPL v3
 */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <poll.h>
#include <syslog.h>
#include <ctype.h>
#include <sys/types.h>

#define MIN_BRIGHTNESS       15     // Minimum allowed brightness (avoids flicker)
#define MIN_DIM_BRIGHTNESS   10     // Minimum allowed dim level
#define POLL_INTERVAL_MS     50     // Polling interval for input events (50ms)
#define CONFIG_PATH          "/etc/touch-timeout.conf"

static volatile int running = 1;    // Used for graceful shutdown on SIGTERM/SIGINT

// --------------------
// Struct for tracking display state
// --------------------
struct display_state {
    int bright_fd;           // File descriptor for /sys/class/backlight/.../brightness
    int user_brightness;     // User-configured brightness level (1–255)
    int dim_brightness;      // Calculated dim brightness (≥10)
    int dim_timeout;         // Seconds before dimming
    int off_timeout;         // Seconds before turning off
    time_t last_input;       // Timestamp of last touch event
    int state;               // 0 = full brightness, 1 = dimmed, 2 = off
};

// --------------------
// Signal handler for systemd stop/restart
// --------------------
static void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

// --------------------
// Trim whitespace from both ends of a string (for config parsing)
// --------------------
static void trim(char *s) {
    char *p = s;
    while (isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
}

// --------------------
// Parse /etc/touch-timeout.conf for key=value pairs
// Supports:
//   brightness=150
//   off_timeout=300
//   backlight=rpi_backlight
//   device=event0
// --------------------
static void load_config(const char *path, int *brightness, int *timeout,
                        char *backlight, size_t bl_sz,
                        char *device, size_t dev_sz) {
    FILE *f = fopen(path, "r");
    if (!f) return; // No config file found — just skip

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        trim(line);
        if (line[0] == '#' || line[0] == ';' || line[0] == '\0')
            continue;

        char key[64], value[64];
        if (sscanf(line, "%63[^=]=%63s", key, value) == 2) {
            trim(key); trim(value);
            if (strcmp(key, "brightness") == 0)
                *brightness = atoi(value);
            else if (strcmp(key, "off_timeout") == 0)
                *timeout = atoi(value);
            else if (strcmp(key, "backlight") == 0)
                strncpy(backlight, value, bl_sz - 1);
            else if (strcmp(key, "device") == 0)
                strncpy(device, value, dev_sz - 1);
        }
    }
    fclose(f);
}

// --------------------
// Write brightness safely to /sys/class/backlight
// Adds lseek() and fsync() for kernel write reliability
// --------------------
static int set_brightness(struct display_state *state, int brightness) {
    if (brightness < MIN_BRIGHTNESS && brightness != 0)
        brightness = MIN_BRIGHTNESS;

    char buf[8];
    int len = snprintf(buf, sizeof(buf), "%d", brightness);
    lseek(state->bright_fd, 0, SEEK_SET);
    int ret = write(state->bright_fd, buf, len);
    fsync(state->bright_fd);

    if (ret != len) {
        syslog(LOG_ERR, "Failed to set brightness: %s", strerror(errno));
        return -1;
    }
    return 0;
}

// --------------------
// Restore full brightness after a touch event
// --------------------
static int restore_brightness(struct display_state *state) {
    if (set_brightness(state, state->user_brightness) == 0) {
        state->state = 0;
        state->last_input = time(NULL);
        syslog(LOG_INFO, "Restored brightness to %d", state->user_brightness);
        return 0;
    }
    return -1;
}

// --------------------
// Check dim/off timeouts relative to last touch event
// --------------------
static void check_timeouts(struct display_state *state) {
    time_t now = time(NULL);
    double idle = difftime(now, state->last_input);

    if (state->state == 0 && idle >= state->dim_timeout) {
        // Transition to DIM state
        if (set_brightness(state, state->dim_brightness) == 0)
            state->state = 1;
    } else if ((state->state == 0 || state->state == 1) && idle >= state->off_timeout) {
        // Transition to OFF state (brightness=0)
        if (set_brightness(state, 0) == 0)
            state->state = 2;
    }
}

// --------------------
// Main entry point
// --------------------
int main(int argc, char *argv[]) {
    openlog("touch-timeout", LOG_PID | LOG_CONS, LOG_DAEMON);

    // Default values (will be overridden by config or CLI)
    int user_brightness = 100;
    int off_timeout = 300;
    char backlight[64] = "rpi_backlight";
    char input_dev[32] = "event0";

    // Load config from /etc/touch-timeout.conf (if present)
    load_config(CONFIG_PATH, &user_brightness, &off_timeout,
                backlight, sizeof(backlight),
                input_dev, sizeof(input_dev));

    // Command-line args override config (if provided)
    if (argc > 1) user_brightness = atoi(argv[1]);
    if (argc > 2) off_timeout = atoi(argv[2]);
    if (argc > 3) strncpy(backlight, argv[3], sizeof(backlight) - 1);
    if (argc > 4) strncpy(input_dev, argv[4], sizeof(input_dev) - 1);

    // Input validation
    if (user_brightness < MIN_BRIGHTNESS || user_brightness > 255) {
        syslog(LOG_ERR, "Brightness must be between %d and 255", MIN_BRIGHTNESS);
        exit(EXIT_FAILURE);
    }
    if (off_timeout < 10) {
        syslog(LOG_ERR, "Timeout must be >= 10s");
        exit(EXIT_FAILURE);
    }

    // Initialize display state structure
    struct display_state state = {
        .user_brightness = user_brightness,
        .dim_brightness = (user_brightness / 10 < MIN_DIM_BRIGHTNESS)
                            ? MIN_DIM_BRIGHTNESS : user_brightness / 10,
        .dim_timeout = off_timeout / 2,
        .off_timeout = off_timeout,
        .last_input = time(NULL),
        .state = 0
    };

    // Open brightness control file
    char bright_path[128];
    snprintf(bright_path, sizeof(bright_path),
             "/sys/class/backlight/%s/brightness", backlight);
    state.bright_fd = open(bright_path, O_WRONLY);
    if (state.bright_fd == -1) {
        syslog(LOG_ERR, "Error opening %s: %s", bright_path, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Open touchscreen input device
    char dev_path[64];
    snprintf(dev_path, sizeof(dev_path), "/dev/input/%s", input_dev);
    int event_fd = open(dev_path, O_RDONLY | O_NONBLOCK);
    if (event_fd == -1) {
        syslog(LOG_ERR, "Error opening %s: %s", dev_path, strerror(errno));
        close(state.bright_fd);
        exit(EXIT_FAILURE);
    }

    // Set initial brightness
    if (set_brightness(&state, user_brightness) != 0) {
        syslog(LOG_ERR, "Failed to set initial brightness");
        close(state.bright_fd);
        close(event_fd);
        exit(EXIT_FAILURE);
    }

    syslog(LOG_INFO, "Started: brightness=%d, dim=%d, dim_timeout=%ds, off_timeout=%ds",
           state.user_brightness, state.dim_brightness, state.dim_timeout, state.off_timeout);

    // Graceful shutdown handling
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    // Poll loop for touchscreen input events
    struct pollfd pfd = {.fd = event_fd, .events = POLLIN};
    struct input_event event;

    while (running) {
        int ret = poll(&pfd, 1, POLL_INTERVAL_MS);

        if (ret > 0 && (pfd.revents & POLLIN)) {
            // Read all queued events
            while (read(event_fd, &event, sizeof(event)) > 0) {
                if (event.type == EV_KEY || event.type == EV_ABS) {
                    // Any touch resets timeout or restores screen
                    if (state.state != 0)
                        restore_brightness(&state);
                    else
                        state.last_input = time(NULL);
                }
            }
        } else if (ret < 0 && errno != EINTR) {
            syslog(LOG_ERR, "Poll error: %s", strerror(errno));
        }

        check_timeouts(&state);
    }

    // Cleanup on shutdown
    syslog(LOG_INFO, "Stopping touch-timeout service...");
    close(state.bright_fd);
    close(event_fd);
    closelog();
    return 0;
}