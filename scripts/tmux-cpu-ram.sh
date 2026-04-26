#!/bin/bash
# CPU 和 RAM 占用率，用于 tmux status-right
CPU=$(top -l 1 | grep "CPU usage" | awk '{print $3}' | tr -d '%')

# macOS RAM: page size=16384, 计算已使用MB
PAGES_ACTIVE=$(vm_stat | awk '/Pages active/ {gsub(/\./,"",$3); print $3}')
PAGES_WIRED=$(vm_stat | awk '/Pages wired/ {gsub(/\./,"",$4); print $4}')
PAGES_COMPRESSED=$(vm_stat | awk '/Pages occupied by compressor/ {gsub(/\./,"",$3); print $3}')
USED_PAGES=$((PAGES_ACTIVE + PAGES_WIRED + PAGES_COMPRESSED))
TOTAL_PAGES=$(sysctl -n hw.memsize | awk '{print $1/16384}')
RAM_PCT=$(( (USED_PAGES * 100) / TOTAL_PAGES ))

echo "CPU:${CPU}% RAM:${RAM_PCT}%"
