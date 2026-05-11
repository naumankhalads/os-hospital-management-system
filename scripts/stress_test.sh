#!/bin/bash
# ============================================================
# Project : Hospital Patient Triage & Bed Allocator
# Script  : stress_test.sh
# Purpose : Spawn 20 patient arrivals in rapid succession
# Usage   : ./scripts/stress_test.sh
# ============================================================

names=("Ali" "Sara" "Umar" "Hina" "Zain" "Mia" "John" "Amy"
       "Bilal" "Nida" "Raza" "Sana" "Asad" "Taha" "Farah"
       "Imran" "Layla" "Omar" "Zara" "Hamza")

echo "[STRESS] Starting 20 patient stress test..."

for i in $(seq 0 19); do
    nm=${names[$i]}
    ag=$((20 + RANDOM % 60))
    sv=$((1 + RANDOM % 10))
    echo "[STRESS] Sending patient $nm age=$ag sev=$sv"
    bash scripts/triage.sh "$nm" "$ag" "$sv" &
    sleep 0.2
done

wait
echo "[STRESS] All 20 patients submitted."
