#!/usr/bin/env bash
# MatrixDSL - build everything and run the test suite.
#   ./scripts/build.sh            Release build + ctest
#   ./scripts/build.sh --clean    remove build/ first
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
[[ "${1:-}" == "--clean" ]] && rm -rf "$ROOT/build"
cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT/build" -j
ctest --test-dir "$ROOT/build" --output-on-failure
