#!/bin/bash
# ============================================================
# Project : Hospital Patient Triage & Bed Allocator
# Script  : start_hospital.sh
# Purpose : Initialize IPC resources and launch admissions
# Usage   : ./start_hospital.sh [--strategy best|first|worst]
# ============================================================

strat="best"
if [ "$1" == "--strategy" ] && [ -n "$2" ]; then
    strat=$2
fi

echo "============================================"
echo "  HOSPITAL PATIENT TRIAGE SYSTEM"
echo "  Strategy: $strat"
echo "============================================"
echo "  ICU Beds    : 4"
echo "  Isolation   : 4"
echo "  General Ward: 12"
echo "  Total Beds  : 20"
echo "============================================"

mkdir -p logs

if ! [ -p /tmp/discharge_fifo ]; then
    mkfifo /tmp/discharge_fifo
    echo "[INIT] FIFO created"
fi

ipcrm -M 0xBEDF00D 2>/dev/null && echo "[INIT] Old SHM cleared"

echo "[START] Launching admissions manager..."
./admissions --strategy "$strat" &
ADM_PID=$!
echo $ADM_PID > /tmp/hospital.pid
echo "[START] Admissions PID: $ADM_PID"
echo "[READY] Hospital is open for patients"
