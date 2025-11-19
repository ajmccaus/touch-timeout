#!/bin/bash
# filepath: /home/user/projects/touch-timeout/run-all-tests.sh
# Complete test setup, compile, and run

set -e

echo "[*] Compiling touch-timeout..."
gcc -O2 -Wall -Wextra -Werror -std=c99 -pedantic -o bin/touch-timeout ../touch-timeout.c

echo "[*] Setting up test fixtures..."
bash test-setup.sh

echo "[*] Running tests..."
bash test-run.sh

echo ""
echo "[✓] All tests complete!"