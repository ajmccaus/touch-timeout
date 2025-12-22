/*
 * test_state.c - Unit tests for state machine and utility functions
 *
 * TEST PHILOSOPHY:
 *   - No mocking: Pure functions tested with mock timestamps
 *   - No external framework: Minimal custom test macros (RUN_TEST, ASSERT_EQ)
 *   - Direct inclusion: #include "../src/main.c" with UNIT_TEST guard
 *
 * COVERAGE:
 *   - State machine transitions (FULL → DIMMED → OFF → FULL)
 *   - Timeout calculations and wraparound handling
 *   - Brightness calculations and clamping
 *   - Input parsing and validation (boundary cases, security)
 *   - Edge cases: zero timeouts, wraparound, extreme values
 *
 * RUNNING TESTS:
 *   make test      - Run tests with summary
 *   make coverage  - Generate coverage report
 *   ./test_state   - Run directly (shows all test names)
 *
 * SEE ALSO:
 *   - src/state.c - State machine implementation under test
 *   - src/main.c - Utility functions under test (parse_int, etc.)
 */

#include "../src/main.c"
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

/* ==================== CALCULATION TESTS ==================== */

TEST(test_dim_brightness_normal) {
    /* 100 brightness at 50% = 50 */
    ASSERT_EQ(calculate_dim_brightness(100, 50), 50);
}

TEST(test_dim_brightness_clamps_to_min) {
    /* 100 brightness at 5% = 5, but MIN_DIM_BRIGHTNESS is 10 */
    ASSERT_EQ(calculate_dim_brightness(100, 5), MIN_DIM_BRIGHTNESS);
}

TEST(test_dim_brightness_low_input) {
    /* 20 brightness at 10% = 2, clamped to MIN_DIM_BRIGHTNESS */
    ASSERT_EQ(calculate_dim_brightness(20, 10), MIN_DIM_BRIGHTNESS);
}

TEST(test_dim_brightness_full_percent) {
    /* 150 brightness at 100% = 150 */
    ASSERT_EQ(calculate_dim_brightness(150, 100), 150);
}

TEST(test_timeouts_normal) {
    uint32_t dim_sec, off_sec;
    calculate_timeouts(300, 10, &dim_sec, &off_sec);
    /* 300s timeout at 10% = dim at 30s, off at 300s */
    ASSERT_EQ(dim_sec, 30);
    ASSERT_EQ(off_sec, 300);
}

TEST(test_timeouts_clamps_dim_to_min) {
    uint32_t dim_sec, off_sec;
    calculate_timeouts(10, 1, &dim_sec, &off_sec);
    /* 10s timeout at 1% would be 0.1s, clamped to MIN_DIM_TIMEOUT_SEC */
    ASSERT_EQ(dim_sec, MIN_DIM_TIMEOUT_SEC);
    ASSERT_EQ(off_sec, 10);
}

TEST(test_timeouts_dim_exceeds_off) {
    uint32_t dim_sec, off_sec;
    calculate_timeouts(10, 100, &dim_sec, &off_sec);
    /* 10s timeout at 100% would make dim == off, halved to 5s */
    ASSERT_TRUE(dim_sec < off_sec);
    ASSERT_EQ(dim_sec, 5);
    ASSERT_EQ(off_sec, 10);
}

TEST(test_timeouts_extreme_small) {
    uint32_t dim_sec, off_sec;
    calculate_timeouts(2, 100, &dim_sec, &off_sec);
    /* 2s timeout at 100%: dim would be 2, halved to 1 */
    ASSERT_EQ(dim_sec, MIN_DIM_TIMEOUT_SEC);
    ASSERT_EQ(off_sec, 2);
}

TEST(test_timeouts_minimum_possible) {
    uint32_t dim_sec, off_sec;
    calculate_timeouts(1, 100, &dim_sec, &off_sec);
    /* 1s timeout at 100%: dim would be 1, halved to 0, clamped to MIN */
    ASSERT_EQ(dim_sec, MIN_DIM_TIMEOUT_SEC);
    ASSERT_EQ(off_sec, 1);
}

/* ==================== INPUT PARSING TESTS ==================== */

TEST(test_parse_int_valid) {
    int val;
    ASSERT_EQ(parse_int("123", &val), 0);
    ASSERT_EQ(val, 123);
}

TEST(test_parse_int_zero) {
    int val;
    ASSERT_EQ(parse_int("0", &val), 0);
    ASSERT_EQ(val, 0);
}

TEST(test_parse_int_negative) {
    int val;
    ASSERT_EQ(parse_int("-50", &val), 0);
    ASSERT_EQ(val, -50);
}

TEST(test_parse_int_rejects_empty) {
    int val;
    ASSERT_EQ(parse_int("", &val), -1);
}

TEST(test_parse_int_rejects_trailing_chars) {
    int val;
    ASSERT_EQ(parse_int("123abc", &val), -1);
}

TEST(test_parse_int_rejects_leading_chars) {
    int val;
    ASSERT_EQ(parse_int("abc123", &val), -1);
}

TEST(test_parse_int_rejects_float) {
    int val;
    ASSERT_EQ(parse_int("3.14", &val), -1);
}

TEST(test_parse_int_rejects_overflow) {
    int val;
    ASSERT_EQ(parse_int("99999999999999", &val), -1);
}

TEST(test_parse_int_rejects_underflow) {
    int val;
    ASSERT_EQ(parse_int("-99999999999999", &val), -1);
}

/* ==================== DEVICE NAME VALIDATION TESTS ==================== */

TEST(test_validate_device_name_valid) {
    ASSERT_TRUE(validate_device_name("event0"));
    ASSERT_TRUE(validate_device_name("rpi_backlight"));
    ASSERT_TRUE(validate_device_name("a"));
}

TEST(test_validate_device_name_rejects_empty) {
    ASSERT_TRUE(!validate_device_name(""));
}

TEST(test_validate_device_name_rejects_slash) {
    ASSERT_TRUE(!validate_device_name("foo/bar"));
    ASSERT_TRUE(!validate_device_name("/etc/passwd"));
}

TEST(test_validate_device_name_rejects_path_traversal) {
    ASSERT_TRUE(!validate_device_name(".."));
    ASSERT_TRUE(!validate_device_name("../etc/passwd"));
    ASSERT_TRUE(!validate_device_name("foo/../bar"));
}

TEST(test_validate_device_name_rejects_dotdot_in_name) {
    /* ".." substring anywhere is rejected */
    ASSERT_TRUE(!validate_device_name("foo..bar"));
    ASSERT_TRUE(!validate_device_name("..hidden"));
}

TEST(test_validate_device_name_allows_single_dot) {
    /* Single dots are fine - only ".." is dangerous */
    ASSERT_TRUE(validate_device_name(".hidden"));
    ASSERT_TRUE(validate_device_name("file.txt"));
}

TEST(test_validate_device_name_rejects_too_long) {
    /* MAX_DEVICE_NAME_LEN is 64, so 64+ chars should fail */
    char long_name[MAX_DEVICE_NAME_LEN + 1];
    memset(long_name, 'a', MAX_DEVICE_NAME_LEN);
    long_name[MAX_DEVICE_NAME_LEN] = '\0';
    ASSERT_TRUE(!validate_device_name(long_name));
}

TEST(test_validate_device_name_accepts_max_minus_one) {
    /* MAX_DEVICE_NAME_LEN - 1 chars should succeed */
    char ok_name[MAX_DEVICE_NAME_LEN];
    memset(ok_name, 'a', MAX_DEVICE_NAME_LEN - 1);
    ok_name[MAX_DEVICE_NAME_LEN - 1] = '\0';
    ASSERT_TRUE(validate_device_name(ok_name));
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

    printf("\nDim brightness calculation:\n");
    RUN_TEST(test_dim_brightness_normal);
    RUN_TEST(test_dim_brightness_clamps_to_min);
    RUN_TEST(test_dim_brightness_low_input);
    RUN_TEST(test_dim_brightness_full_percent);

    printf("\nTimeout calculation:\n");
    RUN_TEST(test_timeouts_normal);
    RUN_TEST(test_timeouts_clamps_dim_to_min);
    RUN_TEST(test_timeouts_dim_exceeds_off);
    RUN_TEST(test_timeouts_extreme_small);
    RUN_TEST(test_timeouts_minimum_possible);

    printf("\nInput parsing:\n");
    RUN_TEST(test_parse_int_valid);
    RUN_TEST(test_parse_int_zero);
    RUN_TEST(test_parse_int_negative);
    RUN_TEST(test_parse_int_rejects_empty);
    RUN_TEST(test_parse_int_rejects_trailing_chars);
    RUN_TEST(test_parse_int_rejects_leading_chars);
    RUN_TEST(test_parse_int_rejects_float);
    RUN_TEST(test_parse_int_rejects_overflow);
    RUN_TEST(test_parse_int_rejects_underflow);

    printf("\nDevice name validation:\n");
    RUN_TEST(test_validate_device_name_valid);
    RUN_TEST(test_validate_device_name_rejects_empty);
    RUN_TEST(test_validate_device_name_rejects_slash);
    RUN_TEST(test_validate_device_name_rejects_path_traversal);
    RUN_TEST(test_validate_device_name_rejects_dotdot_in_name);
    RUN_TEST(test_validate_device_name_allows_single_dot);
    RUN_TEST(test_validate_device_name_rejects_too_long);
    RUN_TEST(test_validate_device_name_accepts_max_minus_one);

    printf("\n========================================\n");
    printf("Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf(" (%d FAILED)", tests_failed);
    }
    printf("\n========================================\n\n");

    return tests_failed > 0 ? 1 : 0;
}
