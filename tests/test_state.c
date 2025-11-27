/*
 * test_state.c
 * ------------
 * Unit tests for state machine module
 *
 * Tests pure logic without hardware dependencies:
 * - State initialization
 * - Touch event handling
 * - Timeout event handling
 * - State transitions (FULL -> DIMMED -> OFF)
 * - Clock adjustment handling
 */

#include "state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

/* Test framework */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void name(void)
#define RUN_TEST(name) do { \
    printf("  %-60s ", #name); \
    fflush(stdout); \
    tests_run++; \
    name(); \
    tests_passed++; \
    printf("PASS\n"); \
} while(0)

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        printf("FAIL\n    %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

/* ==================== INITIALIZATION TESTS ==================== */

TEST(test_state_init_valid) {
    state_t state;
    int ret = state_init(&state, 100, 10, 150, 300);

    ASSERT_EQ(ret, 0);
    ASSERT_EQ(state.current_state, STATE_FULL);
    ASSERT_EQ(state.user_brightness, 100);
    ASSERT_EQ(state.dim_brightness, 10);
    ASSERT_EQ(state.dim_timeout_sec, 150);
    ASSERT_EQ(state.off_timeout_sec, 300);
}

TEST(test_state_init_invalid_null) {
    int ret = state_init(NULL, 100, 10, 150, 300);
    ASSERT_EQ(ret, -1);
}

TEST(test_state_init_invalid_brightness) {
    state_t state;
    int ret = state_init(&state, 0, 10, 150, 300);
    ASSERT_EQ(ret, -1);
}

TEST(test_state_init_invalid_timeout) {
    state_t state;
    int ret = state_init(&state, 100, 10, 0, 300);
    ASSERT_EQ(ret, -1);
}

TEST(test_state_init_invalid_timeout_order) {
    state_t state;
    /* dim_timeout >= off_timeout is invalid */
    int ret = state_init(&state, 100, 10, 300, 300);
    ASSERT_EQ(ret, -1);
}

/* ==================== TOUCH EVENT TESTS ==================== */

TEST(test_state_touch_when_full) {
    state_t state;
    state_init(&state, 100, 10, 150, 300);

    int new_brightness;
    bool changed = state_handle_event(&state, STATE_EVENT_TOUCH, &new_brightness);

    /* Touch when already FULL should not change brightness */
    ASSERT_FALSE(changed);
    ASSERT_EQ(state.current_state, STATE_FULL);
}

TEST(test_state_touch_restores_from_dimmed) {
    state_t state;
    state_init(&state, 100, 10, 5, 10);

    /* Simulate dimming by going back in time */
    state.last_input_time -= 6;
    state.current_state = STATE_DIMMED;

    int new_brightness;
    bool changed = state_handle_event(&state, STATE_EVENT_TOUCH, &new_brightness);

    ASSERT_TRUE(changed);
    ASSERT_EQ(new_brightness, 100);
    ASSERT_EQ(state.current_state, STATE_FULL);
}

TEST(test_state_touch_restores_from_off) {
    state_t state;
    state_init(&state, 100, 10, 5, 10);

    /* Simulate screen off */
    state.last_input_time -= 15;
    state.current_state = STATE_OFF;

    int new_brightness;
    bool changed = state_handle_event(&state, STATE_EVENT_TOUCH, &new_brightness);

    ASSERT_TRUE(changed);
    ASSERT_EQ(new_brightness, 100);
    ASSERT_EQ(state.current_state, STATE_FULL);
}

/* ==================== TIMEOUT EVENT TESTS ==================== */

TEST(test_state_timeout_stays_full_when_active) {
    state_t state;
    state_init(&state, 100, 10, 150, 300);

    /* Just initialized - should stay FULL */
    int new_brightness;
    bool changed = state_handle_event(&state, STATE_EVENT_TIMEOUT, &new_brightness);

    ASSERT_FALSE(changed);
    ASSERT_EQ(state.current_state, STATE_FULL);
}

TEST(test_state_timeout_dims_after_dim_timeout) {
    state_t state;
    state_init(&state, 100, 10, 5, 10);

    /* Go back in time to simulate idle period */
    state.last_input_time -= 6;

    int new_brightness;
    bool changed = state_handle_event(&state, STATE_EVENT_TIMEOUT, &new_brightness);

    ASSERT_TRUE(changed);
    ASSERT_EQ(new_brightness, 10);
    ASSERT_EQ(state.current_state, STATE_DIMMED);
}

TEST(test_state_timeout_turns_off_after_off_timeout) {
    state_t state;
    state_init(&state, 100, 10, 5, 10);

    /* Simulate being past off timeout */
    state.last_input_time -= 15;

    int new_brightness;
    bool changed = state_handle_event(&state, STATE_EVENT_TIMEOUT, &new_brightness);

    ASSERT_TRUE(changed);
    ASSERT_EQ(new_brightness, 0);
    ASSERT_EQ(state.current_state, STATE_OFF);
}

TEST(test_state_timeout_skips_dim_when_past_off) {
    state_t state;
    state_init(&state, 100, 10, 5, 10);

    /* Start in FULL, but idle time is past off_timeout */
    state.last_input_time -= 15;
    state.current_state = STATE_FULL;

    int new_brightness;
    bool changed = state_handle_event(&state, STATE_EVENT_TIMEOUT, &new_brightness);

    /* Should go directly to OFF, skipping DIMMED */
    ASSERT_TRUE(changed);
    ASSERT_EQ(new_brightness, 0);
    ASSERT_EQ(state.current_state, STATE_OFF);
}

TEST(test_state_timeout_from_dimmed_to_off) {
    state_t state;
    state_init(&state, 100, 10, 5, 10);

    /* Simulate being dimmed and then waiting for off */
    state.last_input_time -= 15;
    state.current_state = STATE_DIMMED;

    int new_brightness;
    bool changed = state_handle_event(&state, STATE_EVENT_TIMEOUT, &new_brightness);

    ASSERT_TRUE(changed);
    ASSERT_EQ(new_brightness, 0);
    ASSERT_EQ(state.current_state, STATE_OFF);
}

/* ==================== CLOCK ADJUSTMENT TESTS ==================== */

TEST(test_state_handles_clock_backwards) {
    state_t state;
    state_init(&state, 100, 10, 150, 300);

    /* Simulate clock going backwards (NTP adjustment) */
    state.last_input_time += 100;  /* 100 seconds in future */

    int new_brightness;
    bool changed = state_handle_event(&state, STATE_EVENT_TIMEOUT, &new_brightness);

    /* Should not change state, just reset timer */
    ASSERT_FALSE(changed);
    ASSERT_EQ(state.current_state, STATE_FULL);
    /* Timer should be reset to approximately now */
    ASSERT_TRUE(state.last_input_time <= time(NULL) + 1);
}

/* ==================== GETTER TESTS ==================== */

TEST(test_state_get_current) {
    state_t state;
    state_init(&state, 100, 10, 150, 300);

    ASSERT_EQ(state_get_current(&state), STATE_FULL);

    state.current_state = STATE_DIMMED;
    ASSERT_EQ(state_get_current(&state), STATE_DIMMED);

    state.current_state = STATE_OFF;
    ASSERT_EQ(state_get_current(&state), STATE_OFF);
}

TEST(test_state_get_brightness) {
    state_t state;
    state_init(&state, 100, 10, 150, 300);

    state.current_state = STATE_FULL;
    ASSERT_EQ(state_get_brightness(&state), 100);

    state.current_state = STATE_DIMMED;
    ASSERT_EQ(state_get_brightness(&state), 10);

    state.current_state = STATE_OFF;
    ASSERT_EQ(state_get_brightness(&state), 0);
}

TEST(test_state_get_next_timeout_from_full) {
    state_t state;
    state_init(&state, 100, 10, 150, 300);

    /* Just initialized - should return dim_timeout */
    int timeout = state_get_next_timeout(&state);
    ASSERT_TRUE(timeout > 0);
    ASSERT_TRUE(timeout <= 150);
}

TEST(test_state_get_next_timeout_from_dimmed) {
    state_t state;
    state_init(&state, 100, 10, 150, 300);

    /* Simulate being dimmed */
    state.last_input_time -= 160;  /* Past dim, not past off */
    state.current_state = STATE_DIMMED;

    int timeout = state_get_next_timeout(&state);
    ASSERT_TRUE(timeout > 0);
    /* Allow for timing variance - should be around 140 seconds */
    ASSERT_TRUE(timeout <= 300 - 160 + 1);  /* Allow 1 second variance */
}

TEST(test_state_get_next_timeout_from_off) {
    state_t state;
    state_init(&state, 100, 10, 150, 300);

    state.current_state = STATE_OFF;

    /* No timeout when off - waiting for touch */
    int timeout = state_get_next_timeout(&state);
    ASSERT_EQ(timeout, -1);
}

TEST(test_state_get_next_timeout_expired) {
    state_t state;
    state_init(&state, 100, 10, 150, 300);

    /* Simulate being past timeout */
    state.last_input_time -= 400;

    int timeout = state_get_next_timeout(&state);
    ASSERT_EQ(timeout, 0);  /* Timeout already expired */
}

/* ==================== STATE TRANSITION SEQUENCE TEST ==================== */

TEST(test_state_full_sequence) {
    state_t state;
    state_init(&state, 100, 10, 5, 10);

    /* Verify initial state */
    ASSERT_EQ(state.current_state, STATE_FULL);

    /* Wait past dim timeout */
    state.last_input_time -= 6;
    int new_brightness;
    bool changed = state_handle_event(&state, STATE_EVENT_TIMEOUT, &new_brightness);
    ASSERT_TRUE(changed);
    ASSERT_EQ(state.current_state, STATE_DIMMED);
    ASSERT_EQ(new_brightness, 10);

    /* Wait past off timeout */
    state.last_input_time -= 5;  /* Total 11 seconds */
    changed = state_handle_event(&state, STATE_EVENT_TIMEOUT, &new_brightness);
    ASSERT_TRUE(changed);
    ASSERT_EQ(state.current_state, STATE_OFF);
    ASSERT_EQ(new_brightness, 0);

    /* Touch to restore */
    changed = state_handle_event(&state, STATE_EVENT_TOUCH, &new_brightness);
    ASSERT_TRUE(changed);
    ASSERT_EQ(state.current_state, STATE_FULL);
    ASSERT_EQ(new_brightness, 100);
}

/* ==================== MAIN TEST RUNNER ==================== */

int main(void) {
    printf("\n========================================\n");
    printf("State Machine Module Unit Tests\n");
    printf("========================================\n\n");

    printf("Initialization tests:\n");
    RUN_TEST(test_state_init_valid);
    RUN_TEST(test_state_init_invalid_null);
    RUN_TEST(test_state_init_invalid_brightness);
    RUN_TEST(test_state_init_invalid_timeout);
    RUN_TEST(test_state_init_invalid_timeout_order);

    printf("\nTouch event tests:\n");
    RUN_TEST(test_state_touch_when_full);
    RUN_TEST(test_state_touch_restores_from_dimmed);
    RUN_TEST(test_state_touch_restores_from_off);

    printf("\nTimeout event tests:\n");
    RUN_TEST(test_state_timeout_stays_full_when_active);
    RUN_TEST(test_state_timeout_dims_after_dim_timeout);
    RUN_TEST(test_state_timeout_turns_off_after_off_timeout);
    RUN_TEST(test_state_timeout_skips_dim_when_past_off);
    RUN_TEST(test_state_timeout_from_dimmed_to_off);

    printf("\nClock adjustment tests:\n");
    RUN_TEST(test_state_handles_clock_backwards);

    printf("\nGetter tests:\n");
    RUN_TEST(test_state_get_current);
    RUN_TEST(test_state_get_brightness);
    RUN_TEST(test_state_get_next_timeout_from_full);
    RUN_TEST(test_state_get_next_timeout_from_dimmed);
    RUN_TEST(test_state_get_next_timeout_from_off);
    RUN_TEST(test_state_get_next_timeout_expired);

    printf("\nState transition sequence test:\n");
    RUN_TEST(test_state_full_sequence);

    printf("\n========================================\n");
    printf("Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf(" (%d FAILED)", tests_failed);
    }
    printf("\n========================================\n\n");

    return tests_failed > 0 ? 1 : 0;
}
