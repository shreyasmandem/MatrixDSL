#!/usr/bin/env bash
# MatrixDSL - differential verification and the ablation study.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/build/bin"
"$BIN/mtxverify" "$ROOT/tests/e2e" "$ROOT/examples" "$ROOT/benchmarks/workloads"
"$BIN/mtxbench" "$ROOT/benchmarks/workloads" --md "$ROOT/build/results.md" --csv "$ROOT/build/results.csv"
