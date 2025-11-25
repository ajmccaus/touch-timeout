/*
 * Mock linux/input.h for testing on non-Linux systems
 * Provides minimal struct definitions needed for compilation
 */

#ifndef _MOCK_LINUX_INPUT_H
#define _MOCK_LINUX_INPUT_H

#include <stdint.h>
#include <sys/time.h>

struct input_event {
    struct timeval time;
    uint16_t type;
    uint16_t code;
    int32_t value;
};

#define EV_KEY 0x01
#define EV_ABS 0x03

#endif /* _MOCK_LINUX_INPUT_H */
