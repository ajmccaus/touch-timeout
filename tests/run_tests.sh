#!/bin/bash

echo "=== Test 1: log_level=0 ==="
./test-config-parse tmp/test-log0.conf
echo ""

echo "=== Test 2: log_level=1 ==="
./test-config-parse tmp/test-log1.conf
echo ""

echo "=== Test 3: log_level=2 ==="
./test-config-parse tmp/test-log2.conf
echo ""

echo "=== Test 4: Invalid value 99 (should warn) ==="
./test-config-parse tmp/test-invalid.conf
echo ""

echo "=== Test 5: Invalid value 'abc' (should warn) ==="
./test-config-parse tmp/test-invalid2.conf
echo ""

echo "=== Test 6: Missing log_level (should stay default 0) ==="
./test-config-parse tmp/test-missing.conf
echo ""
