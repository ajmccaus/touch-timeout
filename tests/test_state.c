/*
 * test_state.c - Unit tests for pure state machine
 *
 * Tests are simple - no mocking needed, just pass timestamps in seconds
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

/* Test config: dim at 5s, off at 10s, full=100, dim=10 */
#define BRIGHT_FULL 100
#define BRIGHT_DIM  10
#define DIM_SEC     5
#define OFF_SEC     10

/* ==================== INITIALIZATION TESTS ==================== */

TEST(test_init_sets_full_state) {
    state_s st;
    state_init(&st, BRIGHT_FULL, BRIGHT_DIM, DIM_SEC, OFF_SEC);

    ASSERT_EQ(st.state, STATE_FULL);
    ASSERT_EQ(st.last_touch_sec, 0);
}

TEST(test_init_stores_config) {
    state_s st;
    state_init(&st, BRIGHT_FULL, BRIGHT_DIM, DIM_SEC, OFF_SEC);

    ASSERT_EQ(st.brightness_full, BRIGHT_FULL);
    ASSERT_EQ(st.brightness_dim, BRIGHT_DIM);
    ASSERT_EQ(st.dim_timeout_sec, DIM_SEC);
    ASSERT_EQ(st.off_timeout_sec, OFF_SEC);
}

/* ==================== TOUCH EVENT TESTS ==================== */

TEST(test_touch_when_full_no_change) {
    state_s st;
    state_init(&st, BRIGHT_FULL, BRIGHT_DIM, DIM_SEC, OFF_SEC);
    state_touch(&st, 1);  /* Initial touch */

    int brightness = state_touch(&st, 2);

    ASSERT_EQ(brightness, -1);  /* No change */
    ASSERT_EQ(st.state, STATE_FULL);
    ASSERT_EQ(st.last_touch_sec, 2);
}

TEST(test_touch_from_dimmed_returns_full) {
    state_s st;
    state_init(&st, BRIGHT_FULL, BRIGHT_DIM, DIM_SEC, OFF_SEC);
    state_touch(&st, 0);
    state_timeout(&st, 6);  /* Transition to DIMMED */

    int brightness = state_touch(&st, 7);

    ASSERT_EQ(brightness, BRIGHT_FULL);
    ASSERT_EQ(st.state, STATE_FULL);
}

TEST(test_touch_from_off_returns_full) {
    state_s st;
    state_init(&st, BRIGHT_FULL, BRIGHT_DIM, DIM_SEC, OFF_SEC);
    state_touch(&st, 0);
    state_timeout(&st, 6);   /* FULL -> DIMMED */
    state_timeout(&st, 11);  /* DIMMED -> OFF */

    int brightness = state_touch(&st, 12);

    ASSERT_EQ(brightness, BRIGHT_FULL);
    ASSERT_EQ(st.state, STATE_FULL);
}

TEST(test_touch_updates_timestamp) {
    state_s st;
    state_init(&st, BRIGHT_FULL, BRIGHT_DIM, DIM_SEC, OFF_SEC);

    state_touch(&st, 5);
    ASSERT_EQ(st.last_touch_sec, 5);

    state_touch(&st, 8);
    ASSERT_EQ(st.last_touch_sec, 8);
}

/* ==================== TIMEOUT EVENT TESTS ==================== */

TEST(test_timeout_before_dim_no_change) {
    state_s st;
    state_init(&st, BRIGHT_FULL, BRIGHT_DIM, DIM_SEC, OFF_SEC);
    state_touch(&st, 0);

    int brightness = state_timeout(&st, 4);  /* 4s, dim at 5s */

    ASSERT_EQ(brightness, -1);
    ASSERT_EQ(st.state, STATE_FULL);
}

TEST(test_timeout_at_dim_transitions) {
    state_s st;
    state_init(&st, BRIGHT_FULL, BRIGHT_DIM, DIM_SEC, OFF_SEC);
    state_touch(&st, 0);

    int brightness = state_timeout(&st, 5);  /* Exactly at dim */

    ASSERT_EQ(brightness, BRIGHT_DIM);
    ASSERT_EQ(st.state, STATE_DIMMED);
}

TEST(test_timeout_at_off_transitions) {
    state_s st;
    state_init(&st, BRIGHT_FULL, BRIGHT_DIM, DIM_SEC, OFF_SEC);
    state_touch(&st, 0);
    state_timeout(&st, 5);  /* FULL -> DIMMED */

    int brightness = state_timeout(&st, 10);  /* Exactly at off */

    ASSERT_EQ(brightness, 0);
    ASSERT_EQ(st.state, STATE_OFF);
}

TEST(test_timeout_in_off_no_change) {
    state_s st;
    state_init(&st, BRIGHT_FULL, BRIGHT_DIM, DIM_SEC, OFF_SEC);
    state_touch(&st, 0);
    state_timeout(&st, 6);
    state_timeout(&st, 11);  /* Now OFF */

    int brightness = state_timeout(&st, 20);

    ASSERT_EQ(brightness, -1);
    ASSERT_EQ(st.state, STATE_OFF);
}

TEST(test_timeout_dimmed_to_off) {
    state_s st;
    state_init(&st, BRIGHT_FULL, BRIGHT_DIM, DIM_SEC, OFF_SEC);
    state_touch(&st, 0);
    state_timeout(&st, 6);  /* Now DIMMED */

    /* Wait more time */
    int brightness = state_timeout(&st, 11);

    ASSERT_EQ(brightness, 0);
    ASSERT_EQ(st.state, STATE_OFF);
}

/* ==================== TIMEOUT CALCULATION TESTS ==================== */

TEST(test_get_timeout_from_full) {
    state_s st;
    state_init(&st, BRIGHT_FULL, BRIGHT_DIM, DIM_SEC, OFF_SEC);
    state_touch(&st, 1);

    int timeout = state_get_timeout_sec(&st, 2);  /* 1s idle */

    ASSERT_EQ(timeout, 4);  /* 5 - 1 = 4 */
}

TEST(test_get_timeout_from_dimmed) {
    state_s st;
    state_init(&st, BRIGHT_FULL, BRIGHT_DIM, DIM_SEC, OFF_SEC);
    state_touch(&st, 0);
    state_timeout(&st, 6);  /* DIMMED */

    int timeout = state_get_timeout_sec(&st, 7);  /* 7s idle */

    ASSERT_EQ(timeout, 3);  /* 10 - 7 = 3 */
}

TEST(test_get_timeout_from_off) {
    state_s st;
    state_init(&st, BRIGHT_FULL, BRIGHT_DIM, DIM_SEC, OFF_SEC);
    state_touch(&st, 0);
    state_timeout(&st, 6);
    state_timeout(&st, 11);  /* OFF */

    int timeout = state_get_timeout_sec(&st, 15);

    ASSERT_EQ(timeout, -1);  /* No timeout in OFF */
}

TEST(test_get_timeout_expired_returns_zero) {
    state_s st;
    state_init(&st, BRIGHT_FULL, BRIGHT_DIM, DIM_SEC, OFF_SEC);
    state_touch(&st, 0);

    int timeout = state_get_timeout_sec(&st, 6);  /* Past dim */

    ASSERT_EQ(timeout, 0);
}

/* ==================== BRIGHTNESS GETTER TESTS ==================== */

TEST(test_get_brightness_full) {
    state_s st;
    state_init(&st, BRIGHT_FULL, BRIGHT_DIM, DIM_SEC, OFF_SEC);

    ASSERT_EQ(state_get_brightness(&st), BRIGHT_FULL);
}

TEST(test_get_brightness_dimmed) {
    state_s st;
    state_init(&st, BRIGHT_FULL, BRIGHT_DIM, DIM_SEC, OFF_SEC);
    state_touch(&st, 0);
    state_timeout(&st, 6);

    ASSERT_EQ(state_get_brightness(&st), BRIGHT_DIM);
}

TEST(test_get_brightness_off) {
    state_s st;
    state_init(&st, BRIGHT_FULL, BRIGHT_DIM, DIM_SEC, OFF_SEC);
    state_touch(&st, 0);
    state_timeout(&st, 6);
    state_timeout(&st, 11);

    ASSERT_EQ(state_get_brightness(&st), 0);
}

/* ==================== STATE GETTER TESTS ==================== */

TEST(test_get_current_state) {
    state_s st;
    state_init(&st, BRIGHT_FULL, BRIGHT_DIM, DIM_SEC, OFF_SEC);
    state_touch(&st, 0);

    ASSERT_EQ(state_get_current(&st), STATE_FULL);

    state_timeout(&st, 6);
    ASSERT_EQ(state_get_current(&st), STATE_DIMMED);

    state_timeout(&st, 11);
    ASSERT_EQ(state_get_current(&st), STATE_OFF);
}

/* ==================== EDGE CASE TESTS ==================== */

TEST(test_wraparound_handling) {
    state_s st;
    state_init(&st, BRIGHT_FULL, BRIGHT_DIM, DIM_SEC, OFF_SEC);

    /* Touch near uint32_t max */
    uint32_t near_max = 0xFFFFFFFF - 1;
    state_touch(&st, near_max);

    /* 2s later wraps around */
    uint32_t wrapped = near_max + 2;  /* Wraps to ~0 */
    int timeout = state_get_timeout_sec(&st, wrapped);

    /* idle = wrapped - near_max = 2, should be 3 remaining */
    ASSERT_EQ(timeout, 3);
}

TEST(test_zero_idle_time) {
    state_s st;
    state_init(&st, BRIGHT_FULL, BRIGHT_DIM, DIM_SEC, OFF_SEC);
    state_touch(&st, 5);

    int timeout = state_get_timeout_sec(&st, 5);  /* Same time */

    ASSERT_EQ(timeout, 5);  /* Full dim timeout remaining */
}

/* ==================== FULL SEQUENCE TEST ==================== */

TEST(test_full_lifecycle) {
    state_s st;
    state_init(&st, BRIGHT_FULL, BRIGHT_DIM, DIM_SEC, OFF_SEC);

    /* Initial touch */
    int b = state_touch(&st, 0);
    ASSERT_EQ(b, -1);  /* Already FULL */
    ASSERT_EQ(st.state, STATE_FULL);

    /* Wait 3s - still FULL */
    b = state_timeout(&st, 3);
    ASSERT_EQ(b, -1);
    ASSERT_EQ(st.state, STATE_FULL);

    /* Wait 6s total - transition to DIMMED */
    b = state_timeout(&st, 6);
    ASSERT_EQ(b, BRIGHT_DIM);
    ASSERT_EQ(st.state, STATE_DIMMED);

    /* Wait 11s total - transition to OFF */
    b = state_timeout(&st, 11);
    ASSERT_EQ(b, 0);
    ASSERT_EQ(st.state, STATE_OFF);

    /* Touch - restore to FULL */
    b = state_touch(&st, 12);
    ASSERT_EQ(b, BRIGHT_FULL);
    ASSERT_EQ(st.state, STATE_FULL);

    /* Verify timer reset */
    int timeout = state_get_timeout_sec(&st, 13);
    ASSERT_EQ(timeout, 4);  /* 5 - 1 */
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
