#!/bin/bash
#
# test-performance.sh - Collect performance data for touch-timeout daemon
#
# Outputs machine-readable metrics. Redirect to file for comparison.
# Only requires: ps, awk, ls, sleep, date (standard on minimal Linux)
#
# Usage:
#   scp scripts/test-performance.sh <USER>@<IP>:/run/
#   ssh <USER>@<IP> "bash /run/test-performance.sh" | tee perf-$(date +%Y%m%d).txt
#
# Optional: specify duration (default 30s)
#   ssh <USER>@<IP> "bash /run/test-performance.sh 60"
#

set -e
DURATION=${1:-30}

# Find daemon PID
pid=$(pidof touch-timeout 2>/dev/null || ps -e -o pid,comm | awk '/touch-timeout/ {print $1; exit}')
if [[ -z "$pid" ]]; then
    echo "ERROR: touch-timeout daemon not running" >&2
    exit 1
fi

# Header
echo "# touch-timeout performance data"
echo "# Date: $(date -Iseconds 2>/dev/null || date)"
echo "# Duration: ${DURATION}s"
echo "# PID: $pid"
echo ""

# Memory measurement function
#
# Uses smaps_rollup instead of ps/VmRSS because on Linux 5.x with static binaries,
# VmRSS only counts anonymous pages (~4 KB) while smaps_rollup Rss correctly
# includes file-backed Private_Clean pages from the binary (~320 KB code section).
# The smaps Rss value represents actual physical RAM usage.
#
# See doc/PROJECT-HISTORY.md for details on this kernel accounting quirk.
get_mem_kb() {
    local p=$1
    if [ -f /proc/$p/smaps_rollup ]; then
        awk '/^Rss:/ {print $2; exit}' /proc/$p/smaps_rollup 2>/dev/null
    else
        ps -o rss= -p "$p" 2>/dev/null | tr -d ' '
    fi
}

# Capture start state
mem_start=$(get_mem_kb "$pid")
fd_start=$(ls /proc/$pid/fd 2>/dev/null | wc -l)
write_start=$(awk '/^write_bytes:/ {print $2; exit}' /proc/$pid/io 2>/dev/null || echo "N/A")

# Collect CPU samples (progress dots instead of per-line output)
echo -n "# Collecting CPU samples (${DURATION}s): "
cpu_sum=0
cpu_max=0
i=1
while [ $i -le $DURATION ]; do
    # Verify process still exists (CRITICAL: detect daemon death mid-test)
    if ! kill -0 "$pid" 2>/dev/null; then
        echo ""
        echo "ERROR: Process $pid died during measurement at sample $i" >&2
        exit 1
    fi
    cpu=$(ps -o %cpu= -p "$pid" 2>/dev/null | tr -d ' ')
    cpu=${cpu:-0.0}
    echo -n "."
    cpu_sum=$(awk "BEGIN {print $cpu_sum + $cpu}")
    cpu_max=$(awk "BEGIN {print ($cpu > $cpu_max) ? $cpu : $cpu_max}")
    sleep 1
    i=$((i + 1))
done
echo " done"

# Capture end state
mem_end=$(get_mem_kb "$pid")
fd_end=$(ls /proc/$pid/fd 2>/dev/null | wc -l)
write_end=$(awk '/^write_bytes:/ {print $2; exit}' /proc/$pid/io 2>/dev/null || echo "N/A")

# Calculate metrics (strip non-digits for safety - handles unexpected ps output)
mem_start_safe=${mem_start//[^0-9]/}; mem_start_safe=${mem_start_safe:-0}
mem_end_safe=${mem_end//[^0-9]/}; mem_end_safe=${mem_end_safe:-0}
cpu_avg=$(awk "BEGIN {printf \"%.3f\", $cpu_sum / $DURATION}")
cpu_max=$(awk "BEGIN {printf \"%.1f\", $cpu_max}")
mem_mb=$(awk "BEGIN {printf \"%.2f\", $mem_end_safe / 1024}")

# Summary
echo ""
echo "# Summary"
echo "CPU_AVG_PCT=$cpu_avg"
echo "CPU_MAX_PCT=$cpu_max"
echo "MEM_START_KB=$mem_start_safe"
echo "MEM_END_KB=$mem_end_safe"
echo "MEM_END_MB=$mem_mb"
echo "MEM_GROWTH_KB=$((mem_end_safe - mem_start_safe))"
echo "FD_START=$fd_start"
echo "FD_END=$fd_end"
echo "FD_DELTA=$((fd_end - fd_start))"
if [[ "$write_start" != "N/A" && "$write_end" != "N/A" ]]; then
    echo "SD_WRITE_BYTES=$((write_end - write_start))"
else
    echo "SD_WRITE_BYTES=N/A"
fi
echo ""
echo "# Targets: CPU ~0%, Memory <0.5MB, SD writes = 0, FD delta = 0"
