/*
 * test_state.c - Unit tests for pure state machine
 *
 * Tests are simple - no mocking needed, just pass timestamps
 */

#include "../src/state.h"
#include <stdio.h>
#include <stdlib.h>

/* Test framework */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void name(void)
#define RUN_TEST(name) do { \
    printf("  %-55s ", #name); \
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
        tests_passed--;  /* Undo increment from RUN_TEST */ \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))

/* Test config: dim at 5000ms, off at 10000ms, full=100, dim=10 */
#define BRIGHT_FULL 100
#define BRIGHT_DIM  10
#define DIM_MS      5000
#define OFF_MS      10000

/* ==================== INITIALIZATION TESTS ==================== */

TEST(test_init_sets_full_state) {
    state_t s;
    state_init(&s, BRIGHT_FULL, BRIGHT_DIM, DIM_MS, OFF_MS);

    ASSERT_EQ(s.state, STATE_FULL);
    ASSERT_EQ(s.last_touch_ms, 0);
}

TEST(test_init_stores_config) {
    state_t s;
    state_init(&s, BRIGHT_FULL, BRIGHT_DIM, DIM_MS, OFF_MS);

    ASSERT_EQ(s.brightness_full, BRIGHT_FULL);
    ASSERT_EQ(s.brightness_dim, BRIGHT_DIM);
    ASSERT_EQ(s.dim_timeout_ms, DIM_MS);
    ASSERT_EQ(s.off_timeout_ms, OFF_MS);
}

/* ==================== TOUCH EVENT TESTS ==================== */

TEST(test_touch_when_full_no_change) {
    state_t s;
    state_init(&s, BRIGHT_FULL, BRIGHT_DIM, DIM_MS, OFF_MS);
    state_touch(&s, 1000);  /* Initial touch */

    int brightness = state_touch(&s, 2000);

    ASSERT_EQ(brightness, -1);  /* No change */
    ASSERT_EQ(s.state, STATE_FULL);
    ASSERT_EQ(s.last_touch_ms, 2000);
}

TEST(test_touch_from_dimmed_returns_full) {
    state_t s;
    state_init(&s, BRIGHT_FULL, BRIGHT_DIM, DIM_MS, OFF_MS);
    state_touch(&s, 0);
    state_timeout(&s, 6000);  /* Transition to DIMMED */

    int brightness = state_touch(&s, 7000);

    ASSERT_EQ(brightness, BRIGHT_FULL);
    ASSERT_EQ(s.state, STATE_FULL);
}

TEST(test_touch_from_off_returns_full) {
    state_t s;
    state_init(&s, BRIGHT_FULL, BRIGHT_DIM, DIM_MS, OFF_MS);
    state_touch(&s, 0);
    state_timeout(&s, 6000);   /* FULL -> DIMMED */
    state_timeout(&s, 11000);  /* DIMMED -> OFF */

    int brightness = state_touch(&s, 12000);

    ASSERT_EQ(brightness, BRIGHT_FULL);
    ASSERT_EQ(s.state, STATE_FULL);
}

TEST(test_touch_updates_timestamp) {
    state_t s;
    state_init(&s, BRIGHT_FULL, BRIGHT_DIM, DIM_MS, OFF_MS);

    state_touch(&s, 5000);
    ASSERT_EQ(s.last_touch_ms, 5000);

    state_touch(&s, 8000);
    ASSERT_EQ(s.last_touch_ms, 8000);
}

/* ==================== TIMEOUT EVENT TESTS ==================== */

TEST(test_timeout_before_dim_no_change) {
    state_t s;
    state_init(&s, BRIGHT_FULL, BRIGHT_DIM, DIM_MS, OFF_MS);
    state_touch(&s, 0);

    int brightness = state_timeout(&s, 4000);  /* 4s, dim at 5s */

    ASSERT_EQ(brightness, -1);
    ASSERT_EQ(s.state, STATE_FULL);
}

TEST(test_timeout_at_dim_transitions) {
    state_t s;
    state_init(&s, BRIGHT_FULL, BRIGHT_DIM, DIM_MS, OFF_MS);
    state_touch(&s, 0);

    int brightness = state_timeout(&s, 5000);  /* Exactly at dim */

    ASSERT_EQ(brightness, BRIGHT_DIM);
    ASSERT_EQ(s.state, STATE_DIMMED);
}

TEST(test_timeout_at_off_transitions) {
    state_t s;
    state_init(&s, BRIGHT_FULL, BRIGHT_DIM, DIM_MS, OFF_MS);
    state_touch(&s, 0);
    state_timeout(&s, 5000);  /* FULL -> DIMMED */

    int brightness = state_timeout(&s, 10000);  /* Exactly at off */

    ASSERT_EQ(brightness, 0);
    ASSERT_EQ(s.state, STATE_OFF);
}

TEST(test_timeout_in_off_no_change) {
    state_t s;
    state_init(&s, BRIGHT_FULL, BRIGHT_DIM, DIM_MS, OFF_MS);
    state_touch(&s, 0);
    state_timeout(&s, 6000);
    state_timeout(&s, 11000);  /* Now OFF */

    int brightness = state_timeout(&s, 20000);

    ASSERT_EQ(brightness, -1);
    ASSERT_EQ(s.state, STATE_OFF);
}

TEST(test_timeout_dimmed_to_off) {
    state_t s;
    state_init(&s, BRIGHT_FULL, BRIGHT_DIM, DIM_MS, OFF_MS);
    state_touch(&s, 0);
    state_timeout(&s, 6000);  /* Now DIMMED */

    /* Wait more time */
    int brightness = state_timeout(&s, 11000);

    ASSERT_EQ(brightness, 0);
    ASSERT_EQ(s.state, STATE_OFF);
}

/* ==================== TIMEOUT CALCULATION TESTS ==================== */

TEST(test_get_timeout_from_full) {
    state_t s;
    state_init(&s, BRIGHT_FULL, BRIGHT_DIM, DIM_MS, OFF_MS);
    state_touch(&s, 1000);

    int timeout = state_get_timeout_ms(&s, 2000);  /* 1s idle */

    ASSERT_EQ(timeout, 4000);  /* 5000 - 1000 = 4000 */
}

TEST(test_get_timeout_from_dimmed) {
    state_t s;
    state_init(&s, BRIGHT_FULL, BRIGHT_DIM, DIM_MS, OFF_MS);
    state_touch(&s, 0);
    state_timeout(&s, 6000);  /* DIMMED */

    int timeout = state_get_timeout_ms(&s, 7000);  /* 7s idle */

    ASSERT_EQ(timeout, 3000);  /* 10000 - 7000 = 3000 */
}

TEST(test_get_timeout_from_off) {
    state_t s;
    state_init(&s, BRIGHT_FULL, BRIGHT_DIM, DIM_MS, OFF_MS);
    state_touch(&s, 0);
    state_timeout(&s, 6000);
    state_timeout(&s, 11000);  /* OFF */

    int timeout = state_get_timeout_ms(&s, 15000);

    ASSERT_EQ(timeout, -1);  /* No timeout in OFF */
}

TEST(test_get_timeout_expired_returns_zero) {
    state_t s;
    state_init(&s, BRIGHT_FULL, BRIGHT_DIM, DIM_MS, OFF_MS);
    state_touch(&s, 0);

    int timeout = state_get_timeout_ms(&s, 6000);  /* Past dim */

    ASSERT_EQ(timeout, 0);
}

/* ==================== BRIGHTNESS GETTER TESTS ==================== */

TEST(test_get_brightness_full) {
    state_t s;
    state_init(&s, BRIGHT_FULL, BRIGHT_DIM, DIM_MS, OFF_MS);

    ASSERT_EQ(state_get_brightness(&s), BRIGHT_FULL);
}

TEST(test_get_brightness_dimmed) {
    state_t s;
    state_init(&s, BRIGHT_FULL, BRIGHT_DIM, DIM_MS, OFF_MS);
    state_touch(&s, 0);
    state_timeout(&s, 6000);

    ASSERT_EQ(state_get_brightness(&s), BRIGHT_DIM);
}

TEST(test_get_brightness_off) {
    state_t s;
    state_init(&s, BRIGHT_FULL, BRIGHT_DIM, DIM_MS, OFF_MS);
    state_touch(&s, 0);
    state_timeout(&s, 6000);
    state_timeout(&s, 11000);

    ASSERT_EQ(state_get_brightness(&s), 0);
}

/* ==================== STATE GETTER TESTS ==================== */

TEST(test_get_current_state) {
    state_t s;
    state_init(&s, BRIGHT_FULL, BRIGHT_DIM, DIM_MS, OFF_MS);
    state_touch(&s, 0);

    ASSERT_EQ(state_get_current(&s), STATE_FULL);

    state_timeout(&s, 6000);
    ASSERT_EQ(state_get_current(&s), STATE_DIMMED);

    state_timeout(&s, 11000);
    ASSERT_EQ(state_get_current(&s), STATE_OFF);
}

/* ==================== EDGE CASE TESTS ==================== */

TEST(test_wraparound_handling) {
    state_t s;
    state_init(&s, BRIGHT_FULL, BRIGHT_DIM, DIM_MS, OFF_MS);

    /* Touch near uint32_t max */
    uint32_t near_max = 0xFFFFFFFF - 1000;
    state_touch(&s, near_max);

    /* 2s later wraps around */
    uint32_t wrapped = near_max + 2000;  /* Wraps to ~998 */
    int timeout = state_get_timeout_ms(&s, wrapped);

    /* idle = wrapped - near_max = 2000, should be 3000 remaining */
    ASSERT_EQ(timeout, 3000);
}

TEST(test_zero_idle_time) {
    state_t s;
    state_init(&s, BRIGHT_FULL, BRIGHT_DIM, DIM_MS, OFF_MS);
    state_touch(&s, 5000);

    int timeout = state_get_timeout_ms(&s, 5000);  /* Same time */

    ASSERT_EQ(timeout, 5000);  /* Full dim timeout remaining */
}

/* ==================== FULL SEQUENCE TEST ==================== */

TEST(test_full_lifecycle) {
    state_t s;
    state_init(&s, BRIGHT_FULL, BRIGHT_DIM, DIM_MS, OFF_MS);

    /* Initial touch */
    int b = state_touch(&s, 0);
    ASSERT_EQ(b, -1);  /* Already FULL */
    ASSERT_EQ(s.state, STATE_FULL);

    /* Wait 3s - still FULL */
    b = state_timeout(&s, 3000);
    ASSERT_EQ(b, -1);
    ASSERT_EQ(s.state, STATE_FULL);

    /* Wait 6s total - transition to DIMMED */
    b = state_timeout(&s, 6000);
    ASSERT_EQ(b, BRIGHT_DIM);
    ASSERT_EQ(s.state, STATE_DIMMED);

    /* Wait 11s total - transition to OFF */
    b = state_timeout(&s, 11000);
    ASSERT_EQ(b, 0);
    ASSERT_EQ(s.state, STATE_OFF);

    /* Touch - restore to FULL */
    b = state_touch(&s, 12000);
    ASSERT_EQ(b, BRIGHT_FULL);
    ASSERT_EQ(s.state, STATE_FULL);

    /* Verify timer reset */
    int timeout = state_get_timeout_ms(&s, 13000);
    ASSERT_EQ(timeout, 4000);  /* 5000 - 1000 */
}

/* ==================== MAIN TEST RUNNER ==================== */

int main(void) {
    printf("\n========================================\n");
    printf("State Machine Unit Tests (Pure API)\n");
    printf("========================================\n\n");

    printf("Initialization:\n");
    RUN_TEST(test_init_sets_full_state);
    RUN_TEST(test_init_stores_config);

    printf("\nTouch events:\n");
    RUN_TEST(test_touch_when_full_no_change);
    RUN_TEST(test_touch_from_dimmed_returns_full);
    RUN_TEST(test_touch_from_off_returns_full);
    RUN_TEST(test_touch_updates_timestamp);

    printf("\nTimeout events:\n");
    RUN_TEST(test_timeout_before_dim_no_change);
    RUN_TEST(test_timeout_at_dim_transitions);
    RUN_TEST(test_timeout_at_off_transitions);
    RUN_TEST(test_timeout_in_off_no_change);
    RUN_TEST(test_timeout_dimmed_to_off);

    printf("\nTimeout calculation:\n");
    RUN_TEST(test_get_timeout_from_full);
    RUN_TEST(test_get_timeout_from_dimmed);
    RUN_TEST(test_get_timeout_from_off);
    RUN_TEST(test_get_timeout_expired_returns_zero);

    printf("\nBrightness getter:\n");
    RUN_TEST(test_get_brightness_full);
    RUN_TEST(test_get_brightness_dimmed);
    RUN_TEST(test_get_brightness_off);

    printf("\nState getter:\n");
    RUN_TEST(test_get_current_state);

    printf("\nEdge cases:\n");
    RUN_TEST(test_wraparound_handling);
    RUN_TEST(test_zero_idle_time);

    printf("\nFull lifecycle:\n");
    RUN_TEST(test_full_lifecycle);

    printf("\n========================================\n");
    printf("Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf(" (%d FAILED)", tests_failed);
    }
    printf("\n========================================\n\n");

    return tests_failed > 0 ? 1 : 0;
}
