#!/bin/bash
# ============================================================
# Project : Hospital Patient Triage & Bed Allocator
# Script  : triage.sh
# Group   : Group XX
# Members : Member 1 (ID), Member 2 (ID)
# Date    : 2026-04-01
# Purpose : Compute triage priority and pipe patient data
#           to the admissions manager process.
# Usage   : ./triage.sh <name> <age> <severity 1-10>
# ============================================================

if [ $# -ne 3 ]; then
    echo "Usage: $0 <name> <age> <severity 1-10>"
    exit 1
fi

nm=$1
ag=$2
sv=$3

if [ -z "$nm" ]; then
    echo "Error: Name cannot be empty"
    exit 1
fi

if ! [[ "$ag" =~ ^[0-9]+$ ]]; then
    echo "Error: Age must be a number"
    exit 1
fi

if ! [[ "$sv" =~ ^[0-9]+$ ]]; then
    echo "Error: Severity must be a number"
    exit 1
fi

if [ "$ag" -lt 0 ] || [ "$ag" -gt 150 ]; then
    echo "Error: Age out of range (0-150)"
    exit 1
fi

if [ "$sv" -lt 1 ] || [ "$sv" -gt 10 ]; then
    echo "Error: Severity out of range (1-10)"
    exit 1
fi

if   [ "$sv" -le 2 ]; then pri=1
elif [ "$sv" -le 4 ]; then pri=2
elif [ "$sv" -le 6 ]; then pri=3
elif [ "$sv" -le 8 ]; then pri=4
else                        pri=5
fi

rec="$nm $ag $sv"

echo "--------------------------------------"
echo "Patient  : $nm"
echo "Age      : $ag"
echo "Severity : $sv/10"
echo "Priority : $pri (1=Critical, 5=Minor)"
echo "--------------------------------------"

echo "$rec" > /tmp/discharge_fifo 2>/dev/null || {
    echo "Warning: FIFO not ready yet. Queuing locally."
    echo "$rec" >> /tmp/triage_queue.txt
}

echo "Patient $nm queued successfully."
