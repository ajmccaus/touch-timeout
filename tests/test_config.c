/*
 * test_config.c
 * -------------
 * Unit tests for configuration module
 *
 * Tests:
 * - Default initialization
 * - File parsing (key=value)
 * - Validation (ranges, overflow protection)
 * - Security (path traversal, integer overflow)
 * - Error handling
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <limits.h>

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
#define ASSERT_STR_EQ(a, b) ASSERT_TRUE(strcmp((a), (b)) == 0)

/* Test helpers */
static char *create_temp_config(const char *content) {
    static char path[256];
    snprintf(path, sizeof(path), "/tmp/test_config_%d.conf", getpid());
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

/* ==================== INITIALIZATION TESTS ==================== */

TEST(test_config_init_defaults) {
    config_t *config = config_init();
    ASSERT_TRUE(config != NULL);
    ASSERT_EQ(config->brightness, CONFIG_DEFAULT_BRIGHTNESS);
    ASSERT_EQ(config->off_timeout, CONFIG_DEFAULT_OFF_TIMEOUT);
    ASSERT_EQ(config->dim_percent, CONFIG_DEFAULT_DIM_PERCENT);
    ASSERT_STR_EQ(config->backlight, CONFIG_DEFAULT_BACKLIGHT);
    ASSERT_STR_EQ(config->device, CONFIG_DEFAULT_DEVICE);
}

/* ==================== PARSING TESTS ==================== */

TEST(test_config_load_brightness) {
    config_t *config = config_init();
    const char *content = "brightness=200\n";
    char *path = create_temp_config(content);

    ASSERT_EQ(config_load(config, path), 0);
    ASSERT_EQ(config->brightness, 200);

    cleanup_temp_file(path);
}

TEST(test_config_load_all_values) {
    config_t *config = config_init();
    const char *content =
        "brightness=150\n"
        "off_timeout=600\n"
        "dim_percent=30\n"
        "backlight=10-0045\n"
        "device=event2\n";
    char *path = create_temp_config(content);

    ASSERT_EQ(config_load(config, path), 0);
    ASSERT_EQ(config->brightness, 150);
    ASSERT_EQ(config->off_timeout, 600);
    ASSERT_EQ(config->dim_percent, 30);
    ASSERT_STR_EQ(config->backlight, "10-0045");
    ASSERT_STR_EQ(config->device, "event2");

    cleanup_temp_file(path);
}

TEST(test_config_load_missing_file) {
    config_t *config = config_init();
    int original_brightness = config->brightness;

    /* Should succeed with defaults when file missing */
    ASSERT_EQ(config_load(config, "/nonexistent/file.conf"), 0);
    ASSERT_EQ(config->brightness, original_brightness);
}

TEST(test_config_load_comments_ignored) {
    config_t *config = config_init();
    const char *content =
        "# This is a comment\n"
        "brightness=180\n"
        "; Another comment\n"
        "off_timeout=120\n";
    char *path = create_temp_config(content);

    ASSERT_EQ(config_load(config, path), 0);
    ASSERT_EQ(config->brightness, 180);
    ASSERT_EQ(config->off_timeout, 120);

    cleanup_temp_file(path);
}

TEST(test_config_load_whitespace_handling) {
    config_t *config = config_init();
    const char *content =
        "  brightness = 175  \n"
        "off_timeout=  450\n";
    char *path = create_temp_config(content);

    ASSERT_EQ(config_load(config, path), 0);
    ASSERT_EQ(config->brightness, 175);
    ASSERT_EQ(config->off_timeout, 450);

    cleanup_temp_file(path);
}

/* ==================== VALIDATION TESTS ==================== */

TEST(test_config_validate_brightness_clamping) {
    config_t *config = config_init();
    config->brightness = 300;  /* Above hardware max */

    ASSERT_EQ(config_validate(config, 200), 0);
    ASSERT_EQ(config->brightness, 200);  /* Clamped to max */
}

TEST(test_config_validate_minimum_brightness) {
    config_t *config = config_init();
    config->brightness = 10;  /* Below minimum */

    ASSERT_EQ(config_validate(config, 255), 0);
    ASSERT_EQ(config->brightness, CONFIG_MIN_BRIGHTNESS);
}

TEST(test_config_validate_dim_timeout_calculation) {
    /* Parameterized test using DoE/Taguchi approach for comprehensive coverage */
    struct {
        int off_timeout;
        int dim_percent;
        int expected_dim_timeout;
        const char *description;
    } test_cases[] = {
        /* Boundary: minimum values trigger zero division */
        {10,    1,   5,     "Min values: zero from integer division (10*1/100=0)"},
        /* Edge: non-zero result but below minimum threshold */
        {100,   1,   5,     "Below minimum: 100*1/100=1 -> clamps to 5"},
        /* Edge: exactly at minimum threshold */
        {500,   1,   5,     "At minimum: 500*1/100=5"},
        /* Typical: normal mid-range calculation */
        {300,   50,  150,   "Normal: 300*50/100=150"},
        /* Boundary: maximum values */
        {86400, 100, 86400, "Max values: 86400*100/100=86400"},
    };

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        config_t *config = config_init();
        config->off_timeout = test_cases[i].off_timeout;
        config->dim_percent = test_cases[i].dim_percent;

        int result = config_validate(config, 255);
        if (result != 0) {
            printf("\n    FAIL: %s\n", test_cases[i].description);
            printf("    config_validate() returned %d (expected 0)\n", result);
            ASSERT_TRUE(0);  /* Force failure with context */
        }

        if (config->dim_timeout != test_cases[i].expected_dim_timeout) {
            printf("\n    FAIL: %s\n", test_cases[i].description);
            printf("    Expected dim_timeout=%d, got %d\n",
                   test_cases[i].expected_dim_timeout, config->dim_timeout);
            ASSERT_TRUE(0);  /* Force failure with context */
        }
    }
}

TEST(test_config_validate_dim_brightness_calculation) {
    config_t *config = config_init();
    config->brightness = 200;

    ASSERT_EQ(config_validate(config, 255), 0);
    ASSERT_EQ(config->dim_brightness, 20);  /* 200/10 */
}

TEST(test_config_validate_dim_brightness_minimum) {
    config_t *config = config_init();
    config->brightness = 50;  /* Would result in dim=5 */

    ASSERT_EQ(config_validate(config, 255), 0);
    ASSERT_EQ(config->dim_brightness, CONFIG_MIN_DIM_BRIGHTNESS);
}

/* ==================== RANGE VALIDATION TESTS ==================== */

TEST(test_config_invalid_brightness_negative) {
    config_t *config = config_init();
    const char *content = "brightness=-100\n";
    char *path = create_temp_config(content);

    int original = config->brightness;
    config_load(config, path);
    ASSERT_EQ(config->brightness, original);  /* Unchanged */

    cleanup_temp_file(path);
}

TEST(test_config_invalid_brightness_above_max) {
    config_t *config = config_init();
    const char *content = "brightness=256\n";
    char *path = create_temp_config(content);

    int original = config->brightness;
    config_load(config, path);
    ASSERT_EQ(config->brightness, original);  /* Unchanged */

    cleanup_temp_file(path);
}

TEST(test_config_invalid_timeout_below_minimum) {
    config_t *config = config_init();
    const char *content = "off_timeout=9\n";
    char *path = create_temp_config(content);

    int original = config->off_timeout;
    config_load(config, path);
    ASSERT_EQ(config->off_timeout, original);  /* Unchanged */

    cleanup_temp_file(path);
}

TEST(test_config_valid_timeout_at_minimum) {
    config_t *config = config_init();
    const char *content = "off_timeout=10\n";
    char *path = create_temp_config(content);

    config_load(config, path);
    ASSERT_EQ(config->off_timeout, 10);

    cleanup_temp_file(path);
}

TEST(test_config_invalid_dim_percent_out_of_range) {
    config_t *config = config_init();
    const char *content = "dim_percent=101\n";
    char *path = create_temp_config(content);

    int original = config->dim_percent;
    config_load(config, path);
    ASSERT_EQ(config->dim_percent, original);  /* Unchanged */

    cleanup_temp_file(path);
}

/* ==================== CONFIG FALLBACK TESTS ==================== */

TEST(test_config_fallback_brightness_too_high) {
    config_t *config = config_init();
    const char *content = "brightness=999\n";
    char *path = create_temp_config(content);

    /* Should keep default brightness */
    ASSERT_EQ(config_load(config, path), 0);
    ASSERT_EQ(config->brightness, CONFIG_DEFAULT_BRIGHTNESS);

    cleanup_temp_file(path);
}

TEST(test_config_fallback_multiple_invalid_values) {
    config_t *config = config_init();
    const char *content =
        "brightness=999\n"      /* Too high */
        "off_timeout=5\n"       /* Too low */
        "dim_percent=150\n";    /* Too high */
    char *path = create_temp_config(content);

    /* All should fall back to defaults */
    ASSERT_EQ(config_load(config, path), 0);
    ASSERT_EQ(config->brightness, CONFIG_DEFAULT_BRIGHTNESS);
    ASSERT_EQ(config->off_timeout, CONFIG_DEFAULT_OFF_TIMEOUT);
    ASSERT_EQ(config->dim_percent, CONFIG_DEFAULT_DIM_PERCENT);

    cleanup_temp_file(path);
}

TEST(test_config_fallback_mixed_valid_invalid) {
    config_t *config = config_init();
    const char *content =
        "brightness=150\n"      /* Valid */
        "off_timeout=5\n"       /* Invalid - too low */
        "dim_percent=20\n";     /* Valid */
    char *path = create_temp_config(content);

    /* Valid values applied, invalid falls back */
    ASSERT_EQ(config_load(config, path), 0);
    ASSERT_EQ(config->brightness, 150);                     /* Valid value applied */
    ASSERT_EQ(config->off_timeout, CONFIG_DEFAULT_OFF_TIMEOUT);  /* Fallback to default */
    ASSERT_EQ(config->dim_percent, 20);                     /* Valid value applied */

    cleanup_temp_file(path);
}

TEST(test_config_fallback_invalid_integer_format) {
    config_t *config = config_init();
    const char *content = "brightness=abc123\n";
    char *path = create_temp_config(content);

    /* Should keep default when parsing fails */
    ASSERT_EQ(config_load(config, path), 0);
    ASSERT_EQ(config->brightness, CONFIG_DEFAULT_BRIGHTNESS);

    cleanup_temp_file(path);
}

TEST(test_config_fallback_path_traversal) {
    config_t *config = config_init();
    const char *content = "device=../../../etc/passwd\n";
    char *path = create_temp_config(content);

    char original[64];
    strcpy(original, config->device);

    /* Should keep default when path validation fails */
    ASSERT_EQ(config_load(config, path), 0);
    ASSERT_STR_EQ(config->device, original);

    cleanup_temp_file(path);
}

/* ==================== SECURITY TESTS ==================== */

TEST(test_config_path_traversal_device) {
    config_t *config = config_init();
    const char *content = "device=../sda\n";
    char *path = create_temp_config(content);

    char original[64];
    strcpy(original, config->device);
    config_load(config, path);
    ASSERT_STR_EQ(config->device, original);  /* Unchanged */

    cleanup_temp_file(path);
}

TEST(test_config_path_traversal_backlight) {
    config_t *config = config_init();
    const char *content = "backlight=../../etc\n";
    char *path = create_temp_config(content);

    char original[64];
    strcpy(original, config->backlight);
    config_load(config, path);
    ASSERT_STR_EQ(config->backlight, original);  /* Unchanged */

    cleanup_temp_file(path);
}

TEST(test_config_absolute_path_rejected) {
    config_t *config = config_init();
    const char *content = "device=/etc/passwd\n";
    char *path = create_temp_config(content);

    char original[64];
    strcpy(original, config->device);
    config_load(config, path);
    ASSERT_STR_EQ(config->device, original);  /* Unchanged */

    cleanup_temp_file(path);
}

TEST(test_config_integer_overflow_prevention) {
    config_t *config = config_init();
    config->off_timeout = 100000;  /* Large value */
    config->dim_percent = 50;

    /* Should handle safely without overflow */
    ASSERT_EQ(config_validate(config, 255), 0);
    ASSERT_TRUE(config->dim_timeout > 0);
    ASSERT_TRUE(config->dim_timeout < config->off_timeout);
}

/* ==================== SAFE_ATOI TESTS ==================== */

TEST(test_safe_atoi_valid_positive) {
    int result;
    ASSERT_EQ(config_safe_atoi("123", &result), 0);
    ASSERT_EQ(result, 123);
}

TEST(test_safe_atoi_valid_negative) {
    int result;
    ASSERT_EQ(config_safe_atoi("-456", &result), 0);
    ASSERT_EQ(result, -456);
}

TEST(test_safe_atoi_zero) {
    int result;
    ASSERT_EQ(config_safe_atoi("0", &result), 0);
    ASSERT_EQ(result, 0);
}

TEST(test_safe_atoi_invalid_empty) {
    int result;
    ASSERT_EQ(config_safe_atoi("", &result), -1);
}

TEST(test_safe_atoi_invalid_letters) {
    int result;
    ASSERT_EQ(config_safe_atoi("abc", &result), -1);
}

TEST(test_safe_atoi_invalid_mixed) {
    int result;
    ASSERT_EQ(config_safe_atoi("123abc", &result), -1);
}

TEST(test_safe_atoi_overflow) {
    int result;
    ASSERT_EQ(config_safe_atoi("99999999999999999999", &result), -1);
}

TEST(test_safe_atoi_max_int) {
    int result;
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", INT_MAX);
    ASSERT_EQ(config_safe_atoi(buf, &result), 0);
    ASSERT_EQ(result, INT_MAX);
}

/* ==================== MAIN TEST RUNNER ==================== */

int main(void) {
    printf("\n========================================\n");
    printf("Configuration Module Unit Tests\n");
    printf("========================================\n\n");

    printf("Initialization tests:\n");
    RUN_TEST(test_config_init_defaults);

    printf("\nParsing tests:\n");
    RUN_TEST(test_config_load_brightness);
    RUN_TEST(test_config_load_all_values);
    RUN_TEST(test_config_load_missing_file);
    RUN_TEST(test_config_load_comments_ignored);
    RUN_TEST(test_config_load_whitespace_handling);

    printf("\nValidation tests:\n");
    RUN_TEST(test_config_validate_brightness_clamping);
    RUN_TEST(test_config_validate_minimum_brightness);
    RUN_TEST(test_config_validate_dim_timeout_calculation);
    RUN_TEST(test_config_validate_dim_brightness_calculation);
    RUN_TEST(test_config_validate_dim_brightness_minimum);

    printf("\nRange validation tests:\n");
    RUN_TEST(test_config_invalid_brightness_negative);
    RUN_TEST(test_config_invalid_brightness_above_max);
    RUN_TEST(test_config_invalid_timeout_below_minimum);
    RUN_TEST(test_config_valid_timeout_at_minimum);
    RUN_TEST(test_config_invalid_dim_percent_out_of_range);

    printf("\nConfig fallback tests:\n");
    RUN_TEST(test_config_fallback_brightness_too_high);
    RUN_TEST(test_config_fallback_multiple_invalid_values);
    RUN_TEST(test_config_fallback_mixed_valid_invalid);
    RUN_TEST(test_config_fallback_invalid_integer_format);
    RUN_TEST(test_config_fallback_path_traversal);

    printf("\nSecurity tests:\n");
    RUN_TEST(test_config_path_traversal_device);
    RUN_TEST(test_config_path_traversal_backlight);
    RUN_TEST(test_config_absolute_path_rejected);
    RUN_TEST(test_config_integer_overflow_prevention);

    printf("\nsafe_atoi tests:\n");
    RUN_TEST(test_safe_atoi_valid_positive);
    RUN_TEST(test_safe_atoi_valid_negative);
    RUN_TEST(test_safe_atoi_zero);
    RUN_TEST(test_safe_atoi_invalid_empty);
    RUN_TEST(test_safe_atoi_invalid_letters);
    RUN_TEST(test_safe_atoi_invalid_mixed);
    RUN_TEST(test_safe_atoi_overflow);
    RUN_TEST(test_safe_atoi_max_int);

    printf("\n========================================\n");
    printf("Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf(" (%d FAILED)", tests_failed);
    }
    printf("\n========================================\n\n");

    return tests_failed > 0 ? 1 : 0;
}
