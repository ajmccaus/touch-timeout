/*
 * config.h
 * --------
 * Configuration management for touch-timeout daemon
 *
 * Provides table-driven configuration with validation
 * All parameters validated against CERT C security guidelines
 */

#ifndef TOUCH_TIMEOUT_CONFIG_H
#define TOUCH_TIMEOUT_CONFIG_H

#include <stddef.h>
#include <stdbool.h>

/* Configuration defaults - compile time constants */
#define CONFIG_DEFAULT_BRIGHTNESS       150
#define CONFIG_DEFAULT_OFF_TIMEOUT      300
#define CONFIG_DEFAULT_BACKLIGHT        "rpi_backlight"
#define CONFIG_DEFAULT_DEVICE           "event0"
#define CONFIG_DEFAULT_DIM_PERCENT      10

/* Configuration limits - hardware and safety constraints */
#define CONFIG_MIN_BRIGHTNESS           15      /* Avoid flicker */
#define CONFIG_MIN_DIM_BRIGHTNESS       10      /* Absolute minimum */
#define CONFIG_MAX_BRIGHTNESS           255     /* 8-bit PWM */
#define CONFIG_MIN_OFF_TIMEOUT          10      /* Minimum 10 seconds */
#define CONFIG_MAX_OFF_TIMEOUT          86400   /* Maximum 24 hours */
#define CONFIG_MIN_DIM_PERCENT          1       /* 1% minimum */
#define CONFIG_MAX_DIM_PERCENT          100     /* 100% = no dimming */
#define CONFIG_MIN_DIM_TIMEOUT          5       /* Minimum 5 seconds dim time */
#define CONFIG_PATH_MAX_LEN             256

/* Configuration structure */
typedef struct {
    int brightness;             /* User brightness (CONFIG_MIN_BRIGHTNESS - CONFIG_MAX_BRIGHTNESS) */
    int off_timeout;            /* Timeout in seconds (CONFIG_MIN_OFF_TIMEOUT - CONFIG_MAX_OFF_TIMEOUT) */
    int dim_timeout;            /* Calculated: (off_timeout * dim_percent) / 100 */
    int dim_brightness;         /* Calculated: max(brightness/10, CONFIG_MIN_DIM_BRIGHTNESS) */
    int dim_percent;            /* Dim at N% of off_timeout (CONFIG_MIN_DIM_PERCENT - CONFIG_MAX_DIM_PERCENT) */
    char backlight[64];         /* Backlight device name */
    char device[64];            /* Input device name */
} config_t;

/*
 * Initialize configuration with compile-time defaults
 *
 * Returns: pointer to static config structure
 */
config_t *config_init(void);

/*
 * Load configuration from file
 *
 * Parses key=value pairs from config file
 * Invalid values are logged and defaults preserved
 * Missing file is not an error - uses defaults
 *
 * Parameters:
 *   config: Configuration structure to populate
 *   path:   Path to configuration file (typically /etc/touch-timeout.conf)
 *
 * Returns: 0 on success, -1 on critical error
 */
int config_load(config_t *config, const char *path);

/*
 * Validate and finalize configuration
 *
 * Performs cross-field validation and calculates derived values:
 * - dim_timeout = (off_timeout * dim_percent) / 100
 * - dim_brightness = max(brightness / 10, CONFIG_MIN_DIM_BRIGHTNESS)
 * - Validates dim_timeout < off_timeout
 *
 * Parameters:
 *   config:         Configuration structure to validate
 *   max_brightness: Maximum brightness from hardware (used for clamping)
 *
 * Returns: 0 if valid, -1 if invalid (critical errors logged)
 */
int config_validate(config_t *config, int max_brightness);

/*
 * Safe string to integer conversion with range validation
 *
 * CERT INT31-C compliant: validates for overflow, underflow, invalid chars
 *
 * Parameters:
 *   str:    Input string
 *   result: Output integer
 *
 * Returns: 0 on success, -1 on error
 */
int config_safe_atoi(const char *str, int *result);

#endif /* TOUCH_TIMEOUT_CONFIG_H */
