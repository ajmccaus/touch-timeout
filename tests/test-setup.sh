#!/bin/bash
# filepath: /home/user/projects/touch-timeout/tests/test-setup.sh
# Purpose: Create test fixtures for touch-timeout path validation tests
# Usage: bash test-setup.sh

set -e

TEST_DIR="./tests"
mkdir -p "$TEST_DIR"

echo "[*] Setting up test fixtures in $TEST_DIR..."

# Test 1: Valid backlight (should work)
mkdir -p "$TEST_DIR/test1-valid"
cat > "$TEST_DIR/test1-valid/touch-timeout.conf" <<'EOF'
brightness=100
timeout=300
backlight=rpi_backlight
device=event0
EOF
echo "[+] test1-valid: normal config with standard names"

# Test 2: Long backlight name (exceeds NAME_MAX=255)
mkdir -p "$TEST_DIR/test2-long-backlight"
LONG_NAME=$(python3 -c "print('x' * 256)")  # 256 'x' chars
cat > "$TEST_DIR/test2-long-backlight/touch-timeout.conf" <<EOF
brightness=100
timeout=300
backlight=$LONG_NAME
device=event0
EOF
echo "[+] test2-long-backlight: backlight name = 256 bytes (exceeds NAME_MAX)"

# Test 3: Long device name (exceeds NAME_MAX=255)
mkdir -p "$TEST_DIR/test3-long-device"
LONG_DEV=$(python3 -c "print('d' * 256)")
cat > "$TEST_DIR/test3-long-device/touch-timeout.conf" <<EOF
brightness=100
timeout=300
backlight=rpi_backlight
device=$LONG_DEV
EOF
echo "[+] test3-long-device: device name = 256 bytes (exceeds NAME_MAX)"

# Test 4: Backlight at NAME_MAX boundary (255 bytes, should succeed)
mkdir -p "$TEST_DIR/test4-boundary-backlight"
BOUNDARY_NAME=$(python3 -c "print('b' * 255)")  # exactly NAME_MAX
cat > "$TEST_DIR/test4-boundary-backlight/touch-timeout.conf" <<EOF
brightness=100
timeout=300
backlight=$BOUNDARY_NAME
device=event0
EOF
echo "[+] test4-boundary-backlight: backlight name = 255 bytes (at NAME_MAX, should pass)"

# Test 5: Device at NAME_MAX boundary (255 bytes, should succeed)
mkdir -p "$TEST_DIR/test5-boundary-device"
BOUNDARY_DEV=$(python3 -c "print('e' * 255)")
cat > "$TEST_DIR/test5-boundary-device/touch-timeout.conf" <<EOF
brightness=100
timeout=300
backlight=rpi_backlight
device=$BOUNDARY_DEV
EOF
echo "[+] test5-boundary-device: device name = 255 bytes (at NAME_MAX, should pass)"

# Test 6: Both at boundary (both 255)
mkdir -p "$TEST_DIR/test6-both-boundary"
B255=$(python3 -c "print('b' * 255)")
D255=$(python3 -c "print('d' * 255)")
cat > "$TEST_DIR/test6-both-boundary/touch-timeout.conf" <<EOF
brightness=100
timeout=300
backlight=$B255
device=$D255
EOF
echo "[+] test6-both-boundary: both names = 255 bytes (should pass)"

# Test 7: Names with special chars (but within limit)
mkdir -p "$TEST_DIR/test7-special-chars"
cat > "$TEST_DIR/test7-special-chars/touch-timeout.conf" <<EOF
brightness=100
timeout=300
backlight=my_backlight-123
device=input_event-0
EOF
echo "[+] test7-special-chars: names with hyphens/underscores (should pass)"

# Test 8: Empty backlight name (edge case)
mkdir -p "$TEST_DIR/test8-empty-backlight"
cat > "$TEST_DIR/test8-empty-backlight/touch-timeout.conf" <<EOF
brightness=100
timeout=300
backlight=
device=event0
EOF
echo "[+] test8-empty-backlight: empty backlight name (should use default or fail gracefully)"

echo ""
echo "[✓] Test fixtures created. Run tests with: bash test-run.sh"