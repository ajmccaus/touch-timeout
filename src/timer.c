/*
 * timer.c
 * -------
 * POSIX timer abstraction using timerfd implementation
 *
 * Uses CLOCK_MONOTONIC for robustness against system time changes
 * and suspend/resume cycles
 */

#include "timer.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <sys/timerfd.h>

/* Timer context structure */
struct timer_ctx {
    int timerfd;                /* File descriptor for timerfd */
};

/*
 * Create timer
 */
timer_t *timer_create_ctx(void) {
    timer_t *timer = calloc(1, sizeof(timer_t));
    if (timer == NULL) {
        syslog(LOG_ERR, "timer_create_ctx: malloc failed");
        return NULL;
    }

    /* Create timerfd with CLOCK_MONOTONIC
     * CLOCK_MONOTONIC is not affected by:
     * - System time changes (NTP, manual adjustment)
     * - Suspend/resume (with TFD_TIMER_CANCEL_ON_SET flag removed)
     *
     * TFD_NONBLOCK: non-blocking reads for integration with poll()
     * TFD_CLOEXEC: close-on-exec for security
     */
    timer->timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timer->timerfd < 0) {
        syslog(LOG_ERR, "timerfd_create failed: %s", strerror(errno));
        free(timer);
        return NULL;
    }

    syslog(LOG_INFO, "Timer created with CLOCK_MONOTONIC (fd=%d)", timer->timerfd);

    return timer;
}

/*
 * Destroy timer
 */
void timer_destroy(timer_t *timer) {
    if (timer == NULL)
        return;

    if (timer->timerfd > 0)
        close(timer->timerfd);

    free(timer);
}

/*
 * Get file descriptor for polling
 */
int timer_get_fd(timer_t *timer) {
    if (timer == NULL)
        return -1;

    return timer->timerfd;
}

/*
 * Arm timer (one-shot)
 */
int timer_arm(timer_t *timer, int seconds) {
    if (timer == NULL || timer->timerfd <= 0) {
        syslog(LOG_ERR, "timer_arm: invalid timer");
        return -1;
    }

    if (seconds < 0) {
        syslog(LOG_ERR, "timer_arm: negative timeout");
        return -1;
    }

    struct itimerspec spec;

    if (seconds == 0) {
        /* Disarm timer */
        spec.it_value.tv_sec = 0;
        spec.it_value.tv_nsec = 0;
        spec.it_interval.tv_sec = 0;
        spec.it_interval.tv_nsec = 0;
    } else {
        /* Arm timer for one-shot expiration */
        spec.it_value.tv_sec = seconds;
        spec.it_value.tv_nsec = 0;
        spec.it_interval.tv_sec = 0;   /* One-shot, not periodic */
        spec.it_interval.tv_nsec = 0;
    }

    if (timerfd_settime(timer->timerfd, 0, &spec, NULL) < 0) {
        syslog(LOG_ERR, "timerfd_settime failed: %s", strerror(errno));
        return -1;
    }

    return 0;
}

/*
 * Check if timer expired
 */
bool timer_check_expiration(timer_t *timer) {
    if (timer == NULL || timer->timerfd <= 0)
        return false;

    /* Read expiration count from timerfd
     * Returns number of expirations since last read
     * Non-blocking due to TFD_NONBLOCK flag
     */
    uint64_t expirations;
    ssize_t ret = read(timer->timerfd, &expirations, sizeof(expirations));

    if (ret == sizeof(expirations)) {
        /* Timer expired */
        return true;
    } else if (ret < 0 && errno == EAGAIN) {
        /* No expiration yet - normal case */
        return false;
    } else {
        /* Error reading timerfd */
        if (ret < 0)
            syslog(LOG_WARNING, "timer_check_expiration: read failed: %s", strerror(errno));
        return false;
    }
}
