#!/usr/bin/env bash
#===----------------------------------------------------------------------===#
# MatrixDSL - Build script
#
# Usage:
#   ./scripts/build.sh                 full build (requires LLVM)
#   ./scripts/build.sh --sim-only      MDT simulator only, no LLVM needed
#   ./scripts/build.sh --frontend-only lexer/parser/sema only, no LLVM needed
#   ./scripts/build.sh --release       optimized build
#   ./scripts/build.sh --clean         remove the build directory first
#===----------------------------------------------------------------------===#

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build"
BUILD_TYPE="Debug"
CMAKE_EXTRA=()

for arg in "$@"; do
  case "${arg}" in
    --sim-only)      CMAKE_EXTRA+=("-DMATRIXDSL_BUILD_SIM_ONLY=ON") ;;
    --frontend-only) CMAKE_EXTRA+=("-DMATRIXDSL_BUILD_FRONTEND_ONLY=ON") ;;
    --benchmarks)    CMAKE_EXTRA+=("-DMATRIXDSL_BUILD_BENCHMARKS=ON") ;;
    --release)       BUILD_TYPE="Release" ;;
    --clean)         echo "Removing ${BUILD_DIR}"; rm -rf "${BUILD_DIR}" ;;
    -h|--help)       sed -n '3,12p' "$0"; exit 0 ;;
    *)               echo "Unknown option: ${arg}" >&2; exit 1 ;;
  esac
done

echo "MatrixDSL build"
echo "  root : ${ROOT}"
echo "  type : ${BUILD_TYPE}"

# LLVM_DIR may be supplied by the environment. Without it, CMake falls back to
# a frontend + simulator build and says so.
if [[ -n "${LLVM_DIR:-}" ]]; then
  echo "  llvm : ${LLVM_DIR}"
  CMAKE_EXTRA+=("-DLLVM_DIR=${LLVM_DIR}")
fi

cmake -S "${ROOT}" -B "${BUILD_DIR}" \
      -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
      "${CMAKE_EXTRA[@]}"

JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
cmake --build "${BUILD_DIR}" -j "${JOBS}"

echo
echo "Build complete. Binaries in ${BUILD_DIR}/bin"
ls -1 "${BUILD_DIR}/bin" 2>/dev/null || true
