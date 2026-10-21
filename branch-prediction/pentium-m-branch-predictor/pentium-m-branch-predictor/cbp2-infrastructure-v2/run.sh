#!/usr/bin/env bash
# Bash equivalent of the original csh "run" script (csh/dc are not always
# installed).  Runs src/predict on every trace and prints per-trace MPKI and
# the average.   Usage: ./run.sh traces
set -eu
if [ $# -lt 1 ]; then echo "Usage: $0 <trace-file-directory>"; exit 1; fi
cd "$(dirname "$0")"
make -s -C src predict
sum=0; n=0
for i in $(find "$1" -name '*.trace.*' | sort); do
	mpki=$(./src/predict "$i" | tail -1 | sed -e 's/ MPKI//')
	printf "%-40s\t%0.3f\n" "$i" "$mpki"
	sum=$(awk -v a="$sum" -v b="$mpki" 'BEGIN{printf "%.6f", a+b}')
	n=$((n+1))
done
awk -v s="$sum" -v n="$n" 'BEGIN{printf "average MPKI: %.3f\n", s/n}'
