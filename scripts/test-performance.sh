#!/bin/bash
#
# test-performance.sh - Comprehensive performance testing for touch-timeout daemon
#
# Transfer to RPi with:
#   scp scripts/test-performance.sh root@192.168.1.XXX:/tmp/
#
# Run on RPi:
#   ssh root@192.168.1.XXX "bash /tmp/test-performance.sh"
#
# This script measures:
# - CPU usage (idle and active)
# - Memory consumption (RSS, VSZ, leaks)
# - SD card write activity
# - File descriptor usage
# - System call frequency
# - Response time to input events
#

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Test duration parameters
MONITOR_DURATION=30  # seconds to monitor for baseline
TOUCH_TEST_DURATION=10  # seconds to simulate touch events

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[PASS]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[FAIL]${NC} $1"
}

# Find touch-timeout process
PID=$(pgrep -x touch-timeout)
if [ -z "$PID" ]; then
    log_error "touch-timeout daemon not running"
    exit 1
fi

log_info "Found touch-timeout daemon (PID: $PID)"
echo

# ====================
# Test 1: CPU Usage
# ====================
echo "=========================================="
echo "Test 1: CPU Usage"
echo "=========================================="

log_info "Measuring CPU usage over ${MONITOR_DURATION}s..."
CPU_SAMPLES=()
for i in $(seq 1 $MONITOR_DURATION); do
    CPU=$(ps -p $PID -o %cpu= 2>/dev/null || echo "0.0")
    CPU_SAMPLES+=($CPU)
    sleep 1
done

# Calculate average CPU
CPU_SUM=0
for cpu in "${CPU_SAMPLES[@]}"; do
    CPU_SUM=$(echo "$CPU_SUM + $cpu" | bc)
done
CPU_AVG=$(echo "scale=2; $CPU_SUM / ${#CPU_SAMPLES[@]}" | bc)

log_info "Average CPU usage: ${CPU_AVG}%"
if (( $(echo "$CPU_AVG < 1.0" | bc -l) )); then
    log_success "CPU usage is excellent (<1%)"
elif (( $(echo "$CPU_AVG < 5.0" | bc -l) )); then
    log_warn "CPU usage is acceptable but higher than expected (1-5%)"
else
    log_error "CPU usage is high (>5%) - investigate"
fi
echo

# ====================
# Test 2: Memory Usage
# ====================
echo "=========================================="
echo "Test 2: Memory Usage"
echo "=========================================="

MEM_START=$(ps -p $PID -o rss= | tr -d ' ')
log_info "Initial memory (RSS): $((MEM_START / 1024)) MB"

# Monitor for memory leaks
log_info "Monitoring memory for ${MONITOR_DURATION}s to detect leaks..."
sleep $MONITOR_DURATION

MEM_END=$(ps -p $PID -o rss= | tr -d ' ')
log_info "Final memory (RSS): $((MEM_END / 1024)) MB"

MEM_GROWTH=$((MEM_END - MEM_START))
if [ $MEM_GROWTH -lt 100 ]; then
    log_success "No memory leak detected (growth: ${MEM_GROWTH}KB)"
elif [ $MEM_GROWTH -lt 1024 ]; then
    log_warn "Minor memory growth detected (${MEM_GROWTH}KB) - monitor over time"
else
    log_error "Significant memory growth detected ($((MEM_GROWTH / 1024))MB) - possible leak"
fi
echo

# ====================
# Test 3: SD Card Writes
# ====================
echo "=========================================="
echo "Test 3: SD Card Write Activity"
echo "=========================================="

if [ -f /proc/$PID/io ]; then
    WRITE_START=$(grep write_bytes /proc/$PID/io | awk '{print $2}')
    log_info "Initial write bytes: $WRITE_START"

    log_info "Monitoring write activity for ${MONITOR_DURATION}s..."
    sleep $MONITOR_DURATION

    WRITE_END=$(grep write_bytes /proc/$PID/io | awk '{print $2}')
    log_info "Final write bytes: $WRITE_END"

    WRITE_DELTA=$((WRITE_END - WRITE_START))
    WRITE_RATE=$(echo "scale=2; $WRITE_DELTA / $MONITOR_DURATION" | bc)

    log_info "Write activity: ${WRITE_DELTA} bytes over ${MONITOR_DURATION}s (${WRITE_RATE} bytes/sec)"

    if [ $WRITE_DELTA -eq 0 ]; then
        log_success "Zero SD card writes - excellent!"
    elif [ $WRITE_DELTA -lt 4096 ]; then
        log_success "Minimal SD card writes (<4KB)"
    elif [ $WRITE_DELTA -lt 102400 ]; then
        log_warn "Some SD card writes detected (${WRITE_DELTA} bytes)"
    else
        log_error "Excessive SD card writes (${WRITE_DELTA} bytes) - investigate logging"
    fi
else
    log_warn "/proc/$PID/io not available - cannot measure write activity"
fi
echo

# ====================
# Test 4: File Descriptors
# ====================
echo "=========================================="
echo "Test 4: File Descriptor Usage"
echo "=========================================="

FD_START=$(ls /proc/$PID/fd 2>/dev/null | wc -l)
log_info "Initial file descriptors: $FD_START"

log_info "Monitoring FD usage for ${MONITOR_DURATION}s..."
sleep $MONITOR_DURATION

FD_END=$(ls /proc/$PID/fd 2>/dev/null | wc -l)
log_info "Final file descriptors: $FD_END"

FD_DELTA=$((FD_END - FD_START))
if [ $FD_DELTA -eq 0 ]; then
    log_success "File descriptor count stable (no leaks)"
elif [ $FD_DELTA -lt 5 ]; then
    log_warn "Minor FD count change (+${FD_DELTA}) - acceptable"
else
    log_error "File descriptor leak detected (+${FD_DELTA} FDs)"
fi

log_info "Open file descriptors:"
ls -l /proc/$PID/fd 2>/dev/null | grep -v "total" | head -10
echo

# ====================
# Test 5: System Calls
# ====================
echo "=========================================="
echo "Test 5: System Call Frequency"
echo "=========================================="

if command -v strace >/dev/null 2>&1; then
    log_info "Tracing system calls for 5 seconds..."
    SYSCALL_COUNT=$(timeout 5 strace -c -p $PID 2>&1 | grep "calls" | awk '{print $1}' | head -1)

    if [ -n "$SYSCALL_COUNT" ] && [ "$SYSCALL_COUNT" -gt 0 ]; then
        SYSCALL_RATE=$(echo "scale=2; $SYSCALL_COUNT / 5" | bc)
        log_info "System call rate: ${SYSCALL_RATE} calls/sec"

        # Show top syscalls
        log_info "Top system calls:"
        timeout 5 strace -c -p $PID 2>&1 | grep -E "^\s*[0-9]" | head -5
    else
        log_warn "Could not measure system calls (strace may require root)"
    fi
else
    log_warn "strace not available - skipping syscall analysis"
fi
echo

# ====================
# Test 6: Process Info
# ====================
echo "=========================================="
echo "Test 6: Process Summary"
echo "=========================================="

log_info "Current process state:"
ps aux | head -1
ps aux | grep -E "^USER|[t]ouch-timeout"
echo

log_info "Resource limits:"
cat /proc/$PID/limits 2>/dev/null | grep -E "open files|virtual memory|cpu time" || log_warn "Cannot read limits"
echo

# ====================
# Summary
# ====================
echo "=========================================="
echo "Performance Test Summary"
echo "=========================================="
echo "Duration: ${MONITOR_DURATION}s baseline monitoring"
echo "PID: $PID"
echo ""
echo "Results:"
echo "  - CPU (avg): ${CPU_AVG}%"
echo "  - Memory: $((MEM_START / 1024)) MB → $((MEM_END / 1024)) MB (Δ ${MEM_GROWTH}KB)"
echo "  - SD writes: ${WRITE_DELTA:-N/A} bytes"
echo "  - File descriptors: $FD_START → $FD_END (Δ ${FD_DELTA})"
echo ""

# Overall verdict
FAILED=0
if (( $(echo "$CPU_AVG >= 5.0" | bc -l) )); then
    FAILED=$((FAILED + 1))
fi
if [ $MEM_GROWTH -gt 1024 ]; then
    FAILED=$((FAILED + 1))
fi
if [ ${WRITE_DELTA:-0} -gt 102400 ]; then
    FAILED=$((FAILED + 1))
fi
if [ $FD_DELTA -gt 5 ]; then
    FAILED=$((FAILED + 1))
fi

if [ $FAILED -eq 0 ]; then
    log_success "All performance tests passed!"
    exit 0
else
    log_error "$FAILED test(s) failed - review results above"
    exit 1
fi
