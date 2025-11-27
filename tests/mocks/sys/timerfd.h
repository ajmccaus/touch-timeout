/*
 * Mock sys/timerfd.h for testing on non-Linux systems
 * Provides minimal definitions for timerfd API
 */

#ifndef _MOCK_SYS_TIMERFD_H
#define _MOCK_SYS_TIMERFD_H

#include <time.h>
#include <stdint.h>

/* Flags for timerfd_create */
#define TFD_NONBLOCK  04000
#define TFD_CLOEXEC   02000000

/* Compatibility definition for itimerspec if not available */
#ifndef _STRUCT_ITIMERSPEC
#define _STRUCT_ITIMERSPEC
struct itimerspec {
    struct timespec it_interval;  /* Timer interval */
    struct timespec it_value;     /* Initial expiration */
};
#endif

/* Stub function declarations */
static inline int timerfd_create(int clockid, int flags) {
    (void)clockid; (void)flags;
    return -1;  /* Not functional on non-Linux */
}

static inline int timerfd_settime(int fd, int flags,
                                  const struct itimerspec *new_value,
                                  struct itimerspec *old_value) {
    (void)fd; (void)flags; (void)new_value; (void)old_value;
    return -1;  /* Not functional on non-Linux */
}

#endif /* _MOCK_SYS_TIMERFD_H */
