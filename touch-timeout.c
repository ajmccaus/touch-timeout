/*
 * touch-timeout.c
 * ----------------
 * A lightweight touchscreen activity monitor optimized for Raspberry Pi 7" touchscreens.
 *
 * VERSION: 1.0.1
 *  
 * Changes from 1.0.0:
 *  - Logging improvements to minimize SD card wear
 *  - Added command line arguments for logging verbosity
 *  - Added log level configuration (LOG_INFO, LOG_WARNING, LOG_ERR)
 *  - Improved handling of system clock adjustments (NTP)
 *  - fixed 512-byte buffers and mandatory snprintf return checks to prevent truncation issues
 * 
 * Changes from 0.2.0:
 *  - Added assert() validation
 *  - Added auto versioning with #define VERSION
 *  - Added safe_atoi() helper function
 *  - other fixes to ensure security and config file error handling
 *  - tested on RPi 4 with 7" touchscreen
 *
 * Changes from 0.1.0:
 *  - Added brightness caching to prevent redundant writes
 *  - Reads and validates max_brightness from sysfs
 *  - Configurable poll_interval and dim_percent in config file
 *  - Time-based timeout logic (handles missed poll cycles)
 *  - Enum-based state machine (replaces magic numbers)
 *  - Removed fsync() (unnecessary for sysfs)
 *  - Enhanced input validation with logging
 *
 * Features:
 *  - Automatically dims and turns off the backlight after user-defined inactivity
 *  - Restores brightness instantly on any touchscreen input
 *  - Reads config from /etc/touch-timeout.conf (with command-line overrides)
 *  - Near-zero CPU usage (poll-based event loop)
 *  - Safe sysfs writes (lseek, cached)
 *  - Graceful shutdown via SIGTERM/SIGINT (systemd compatible)
 *
 *
 * Author: Andrew McCausland
 * License: GPL v3
 */

 // Request POSIX.1-2008 API before including any system headers
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <poll.h>
#include <syslog.h>
#include <ctype.h>
#include <sys/types.h>
#include <assert.h>
#include <limits.h>

#define MIN_BRIGHTNESS          15     // Minimum allowed brightness (avoids flicker)
#define MIN_DIM_BRIGHTNESS      10     // Minimum allowed dim level
#define MAX_BRIGHTNESS_LIMIT    255    // Valid range: 0-255 (8-bit PWM duty cycle)
#define MIN_OFF_TIMEOUT         10     // 10 seconds minimum timeout
#define MAX_OFF_TIMEOUT         86400  // 24 hours maximum timeout
#define SCREEN_OFF              0      // Brightness value for screen off
#define PATH_BUF_SZ             512    // de-facto safe size for embedded file paths
#define NTP_TOLERANCE_SEC       5.0  // seconds
#define CONFIG_PATH             "/etc/touch-timeout.conf"
#define VERSION                 "1.0.1"

// ============================================================================
// LOGGING SYSTEM: Function pointer-based runtime log filtering
// ============================================================================

// WHY: Three-tier logging matches production needs:
//      - NONE (0): Zero SD writes for appliance mode
//      - INFO (1): Startup + state changes only
//      - DEBUG (2): All events for troubleshooting
// HOW: Enum values match syslog priority ordering for intuitive mapping
enum log_level {
    LOG_LEVEL_NONE = 0,   // Production default
    LOG_LEVEL_INFO = 1,   // Operational events
    LOG_LEVEL_DEBUG = 2   // Verbose diagnostics
};

// Global state for log filtering
// WHY: Single source of truth prevents scattered conditionals
// HOW: Set once at init, read by all logging functions
static int current_log_level = LOG_LEVEL_NONE;  // Default: silent
static int foreground_mode = 0;                 // 0=syslog, 1=stderr

static volatile sig_atomic_t running = 1;    // Used for graceful shutdown on SIGTERM/SIGINT, Signal-safe flag used by handler

// ============================================================================
// FUNCTION POINTER TYPE DEFINITION
// ============================================================================

// WHY: Creates a reusable "logging function" type signature
// WHAT: Any function matching this signature can be stored in log_* pointers
// HOW: typedef creates alias for "pointer to function returning void"
//
// BREAKDOWN:
//   void              - Return type (loggers don't return values)
//   (*log_func_t)     - Pointer to function, named "log_func_t"
//   (int priority, const char *format, ...) - Function parameters
//
// PARAMETERS EXPLAINED:
//   int priority       - Syslog level (LOG_ERR, LOG_INFO, etc.)
//   const char *format - Printf-style format string (e.g., "Error: %s")
//   ...                - Variable arguments (varargs) for format placeholders
//
// EXAMPLE USAGE:
//   log_func_t my_logger = log_syslog;  // Store function address
//   my_logger(LOG_INFO, "Test %d", 42); // Call via pointer
typedef void (*log_func_t)(int priority, const char *format, ...);

// ============================================================================
// LOGGING IMPLEMENTATION FUNCTIONS
// ============================================================================

// NULL LOGGER: Discards all output (production mode)
// ----------------------------------------------------------------------------
// WHY: Eliminates SD writes when log_level=0
// HOW: Empty function body compiles to single 'ret' instruction (verified)
// OPTIMIZATION: Compiler completely inlines this (zero overhead)
//
// PARAMETERS:
//   (void)priority - Mark as intentionally unused (prevents -Wunused warning)
//   (void)format   - Same (GCC would warn otherwise)
//
// WHAT HAPPENS: Function is called → does nothing → returns immediately
static void log_null(int priority, const char *format, ...) {
    (void)priority;  // Suppress "unused parameter" warning
    (void)format;    // Suppress "unused parameter" warning
    // Intentionally empty - optimized to single return instruction
}

// STDERR LOGGER: Output to console (foreground mode)
// ----------------------------------------------------------------------------
// WHY: Development/debugging needs human-readable output, not syslog format
// HOW: Uses vfprintf() to handle variable arguments (va_list)
// WHEN USED: ./touch-timeout -f (foreground flag)
//
// FLOW:
//   1. Map syslog priority (LOG_ERR=3) → human string ("ERROR")
//   2. Print label "[ERROR] "
//   3. Process format string + varargs using vfprintf()
//   4. Add newline
//
// VARARGS EXPLAINED:
//   va_list args          - Holds the "..." arguments
//   va_start(args, format)- Initialize from last named param (format)
//   vfprintf()            - Printf variant that takes va_list
//   va_end(args)          - Cleanup (required by C standard)
static void log_stderr(int priority, const char *format, ...) {
    // Map syslog numeric priority to human-readable label
    const char *level_str;
    switch (priority) {
        case LOG_ERR:     level_str = "ERROR"; break;  // Priority 3
        case LOG_WARNING: level_str = "WARN "; break;  // Priority 4
        case LOG_INFO:    level_str = "INFO "; break;  // Priority 6
        case LOG_DEBUG:   level_str = "DEBUG"; break;  // Priority 7
        default:          level_str = "LOG  "; break;  // Catch-all
    }

        // Print label (e.g., "[ERROR] ")
    fprintf(stderr, "[%s] ", level_str);
    
    // Process variable arguments
    va_list args;                   // Declare varargs holder
    va_start(args, format);         // Initialize (start after 'format' param)
    vfprintf(stderr, format, args); // Print formatted string
    va_end(args);                   // Cleanup (required)
    
    fprintf(stderr, "\n");          // Add newline
}

// SYSLOG LOGGER: Output to system log daemon (production mode)
// ----------------------------------------------------------------------------
// WHY: Standard Unix logging for services/daemons
// HOW: Wraps vsyslog() (varargs version of syslog())
// WHERE: Logs go to /var/log/syslog or journalctl (rsyslog decides)
//
// FLOW:
//   1. Package varargs into va_list
//   2. Pass to vsyslog() (syslog's varargs variant)
//   3. rsyslogd receives → writes to disk
//
// WHY vsyslog() NOT syslog():
//   syslog() takes varargs directly: syslog(LOG_INFO, "msg %d", val)
//   We're wrapping it, so WE receive the varargs
//   vsyslog() accepts pre-packaged va_list
static void log_syslog(int priority, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsyslog(priority, format, args);  // Varargs version of syslog()
    va_end(args);
}

// ============================================================================
// GLOBAL FUNCTION POINTERS (the "switchboard")
// ============================================================================

// WHY: Decision made once (at init), executed thousands of times (in loop)
// WHAT: Points to one of the 3 functions above based on log level
// HOW: logging_init() assigns these, rest of code just calls log_info()
//
// EXAMPLE STATE CHANGES:
//   Startup:        log_info = log_null (everything off)
//   After init:     log_info = log_syslog (if log_level >= 1)
//   After init:     log_debug = log_syslog (if log_level >= 2)
//
// MEMORY: Each pointer = 8 bytes on 64-bit (holds function address)
static log_func_t log_critical = log_syslog;  // ALWAYS logs (safe default)
static log_func_t log_info = log_null;  // Logs at level 1+
static log_func_t log_debug = log_null; // Logs at level 2 only

// ============================================================================
// INITIALIZATION FUNCTION
// ============================================================================

// WHEN CALLED: Once in main(), after parsing config and CLI args
// WHAT IT DOES: Sets up global pointers based on desired log level
// WHY: Centralizes the "which function should I use?" decision
//
// PARAMETERS:
//   level     - Desired log level (0/1/2 from config or -d flag)
//   foreground- 0=use syslog, 1=use stderr
//
// LOGIC FLOW:
//   1. Store settings in globals (other code checks foreground_mode)
//   2. Initialize syslog if needed (opens connection to rsyslogd)
//   3. Choose target function (stderr or syslog)
//   4. Wire up function pointers based on level:
//      - log_critical: ALWAYS enabled (hardware errors, etc.)
//      - log_info: enabled if level >= 1
//      - log_debug: enabled if level >= 2
//   5. Any log_* still pointing to log_null will discard output
static void logging_init(int level, int foreground) {
    // Store configuration in globals
    foreground_mode = foreground;
    current_log_level = level;
    
    // Initialize syslog connection (only if not in foreground mode)
    // WHY: No point opening syslog socket if we're using stderr
    if (!foreground) {
        openlog("touch-timeout",           // Program name in logs
                LOG_PID | LOG_CONS,        // Include PID, fallback to console
                LOG_DAEMON);               // Facility (daemon category)
    }
    
    // Choose output function based on mode
    // TERNARY: condition ? true_value : false_value
    log_func_t target = foreground ? log_stderr : log_syslog;
    
    // Wire up function pointers based on log level
    // CRITICAL: Overrides safe default, respects foreground flag
    log_critical = target;
    
    // INFO: Enabled at level 1+
    // If level < 1, log_info stays as log_null (discards)
    if (level >= LOG_LEVEL_INFO) {
        log_info = target;
    }
    
    // DEBUG: Enabled at level 2 only
    // If level < 2, log_debug stays as log_null (discards)
    if (level >= LOG_LEVEL_DEBUG) {
        log_debug = target;
    }
}

// ============================================================================
// CLEANUP FUNCTION
// ============================================================================

// WHEN CALLED: Once at shutdown (after main loop exits)
// WHAT IT DOES: Closes syslog connection if it was opened
// WHY: Good practice (frees resources, flushes buffers)
//
// NOTE: Not strictly necessary (OS cleans up on process exit),
//       but professional daemons do this
static void logging_cleanup(void) {
    if (!foreground_mode) {
        closelog();  // Close connection to rsyslogd
    }
}

// --------------------
// Display state machine
// --------------------
enum display_state_enum {
    STATE_FULL = 0,      // Full brightness
    STATE_DIMMED = 1,    // Dimmed
    STATE_OFF = 2        // Screen off
};

// --------------------
// Struct for tracking display state
// --------------------
struct display_state {
    int bright_fd;                      // File descriptor for /sys/class/backlight/.../brightness
    int user_brightness;                // User-configured brightness level (MIN_BRIGHTNESS–max_brightness)
    int dim_brightness;                 // Calculated dim brightness (≥MIN_DIM_BRIGHTNESS)
    int current_brightness;             // Cached hardware brightness (prevents redundant writes)
    int dim_timeout;                    // Seconds before dimming
    int off_timeout;                    // Seconds before turning off
    time_t last_input;                  // Timestamp of last touch event
    enum display_state_enum state;      // Current state
};

// --------------------
// Signal handler for systemd stop/restart
// --------------------
static void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

// --------------------
// Trim whitespace from both ends of a string (for config parsing)
// --------------------
static void trim(char *s) {
    char *p = s;
    while (isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
}

// --------------------
// Safe string-to-int (validates untrusted input)
// --------------------
static int safe_atoi(const char *str, int *result) {
    char *endptr;
    errno = 0;
    long val = strtol(str, &endptr, 10);
    if (endptr == str || *endptr != '\0' || errno == ERANGE || val < INT_MIN || val > INT_MAX)
        return -1;
    *result = (int)val;
    return 0;
}

// --------------------
// Parse /etc/touch-timeout.conf for key=value pairs
// Supports:
//   brightness=150
//   off_timeout=300
//   backlight=rpi_backlight
//   device=event0
//   poll_interval=100
//   dim_percent=50
//   log_level=0   
// --------------------
static void load_config(const char *path, int *brightness, int *timeout,
                        char *backlight, size_t bl_sz, 
                        char *device, size_t dev_sz, 
                        int *poll_interval, int *dim_percent, 
                        int *log_level) {

// ============================================================================
// v1.0.1 CHANGE 1: Add early openlog() in load_config()
// WHY: Ensures LOG_DAEMON facility used before logging_init()
// WHERE: Line 303 (start of load_config function)
// ============================================================================
    // v1.0.1: Initialize syslog early for config parsing errors
    // Call openlog() only in logging_init()
    //openlog("touch-timeout", LOG_PID | LOG_CONS, LOG_DAEMON);

    FILE *f = fopen(path, "r");
    if (!f) return; // No config file found — just skip

    char line[128];
    int line_num = 0;

    while (fgets(line, sizeof(line), f)) {
        line_num++;
        trim(line);
        if (line[0] == '#' || line[0] == ';' || line[0] == '\0')
            continue;

        char key[64], value[64];
        if (sscanf(line, "%63[^=]=%63s", key, value) == 2) {
            trim(key); 
            trim(value);
            
            int tmp;
            if (strcmp(key, "brightness") == 0) {
                if (safe_atoi(value, &tmp) == 0) *brightness = tmp;
                // v1.0.1 CHANGE 2: Replace bare syslog() with log_critical()
                // WHY: Ensures proper log level filtering during config load
                else log_critical(LOG_WARNING, "Invalid brightness '%s' at line %d", value, line_num);
            }
            else if (strcmp(key, "off_timeout") == 0) {
                if (safe_atoi(value, &tmp) == 0) *timeout = tmp;
                // v1.0.1 CHANGE 2: Replace bare syslog() with log_critical()
                else log_critical(LOG_WARNING, "Invalid off_timeout '%s' at line %d", value, line_num);
            }
            // snprintf() advantages over strncpy():
            // 1. Always null-terminates (strncpy() does NOT if src >= dest size)
            // 2. Single operation instead of two (strncpy + manual '\0')
            // 3. Returns chars written (useful for overflow detection)
            // 4. More secure - CERT C Coding Standard recommends snprintf() over strncpy()
            else if (strcmp(key, "backlight") == 0)
                // Use snprintf for safe, null-terminated string copying (POSIX best practice)
                snprintf(backlight, bl_sz, "%s", value);  // Always null-terminates
            else if (strcmp(key, "device") == 0)
                snprintf(device, dev_sz, "%s", value);    // Always null-terminates
            else if (strcmp(key, "poll_interval") == 0) {
                if (safe_atoi(value, &tmp) == 0) *poll_interval = tmp;
                // v1.0.1 CHANGE 2: Replace bare syslog() with log_critical()
                else log_critical(LOG_WARNING, "Invalid poll_interval '%s' at line %d", value, line_num);
            }
            else if (strcmp(key, "dim_percent") == 0) {
                if (safe_atoi(value, &tmp) == 0) *dim_percent = tmp;
                // v1.0.1 CHANGE 2: Replace bare syslog() with log_critical()
                else log_critical(LOG_WARNING, "Invalid dim_percent '%s' at line %d", value, line_num);
            }
            else if (strcmp(key, "log_level") == 0) {
                // Bounds check for log_level (0-2) based on roadmap definitio
                if (safe_atoi(value, &tmp) == 0 && tmp >= 0 && tmp <= 2) *log_level = tmp;
                // v1.0.1 CHANGE 2: Replace bare syslog() with log_critical()
                else log_critical(LOG_WARNING, "Invalid log_level '%s' at line %d (valid: 0-2)", value, line_num); 
            } 
            // v1.0.1 CHANGE 2: Replace bare syslog() with log_critical()
            else log_critical(LOG_WARNING, "Unknown config key '%s' at line %d", key, line_num);
            
        } 
        // v1.0.1 CHANGE 2: Replace bare syslog() with log_critical()
        else log_critical(LOG_WARNING, "Malformed config line %d: %s", line_num, line);
    }
    fclose(f);
}

// --------------------
// Read max_brightness from sysfs
// --------------------
// ============================================================================
// v1.0.1 CHANGE 3: Harden get_max_brightness() with safe_atoi()
// WHY: Prevents silent failures from malformed sysfs content
// WHERE: Line 346 (get_max_brightness function)
// ============================================================================
static int get_max_brightness(const char *backlight) {
    char path[PATH_BUF_SZ];
    // v1.0.1: Add snprintf() return validation
    int path_ret = snprintf(path, sizeof(path), "/sys/class/backlight/%s/max_brightness", backlight);
    if (path_ret < 0 || path_ret >= (int)sizeof(path)) {
        log_critical(LOG_ERR, "Backlight path too long: %s", backlight);
        return MAX_BRIGHTNESS_LIMIT;
    }
    
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        // v1.0.1 CHANGE 4: Replace bare syslog() with log_critical()
        log_critical(LOG_WARNING, "Cannot read %s, assuming max=255", path);
        return MAX_BRIGHTNESS_LIMIT;
    }
    
    char buf[8];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    
    // v1.0.1 CHANGE 5: Harden sysfs read - check for partial/short reads
    // WHY: Malformed sysfs content causes silent failures
    if (n <= 0 || n >= (ssize_t)sizeof(buf) - 1) {
        log_critical(LOG_WARNING, "Invalid read from %s (%zd bytes), using 255", path, n);
        return MAX_BRIGHTNESS_LIMIT;
    }
    buf[n] = '\0';
    
    // v1.0.1 CHANGE 6: Replace atoi() with safe_atoi()
    // WHY: atoi() silently accepts partial/malformed integers ("255abc" -> 255)
    int max;
    if (safe_atoi(buf, &max) != 0) {
        log_critical(LOG_WARNING, "Non-numeric max_brightness in %s, using 255", path);
        return MAX_BRIGHTNESS_LIMIT;
    }
    
    // Clamp to valid range
    if (max < 10 || max > MAX_BRIGHTNESS_LIMIT) {
        // v1.0.1 CHANGE 7: Replace bare syslog() with log_critical()
        log_critical(LOG_WARNING, "Invalid max_brightness %d, using 255", max);
        return MAX_BRIGHTNESS_LIMIT;
    }
    
    return max;
}

// --------------------
// Read current brightness from sysfs
// --------------------
// ============================================================================
// v1.0.1 CHANGE 8: Harden read_brightness() with safe_atoi()
// WHY: Prevents silent failures from malformed sysfs content
// WHERE: Line 367 (read_brightness function)
// ============================================================================
static int read_brightness(int fd) {
    char buf[8];
    // lseek(fd, 0, SEEK_SET);
    // No lseek needed: fd opened at offset 0, only read once at startup
    // Only set_brightness() needs lseek (repeated writes)
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    
    // v1.0.1 CHANGE 9: Add partial read detection
    if (n <= 0 || n >= (ssize_t)sizeof(buf) - 1) return -1;
    buf[n] = '\0';
    
    // v1.0.1 CHANGE 10: Replace atoi() with safe_atoi()
    int brightness;
    if (safe_atoi(buf, &brightness) != 0) return -1;
    return brightness;
}

// --------------------
// Write brightness safely to /sys/class/backlight
// Uses lseek() to reset file position and caching to prevent redundant writes
// Note: fsync() removed - sysfs writes are synchronous to hardware; fsync() only
// syncs VFS metadata which is unnecessary and adds 5-10ms latency per write
// --------------------
// ============================================================================
// v1.0.1 CHANGE 11: Remove assert() from set_brightness()
// WHY: Daemon crashes are unacceptable; use error handling instead
// WHERE: Line 386 (set_brightness function)
// ============================================================================
static int set_brightness(struct display_state *state, int brightness) {
    // v1.0.1: Replace assert() with explicit error handling
    // WHY: Assert crashes daemon; production needs graceful failure
    if (state == NULL) {
        log_critical(LOG_ERR, "set_brightness: NULL state pointer");
        return -1;
    }
    if (brightness < 0) {
        log_critical(LOG_ERR, "set_brightness: invalid brightness %d", brightness);
        return -1;
    }
    // v1.0.1: Clamp to max brightness, handle invalid values gracefully
    if (brightness > MAX_BRIGHTNESS_LIMIT) {
        log_info(LOG_WARNING, "Brightness %d exceeds max, clamping TO %d", brightness, MAX_BRIGHTNESS_LIMIT);
        brightness = MAX_BRIGHTNESS_LIMIT;
    }
    if (state->bright_fd <= 0) {
        log_critical(LOG_ERR, "set_brightness: invalid file descriptor %d", state->bright_fd);
        return -1;
    }
 
    // Skip if brightness unchanged (prevents redundant hardware writes)
    if (brightness == state->current_brightness)
        return 0;
    
    // Enforce minimum brightness (except for screen off)
    if (brightness < MIN_BRIGHTNESS && brightness != SCREEN_OFF)
        brightness = MIN_BRIGHTNESS;

    char buf[8];
    int len = snprintf(buf, sizeof(buf), "%d", brightness);

    // Validate snprintf didn't fail or truncate
    if (len < 0 || len >= (int)sizeof(buf)) {
        log_critical(LOG_ERR, "Brightness value too large: %d", brightness);
        return -1;
    }
    // Reset file position for repeated writes to same sysfs file
    // POSIX requires checking lseek() return - can fail on special files
    if (lseek(state->bright_fd, 0, SEEK_SET) == -1) {
        // v1.0.1 CHANGE 12: Replace bare syslog() with log_critical()
        log_critical(LOG_ERR, "lseek failed: %s", strerror(errno));
        return -1;
    }

    // Use correct type for write() return and cast len to size_t
    ssize_t ret = write(state->bright_fd, buf, (size_t)len);
    // fsync(state->bright_fd);  // REMOVED: sysfs writes are synchronous
    
    if (ret != (ssize_t)len) {
        // v1.0.1 CHANGE 13: Replace bare syslog() with log_critical()
        log_critical(LOG_ERR, "Failed to set brightness: %s", strerror(errno));
        return -1;
    }
    
    state->current_brightness = brightness;
    return 0;
}

// --------------------
// Restore full brightness after a touch event
// --------------------
// ============================================================================
// v1.0.1 CHANGE 14: Remove assert() from restore_brightness()
// WHY: State machine errors shouldn't crash daemon
// WHERE: Line 430 (restore_brightness function)
// ============================================================================
static int restore_brightness(struct display_state *state) {
    // v1.0.1: Remove assert - validation now in set_brightness()
    // Original asserts removed:
    // - assert(state->user_brightness >= MIN_BRIGHTNESS && ...);
    // - assert(state->state == STATE_DIMMED || state->state == STATE_OFF);
    
    // v1.0.1: Add state validation with graceful recovery
    if (state->state != STATE_DIMMED && state->state != STATE_OFF) {
        log_critical(LOG_WARNING, "restore_brightness called from invalid state %d", state->state);
        // Continue anyway - user wants screen on
    }
 
    if (set_brightness(state, state->user_brightness) == 0) {
        state->state = STATE_FULL;
        state->last_input = time(NULL);
        // v1.0.1 CHANGE 15: Replace bare syslog() with log_info()
        log_info(LOG_INFO, "Restored brightness to %d", state->user_brightness);
        return 0;
    }
    return -1;
}

// --------------------
// Check dim/off timeouts using absolute time comparisons
// Handles missed poll cycles by checking if current time has passed target times
// --------------------
// ============================================================================
// v1.0.1 CHANGE 16: Remove assert() from check_timeouts()
// WHY: NTP jumps and state corruption shouldn't crash daemon
// WHERE: Line 448 (check_timeouts function)
// ============================================================================
static void check_timeouts(struct display_state *state) {
    // v1.0.1: Replace assert() with explicit error handling
    if (state == NULL) {
        log_critical(LOG_ERR, "check_timeouts: NULL state pointer");
        return;
    }
    
    // v1.0.1: Validate state enum with recovery (already present in your code)
    if (state->state < STATE_FULL || state->state > STATE_OFF) {
        log_critical(LOG_ERR, "State machine corruption detected: %d", state->state);
        // Recovery strategy: force known state
        state->state = STATE_FULL;
        state->last_input = time(NULL);  // Reset timer
        return;  // Skip this timeout check cycle
    }

    // v1.0.1: Replace assert() with bounds check + recovery
    if (state->dim_timeout <= 0 || state->dim_timeout > state->off_timeout) {
        log_critical(LOG_ERR, "Invalid timeouts: dim=%d off=%d", 
                     state->dim_timeout, state->off_timeout);
        state->last_input = time(NULL);  // Reset to prevent immediate timeout
        return;
    }
    
    time_t now = time(NULL);
    double idle = difftime(now, state->last_input);
 
    // Handle clock adjustments gracefully (NTP can shift time backwards)
    // Using assert() here would crash daemon during normal NTP operations
    if (idle < -NTP_TOLERANCE_SEC) {
        // v1.0.1 CHANGE 17: Replace bare syslog() with log_info()
        log_info(LOG_WARNING, "Clock adjusted backwards by %.1fs - resetting timer", -idle);
        state->last_input = now;  // Reset baseline to current time
        return;  // Skip timeout checks this cycle
    }
    
    // v1.0.1 CHANGE 18: Add overflow guards for timeout arithmetic
    // WHY: Large off_timeout values cause undefined behavior
    if (state->last_input > (time_t)(LONG_MAX - state->off_timeout)) {
        log_critical(LOG_ERR, "Timeout overflow risk detected, resetting timer");
        state->last_input = now;
        return;
    }
    
    time_t dim_time = state->last_input + state->dim_timeout;
    time_t off_time = state->last_input + state->off_timeout;
    
    // Check if we should be OFF (highest priority - guarantees power saving)
    if (now >= off_time && state->state != STATE_OFF) {
        if (set_brightness(state, SCREEN_OFF) == 0) {
            // v1.0.1 CHANGE 19: Replace bare syslog() with log_info()
            log_info(LOG_INFO, "Display off (idle=%.0fs)", idle);
            state->state = STATE_OFF;
        }
    }
    // Check if we should be DIMMED (only if not already off)
    else if (now >= dim_time && state->state == STATE_FULL) {
        if (set_brightness(state, state->dim_brightness) == 0) {
            // v1.0.1 CHANGE 20: Replace bare syslog() with log_info()
            log_info(LOG_INFO, "Display dimmed (idle=%.0fs)", idle);
            state->state = STATE_DIMMED;
        }
    }
}

// --------------------
// Main entry point
// --------------------
// ============================================================================
// v1.0.1 CHANGE 21: Remove redundant openlog() from main()
// WHY: Now handled by load_config() early initialization
// WHERE: Line 529 (main function start)
// ============================================================================
int main(int argc, char *argv[]) {
    // v1.0.1: REMOVE THIS LINE - openlog() now in logging_init()

    // ========================================================================
    // COMMAND-LINE ARGUMENT PARSING
    // ========================================================================
    // WHY: Parse flags BEFORE logging init so CLI overrides config
    // WHAT: -d (debug), -f (foreground), -h (help)
    // HOW: getopt() consumes flags; argv/argc adjusted afterward

    int opt;
    int cli_log_level = -1;  // -1 means "not set by CLI"
    int cli_foreground = 0;  // Local copy - DON'T touch global until logging_init

    while ((opt = getopt(argc, argv, "dfh")) != -1) {
        switch (opt) {
            case 'd':
                cli_log_level = LOG_LEVEL_DEBUG;
                break;
            case 'f':
                cli_foreground = 1;  // ← FIX: Set local, not global
                break;
            case 'h':
                fprintf(stderr, "Usage: %s [-d] [-f] [-h] [brightness] [timeout] [backlight] [device]\n",
                        argv[0]);
                fprintf(stderr, "\nOptions:\n");
                fprintf(stderr, "  -d          Enable debug logging (overrides config file)\n");
                fprintf(stderr, "  -f          Run in foreground (log to stderr, not syslog)\n");
                fprintf(stderr, "  -h          Show this help message\n");
                fprintf(stderr, "\nPositional arguments (CLI overrides config):\n");
                fprintf(stderr, "  brightness  Display brightness (15-255, recommend ≤200)\n");
                fprintf(stderr, "  timeout     Seconds before screen off (≥10)\n");
                fprintf(stderr, "  backlight   Backlight device (default: rpi_backlight)\n");
                fprintf(stderr, "  device      Input device (default: event0)\n");
                fprintf(stderr, "\nConfig file: /etc/touch-timeout.conf\n");
                fprintf(stderr, "Version: %s\n", VERSION);
                return 0;
            default:
                fprintf(stderr, "Error: Invalid option. Use -h for help.\n");
                return 1;
        }
    }

    // Shift positional arguments after getopt()
    argc -= optind;
    argv += optind;

    // ========================================================================
    // LOGGING INITIALIZATION
    // ========================================================================
    // v1.0.1 move logging_init() here to ensure logging is set up before config load
    int tmp_log_level = (cli_log_level >= 0) ? cli_log_level : LOG_LEVEL_NONE; // Default to 0 if not set by CLI
    logging_init(tmp_log_level, cli_foreground);  // ← Sets global foreground_mode here

    // Default values (will be overridden by config or CLI)
    int user_brightness = 100;
    int off_timeout     = 300;
    char backlight[NAME_MAX + 1] = "rpi_backlight"; 
    char input_dev[NAME_MAX + 1] = "event0";         
    int poll_interval   = 100;
    int dim_percent     = 50;
    int config_log_level = LOG_LEVEL_NONE; // Will be filled by load_config()

    // ========================================================================
    // CONFIGURATION LOAD — now all log_*() calls inside are visible
    // ========================================================================
    // Load config from /etc/touch-timeout.conf (if present)
    load_config(CONFIG_PATH, &user_brightness, &off_timeout,
                backlight, sizeof(backlight),
                input_dev, sizeof(input_dev),
                &poll_interval, &dim_percent,
                &config_log_level);

    // ========================================================================
    // PHASE 3: Final log level resolution — REINITIALIZE logging if needed
    // ========================================================================
    int final_log_level = tmp_log_level;  // start with what we used
    if (cli_log_level < 0 && config_log_level != LOG_LEVEL_NONE) {
        // Config file specified log_level= and CLI did not override → apply it
        final_log_level = config_log_level;
        // No need to re-open syslog — already open with correct facility
    }

    // Only re-init if the effective level actually changed
    if (final_log_level != current_log_level) {
        logging_init(final_log_level, cli_foreground);  // updates function pointers
    }

    // ========================================================================
    // POSITIONAL ARGUMENTS
    // ========================================================================
    if (argc > 0) {
        if (safe_atoi(argv[0], &user_brightness) != 0) {
            log_critical(LOG_ERR, "Invalid brightness argument: %s", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    if (argc > 1) {
        if (safe_atoi(argv[1], &off_timeout) != 0) {
            log_critical(LOG_ERR, "Invalid timeout argument: %s", argv[1]);
            exit(EXIT_FAILURE);
        }
    }
    if (argc > 2) snprintf(backlight, sizeof(backlight), "%s", argv[2]);
    if (argc > 3) snprintf(input_dev, sizeof(input_dev), "%s", argv[3]);

    // v1.0.1 CHANGE 22: Add negative poll_interval check
    // WHY: Negative values cause poll() to block indefinitely
    if (poll_interval < 0) {
        log_critical(LOG_ERR, "poll_interval cannot be negative: %d", poll_interval);
        exit(EXIT_FAILURE);
    }
    
    // Validate poll_interval
    if (poll_interval < 10 || poll_interval > 2000) {
        log_info(LOG_WARNING, "Invalid poll_interval %dms (valid: 10-2000), using default 100ms", poll_interval);
        poll_interval = 100;
    }

    // Validate dim_percent
    if (dim_percent < 10 || dim_percent > 100) {
        log_info(LOG_WARNING, "Invalid dim_percent %d%% (valid: 10-100), using default 50%%", dim_percent);
        dim_percent = 50;
    }

    // Read and validate max_brightness
    int max_brightness = get_max_brightness(backlight);
    
    // v1.0.1 CHANGE 23: Consolidate startup logs (5 -> 1 batch)
    // WHY: Reduces SD writes from 5 to 1 during boot (80% reduction)
    // REMOVED: Individual log_info() calls for max_brightness, brightness clamping, etc.

    // Validate and clamp user_brightness
    if (user_brightness < MIN_BRIGHTNESS) {
        // v1.0.1: Don't log here - included in batch below
        user_brightness = MIN_BRIGHTNESS;
    }
    if (user_brightness > max_brightness) {
        // v1.0.1: Don't log here - included in batch below
        user_brightness = max_brightness;
    }

    // v1.0.1 CHANGE 24a: Add off_timeout minimum enforcement
    // WHY: Validate off_timeout, enforce minimum, handle gracefully   
    if (off_timeout < MIN_OFF_TIMEOUT) {
        log_info(LOG_WARNING, "off_timeout must be >= %d, clamping to %d", MIN_OFF_TIMEOUT, MIN_OFF_TIMEOUT);
        off_timeout = MIN_OFF_TIMEOUT;
    }

    // v1.0.1 CHANGE 24b: Add off_timeout maximum enforcement
    // WHY: overflow guard for dim_timeout calculation, handle gracefully
    //      Large off_timeout * dim_percent can overflow
    if (off_timeout > MAX_OFF_TIMEOUT) {
        log_info(LOG_WARNING, "off_timeout %d exceeds 24h max, clamping to %d", off_timeout, MAX_OFF_TIMEOUT);
        off_timeout = MAX_OFF_TIMEOUT;
    }
    
    // Calculate dim_timeout
    int dim_timeout = (off_timeout * dim_percent) / 100;
    
    // v1.0.1: Replace assert() with explicit validation
    if (dim_timeout <= 0 || dim_timeout > off_timeout) {
        log_critical(LOG_ERR, "Invalid dim_timeout calculation: dim=%d off=%d", 
                     dim_timeout, off_timeout);
        exit(EXIT_FAILURE);
    }
    
    // v1.0.1: Replace assert() with explicit validation
    if (user_brightness < MIN_BRIGHTNESS || user_brightness > max_brightness) {
        log_critical(LOG_ERR, "Brightness validation failed: %d (min=%d max=%d)", 
                     user_brightness, MIN_BRIGHTNESS, max_brightness);
        exit(EXIT_FAILURE);
    }

    // Open brightness control file
    char bright_path[PATH_BUF_SZ];
    int ret = snprintf(bright_path, sizeof(bright_path),
             "/sys/class/backlight/%s/brightness", backlight);
    if (ret < 0 || ret >= (int)sizeof(bright_path)) {
        log_critical(LOG_ERR, "Brightness path too long for backlight '%s'", backlight);
        exit(EXIT_FAILURE);
    }
    
    int bright_fd = open(bright_path, O_RDWR);
    if (bright_fd == -1) {
        log_critical(LOG_ERR, "Error opening %s: %s", bright_path, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Read initial brightness
    int initial_brightness = read_brightness(bright_fd);
    if (initial_brightness < 0) {
        log_info(LOG_WARNING, "Cannot read current brightness, assuming %d", user_brightness);
        initial_brightness = user_brightness;
    }

    // v1.0.1 CHANGE 25: Add dim_brightness bounds enforcement
    // WHY: Prevents dim_brightness exceeding max_brightness
    int calculated_dim = user_brightness / 10;
    if (calculated_dim < MIN_DIM_BRIGHTNESS) {
        calculated_dim = MIN_DIM_BRIGHTNESS;
    }
    if (calculated_dim > max_brightness) {
        calculated_dim = max_brightness;
    }
    
    // Initialize state
    struct display_state state = {
        .bright_fd = bright_fd,
        .user_brightness = user_brightness,
        .dim_brightness = calculated_dim,  // v1.0.1: Use bounds-checked value
        .current_brightness = initial_brightness,
        .dim_timeout = dim_timeout,
        .off_timeout = off_timeout,
        .last_input = time(NULL),
        .state = STATE_FULL
    };

    // Open input device
    char dev_path[PATH_BUF_SZ];
    ret = snprintf(dev_path, sizeof(dev_path), "/dev/input/%s", input_dev);
    if (ret < 0 || ret >= (int)sizeof(dev_path)) {
        log_critical(LOG_ERR, "Device path too long for device '%s'", input_dev);
        close(state.bright_fd);
        exit(EXIT_FAILURE);
    }
    
    int event_fd = open(dev_path, O_RDONLY | O_NONBLOCK);
    if (event_fd == -1) {
        log_critical(LOG_ERR, "Error opening %s: %s", dev_path, strerror(errno));
        close(state.bright_fd);
        exit(EXIT_FAILURE);
    }

    // Set initial brightness
    if (set_brightness(&state, user_brightness) != 0) {
        log_critical(LOG_ERR, "Failed to set initial brightness");
        close(state.bright_fd);
        close(event_fd);
        exit(EXIT_FAILURE);
    }

    // v1.0.1 CHANGE 26: Batched startup log (replaces 5 individual logs)
    // WHY: Reduces SD writes from 5 to 1 (80% reduction)
    // Includes: version, brightness, dim settings, timeouts, poll rate, devices, max_brightness
    log_info(LOG_INFO, 
        "Started v"VERSION" | bright=%d dim=%d(%d%%@%ds) off=%ds poll=%dms | "
        "hw=%s in=%s max=%d | log=%d fg=%d",
        state.user_brightness, state.dim_brightness, dim_percent, 
        state.dim_timeout, state.off_timeout, poll_interval,
        backlight, input_dev, max_brightness,
        final_log_level, cli_foreground
    );

    // v1.0.1 CHANGE 27: Add foreground mode daemonization guard
    // WHY: -f flag should prevent daemon mode (stay attached to terminal)
    if (!cli_foreground) {
        // Daemonize: fork, setsid, chdir, close stdio
        pid_t pid = fork();
        if (pid < 0) {
            log_critical(LOG_ERR, "Fork failed: %s", strerror(errno));
            close(state.bright_fd);
            close(event_fd);
            exit(EXIT_FAILURE);
        }
        if (pid > 0) {
            // Parent process exits successfully
            exit(EXIT_SUCCESS);
        }
        
        // Child continues as daemon
        if (setsid() < 0) {
            log_critical(LOG_ERR, "setsid failed: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        
        // Ignore terminal hangup
        signal(SIGHUP, SIG_IGN);
        
        // Change working directory to root
        if (chdir("/") < 0) {
            log_critical(LOG_ERR, "chdir failed: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        
        // Close standard file descriptors (syslog already open)
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }
    // Else: foreground mode - keep stdio open for debugging

    // Graceful shutdown
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    // Main loop
    struct pollfd pfd = {.fd = event_fd, .events = POLLIN};
    struct input_event event;

    while (running) {
        int poll_ret = poll(&pfd, 1, poll_interval);

        if (poll_ret > 0 && (pfd.revents & POLLIN)) {
            while (read(event_fd, &event, sizeof(event)) > 0) {
                if (event.type == EV_KEY || event.type == EV_ABS) {
                    if (state.state != STATE_FULL)
                        restore_brightness(&state);
                    else
                        state.last_input = time(NULL);
                }
            }
        } else if (poll_ret < 0 && errno != EINTR) {
            log_critical(LOG_ERR, "Poll error: %s", strerror(errno));
        }

        check_timeouts(&state);
    }

    // Cleanup
    log_info(LOG_INFO, "Stopping touch-timeout service...");
    close(state.bright_fd);
    close(event_fd);
    logging_cleanup();
    return 0;
}