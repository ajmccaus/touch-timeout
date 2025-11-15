/*
 * touch-timeout.c
 * ----------------
 * A lightweight touchscreen activity monitor optimized for Raspberry Pi 7" touchscreens.
 *
 * VERSION: 1.0.0
 * 
 * Changes from 0.2.0:
 *  - Added assert() validation
 *  - Added auto versioning with #define VERSION
 *  - Added safe_atoi() helper function
 *  - other fixes to ensure security and config file error handling
 *  - tested on RPi 4 with 7" touchscreen
 *
 * Changes from 0.1.0:
 *  - Added brightness caching to prevent redundant writes
 *  - Reads and validates max_brightness from sysfs
 *  - Configurable poll_interval and dim_percent in config file
 *  - Time-based timeout logic (handles missed poll cycles)
 *  - Enum-based state machine (replaces magic numbers)
 *  - Removed fsync() (unnecessary for sysfs)
 *  - Enhanced input validation with logging
 *
 * Features:
 *  - Automatically dims and turns off the backlight after user-defined inactivity
 *  - Restores brightness instantly on any touchscreen input
 *  - Reads config from /etc/touch-timeout.conf (with command-line overrides)
 *  - Near-zero CPU usage (poll-based event loop)
 *  - Safe sysfs writes (lseek, cached)
 *  - Graceful shutdown via SIGTERM/SIGINT (systemd compatible)
 *
  * TODO for v1.1.0:
 *  - [ ] Make dim_brightness separately configurable (config key: dim_brightness)
 *        Current: auto-calculated as user_brightness/10
 *        Proposed: dim_brightness=20 in config file
 *  - [ ] Reduce restore_brightness() log level to LOG_DEBUG
 *
 * Author: Andrew McCausland
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
#include <assert.h>
#include <limits.h>

#define MIN_BRIGHTNESS       15     // Minimum allowed brightness (avoids flicker)
#define MIN_DIM_BRIGHTNESS   10     // Minimum allowed dim level
#define MAX_BRIGHTNESS_LIMIT 255    // Valid range: 0-255 (8-bit PWM duty cycle)
#define SCREEN_OFF           0      // Brightness value for screen off
#define CONFIG_PATH          "/etc/touch-timeout.conf"
#define VERSION              "1.0.0"

static volatile int running = 1;    // Used for graceful shutdown on SIGTERM/SIGINT

// --------------------
// Display state machine
// --------------------
enum display_state_enum {
    STATE_FULL = 0,      // Full brightness
    STATE_DIMMED = 1,    // Dimmed
    STATE_OFF = 2        // Screen off
};

// --------------------
// Struct for tracking display state
// --------------------
struct display_state {
    int bright_fd;                      // File descriptor for /sys/class/backlight/.../brightness
    int user_brightness;                // User-configured brightness level (MIN_BRIGHTNESS–max_brightness)
    int dim_brightness;                 // Calculated dim brightness (≥MIN_DIM_BRIGHTNESS)
    int current_brightness;             // Cached hardware brightness (prevents redundant writes)
    int dim_timeout;                    // Seconds before dimming
    int off_timeout;                    // Seconds before turning off
    time_t last_input;                  // Timestamp of last touch event
    enum display_state_enum state;      // Current state
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
// Safe string-to-int (validates untrusted input)
// --------------------
static int safe_atoi(const char *str, int *result) {
    char *endptr;
    errno = 0;
    long val = strtol(str, &endptr, 10);
    if (endptr == str || *endptr != '\0' || errno == ERANGE || val < INT_MIN || val > INT_MAX)
        return -1;
    *result = (int)val;
    return 0;
}

// --------------------
// Parse /etc/touch-timeout.conf for key=value pairs
// Supports:
//   brightness=150
//   off_timeout=300
//   backlight=rpi_backlight
//   device=event0
//   poll_interval=100
//   dim_percent=50
// --------------------
static void load_config(const char *path, int *brightness, int *timeout,
                        char *backlight, size_t bl_sz,
                        char *device, size_t dev_sz,
                        int *poll_interval, int *dim_percent) {
    FILE *f = fopen(path, "r");
    if (!f) return; // No config file found — just skip

    char line[128];
    int line_num = 0;
    while (fgets(line, sizeof(line), f)) {
        line_num++;
        trim(line);
        if (line[0] == '#' || line[0] == ';' || line[0] == '\0')
            continue;

        char key[64], value[64];
        if (sscanf(line, "%63[^=]=%63s", key, value) == 2) {
            trim(key); trim(value);
            
            int tmp;
            if (strcmp(key, "brightness") == 0) {
                if (safe_atoi(value, &tmp) == 0) *brightness = tmp;
                else syslog(LOG_WARNING, "Invalid brightness '%s' at line %d", value, line_num);
            }
            else if (strcmp(key, "off_timeout") == 0) {
                if (safe_atoi(value, &tmp) == 0) *timeout = tmp;
                else syslog(LOG_WARNING, "Invalid off_timeout '%s' at line %d", value, line_num);
            }
            // snprintf() advantages over strncpy():
            // 1. Always null-terminates (strncpy() does NOT if src >= dest size)
            // 2. Single operation instead of two (strncpy + manual '\0')
            // 3. Returns chars written (useful for overflow detection)
            // 4. More secure - CERT C Coding Standard recommends snprintf() over strncpy()
            else if (strcmp(key, "backlight") == 0)
                snprintf(backlight, bl_sz, "%s", value);  // Always null-terminates
            else if (strcmp(key, "device") == 0)
                snprintf(device, dev_sz, "%s", value);    // Always null-terminates
            else if (strcmp(key, "poll_interval") == 0) {
                if (safe_atoi(value, &tmp) == 0) *poll_interval = tmp;
                else syslog(LOG_WARNING, "Invalid poll_interval '%s' at line %d", value, line_num);
            }
            else if (strcmp(key, "dim_percent") == 0) {
                if (safe_atoi(value, &tmp) == 0) *dim_percent = tmp;
                else syslog(LOG_WARNING, "Invalid dim_percent '%s' at line %d", value, line_num);
            }
            else
                syslog(LOG_WARNING, "Unknown config key '%s' at line %d", key, line_num);
        } else {
            syslog(LOG_WARNING, "Malformed config line %d: %s", line_num, line);
        }
    }
    fclose(f);
}

// --------------------
// Read max_brightness from sysfs
// --------------------
static int get_max_brightness(const char *backlight) {
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/backlight/%s/max_brightness", backlight);
    
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        syslog(LOG_WARNING, "Cannot read %s, assuming max=255", path);
        return MAX_BRIGHTNESS_LIMIT;
    }
    
    char buf[8];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    
    if (n <= 0) return MAX_BRIGHTNESS_LIMIT;
    buf[n] = '\0';
    int max = atoi(buf);
    
    // Clamp to valid range
    if (max < 10 || max > MAX_BRIGHTNESS_LIMIT) {
        syslog(LOG_WARNING, "Invalid max_brightness %d, using 255", max);
        return MAX_BRIGHTNESS_LIMIT;
    }
    
    return max;
}

// --------------------
// Read current brightness from sysfs
// --------------------
static int read_brightness(int fd) {
    char buf[8];
    // lseek(fd, 0, SEEK_SET);
    // No lseek needed: fd opened at offset 0, only read once at startup
    // Only set_brightness() needs lseek (repeated writes)
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0) return -1;
    buf[n] = '\0';
    return atoi(buf);
}

// --------------------
// Write brightness safely to /sys/class/backlight
// Uses lseek() to reset file position and caching to prevent redundant writes
// Note: fsync() removed - sysfs writes are synchronous to hardware; fsync() only
// syncs VFS metadata which is unnecessary and adds 5-10ms latency per write
// --------------------
static int set_brightness(struct display_state *state, int brightness) {
    // Catches: NULL pointer dereference from uninitialized state
    assert(state != NULL);
    // Catches: Brightness overflow from config parsing or calculation bugs  
    // Valid range: 0-255 (8-bit PWM duty cycle)
    assert(brightness >= 0 && brightness <= MAX_BRIGHTNESS_LIMIT);
    // Catches: File descriptor corruption or early close
    assert(state->bright_fd > 0);
 
    // Skip if brightness unchanged (prevents redundant hardware writes)
    if (brightness == state->current_brightness)
        return 0;
    
    // Enforce minimum brightness (except for screen off)
    if (brightness < MIN_BRIGHTNESS && brightness != SCREEN_OFF)
        brightness = MIN_BRIGHTNESS;

    char buf[8];
    int len = snprintf(buf, sizeof(buf), "%d", brightness);
    // Reset file position for repeated writes to same sysfs file
    // POSIX requires checking lseek() return - can fail on special files
    if (lseek(state->bright_fd, 0, SEEK_SET) == -1) {
        syslog(LOG_ERR, "lseek failed: %s", strerror(errno));
        return -1;
    }
    int ret = write(state->bright_fd, buf, len);
    // fsync(state->bright_fd);  // REMOVED: sysfs writes are synchronous
    
    if (ret != len) {
        syslog(LOG_ERR, "Failed to set brightness: %s", strerror(errno));
        return -1;
    }
    
    state->current_brightness = brightness;
    return 0;
}

// --------------------
// Restore full brightness after a touch event
// --------------------
static int restore_brightness(struct display_state *state) {
    // Catches: Invalid user_brightness from config validation bypass
    assert(state->user_brightness >= MIN_BRIGHTNESS && state->user_brightness <= MAX_BRIGHTNESS_LIMIT);
    // Catches: State machine corruption before restore
    assert(state->state == STATE_DIMMED || state->state == STATE_OFF);
 
    if (set_brightness(state, state->user_brightness) == 0) {
        state->state = STATE_FULL;
        state->last_input = time(NULL);
        syslog(LOG_INFO, "Restored brightness to %d", state->user_brightness);
        return 0;
    }
    return -1;
}

// --------------------
// Check dim/off timeouts using absolute time comparisons
// Handles missed poll cycles by checking if current time has passed target times
// --------------------
static void check_timeouts(struct display_state *state) {
    // Catches: NULL pointer from incorrect function call
    assert(state != NULL);
    // Catches: Invalid state enum (memory corruption or uninitialized)
    assert(state->state >= STATE_FULL && state->state <= STATE_OFF);
    // Catches: Timeout misconfiguration or integer overflow
    assert(state->dim_timeout > 0 && state->dim_timeout <= state->off_timeout);
    
    time_t now = time(NULL);
    double idle = difftime(now, state->last_input);
 
    // Handle clock adjustments gracefully (NTP can shift time backwards)
    // Using assert() here would crash daemon during normal NTP operations
    if (idle < -5.0) {
        syslog(LOG_WARNING, "Clock adjusted backwards by %.1fs - resetting timer", -idle);
        state->last_input = now;  // Reset baseline to current time
        return;  // Skip timeout checks this cycle
    }
    
    time_t dim_time = state->last_input + state->dim_timeout;
    time_t off_time = state->last_input + state->off_timeout;
    
    // Check if we should be OFF (highest priority - guarantees power saving)
    if (now >= off_time && state->state != STATE_OFF) {
        if (set_brightness(state, SCREEN_OFF) == 0) {
            syslog(LOG_INFO, "Display off (idle=%.0fs)", idle);
            state->state = STATE_OFF;
        }
    }
    // Check if we should be DIMMED (only if not already off)
    else if (now >= dim_time && state->state == STATE_FULL) {
        if (set_brightness(state, state->dim_brightness) == 0) {
            syslog(LOG_INFO, "Display dimmed (idle=%.0fs)", idle);
            state->state = STATE_DIMMED;
        }
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
    char input_dev[64] = "event0";
    int poll_interval = 100;        // Default 100ms (recommended: 50-1000ms)
    int dim_percent = 50;            // Default 50% of off_timeout

    // Load config from /etc/touch-timeout.conf (if present)
    load_config(CONFIG_PATH, &user_brightness, &off_timeout,
                backlight, sizeof(backlight),
                input_dev, sizeof(input_dev),
                &poll_interval, &dim_percent);

    // Command-line args override config (if provided)
    //if (argc > 1) user_brightness = atoi(argv[1]);
    //if (argc > 2) off_timeout = atoi(argv[2]);
    if (argc > 1) {
        if (safe_atoi(argv[1], &user_brightness) != 0) {
            syslog(LOG_ERR, "Invalid brightness argument: %s", argv[1]);
            exit(EXIT_FAILURE);
        }
    }
    if (argc > 2) {
        if (safe_atoi(argv[2], &off_timeout) != 0) {
            syslog(LOG_ERR, "Invalid timeout argument: %s", argv[2]);
            exit(EXIT_FAILURE);
        }
    }
    if (argc > 3) strncpy(backlight, argv[3], sizeof(backlight) - 1);
    if (argc > 4) strncpy(input_dev, argv[4], sizeof(input_dev) - 1);

    // Validate poll_interval (hardware limits: 10ms to 2000ms)
    // Recommended: 50-1000ms for balance of responsiveness and efficiency
    if (poll_interval < 10 || poll_interval > 2000) {
        syslog(LOG_WARNING, "Invalid poll_interval %dms (valid: 10-2000), using default 100ms", poll_interval);
        poll_interval = 100;
    }

    // Validate dim_percent (10-100%)
    if (dim_percent < 10 || dim_percent > 100) {
        syslog(LOG_WARNING, "Invalid dim_percent %d%% (valid: 10-100), using default 50%%", dim_percent);
        dim_percent = 50;
    }

    // Read and validate max_brightness
    // NOTE: For RPi official 7" touchscreen, max brightness and current draw verified
    // at 200 by forum users. Values 201-255 may decrease brightness on some displays
    // due to hardware quirks, but protocol supports full 0-255 range.
    // Recommend setting brightness ≤200 for this display.
    int max_brightness = get_max_brightness(backlight);
    syslog(LOG_INFO, "Max brightness for %s: %d (recommend ≤200 for RPi 7\" display)", 
           backlight, max_brightness);

    // Validate and clamp user_brightness
    if (user_brightness < MIN_BRIGHTNESS) {
        syslog(LOG_WARNING, "Brightness %d below minimum, using %d", user_brightness, MIN_BRIGHTNESS);
        user_brightness = MIN_BRIGHTNESS;
    }
    if (user_brightness > max_brightness) {
        syslog(LOG_WARNING, "Brightness %d exceeds max, clamping to %d", user_brightness, max_brightness);
        user_brightness = max_brightness;
    }

    // Validate off_timeout
    if (off_timeout < 10) {
        syslog(LOG_ERR, "Timeout must be >= 10s");
        exit(EXIT_FAILURE);
    }

    // Calculate dim_timeout from percentage
    int dim_timeout = (off_timeout * dim_percent) / 100;
    
    // Catches: Arithmetic overflow or invalid config combinations
    assert(dim_timeout > 0 && dim_timeout <= off_timeout);
    // Catches: Validation bypass allowing out-of-range brightness
    assert(user_brightness >= MIN_BRIGHTNESS && user_brightness <= max_brightness);
  
    if (dim_percent == 100) {
        syslog(LOG_INFO, "Dimming disabled (dim_percent=100%%)");
    }

    // Open brightness control file (O_RDWR for reading initial state)
    char bright_path[128];
    snprintf(bright_path, sizeof(bright_path),
             "/sys/class/backlight/%s/brightness", backlight);
    int bright_fd = open(bright_path, O_RDWR);
    if (bright_fd == -1) {
        syslog(LOG_ERR, "Error opening %s: %s", bright_path, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Read initial brightness from hardware
    int initial_brightness = read_brightness(bright_fd);
    if (initial_brightness < 0) {
        syslog(LOG_WARNING, "Cannot read current brightness, assuming %d", user_brightness);
        initial_brightness = user_brightness;
    }

    // Initialize display state structure
    struct display_state state = {
        .bright_fd = bright_fd,
        .user_brightness = user_brightness,
        .dim_brightness = (user_brightness / 10 < MIN_DIM_BRIGHTNESS)
                            ? MIN_DIM_BRIGHTNESS : user_brightness / 10,
        .current_brightness = initial_brightness,
        .dim_timeout = dim_timeout,
        .off_timeout = off_timeout,
        .last_input = time(NULL),
        .state = STATE_FULL
    };

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

    syslog(LOG_INFO, "Started v"VERSION": brightness=%d, dim=%d (%d%% @ %ds), off=%ds, poll=%dms",
           state.user_brightness, state.dim_brightness, dim_percent, 
           state.dim_timeout, state.off_timeout, poll_interval);

    // Graceful shutdown handling
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    // Poll loop for touchscreen input events
    struct pollfd pfd = {.fd = event_fd, .events = POLLIN};
    struct input_event event;

    while (running) {
        int ret = poll(&pfd, 1, poll_interval);

        if (ret > 0 && (pfd.revents & POLLIN)) {
            // Read all queued events
            while (read(event_fd, &event, sizeof(event)) > 0) {
                if (event.type == EV_KEY || event.type == EV_ABS) {
                    // Any touch resets timeout or restores screen
                    if (state.state != STATE_FULL)
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
