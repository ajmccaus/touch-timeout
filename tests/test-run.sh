#!/bin/bash
# filepath: /home/user/projects/touch-timeout/tests/test-run.sh
# Purpose: Run touch-timeout binary against test fixtures and validate output
# Usage: bash test-run.sh

set -e

BINARY="./touch-timeout"
TEST_DIR="./tests"
RESULTS_DIR="./tests/test-results"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Ensure binary exists
if [ ! -f "$BINARY" ]; then
    echo -e "${RED}[!] Binary not found: $BINARY${NC}"
    echo "Compile with: gcc -O2 -Wall -Wextra -Werror -std=c99 -pedantic -o bin/touch-timeout touch-timeout.c"
    exit 1
fi

# Create results directory
mkdir -p "$RESULTS_DIR"

echo "=========================================="
echo "touch-timeout Path Validation Test Suite"
echo "=========================================="
echo ""

# Test case 1: Valid config (should start successfully, then timeout)
echo -e "${YELLOW}[TEST 1] Valid backlight/device names${NC}"
echo "Expected: Should run, then timeout gracefully (no devices exist in test)"
"$BINARY" 100 300 rpi_backlight event0 2>&1 | tee "$RESULTS_DIR/test1.log" &
TEST_PID=$!
sleep 2
kill $TEST_PID 2>/dev/null || true
wait $TEST_PID 2>/dev/null || true
echo ""

# Test case 2: Long backlight name (should FAIL with NAME_MAX error)
echo -e "${YELLOW}[TEST 2] Backlight name exceeds NAME_MAX (should fail)${NC}"
LONG_BL=$(python3 -c "print('x' * 256)")
echo "Testing with backlight name length: ${#LONG_BL} bytes"
set +e
"$BINARY" 100 300 "$LONG_BL" event0 2>&1 | tee "$RESULTS_DIR/test2.log"
RESULT=$?
set -e
if grep -q "exceeds NAME_MAX" "$RESULTS_DIR/test2.log"; then
    echo -e "${GREEN}[✓] PASS: Correctly rejected long backlight name${NC}"
else
    echo -e "${RED}[✗] FAIL: Did not reject long backlight name${NC}"
fi
echo ""

# Test case 3: Long device name (should FAIL with NAME_MAX error)
echo -e "${YELLOW}[TEST 3] Device name exceeds NAME_MAX (should fail)${NC}"
LONG_DEV=$(python3 -c "print('d' * 256)")
echo "Testing with device name length: ${#LONG_DEV} bytes"
set +e
"$BINARY" 100 300 rpi_backlight "$LONG_DEV" 2>&1 | tee "$RESULTS_DIR/test3.log"
RESULT=$?
set -e
if grep -q "exceeds NAME_MAX" "$RESULTS_DIR/test3.log"; then
    echo -e "${GREEN}[✓] PASS: Correctly rejected long device name${NC}"
else
    echo -e "${RED}[✗] FAIL: Did not reject long device name${NC}"
fi
echo ""

# Test case 4: Backlight at boundary (255 bytes, should pass validation)
echo -e "${YELLOW}[TEST 4] Backlight name at NAME_MAX boundary (255 bytes)${NC}"
BOUNDARY_BL=$(python3 -c "print('b' * 255)")
echo "Testing with backlight name length: ${#BOUNDARY_BL} bytes"
set +e
"$BINARY" 100 300 "$BOUNDARY_BL" event0 2>&1 | tee "$RESULTS_DIR/test4.log" &
TEST_PID=$!
sleep 2
kill $TEST_PID 2>/dev/null || true
wait $TEST_PID 2>/dev/null || true
set -e
if ! grep -q "exceeds NAME_MAX" "$RESULTS_DIR/test4.log"; then
    echo -e "${GREEN}[✓] PASS: Accepted name at NAME_MAX boundary${NC}"
else
    echo -e "${RED}[✗] FAIL: Incorrectly rejected boundary-length name${NC}"
fi
echo ""

# Test case 5: Device at boundary (255 bytes, should pass validation)
echo -e "${YELLOW}[TEST 5] Device name at NAME_MAX boundary (255 bytes)${NC}"
BOUNDARY_DEV=$(python3 -c "print('e' * 255)")
echo "Testing with device name length: ${#BOUNDARY_DEV} bytes"
set +e
"$BINARY" 100 300 rpi_backlight "$BOUNDARY_DEV" 2>&1 | tee "$RESULTS_DIR/test5.log" &
TEST_PID=$!
sleep 2
kill $TEST_PID 2>/dev/null || true
wait $TEST_PID 2>/dev/null || true
set -e
if ! grep -q "exceeds NAME_MAX" "$RESULTS_DIR/test5.log"; then
    echo -e "${GREEN}[✓] PASS: Accepted device name at NAME_MAX boundary${NC}"
else
    echo -e "${RED}[✗] FAIL: Incorrectly rejected boundary-length name${NC}"
fi
echo ""

# Test case 6: Both at boundary
echo -e "${YELLOW}[TEST 6] Both names at NAME_MAX boundary${NC}"
set +e
"$BINARY" 100 300 "$BOUNDARY_BL" "$BOUNDARY_DEV" 2>&1 | tee "$RESULTS_DIR/test6.log" &
TEST_PID=$!
sleep 2
kill $TEST_PID 2>/dev/null || true
wait $TEST_PID 2>/dev/null || true
set -e
if ! grep -q "exceeds NAME_MAX" "$RESULTS_DIR/test6.log"; then
    echo -e "${GREEN}[✓] PASS: Accepted both names at boundary${NC}"
else
    echo -e "${RED}[✗] FAIL: Rejected valid boundary names${NC}"
fi
echo ""

# Test case 7: Invalid brightness (should fail)
echo -e "${YELLOW}[TEST 7] Invalid brightness argument${NC}"
set +e
"$BINARY" not_a_number 300 rpi_backlight event0 2>&1 | tee "$RESULTS_DIR/test7.log"
RESULT=$?
set -e
if grep -q "Invalid brightness" "$RESULTS_DIR/test7.log"; then
    echo -e "${GREEN}[✓] PASS: Correctly rejected non-numeric brightness${NC}"
else
    echo -e "${RED}[✗] FAIL: Did not reject invalid brightness${NC}"
fi
echo ""

# Test case 8: Invalid timeout (should fail)
echo -e "${YELLOW}[TEST 8] Invalid timeout argument${NC}"
set +e
"$BINARY" 100 not_a_number rpi_backlight event0 2>&1 | tee "$RESULTS_DIR/test8.log"
RESULT=$?
set -e
if grep -q "Invalid timeout" "$RESULTS_DIR/test8.log"; then
    echo -e "${GREEN}[✓] PASS: Correctly rejected non-numeric timeout${NC}"
else
    echo -e "${RED}[✗] FAIL: Did not reject invalid timeout${NC}"
fi
echo ""

echo "=========================================="
echo "Test Summary"
echo "=========================================="
echo "Results saved to: $RESULTS_DIR/"
echo ""
echo "Test logs:"
ls -lh "$RESULTS_DIR"
echo ""
echo "Key tests to verify:"
echo "  - test2.log & test3.log: Should contain 'exceeds NAME_MAX'"
echo "  - test4.log & test5.log: Should NOT contain 'exceeds NAME_MAX'"
echo "  - test7.log & test8.log: Should contain 'Invalid' messages"