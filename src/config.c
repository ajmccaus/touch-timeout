/*
 * config.c
 * --------
 * Configuration management implementation
 *
 * Table-driven configuration with validation
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <syslog.h>

/* Static configuration instance */
static config_t g_config;

/* Configuration parameter descriptor for table-driven parsing */
typedef struct {
    const char *key;
    enum { TYPE_INT, TYPE_STRING } type;
    size_t offset;          /* Offset within config_t */
    int min_value;          /* For TYPE_INT only */
    int max_value;          /* For TYPE_INT only */
    size_t max_length;      /* For TYPE_STRING only */
    bool validate_path;     /* For TYPE_STRING: check for path traversal */
} config_param_t;

/* Forward declarations */
static void trim(char *s);
static int validate_path_component(const char *path);

/* Table-driven configuration parameters */
static const config_param_t config_params[] = {
    {
        .key = "brightness",
        .type = TYPE_INT,
        .offset = offsetof(config_t, brightness),
        .min_value = 0,
        .max_value = CONFIG_MAX_BRIGHTNESS,
    },
    {
        .key = "off_timeout",
        .type = TYPE_INT,
        .offset = offsetof(config_t, off_timeout),
        .min_value = CONFIG_MIN_OFF_TIMEOUT,
        .max_value = CONFIG_MAX_OFF_TIMEOUT,
    },
    {
        .key = "dim_percent",
        .type = TYPE_INT,
        .offset = offsetof(config_t, dim_percent),
        .min_value = CONFIG_MIN_DIM_PERCENT,
        .max_value = CONFIG_MAX_DIM_PERCENT,
    },
    {
        .key = "backlight",
        .type = TYPE_STRING,
        .offset = offsetof(config_t, backlight),
        .max_length = sizeof(((config_t *)0)->backlight),
        .validate_path = true,
    },
    {
        .key = "device",
        .type = TYPE_STRING,
        .offset = offsetof(config_t, device),
        .max_length = sizeof(((config_t *)0)->device),
        .validate_path = true,
    },
};

static const size_t config_params_count = sizeof(config_params) / sizeof(config_params[0]);

/*
 * Trim whitespace from both ends of string (in-place)
 */
static void trim(char *s) {
    if (s == NULL) return;

    /* Trim leading whitespace */
    char *p = s;
    while (isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);

    /* Trim trailing whitespace */
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
}

/*
 * Validate path component (CERT FIO32-C)
 * Returns 0 if safe, -1 if contains traversal sequences
 */
static int validate_path_component(const char *path) {
    if (path == NULL || path[0] == '\0')
        return -1;

    /* Reject absolute paths */
    if (path[0] == '/')
        return -1;

    /* Reject path traversal */
    if (strstr(path, "..") != NULL)
        return -1;

    /* Reject path separators (should be just a filename) */
    if (strchr(path, '/') != NULL)
        return -1;

    return 0;
}

/*
 * Safe string to integer conversion (CERT INT31-C compliant)
 */
int config_safe_atoi(const char *str, int *result) {
    if (str == NULL || result == NULL)
        return -1;

    char *endptr;
    errno = 0;
    long val = strtol(str, &endptr, 10);

    /* Check for conversion errors */
    if (endptr == str || *endptr != '\0' || errno == ERANGE)
        return -1;

    /* Check for overflow */
    if (val < INT_MIN || val > INT_MAX)
        return -1;

    *result = (int)val;
    return 0;
}

/*
 * Parse single configuration line using table-driven approach
 */
static int parse_config_line(config_t *config, const char *key, const char *value, int line_num) {
    /* Find matching parameter in table */
    for (size_t i = 0; i < config_params_count; i++) {
        const config_param_t *param = &config_params[i];

        if (strcmp(key, param->key) != 0)
            continue;

        /* Found matching parameter */
        void *field_ptr = (char *)config + param->offset;

        if (param->type == TYPE_INT) {
            int tmp;
            if (config_safe_atoi(value, &tmp) != 0) {
                syslog(LOG_WARNING, "Invalid value for %s: '%s' at line %d, keeping default %d",
                       param->key, value, line_num, *(int *)field_ptr);
                return 0;  /* Fallback to default - not a fatal error */
            }

            /* Validate range - fallback to default if out of range */
            if (tmp < param->min_value || tmp > param->max_value) {
                syslog(LOG_WARNING, "%s=%d out of range (%d-%d) at line %d, keeping default %d",
                       param->key, tmp, param->min_value, param->max_value, line_num, *(int *)field_ptr);
                return 0;  /* Fallback to default - not a fatal error */
            }

            *(int *)field_ptr = tmp;
            return 0;

        } else if (param->type == TYPE_STRING) {
            /* Validate path if required */
            if (param->validate_path && validate_path_component(value) != 0) {
                syslog(LOG_WARNING, "Invalid path for %s: '%s' (no / or ..) at line %d, keeping default '%s'",
                       param->key, value, line_num, (char *)field_ptr);
                return 0;  /* Fallback to default - not a fatal error */
            }

            /* Copy string with bounds checking */
            snprintf((char *)field_ptr, param->max_length, "%s", value);
            return 0;
        }
    }

    /* Unknown parameter - log warning but continue (graceful degradation) */
    syslog(LOG_WARNING, "Unknown config key '%s' at line %d (ignored)", key, line_num);
    return 0;
}

/*
 * Initialize configuration with defaults
 */
config_t *config_init(void) {
    g_config.brightness = CONFIG_DEFAULT_BRIGHTNESS;
    g_config.off_timeout = CONFIG_DEFAULT_OFF_TIMEOUT;
    g_config.dim_percent = CONFIG_DEFAULT_DIM_PERCENT;
    g_config.dim_timeout = 0;       /* Calculated later */
    g_config.dim_brightness = 0;    /* Calculated later */

    snprintf(g_config.backlight, sizeof(g_config.backlight), "%s", CONFIG_DEFAULT_BACKLIGHT);
    snprintf(g_config.device, sizeof(g_config.device), "%s", CONFIG_DEFAULT_DEVICE);

    return &g_config;
}

/*
 * Load configuration from file
 */
int config_load(config_t *config, const char *path) {
    if (config == NULL || path == NULL)
        return -1;

    FILE *f = fopen(path, "r");
    if (f == NULL) {
        /* Missing config file is not an error - use defaults */
        return 0;
    }

    char line[128];
    int line_num = 0;

    while (fgets(line, sizeof(line), f)) {
        line_num++;
        trim(line);

        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == ';' || line[0] == '\0')
            continue;

        /* Parse key=value */
        char key[64], value[64];
        if (sscanf(line, "%63[^=]=%63s", key, value) != 2) {
            syslog(LOG_WARNING, "Malformed config line %d: %s", line_num, line);
            continue;
        }

        trim(key);
        trim(value);

        parse_config_line(config, key, value, line_num);
    }

    fclose(f);
    return 0;
}

/*
 * Validate and finalize configuration
 */
int config_validate(config_t *config, int max_brightness) {
    if (config == NULL)
        return -1;

    /* Clamp brightness to hardware maximum */
    if (config->brightness > max_brightness) {
        syslog(LOG_WARNING, "Brightness %d exceeds hardware max %d, clamping",
               config->brightness, max_brightness);
        config->brightness = max_brightness;
    }

    /* Enforce minimum brightness */
    if (config->brightness < CONFIG_MIN_BRIGHTNESS) {
        syslog(LOG_WARNING, "Brightness %d below minimum %d, using minimum",
               config->brightness, CONFIG_MIN_BRIGHTNESS);
        config->brightness = CONFIG_MIN_BRIGHTNESS;
    }

    /* Calculate dim_timeout (CERT INT32-C: use long to prevent overflow) */
    long dim_timeout_long = ((long)config->off_timeout * config->dim_percent) / 100;

    /* Handle zero from integer division (graceful fallback) */
    if (dim_timeout_long <= 0) {
        syslog(LOG_WARNING, "Calculated dim_timeout is 0 (off_timeout=%d, dim_percent=%d%%), using minimum %ds",
               config->off_timeout, config->dim_percent, CONFIG_MIN_DIM_TIMEOUT);
        dim_timeout_long = CONFIG_MIN_DIM_TIMEOUT;
    }

    /* Check for overflow (should be impossible with validated inputs - indicates corruption/bug) */
    if (dim_timeout_long > config->off_timeout) {
        syslog(LOG_ERR, "Invalid dim_timeout calculation (overflow) - possible memory corruption");
        return -1;
    }

    config->dim_timeout = (int)dim_timeout_long;

    /* Enforce minimum dim time to prevent near-zero dim periods */
    if (config->dim_timeout < CONFIG_MIN_DIM_TIMEOUT) {
        syslog(LOG_WARNING, "Calculated dim_timeout %ds below minimum %ds, using minimum",
               config->dim_timeout, CONFIG_MIN_DIM_TIMEOUT);
        config->dim_timeout = CONFIG_MIN_DIM_TIMEOUT;
    }

    /* Calculate dim_brightness */
    int calculated_dim = config->brightness / 10;
    config->dim_brightness = (calculated_dim < CONFIG_MIN_DIM_BRIGHTNESS)
                             ? CONFIG_MIN_DIM_BRIGHTNESS : calculated_dim;

    /* Validate derived values */
    if (config->dim_timeout >= config->off_timeout && config->dim_percent < 100) {
        syslog(LOG_ERR, "dim_timeout (%d) >= off_timeout (%d)",
               config->dim_timeout, config->off_timeout);
        return -1;
    }

    return 0;
}
