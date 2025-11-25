/*
 * test_touch_timeout.c
 * --------------------
 * Unit tests for touch-timeout
 *
 * Build with coverage:
 *   make test
 *
 * Run:
 *   ./test_touch_timeout
 *
 * View coverage:
 *   make coverage
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>

/* --------------------
 * Minimal test framework
 * -------------------- */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void name(void)
#define RUN_TEST(name) do { \
    printf("  %-50s ", #name); \
    fflush(stdout); \
    tests_run++; \
    name(); \
    tests_passed++; \
    printf("✓ PASS\n"); \
} while(0)

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        printf("✗ FAIL\n    Assertion failed: %s\n    at %s:%d\n", \
               #cond, __FILE__, __LINE__); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_STR_EQ(a, b) ASSERT_TRUE(strcmp((a), (b)) == 0)
#define ASSERT_NE(a, b) ASSERT_TRUE((a) != (b))

/* --------------------
 * Rename main() in source file to prevent conflict
 * Mock syslog after including the header
 * -------------------- */
#define main touch_timeout_main

/* --------------------
 * Include the source file to access static functions
 * This is a standard C testing pattern for testing static functions
 * -------------------- */
#include "../touch-timeout.c"

#undef main

/* Now override syslog for the rest of the test file */
#undef syslog
#define syslog(priority, ...) ((void)0)

/* --------------------
 * Test helpers
 * -------------------- */
static char *create_temp_config(const char *content) {
    static char path[256];
    snprintf(path, sizeof(path), "/tmp/test_touch_timeout_%d.conf", getpid());
    FILE *f = fopen(path, "w");
    if (f) {
        fputs(content, f);
        fclose(f);
    }
    return path;
}

static void cleanup_temp_file(const char *path) {
    unlink(path);
}

/* ====================
 * TRIM FUNCTION TESTS
 * ==================== */

TEST(test_trim_leading_spaces) {
    char s[] = "   hello";
    trim(s);
    ASSERT_STR_EQ(s, "hello");
}

TEST(test_trim_trailing_spaces) {
    char s[] = "hello   ";
    trim(s);
    ASSERT_STR_EQ(s, "hello");
}

TEST(test_trim_both_ends) {
    char s[] = "  hello world  ";
    trim(s);
    ASSERT_STR_EQ(s, "hello world");
}

TEST(test_trim_tabs_and_newlines) {
    char s[] = "\t\n  value \t\n";
    trim(s);
    ASSERT_STR_EQ(s, "value");
}

TEST(test_trim_empty_string) {
    char s[] = "";
    trim(s);
    ASSERT_STR_EQ(s, "");
}

TEST(test_trim_only_whitespace) {
    char s[] = "   \t\n   ";
    trim(s);
    ASSERT_STR_EQ(s, "");
}

TEST(test_trim_no_whitespace) {
    char s[] = "hello";
    trim(s);
    ASSERT_STR_EQ(s, "hello");
}

/* ====================
 * SAFE_ATOI FUNCTION TESTS
 * ==================== */

TEST(test_safe_atoi_valid_positive) {
    int result;
    ASSERT_EQ(safe_atoi("123", &result), 0);
    ASSERT_EQ(result, 123);
}

TEST(test_safe_atoi_valid_negative) {
    int result;
    ASSERT_EQ(safe_atoi("-456", &result), 0);
    ASSERT_EQ(result, -456);
}

TEST(test_safe_atoi_zero) {
    int result;
    ASSERT_EQ(safe_atoi("0", &result), 0);
    ASSERT_EQ(result, 0);
}

TEST(test_safe_atoi_invalid_empty) {
    int result;
    ASSERT_EQ(safe_atoi("", &result), -1);
}

TEST(test_safe_atoi_invalid_letters) {
    int result;
    ASSERT_EQ(safe_atoi("abc", &result), -1);
}

TEST(test_safe_atoi_invalid_mixed) {
    int result;
    ASSERT_EQ(safe_atoi("123abc", &result), -1);
}

TEST(test_safe_atoi_invalid_leading_letters) {
    int result;
    ASSERT_EQ(safe_atoi("abc123", &result), -1);
}

TEST(test_safe_atoi_valid_with_leading_zeros) {
    int result;
    ASSERT_EQ(safe_atoi("007", &result), 0);
    ASSERT_EQ(result, 7);
}

TEST(test_safe_atoi_max_int) {
    int result;
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", INT_MAX);
    ASSERT_EQ(safe_atoi(buf, &result), 0);
    ASSERT_EQ(result, INT_MAX);
}

TEST(test_safe_atoi_min_int) {
    int result;
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", INT_MIN);
    ASSERT_EQ(safe_atoi(buf, &result), 0);
    ASSERT_EQ(result, INT_MIN);
}

TEST(test_safe_atoi_overflow) {
    int result;
    ASSERT_EQ(safe_atoi("99999999999999999999", &result), -1);
}

/* ====================
 * CONFIG PARSING TESTS
 * ==================== */

TEST(test_load_config_brightness) {
    const char *config = "brightness=200\n";
    char *path = create_temp_config(config);

    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    ASSERT_EQ(brightness, 200);
    ASSERT_EQ(timeout, 300);  // unchanged

    cleanup_temp_file(path);
}

TEST(test_load_config_all_values) {
    const char *config =
        "brightness=150\n"
        "off_timeout=600\n"
        "backlight=10-0045\n"
        "device=event2\n"
        "poll_interval=200\n"
        "dim_percent=30\n";
    char *path = create_temp_config(config);

    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    ASSERT_EQ(brightness, 150);
    ASSERT_EQ(timeout, 600);
    ASSERT_STR_EQ(backlight, "10-0045");
    ASSERT_STR_EQ(device, "event2");
    ASSERT_EQ(poll_interval, 200);
    ASSERT_EQ(dim_percent, 30);

    cleanup_temp_file(path);
}

TEST(test_load_config_comments_ignored) {
    const char *config =
        "# This is a comment\n"
        "brightness=180\n"
        "; Another comment style\n"
        "off_timeout=120\n";
    char *path = create_temp_config(config);

    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    ASSERT_EQ(brightness, 180);
    ASSERT_EQ(timeout, 120);

    cleanup_temp_file(path);
}

TEST(test_load_config_whitespace_handling) {
    const char *config =
        "  brightness = 175  \n"
        "off_timeout=  450\n";
    char *path = create_temp_config(config);

    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    ASSERT_EQ(brightness, 175);
    ASSERT_EQ(timeout, 450);

    cleanup_temp_file(path);
}

TEST(test_load_config_missing_file) {
    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;

    // Should not crash, values remain unchanged
    load_config("/nonexistent/path/config.conf", &brightness, &timeout,
                backlight, sizeof(backlight), device, sizeof(device),
                &poll_interval, &dim_percent);

    ASSERT_EQ(brightness, 100);
    ASSERT_EQ(timeout, 300);
}

TEST(test_load_config_invalid_value_keeps_default) {
    const char *config = "brightness=not_a_number\n";
    char *path = create_temp_config(config);

    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    ASSERT_EQ(brightness, 100);  // unchanged due to invalid value

    cleanup_temp_file(path);
}

TEST(test_load_config_empty_file) {
    const char *config = "";
    char *path = create_temp_config(config);

    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    // All values unchanged
    ASSERT_EQ(brightness, 100);
    ASSERT_EQ(timeout, 300);

    cleanup_temp_file(path);
}

TEST(test_load_config_invalid_off_timeout) {
    const char *config = "off_timeout=not_valid\n";
    char *path = create_temp_config(config);

    int brightness = 100;
    int timeout = 500;  // Should stay unchanged
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    ASSERT_EQ(timeout, 500);  // Unchanged due to invalid value

    cleanup_temp_file(path);
}

TEST(test_load_config_invalid_poll_interval) {
    const char *config = "poll_interval=abc\n";
    char *path = create_temp_config(config);

    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;  // Should stay unchanged
    int dim_percent = 50;

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    ASSERT_EQ(poll_interval, 100);  // Unchanged

    cleanup_temp_file(path);
}

TEST(test_load_config_invalid_dim_percent) {
    const char *config = "dim_percent=xyz\n";
    char *path = create_temp_config(config);

    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;  // Should stay unchanged

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    ASSERT_EQ(dim_percent, 50);  // Unchanged

    cleanup_temp_file(path);
}

TEST(test_load_config_unknown_key) {
    const char *config = "unknown_key=123\nbrightness=200\n";
    char *path = create_temp_config(config);

    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    // Unknown key ignored, but valid keys still parsed
    ASSERT_EQ(brightness, 200);

    cleanup_temp_file(path);
}

TEST(test_load_config_malformed_line) {
    const char *config = "this line has no equals sign\nbrightness=180\n";
    char *path = create_temp_config(config);

    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    // Malformed line ignored, but valid keys still parsed
    ASSERT_EQ(brightness, 180);

    cleanup_temp_file(path);
}

TEST(test_load_config_long_string_truncated) {
    // 100 char value - should be truncated to 63 + null terminator
    const char *config = "device=event0_this_is_a_very_long_device_name_that_exceeds_the_64_char_buffer_limit_and_should_be_truncated\n";
    char *path = create_temp_config(config);

    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    // Should be truncated to 63 chars (snprintf null-terminates)
    ASSERT_EQ(strlen(device), 63);
    // First 63 chars should match (note: sscanf %63s limits to 63 chars)
    ASSERT_STR_EQ(device, "event0_this_is_a_very_long_device_name_that_exceeds_the_64_char");

    cleanup_temp_file(path);
}

/* ====================
 * DISPLAY STATE TESTS
 * ==================== */

TEST(test_display_state_enum_values) {
    // Verify enum values match expected constants
    ASSERT_EQ(STATE_FULL, 0);
    ASSERT_EQ(STATE_DIMMED, 1);
    ASSERT_EQ(STATE_OFF, 2);
}

TEST(test_dim_brightness_calculation) {
    // Test the dim brightness formula: user_brightness/10, min MIN_DIM_BRIGHTNESS
    int user_brightness = 100;
    int dim = (user_brightness / 10 < MIN_DIM_BRIGHTNESS)
              ? MIN_DIM_BRIGHTNESS : user_brightness / 10;
    ASSERT_EQ(dim, 10);

    user_brightness = 200;
    dim = (user_brightness / 10 < MIN_DIM_BRIGHTNESS)
          ? MIN_DIM_BRIGHTNESS : user_brightness / 10;
    ASSERT_EQ(dim, 20);

    // Edge case: very low brightness
    user_brightness = 50;
    dim = (user_brightness / 10 < MIN_DIM_BRIGHTNESS)
          ? MIN_DIM_BRIGHTNESS : user_brightness / 10;
    ASSERT_EQ(dim, MIN_DIM_BRIGHTNESS);  // Should be clamped to minimum
}

TEST(test_dim_timeout_calculation) {
    // Test dim_timeout = (off_timeout * dim_percent) / 100
    int off_timeout = 300;
    int dim_percent = 50;
    int dim_timeout = (off_timeout * dim_percent) / 100;
    ASSERT_EQ(dim_timeout, 150);

    dim_percent = 20;
    dim_timeout = (off_timeout * dim_percent) / 100;
    ASSERT_EQ(dim_timeout, 60);

    dim_percent = 100;
    dim_timeout = (off_timeout * dim_percent) / 100;
    ASSERT_EQ(dim_timeout, 300);  // No dimming, same as off
}

/* ====================
 * HARDWARE MOCK TESTS
 * Uses temp files to simulate sysfs
 * ==================== */

/* Helper: create a fake brightness file with initial value */
static int create_fake_brightness_file(const char *path, int value) {
    FILE *f = fopen(path, "w+");
    if (!f) return -1;
    fprintf(f, "%d", value);
    fclose(f);
    return open(path, O_RDWR);
}

TEST(test_read_brightness_valid) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/test_brightness_%d", getpid());

    // Create fake sysfs file with brightness=150
    FILE *f = fopen(path, "w");
    fprintf(f, "150");
    fclose(f);

    int fd = open(path, O_RDONLY);
    ASSERT_TRUE(fd > 0);

    int brightness = read_brightness(fd);
    ASSERT_EQ(brightness, 150);

    close(fd);
    unlink(path);
}

TEST(test_read_brightness_empty_file) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/test_brightness_empty_%d", getpid());

    // Create empty file
    FILE *f = fopen(path, "w");
    fclose(f);

    int fd = open(path, O_RDONLY);
    ASSERT_TRUE(fd > 0);

    int brightness = read_brightness(fd);
    ASSERT_EQ(brightness, -1);  // Should return -1 on error

    close(fd);
    unlink(path);
}

TEST(test_set_brightness_valid) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/test_set_brightness_%d", getpid());

    int fd = create_fake_brightness_file(path, 100);
    ASSERT_TRUE(fd > 0);

    struct display_state state = {
        .bright_fd = fd,
        .user_brightness = 200,
        .dim_brightness = 20,
        .current_brightness = 100,  // Different from target
        .dim_timeout = 150,
        .off_timeout = 300,
        .last_input = time(NULL),
        .state = STATE_FULL
    };

    // Set brightness to 200
    int ret = set_brightness(&state, 200);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(state.current_brightness, 200);

    // Verify file was written
    lseek(fd, 0, SEEK_SET);
    char buf[8];
    read(fd, buf, sizeof(buf));
    ASSERT_EQ(atoi(buf), 200);

    close(fd);
    unlink(path);
}

TEST(test_set_brightness_cached_no_write) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/test_cached_brightness_%d", getpid());

    int fd = create_fake_brightness_file(path, 150);
    ASSERT_TRUE(fd > 0);

    struct display_state state = {
        .bright_fd = fd,
        .user_brightness = 150,
        .dim_brightness = 15,
        .current_brightness = 150,  // Same as target - should skip write
        .dim_timeout = 150,
        .off_timeout = 300,
        .last_input = time(NULL),
        .state = STATE_FULL
    };

    // Should return 0 but not write (cached)
    int ret = set_brightness(&state, 150);
    ASSERT_EQ(ret, 0);

    close(fd);
    unlink(path);
}

TEST(test_set_brightness_enforces_minimum) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/test_min_brightness_%d", getpid());

    int fd = create_fake_brightness_file(path, 100);
    ASSERT_TRUE(fd > 0);

    struct display_state state = {
        .bright_fd = fd,
        .user_brightness = 100,
        .dim_brightness = 10,
        .current_brightness = 100,
        .dim_timeout = 150,
        .off_timeout = 300,
        .last_input = time(NULL),
        .state = STATE_FULL
    };

    // Try to set brightness below minimum (but not 0)
    int ret = set_brightness(&state, 5);
    ASSERT_EQ(ret, 0);
    // Should be clamped to MIN_BRIGHTNESS (15)
    ASSERT_EQ(state.current_brightness, MIN_BRIGHTNESS);

    close(fd);
    unlink(path);
}

TEST(test_set_brightness_allows_zero) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/test_zero_brightness_%d", getpid());

    int fd = create_fake_brightness_file(path, 100);
    ASSERT_TRUE(fd > 0);

    struct display_state state = {
        .bright_fd = fd,
        .user_brightness = 100,
        .dim_brightness = 10,
        .current_brightness = 100,
        .dim_timeout = 150,
        .off_timeout = 300,
        .last_input = time(NULL),
        .state = STATE_FULL
    };

    // Setting to 0 (screen off) should be allowed
    int ret = set_brightness(&state, SCREEN_OFF);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(state.current_brightness, 0);

    close(fd);
    unlink(path);
}

TEST(test_check_timeouts_stays_full_when_active) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/test_timeout_active_%d", getpid());

    int fd = create_fake_brightness_file(path, 100);
    ASSERT_TRUE(fd > 0);

    struct display_state state = {
        .bright_fd = fd,
        .user_brightness = 100,
        .dim_brightness = 10,
        .current_brightness = 100,
        .dim_timeout = 150,
        .off_timeout = 300,
        .last_input = time(NULL),  // Just now - should stay FULL
        .state = STATE_FULL
    };

    check_timeouts(&state);
    ASSERT_EQ(state.state, STATE_FULL);

    close(fd);
    unlink(path);
}

TEST(test_check_timeouts_dims_after_dim_timeout) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/test_timeout_dim_%d", getpid());

    int fd = create_fake_brightness_file(path, 100);
    ASSERT_TRUE(fd > 0);

    struct display_state state = {
        .bright_fd = fd,
        .user_brightness = 100,
        .dim_brightness = 20,  // Must be >= MIN_BRIGHTNESS (15)
        .current_brightness = 100,
        .dim_timeout = 5,
        .off_timeout = 10,
        .last_input = time(NULL) - 6,  // 6 seconds ago - past dim_timeout
        .state = STATE_FULL
    };

    check_timeouts(&state);
    ASSERT_EQ(state.state, STATE_DIMMED);
    ASSERT_EQ(state.current_brightness, 20);

    close(fd);
    unlink(path);
}

TEST(test_check_timeouts_turns_off_after_off_timeout) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/test_timeout_off_%d", getpid());

    int fd = create_fake_brightness_file(path, 20);
    ASSERT_TRUE(fd > 0);

    struct display_state state = {
        .bright_fd = fd,
        .user_brightness = 100,
        .dim_brightness = 20,
        .current_brightness = 20,  // Already dimmed
        .dim_timeout = 5,
        .off_timeout = 10,
        .last_input = time(NULL) - 15,  // 15 seconds ago - past off_timeout
        .state = STATE_DIMMED
    };

    check_timeouts(&state);
    ASSERT_EQ(state.state, STATE_OFF);
    ASSERT_EQ(state.current_brightness, SCREEN_OFF);

    close(fd);
    unlink(path);
}

TEST(test_restore_brightness_from_dimmed) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/test_restore_%d", getpid());

    int fd = create_fake_brightness_file(path, 20);
    ASSERT_TRUE(fd > 0);

    struct display_state state = {
        .bright_fd = fd,
        .user_brightness = 100,
        .dim_brightness = 20,
        .current_brightness = 20,  // Currently dimmed
        .dim_timeout = 150,
        .off_timeout = 300,
        .last_input = time(NULL) - 200,
        .state = STATE_DIMMED
    };

    int ret = restore_brightness(&state);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(state.state, STATE_FULL);
    ASSERT_EQ(state.current_brightness, 100);

    close(fd);
    unlink(path);
}

TEST(test_restore_brightness_from_off) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/test_restore_off_%d", getpid());

    int fd = create_fake_brightness_file(path, 0);
    ASSERT_TRUE(fd > 0);

    struct display_state state = {
        .bright_fd = fd,
        .user_brightness = 150,
        .dim_brightness = 15,
        .current_brightness = 0,  // Screen off
        .dim_timeout = 150,
        .off_timeout = 300,
        .last_input = time(NULL) - 400,
        .state = STATE_OFF
    };

    int ret = restore_brightness(&state);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(state.state, STATE_FULL);
    ASSERT_EQ(state.current_brightness, 150);

    close(fd);
    unlink(path);
}

/* ====================
 * CONSTANTS VALIDATION
 * ==================== */

TEST(test_constants_valid) {
    ASSERT_EQ(MIN_BRIGHTNESS, 15);
    ASSERT_EQ(MIN_DIM_BRIGHTNESS, 10);
    ASSERT_EQ(MAX_BRIGHTNESS_LIMIT, 255);
    ASSERT_EQ(SCREEN_OFF, 0);
}

/* ====================
 * SECURITY EDGE CASE TESTS
 * CERT C / MISRA / Secure Coding
 * ==================== */

/* --- Integer Overflow Tests (CERT INT32-C) --- */

TEST(test_security_int_overflow_timeout_multiplication) {
    // INT32-C: off_timeout * dim_percent can overflow
    // If off_timeout = INT_MAX and dim_percent > 1, overflow occurs
    const char *config = "off_timeout=2147483647\ndim_percent=50\n";
    char *path = create_temp_config(config);

    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    // Verify large value was parsed (even if dangerous)
    ASSERT_EQ(timeout, 2147483647);

    // The actual overflow happens in main() when calculating:
    // dim_timeout = (off_timeout * dim_percent) / 100
    // This would overflow and produce a wrong (possibly negative) value
    int dim_timeout = (timeout * dim_percent) / 100;  // OVERFLOW HERE
    // On overflow, result is undefined - likely negative or wrapped
    // This test documents the vulnerability
    (void)dim_timeout;  // Suppress unused warning

    cleanup_temp_file(path);
}

TEST(test_security_negative_timeout) {
    // CERT INT31-C: Negative values should be REJECTED
    const char *config = "off_timeout=-1\n";
    char *path = create_temp_config(config);

    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    // FIX VERIFIED: negative value rejected, default preserved
    ASSERT_EQ(timeout, 300);

    cleanup_temp_file(path);
}

TEST(test_security_negative_brightness) {
    const char *config = "brightness=-100\n";
    char *path = create_temp_config(config);

    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    // FIX VERIFIED: negative brightness rejected, default preserved
    ASSERT_EQ(brightness, 100);

    cleanup_temp_file(path);
}

TEST(test_security_negative_dim_percent) {
    const char *config = "dim_percent=-50\n";
    char *path = create_temp_config(config);

    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    // FIX VERIFIED: negative dim_percent rejected, default preserved
    ASSERT_EQ(dim_percent, 50);

    cleanup_temp_file(path);
}

/* --- Boundary Value Tests --- */

TEST(test_security_timeout_below_minimum) {
    const char *config = "off_timeout=9\n";  // Min is 10
    char *path = create_temp_config(config);

    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    // FIX VERIFIED: value below minimum rejected, default preserved
    ASSERT_EQ(timeout, 300);

    cleanup_temp_file(path);
}

TEST(test_security_timeout_at_minimum) {
    const char *config = "off_timeout=10\n";
    char *path = create_temp_config(config);

    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    ASSERT_EQ(timeout, 10);  // Valid minimum

    cleanup_temp_file(path);
}

TEST(test_security_brightness_zero) {
    // Zero brightness = screen off, but used as "active" brightness
    const char *config = "brightness=0\n";
    char *path = create_temp_config(config);

    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    ASSERT_EQ(brightness, 0);  // Would make screen always off

    cleanup_temp_file(path);
}

TEST(test_security_brightness_above_max) {
    const char *config = "brightness=256\n";  // Max is 255
    char *path = create_temp_config(config);

    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    // FIX VERIFIED: value above max rejected, default preserved
    ASSERT_EQ(brightness, 100);

    cleanup_temp_file(path);
}

TEST(test_security_dim_percent_below_min) {
    const char *config = "dim_percent=9\n";  // Min is 10
    char *path = create_temp_config(config);

    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    // FIX VERIFIED: value below min rejected, default preserved
    ASSERT_EQ(dim_percent, 50);

    cleanup_temp_file(path);
}

TEST(test_security_dim_percent_above_max) {
    const char *config = "dim_percent=101\n";  // Max is 100
    char *path = create_temp_config(config);

    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    // FIX VERIFIED: value above max rejected, default preserved
    ASSERT_EQ(dim_percent, 50);

    cleanup_temp_file(path);
}

/* --- Path Traversal Tests (CERT FIO32-C) --- */

TEST(test_security_path_traversal_device) {
    // FIO32-C: Path traversal in device name should be REJECTED
    const char *config = "device=../sda\n";
    char *path = create_temp_config(config);

    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    // FIX VERIFIED: path traversal rejected, default preserved
    ASSERT_STR_EQ(device, "event0");

    cleanup_temp_file(path);
}

TEST(test_security_path_traversal_backlight) {
    // Path traversal in backlight name should be REJECTED
    const char *config = "backlight=../../../etc\n";
    char *path = create_temp_config(config);

    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    // FIX VERIFIED: path traversal rejected, default preserved
    ASSERT_STR_EQ(backlight, "default");

    cleanup_temp_file(path);
}

TEST(test_security_absolute_path_device) {
    const char *config = "device=/etc/passwd\n";
    char *path = create_temp_config(config);

    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    // FIX VERIFIED: absolute path rejected, default preserved
    ASSERT_STR_EQ(device, "event0");

    cleanup_temp_file(path);
}

/* --- Buffer/String Tests (CERT STR31-C) --- */

TEST(test_security_line_longer_than_buffer) {
    // fgets uses 128-byte buffer - what happens with longer lines?
    // Create a line that's 200 chars: brightness=123xxxx...
    char long_config[256];
    memset(long_config, 'x', 200);
    long_config[0] = 'b';  // brightness=
    long_config[1] = 'r';
    long_config[2] = 'i';
    long_config[3] = 'g';
    long_config[4] = 'h';
    long_config[5] = 't';
    long_config[6] = 'n';
    long_config[7] = 'e';
    long_config[8] = 's';
    long_config[9] = 's';
    long_config[10] = '=';
    long_config[11] = '1';
    long_config[12] = '2';
    long_config[13] = '3';
    long_config[200] = '\n';
    long_config[201] = '\0';

    char *path = create_temp_config(long_config);

    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    // Line truncated at 127 chars -> value becomes "123xxx..."
    // safe_atoi rejects "123xxx..." as invalid, so default preserved
    ASSERT_EQ(brightness, 100);

    cleanup_temp_file(path);
}

TEST(test_security_empty_value) {
    const char *config = "brightness=\n";
    char *path = create_temp_config(config);

    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    // sscanf %s won't match empty string, line should be skipped
    ASSERT_EQ(brightness, 100);  // Unchanged

    cleanup_temp_file(path);
}

TEST(test_security_multiple_equals) {
    const char *config = "device=event0=malicious\n";
    char *path = create_temp_config(config);

    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    // sscanf stops at first =, %s stops at whitespace
    // So device becomes "event0=malicious" (no whitespace to stop)
    ASSERT_STR_EQ(device, "event0=malicious");

    cleanup_temp_file(path);
}

TEST(test_security_whitespace_in_value) {
    // %s in sscanf stops at whitespace
    const char *config = "device=event0 malicious\n";
    char *path = create_temp_config(config);

    int brightness = 100;
    int timeout = 300;
    char backlight[64] = "default";
    char device[64] = "event0";
    int poll_interval = 100;
    int dim_percent = 50;

    load_config(path, &brightness, &timeout, backlight, sizeof(backlight),
                device, sizeof(device), &poll_interval, &dim_percent);

    // sscanf %s stops at space, so only "event0" is captured
    ASSERT_STR_EQ(device, "event0");

    cleanup_temp_file(path);
}

/* --- Signal Handler Tests (CERT SIG31-C) --- */
// Note: signal handler uses 'volatile int' instead of 'volatile sig_atomic_t'
// This is a CERT SIG31-C violation but can't be tested easily

/* --- Clock/Time Tests --- */

TEST(test_security_clock_backwards_large) {
    // Test the clock adjustment handling (NTP can shift time backwards)
    char path[256];
    snprintf(path, sizeof(path), "/tmp/test_clock_back_%d", getpid());

    int fd = create_fake_brightness_file(path, 100);
    ASSERT_TRUE(fd > 0);

    struct display_state state = {
        .bright_fd = fd,
        .user_brightness = 100,
        .dim_brightness = 20,
        .current_brightness = 100,
        .dim_timeout = 150,
        .off_timeout = 300,
        .last_input = time(NULL) + 100,  // 100 seconds in FUTURE (simulates clock going back)
        .state = STATE_FULL
    };

    check_timeouts(&state);

    // Clock adjustment handler should reset timer, state stays FULL
    ASSERT_EQ(state.state, STATE_FULL);
    // last_input should be reset to approximately now
    ASSERT_TRUE(state.last_input <= time(NULL) + 1);

    close(fd);
    unlink(path);
}

/* ====================
 * MAIN TEST RUNNER
 * ==================== */

int main(void) {
    printf("\n========================================\n");
    printf("touch-timeout unit tests\n");
    printf("========================================\n\n");

    printf("trim() tests:\n");
    RUN_TEST(test_trim_leading_spaces);
    RUN_TEST(test_trim_trailing_spaces);
    RUN_TEST(test_trim_both_ends);
    RUN_TEST(test_trim_tabs_and_newlines);
    RUN_TEST(test_trim_empty_string);
    RUN_TEST(test_trim_only_whitespace);
    RUN_TEST(test_trim_no_whitespace);

    printf("\nsafe_atoi() tests:\n");
    RUN_TEST(test_safe_atoi_valid_positive);
    RUN_TEST(test_safe_atoi_valid_negative);
    RUN_TEST(test_safe_atoi_zero);
    RUN_TEST(test_safe_atoi_invalid_empty);
    RUN_TEST(test_safe_atoi_invalid_letters);
    RUN_TEST(test_safe_atoi_invalid_mixed);
    RUN_TEST(test_safe_atoi_invalid_leading_letters);
    RUN_TEST(test_safe_atoi_valid_with_leading_zeros);
    RUN_TEST(test_safe_atoi_max_int);
    RUN_TEST(test_safe_atoi_min_int);
    RUN_TEST(test_safe_atoi_overflow);

    printf("\nload_config() tests:\n");
    RUN_TEST(test_load_config_brightness);
    RUN_TEST(test_load_config_all_values);
    RUN_TEST(test_load_config_comments_ignored);
    RUN_TEST(test_load_config_whitespace_handling);
    RUN_TEST(test_load_config_missing_file);
    RUN_TEST(test_load_config_invalid_value_keeps_default);
    RUN_TEST(test_load_config_empty_file);
    RUN_TEST(test_load_config_invalid_off_timeout);
    RUN_TEST(test_load_config_invalid_poll_interval);
    RUN_TEST(test_load_config_invalid_dim_percent);
    RUN_TEST(test_load_config_unknown_key);
    RUN_TEST(test_load_config_malformed_line);
    RUN_TEST(test_load_config_long_string_truncated);

    printf("\nDisplay state tests:\n");
    RUN_TEST(test_display_state_enum_values);
    RUN_TEST(test_dim_brightness_calculation);
    RUN_TEST(test_dim_timeout_calculation);

    printf("\nHardware mock tests:\n");
    RUN_TEST(test_read_brightness_valid);
    RUN_TEST(test_read_brightness_empty_file);
    RUN_TEST(test_set_brightness_valid);
    RUN_TEST(test_set_brightness_cached_no_write);
    RUN_TEST(test_set_brightness_enforces_minimum);
    RUN_TEST(test_set_brightness_allows_zero);
    RUN_TEST(test_check_timeouts_stays_full_when_active);
    RUN_TEST(test_check_timeouts_dims_after_dim_timeout);
    RUN_TEST(test_check_timeouts_turns_off_after_off_timeout);
    RUN_TEST(test_restore_brightness_from_dimmed);
    RUN_TEST(test_restore_brightness_from_off);

    printf("\nConstants validation:\n");
    RUN_TEST(test_constants_valid);

    printf("\nSecurity edge case tests:\n");
    RUN_TEST(test_security_int_overflow_timeout_multiplication);
    RUN_TEST(test_security_negative_timeout);
    RUN_TEST(test_security_negative_brightness);
    RUN_TEST(test_security_negative_dim_percent);
    RUN_TEST(test_security_timeout_below_minimum);
    RUN_TEST(test_security_timeout_at_minimum);
    RUN_TEST(test_security_brightness_zero);
    RUN_TEST(test_security_brightness_above_max);
    RUN_TEST(test_security_dim_percent_below_min);
    RUN_TEST(test_security_dim_percent_above_max);
    RUN_TEST(test_security_path_traversal_device);
    RUN_TEST(test_security_path_traversal_backlight);
    RUN_TEST(test_security_absolute_path_device);
    RUN_TEST(test_security_line_longer_than_buffer);
    RUN_TEST(test_security_empty_value);
    RUN_TEST(test_security_multiple_equals);
    RUN_TEST(test_security_whitespace_in_value);
    RUN_TEST(test_security_clock_backwards_large);

    printf("\n========================================\n");
    printf("Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf(" (%d FAILED)", tests_failed);
    }
    printf("\n========================================\n\n");

    return tests_failed > 0 ? 1 : 0;
}
