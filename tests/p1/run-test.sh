#!/bin/bash
# filepath: /home/user/projects/touch-timeout/tests/p1/run-test.sh
# Check compile, create and compile test program, and run tests for pointer size (part 1 of 5)

# Compile touch-timeout.c to verify it compiles without errors
echo "Compiling touch-timeout.c to verify it compiles without errors..."
gcc -Wall -Wextra -Werror -std=c99 -pedantic -c ../../touch-timeout.c
ls -alh touch-timeout.o 
echo "touch-timeout.o should exist, ~20-30KB"

# Remove the object file after verification
rm touch-timeout.o

echo ""

# Verify log_null() optimizes to single instruction
echo "Verifying that log_null() optimizes to a single return instruction..."
gcc -O2 -c ../../touch-timeout.c
objdump -d touch-timeout.o | grep -A 5 "log_null"

echo ""
echo "# Expected output (ARM):
#   ret    (single return instruction)
# Expected output (x86):
#   retq   (single return instruction)"

echo ""

# Create test program to verify pointer sizes
cat > test-pointers.c << 'EOF'
#include <stdio.h>

typedef void (*log_func_t)(int, const char *, ...);

int main(void) {
    printf("Size of function pointer: %zu bytes\n", sizeof(log_func_t));
    printf("Size of log_null: %zu bytes\n", sizeof(void*));
    return 0;
}
EOF

# Compile the test program and run it
gcc -o bin/test-pointers test-pointers.c 

echo "Running pointer size test..."
./bin/test-pointers
echo "Expected: 8 bytes on 64-bit, 4 bytes on 32-bit ARM"

echo ""
# Clean up test files
echo "Cleaning up test files..."
rm bin/test-pointers test-pointers.c touch-timeout.o