/*
 * main.c - Touch-timeout daemon
 *
 * Lightweight touchscreen backlight manager for Raspberry Pi
 * Dims display after inactivity, turns off after timeout, wakes on touch
 */

#include "state.h"
#include "version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <poll.h>
#include <fcntl.h>
#include <time.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <linux/input.h>

/* Systemd notification support */
#ifdef HAVE_SYSTEMD
#include <systemd/sd-daemon.h>
#else
static inline int sd_notify(int unset_environment, const char *state) {
    (void)unset_environment; (void)state;
    return 0;
}
#endif

/* ============================================================
 * SECTION: Defaults and Limits
 * ============================================================ */

#define DEFAULT_BRIGHTNESS   150
#define DEFAULT_TIMEOUT_SEC  300
#define DEFAULT_DIM_PERCENT  10
#define DEFAULT_BACKLIGHT    "rpi_backlight"
#define DEFAULT_DEVICE       "event0"

#define MIN_BRIGHTNESS       15
#define MAX_BRIGHTNESS       255
#define MIN_TIMEOUT_SEC      10
#define MAX_TIMEOUT_SEC      86400
#define MIN_DIM_PERCENT      1
#define MAX_DIM_PERCENT      100
#define MIN_DIM_BRIGHTNESS   10
#define MIN_DIM_TIMEOUT_MS   1000

/* Ensure timeout fits in poll() int parameter */
_Static_assert(MAX_TIMEOUT_SEC <= INT_MAX / 1000,
               "MAX_TIMEOUT_SEC too large for poll() timeout");

/* Ensure brightness * dim_percent fits in int */
_Static_assert(MAX_BRIGHTNESS * MAX_DIM_PERCENT <= INT_MAX,
               "MAX_BRIGHTNESS * MAX_DIM_PERCENT overflow risk");

/* ============================================================
 * SECTION: Buffer Sizes and Paths
 * ============================================================ */

/* Filesystem paths (Linux sysfs/devfs conventions) */
#define SYSFS_BACKLIGHT_DIR  "/sys/class/backlight"
#define DEV_INPUT_DIR        "/dev/input"

/* Buffer sizes */
#define MAX_DEVICE_NAME_LEN  64
#define SYSFS_VALUE_LEN      16
/* Path buffer: dir + "/" + name + "/max_brightness" + null */
#define PATH_BUFFER_LEN      (sizeof(SYSFS_BACKLIGHT_DIR) + 1 + MAX_DEVICE_NAME_LEN + 16)

/* Compile-time buffer safety checks */
_Static_assert(PATH_BUFFER_LEN >= sizeof(SYSFS_BACKLIGHT_DIR) + 1 + MAX_DEVICE_NAME_LEN + sizeof("/max_brightness"),
               "PATH_BUFFER_LEN too small for backlight paths");
_Static_assert(PATH_BUFFER_LEN >= sizeof(DEV_INPUT_DIR) + 1 + MAX_DEVICE_NAME_LEN,
               "PATH_BUFFER_LEN too small for input paths");

/* ============================================================
 * SECTION: Type Definitions
 * ============================================================ */

typedef struct {
    int brightness;
    int timeout_sec;
    int dim_percent;
    char backlight[MAX_DEVICE_NAME_LEN];
    char device[MAX_DEVICE_NAME_LEN];
} config_s;

/* ============================================================
 * SECTION: Global State
 * ============================================================ */

static volatile sig_atomic_t g_running = 1;
static volatile sig_atomic_t g_wake_requested = 0;
static bool g_verbose = false;

/* ============================================================
 * SECTION: Logging Macros
 * ============================================================ */

#define log_info(fmt, ...)    fprintf(stderr, "INFO: " fmt "\n", ##__VA_ARGS__)
#define log_warn(fmt, ...)    fprintf(stderr, "WARN: " fmt "\n", ##__VA_ARGS__)
#define log_err(fmt, ...)     fprintf(stderr, "ERROR: " fmt "\n", ##__VA_ARGS__)
#define log_verbose(fmt, ...) do { if (g_verbose) fprintf(stderr, "DEBUG: " fmt "\n", ##__VA_ARGS__); } while(0)

/* ============================================================
 * SECTION: Utility Functions
 * ============================================================ */

/*
 * Get current time in milliseconds (CLOCK_MONOTONIC).
 * Returns uint32_t which wraps every ~49.7 days.
 * Wraparound is intentional: subtraction still yields correct elapsed time
 * due to unsigned arithmetic. See test_wraparound_handling in test suite.
 */
static uint32_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/* Parse integer from string, returns -1 on error */
static int parse_int(const char *str, int *out) {
    char *end;
    errno = 0;
    long val = strtol(str, &end, 10);
    if (end == str || *end != '\0' || errno == ERANGE ||
        val < INT_MIN || val > INT_MAX)
        return -1;
    *out = (int)val;
    return 0;
}

/* ============================================================
 * SECTION: CLI Parsing
 * ============================================================ */

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [OPTIONS]\n"
        "\n"
        "Options:\n"
        "  -b, --brightness=N   Full brightness (15-255, default %d)\n"
        "  -t, --timeout=N      Off timeout in seconds (10-86400, default %d)\n"
        "  -d, --dim-percent=N  Dim at N%% of timeout (1-100, default %d)\n"
        "  -l, --backlight=NAME Backlight device (default %s)\n"
        "  -i, --input=NAME     Input device (default %s)\n"
        "  -v, --verbose        Verbose logging\n"
        "  -V, --version        Show version\n"
        "  -h, --help           Show this help\n"
        "\n"
        "External wake: Send SIGUSR1 to wake display\n"
        "  pkill -USR1 touch-timeout\n",
        prog, DEFAULT_BRIGHTNESS, DEFAULT_TIMEOUT_SEC, DEFAULT_DIM_PERCENT,
        DEFAULT_BACKLIGHT, DEFAULT_DEVICE);
}

static bool validate_device_name(const char *name) {
    /* Reject paths containing / or .. to prevent path traversal */
    if (strchr(name, '/') != NULL || strstr(name, "..") != NULL)
        return false;
    size_t len = strlen(name);
    if (len == 0 || len >= MAX_DEVICE_NAME_LEN)
        return false;
    return true;
}

static void parse_args(int argc, char **argv, config_s *cfg) {
    static const struct option long_options[] = {
        {"brightness",  required_argument, 0, 'b'},
        {"timeout",     required_argument, 0, 't'},
        {"dim-percent", required_argument, 0, 'd'},
        {"backlight",   required_argument, 0, 'l'},
        {"input",       required_argument, 0, 'i'},
        {"verbose",     no_argument,       0, 'v'},
        {"version",     no_argument,       0, 'V'},
        {"help",        no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "b:t:d:l:i:vVh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'b':
                if (parse_int(optarg, &cfg->brightness) < 0) {
                    log_err("Invalid brightness: %s", optarg);
                    exit(EXIT_FAILURE);
                }
                break;
            case 't':
                if (parse_int(optarg, &cfg->timeout_sec) < 0) {
                    log_err("Invalid timeout: %s", optarg);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'd':
                if (parse_int(optarg, &cfg->dim_percent) < 0) {
                    log_err("Invalid dim-percent: %s", optarg);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'l':
                if (!validate_device_name(optarg)) {
                    log_err("Invalid backlight name: %s", optarg);
                    exit(EXIT_FAILURE);
                }
                snprintf(cfg->backlight, sizeof(cfg->backlight), "%s", optarg);
                break;
            case 'i':
                if (!validate_device_name(optarg)) {
                    log_err("Invalid input device name: %s", optarg);
                    exit(EXIT_FAILURE);
                }
                snprintf(cfg->device, sizeof(cfg->device), "%s", optarg);
                break;
            case 'v':
                g_verbose = true;
                break;
            case 'V':
                printf("touch-timeout %s\n", VERSION_STRING);
                exit(EXIT_SUCCESS);
            case 'h':
                usage(argv[0]);
                exit(EXIT_SUCCESS);
            default:
                usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    /* Validate ranges - use defaults for out-of-range values */
    if (cfg->brightness < MIN_BRIGHTNESS || cfg->brightness > MAX_BRIGHTNESS) {
        log_warn("brightness %d out of range (%d-%d), using default %d",
                 cfg->brightness, MIN_BRIGHTNESS, MAX_BRIGHTNESS, DEFAULT_BRIGHTNESS);
        cfg->brightness = DEFAULT_BRIGHTNESS;
    }
    if (cfg->timeout_sec < MIN_TIMEOUT_SEC || cfg->timeout_sec > MAX_TIMEOUT_SEC) {
        log_warn("timeout %d out of range (%d-%d), using default %d",
                 cfg->timeout_sec, MIN_TIMEOUT_SEC, MAX_TIMEOUT_SEC, DEFAULT_TIMEOUT_SEC);
        cfg->timeout_sec = DEFAULT_TIMEOUT_SEC;
    }
    if (cfg->dim_percent < MIN_DIM_PERCENT || cfg->dim_percent > MAX_DIM_PERCENT) {
        log_warn("dim-percent %d out of range (%d-%d), using default %d",
                 cfg->dim_percent, MIN_DIM_PERCENT, MAX_DIM_PERCENT, DEFAULT_DIM_PERCENT);
        cfg->dim_percent = DEFAULT_DIM_PERCENT;
    }
}

/* ============================================================
 * SECTION: Device I/O
 * ============================================================ */

static int open_backlight(const char *name) {
    char path[PATH_BUFFER_LEN];
    snprintf(path, sizeof(path), "%s/%s/brightness", SYSFS_BACKLIGHT_DIR, name);

    int fd = open(path, O_RDWR);
    if (fd < 0) {
        log_err("Cannot open %s: %s", path, strerror(errno));
        return -1;
    }
    return fd;
}

static int get_max_brightness(const char *name) {
    char path[PATH_BUFFER_LEN];
    snprintf(path, sizeof(path), "%s/%s/max_brightness", SYSFS_BACKLIGHT_DIR, name);

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        log_err("Cannot read %s: %s (assuming max=%d)", path, strerror(errno), MAX_BRIGHTNESS);
        return MAX_BRIGHTNESS;
    }

    char buf[SYSFS_VALUE_LEN];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (n <= 0) {
        log_err("Empty read from %s (assuming max=%d)", path, MAX_BRIGHTNESS);
        return MAX_BRIGHTNESS;
    }

    buf[n] = '\0';
    /* Trim trailing whitespace (sysfs includes newline) */
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == ' '))
        buf[--n] = '\0';

    int val;
    if (parse_int(buf, &val) < 0 || val <= 0) {
        log_err("Invalid max_brightness '%s' (assuming max=%d)", buf, MAX_BRIGHTNESS);
        return MAX_BRIGHTNESS;
    }
    return val;
}

static int set_brightness(int fd, int value) {
    char buf[SYSFS_VALUE_LEN];
    int len = snprintf(buf, sizeof(buf), "%d", value);

    if (len < 0 || len >= (int)sizeof(buf)) {
        log_err("brightness value formatting failed");
        return -1;
    }

    if (lseek(fd, 0, SEEK_SET) < 0) {
        log_err("lseek failed: %s", strerror(errno));
        return -1;
    }

    ssize_t written = write(fd, buf, (size_t)len);
    if (written != (ssize_t)len) {
        log_err("brightness write failed: %s", strerror(errno));
        return -1;
    }

    return 0;
}

static int open_input(const char *name) {
    char path[PATH_BUFFER_LEN];
    snprintf(path, sizeof(path), "%s/%s", DEV_INPUT_DIR, name);

    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        log_err("Cannot open %s: %s", path, strerror(errno));
        return -1;
    }
    return fd;
}

static bool drain_touch_events(int fd) {
    struct input_event ev;
    bool had_touch = false;

    while (read(fd, &ev, sizeof(ev)) == sizeof(ev)) {
        /* Count any event as activity */
        had_touch = true;
    }

    return had_touch;
}

/* ============================================================
 * SECTION: Signal Handling
 * ============================================================ */

static void handle_signal(int sig) {
    if (sig == SIGUSR1) {
        g_wake_requested = 1;
    } else {
        g_running = 0;
    }
}

/* ============================================================
 * SECTION: Main
 * ============================================================ */

int main(int argc, char *argv[]) {
    /* Initialize config with defaults */
    config_s cfg = {
        .brightness = DEFAULT_BRIGHTNESS,
        .timeout_sec = DEFAULT_TIMEOUT_SEC,
        .dim_percent = DEFAULT_DIM_PERCENT,
        .backlight = DEFAULT_BACKLIGHT,
        .device = DEFAULT_DEVICE
    };

    /* Parse CLI args */
    parse_args(argc, argv, &cfg);

    /* Open devices (fail fast on error) */
    int bl_fd = open_backlight(cfg.backlight);
    if (bl_fd < 0)
        return EXIT_FAILURE;

    int input_fd = open_input(cfg.device);
    if (input_fd < 0) {
        close(bl_fd);
        return EXIT_FAILURE;
    }

    /* Clamp brightness to hardware max */
    int hw_max = get_max_brightness(cfg.backlight);
    if (cfg.brightness > hw_max) {
        log_info("brightness %d exceeds hardware max %d, clamping", cfg.brightness, hw_max);
        cfg.brightness = hw_max;
    }

    /* Calculate derived values */
    int dim_bright = cfg.brightness * cfg.dim_percent / 100;
    if (dim_bright < MIN_DIM_BRIGHTNESS)
        dim_bright = MIN_DIM_BRIGHTNESS;

    /* Calculate timeouts: off_ms first, then dim_ms as percentage of off_ms */
    uint32_t off_ms = (uint32_t)cfg.timeout_sec * 1000U;
    uint32_t dim_ms = (off_ms * (uint32_t)cfg.dim_percent) / 100U;
    if (dim_ms < MIN_DIM_TIMEOUT_MS)
        dim_ms = MIN_DIM_TIMEOUT_MS;

    /* Ensure dim_ms < off_ms */
    if (dim_ms >= off_ms) {
        dim_ms = off_ms / 2;
        if (dim_ms < MIN_DIM_TIMEOUT_MS)
            dim_ms = MIN_DIM_TIMEOUT_MS;
    }

    /* Initialize state machine */
    state_s state;
    state_init(&state, cfg.brightness, dim_bright, dim_ms, off_ms);

    /* Set initial timestamp and brightness */
    uint32_t now = now_ms();
    state_touch(&state, now);
    if (set_brightness(bl_fd, cfg.brightness) < 0) {
        log_err("Cannot set initial brightness - check permissions");
        close(input_fd);
        close(bl_fd);
        return EXIT_FAILURE;
    }
    int cached_brightness = cfg.brightness;

    /* Setup signals */
    struct sigaction sa = {0};
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGTERM, &sa, NULL) < 0 ||
        sigaction(SIGINT, &sa, NULL) < 0 ||
        sigaction(SIGUSR1, &sa, NULL) < 0) {
        log_err("sigaction failed: %s", strerror(errno));
        close(input_fd);
        close(bl_fd);
        return EXIT_FAILURE;
    }
    signal(SIGPIPE, SIG_IGN);

    /* Notify systemd */
    sd_notify(0, "READY=1");

    log_info("touch-timeout v%s: brightness=%d, dim=%d, dim_ms=%u, off_ms=%u",
             VERSION_STRING, cfg.brightness, dim_bright, dim_ms, off_ms);

    /* Event loop */
    struct pollfd pfd = { .fd = input_fd, .events = POLLIN };

    while (g_running) {
        now = now_ms();
        int timeout_ms = state_get_timeout_ms(&state, now);

        int ret = poll(&pfd, 1, timeout_ms);

        if (ret < 0) {
            if (errno == EINTR) {
                /* Check for SIGUSR1 wake */
                if (g_wake_requested) {
                    g_wake_requested = 0;
                    now = now_ms();
                    int new_bright = state_touch(&state, now);
                    if (new_bright >= 0 && new_bright != cached_brightness) {
                        if (set_brightness(bl_fd, new_bright) == 0) {
                            cached_brightness = new_bright;
                            log_verbose("SIGUSR1 -> FULL (brightness %d)", new_bright);
                        }
                    }
                }
                continue;
            }
            log_err("poll() failed: %s", strerror(errno));
            break;
        }

        now = now_ms();
        int new_bright = -1;

        if (ret > 0 && (pfd.revents & POLLIN)) {
            /* Touch event */
            if (drain_touch_events(input_fd)) {
                new_bright = state_touch(&state, now);
                if (new_bright >= 0) {
                    log_verbose("Touch -> FULL (brightness %d)", new_bright);
                }
            }
        } else if (ret == 0) {
            /* Timeout */
            new_bright = state_timeout(&state, now);
            if (new_bright >= 0) {
                const char *state_name = (new_bright == 0) ? "OFF" : "DIMMED";
                log_verbose("Timeout -> %s (brightness %d)", state_name, new_bright);
            }
        }

        /* Update brightness if changed (with caching) */
        if (new_bright >= 0 && new_bright != cached_brightness) {
            if (set_brightness(bl_fd, new_bright) == 0) {
                cached_brightness = new_bright;
            }
        }
    }

    /* Restore brightness and cleanup */
    if (set_brightness(bl_fd, cfg.brightness) == 0) {
        log_info("Brightness restored to %d, shutting down", cfg.brightness);
    } else {
        log_warn("Could not restore brightness on shutdown");
    }

    sd_notify(0, "STOPPING=1");
    close(input_fd);
    close(bl_fd);

    return EXIT_SUCCESS;
}
