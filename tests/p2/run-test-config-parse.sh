#!/bin/bash
# filepath: /home/user/projects/touch-timeout/tests/p2/run-test-config-parse.sh
# Create test files, compile test harness, and run tests for Piece 2 of pointer function logging code implementation

# Create test files for various log_level scenarios
# Test valid configs
cat > tmp/test-log0.conf << 'EOF'
brightness=100
log_level=0
EOF

cat > tmp/test-log1.conf << 'EOF'
brightness=100
log_level=1
EOF

cat > tmp/test-log2.conf << 'EOF'
brightness=100
log_level=2
EOF

# Test invalid values
cat > tmp/test-invalid.conf << 'EOF'
brightness=100
log_level=99
EOF

cat > tmp/test-invalid2.conf << 'EOF'
brightness=100
log_level=abc
EOF

# Test missing log_level (should use default)
cat > tmp/test-missing.conf << 'EOF'
brightness=100
off_timeout=300
EOF

# Create test harness source code
cat > test-config-parse.c << 'EOF'
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#define LOG_LEVEL_NONE 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_DEBUG 2

static void trim(char *s) {
    char *p = s;
    while (isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
}

static int safe_atoi(const char *str, int *result) {
    char *endptr;
    errno = 0;
    long val = strtol(str, &endptr, 10);
    if (endptr == str || *endptr != '\0' || errno == ERANGE || val < INT_MIN || val > INT_MAX)
        return -1;
    *result = (int)val;
    return 0;
}

static void load_config(const char *path, int *log_level) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "ERROR: Cannot open %s\n", path);
        return;
    }

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
            if (strcmp(key, "log_level") == 0) {
                if (safe_atoi(value, &tmp) == 0 && tmp >= 0 && tmp <= 2) {
                    *log_level = tmp;
                    printf("✓ Parsed log_level=%d from line %d\n", tmp, line_num);
                } else {
                    fprintf(stderr, "WARNING: Invalid log_level '%s' at line %d (valid: 0-2)\n",
                            value, line_num);
                }
            }
        }
    }
    fclose(f);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <config_file>\n", argv[0]);
        return 1;
    }
    
    int log_level = LOG_LEVEL_NONE;  // Default
    printf("Initial log_level: %d\n", log_level);
    
    load_config(argv[1], &log_level);
    
    printf("Final log_level: %d\n", log_level);
    
    // Validate it's in range
    if (log_level >= 0 && log_level <= 2) {
        printf("✓ PASS: log_level is valid\n");
        return 0;
    } else {
        printf("✗ FAIL: log_level out of range\n");
        return 1;
    }
}
EOF

# Compile the test harness
gcc -Wall -Wextra -o bin/test-config-parse test-config-parse.c

#Run the test harness for each config file
echo "=== Test 1: log_level=0 ==="
./bin/test-config-parse tmp/test-log0.conf
echo ""

echo "=== Test 2: log_level=1 ==="
./bin/test-config-parse tmp/test-log1.conf
echo ""

echo "=== Test 3: log_level=2 ==="
./bin/test-config-parse tmp/test-log2.conf
echo ""

echo "=== Test 4: Invalid value 99 (should warn) ==="
./bin/test-config-parse tmp/test-invalid.conf
echo ""

echo "=== Test 5: Invalid value 'abc' (should warn) ==="
./bin/test-config-parse tmp/test-invalid2.conf
echo ""

echo "=== Test 6: Missing log_level (should stay default 0) ==="
./bin/test-config-parse tmp/test-missing.conf
echo ""

# Clean up test files
rm -f bin/test-config-parse #test-config-parse.c
rm -f tmp/test-*.conf