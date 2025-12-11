/*
 * main.c
 * ------
 * Touch-timeout daemon - main entry point
 *
 * Coordinates all modules:
 * - Configuration management
 * - Display/input hardware
 * - State machine
 * - Timer (timerfd + CLOCK_MONOTONIC)
 * - Systemd integration (sd_notify + watchdog)
 *
 * VERSION: See version.h (auto-generated from Makefile)
 */

#include "config.h"
#include "display.h"
#include "input.h"
#include "state.h"
#include "timer.h"
#include "version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <syslog.h>
#include <poll.h>
#include <sys/types.h>
#include <stdbool.h>
#include <inttypes.h>

/* Systemd notification support */
#ifdef HAVE_SYSTEMD
#include <systemd/sd-daemon.h>
#else
/* Stub implementations if systemd not available */
static inline int sd_notify(int unset_environment, const char *state) {
    (void)unset_environment; (void)state;
    return 0;
}
static inline int sd_watchdog_enabled(int unset_environment, uint64_t *usec) {
    (void)unset_environment; (void)usec;
    return 0;
}
#endif

#define CONFIG_PATH "/etc/touch-timeout.conf"

/* Signal handling */
static volatile sig_atomic_t g_running = 1;

/*
 * Signal handler for graceful shutdown
 */
static void signal_handler(int signum) {
    (void)signum;
    g_running = 0;
}

/*
 * Setup signal handlers
 */
static int setup_signals(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGTERM, &sa, NULL) < 0) {
        syslog(LOG_ERR, "sigaction(SIGTERM) failed: %s", strerror(errno));
        return -1;
    }

    if (sigaction(SIGINT, &sa, NULL) < 0) {
        syslog(LOG_ERR, "sigaction(SIGINT) failed: %s", strerror(errno));
        return -1;
    }

    /* Ignore SIGPIPE */
    signal(SIGPIPE, SIG_IGN);

    return 0;
}

/*
 * Send watchdog keepalive to systemd
 */
static void watchdog_ping(void) {
    sd_notify(0, "WATCHDOG=1");
}

/*
 * Main entry point
 */
int main(int argc, char *argv[]) {
    int exit_code = EXIT_FAILURE;

    /* Initialize syslog */
    openlog("touch-timeout", LOG_PID | LOG_CONS, LOG_DAEMON);
    syslog(LOG_INFO, "Starting touch-timeout v%s", VERSION_STRING);

    /* Setup signal handlers */
    if (setup_signals() < 0) {
        syslog(LOG_ERR, "Failed to setup signal handlers");
        goto cleanup;
    }

    /* Initialize configuration */
    config_t *config = config_init();
    if (config == NULL) {
        syslog(LOG_ERR, "Failed to initialize configuration");
        goto cleanup;
    }

    /* Load configuration from file */
    if (config_load(config, CONFIG_PATH) < 0) {
        syslog(LOG_ERR, "Failed to load configuration");
        goto cleanup;
    }

    /* Parse command-line arguments (override config with validation) */
    static const char *cli_keys[] = {"brightness", "off_timeout", "backlight", "device"};
    for (int i = 1; i < argc && i <= 4; i++) {
        if (config_set_value(config, cli_keys[i - 1], argv[i]) != 0)
            goto cleanup;
    }

    /* Open display device */
    display_t *display = display_open(config->backlight);
    if (display == NULL) {
        syslog(LOG_ERR, "Failed to open display device");
        goto cleanup;
    }

    /* Validate configuration with hardware limits */
    int max_brightness = display_get_max_brightness(display);
    if (config_validate(config, max_brightness) < 0) {
        syslog(LOG_ERR, "Configuration validation failed");
        goto cleanup_display;
    }

    /* Open input device */
    input_t *input = input_open(config->device);
    if (input == NULL) {
        syslog(LOG_ERR, "Failed to open input device");
        goto cleanup_display;
    }

    /* Initialize state machine */
    state_t state;
    if (state_init(&state, config->brightness, config->dim_brightness,
                   config->dim_timeout, config->off_timeout) < 0) {
        syslog(LOG_ERR, "Failed to initialize state machine");
        goto cleanup_input;
    }

    /* Create timer */
    timer_ctx_s *timer = timer_create_ctx();
    if (timer == NULL) {
        syslog(LOG_ERR, "Failed to create timer");
        goto cleanup_input;
    }

    /* Set initial brightness */
    if (display_set_brightness(display, config->brightness) < 0) {
        syslog(LOG_ERR, "Failed to set initial brightness");
        goto cleanup_timer;
    }

    /* Arm timer for first timeout */
    int next_timeout = state_get_next_timeout(&state);
    if (next_timeout > 0)
        timer_arm(timer, next_timeout);

    /* Log startup configuration */
    syslog(LOG_INFO, "Started: brightness=%d, dim=%d (at %ds), off=%ds, device=%s",
           config->brightness, config->dim_brightness,
           config->dim_timeout, config->off_timeout, config->device);

    /* Setup systemd watchdog */
    uint64_t watchdog_usec = 0;
    int watchdog_enabled = sd_watchdog_enabled(0, &watchdog_usec);
    if (watchdog_enabled > 0) {
        syslog(LOG_INFO, "Systemd watchdog enabled: timeout=%" PRIu64 "us", watchdog_usec);
    }

    /* Notify systemd that we're ready */
    sd_notify(0, "READY=1");
    syslog(LOG_INFO, "Service ready");

    /* Setup poll array */
    struct pollfd fds[2];
    fds[0].fd = input_get_fd(input);
    fds[0].events = POLLIN;
    fds[1].fd = timer_get_fd(timer);
    fds[1].events = POLLIN;

    /* Main event loop */
    while (g_running) {
        /* Poll for events - no timeout needed, timer provides timing */
        int ret = poll(fds, 2, -1);

        if (ret < 0) {
            if (errno == EINTR)
                continue;  /* Interrupted by signal */
            syslog(LOG_ERR, "poll() failed: %s", strerror(errno));
            break;
        }

        /* Check for input events */
        if (fds[0].revents & POLLIN) {
            if (input_has_touch_event(input)) {
                int new_brightness;
                if (state_handle_event(&state, STATE_EVENT_TOUCH, &new_brightness)) {
                    display_set_brightness(display, new_brightness);
                }

                /* Rearm timer for next timeout */
                next_timeout = state_get_next_timeout(&state);
                if (next_timeout > 0)
                    timer_arm(timer, next_timeout);
            }
        }

        /* Check for timer expiration */
        if (fds[1].revents & POLLIN) {
            if (timer_check_expiration(timer)) {
                int new_brightness;
                if (state_handle_event(&state, STATE_EVENT_TIMEOUT, &new_brightness)) {
                    display_set_brightness(display, new_brightness);
                }

                /* Rearm timer for next timeout */
                next_timeout = state_get_next_timeout(&state);
                if (next_timeout > 0)
                    timer_arm(timer, next_timeout);
            }
        }

        /* Watchdog ping */
        if (watchdog_enabled > 0) {
            watchdog_ping();
        }
    }

    syslog(LOG_INFO, "Shutting down...");
    exit_code = EXIT_SUCCESS;

cleanup_timer:
    timer_destroy(timer);

cleanup_input:
    input_close(input);

cleanup_display:
    display_close(display);

cleanup:
    /* Notify systemd we're stopping */
    sd_notify(0, "STOPPING=1");
    closelog();
    return exit_code;
}
