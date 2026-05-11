#!/bin/bash
# ============================================================
# Project : Hospital Patient Triage & Bed Allocator
# Script  : stop_hospital.sh
# Purpose : Gracefully shut down the hospital system
# Usage   : ./stop_hospital.sh
# ============================================================

echo "[STOP] Initiating hospital shutdown..."

if [ -f /tmp/hospital.pid ]; then
    pid=$(cat /tmp/hospital.pid)
    kill -SIGTERM "$pid" 2>/dev/null && echo "[STOP] Sent SIGTERM to PID $pid"
    sleep 2
    kill -9 "$pid" 2>/dev/null
    rm -f /tmp/hospital.pid
else
    pkill -SIGTERM admissions 2>/dev/null
fi

echo "[CLEAN] Removing IPC resources..."
ipcrm -M 0xBEDF00D 2>/dev/null && echo "[CLEAN] Shared memory removed"
rm -f /tmp/discharge_fifo && echo "[CLEAN] FIFO removed"

echo "[SUMMARY] ============================================"
echo "[SUMMARY] Logs saved to logs/"
if [ -f logs/memory_log.txt ]; then
    cnt=$(wc -l < logs/memory_log.txt)
    echo "[SUMMARY] Memory events: $cnt"
fi
if [ -f logs/schedule_log.txt ]; then
    echo "[SUMMARY] Schedule log available"
fi
echo "[SUMMARY] Hospital shutdown complete."
echo "[SUMMARY] ============================================"
