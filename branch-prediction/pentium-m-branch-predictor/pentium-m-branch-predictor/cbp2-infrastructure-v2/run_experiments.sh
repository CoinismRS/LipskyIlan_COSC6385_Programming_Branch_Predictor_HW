#!/usr/bin/env bash
# Runs the design-space experiment driver over every trace and writes
# results/experiments.csv (one row per trace x predictor configuration).
# Usage: ./run_experiments.sh [trace-dir] [jobs]
set -euo pipefail
cd "$(dirname "$0")"
TRACES=${1:-traces}
JOBS=${2:-$(nproc 2>/dev/null || echo 2)}
make -s -C src experiments
mkdir -p ../results
OUT=../results/experiments.csv
echo "trace,predictor,cond_branches,mispredictions,mpki,global_hit_rate,global_hit_accuracy,bimodal_used_accuracy,bimodal_alone_accuracy,allocations,loop_used_rate,loop_used_accuracy,loop_allocations" > "$OUT"
find "$TRACES" -name '*.trace.*' | sort | xargs -P "$JOBS" -n 1 ./src/experiments | sort >> "$OUT"
echo "wrote $OUT"
