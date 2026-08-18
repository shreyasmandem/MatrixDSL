#!/usr/bin/env bash
#===----------------------------------------------------------------------===#
# MatrixDSL - Test script
#
# Usage:
#   ./scripts/test.sh                run every test
#   ./scripts/test.sh valid          run one category
#   ./scripts/test.sh invalid
#   ./scripts/test.sh boundary
#   ./scripts/test.sh --verbose      show full CTest output
#===----------------------------------------------------------------------===#

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build"

if [[ ! -d "${BUILD_DIR}" ]]; then
  echo "No build directory. Run ./scripts/build.sh first." >&2
  exit 1
fi

CATEGORY=""
VERBOSE=""

for arg in "$@"; do
  case "${arg}" in
    valid|invalid|boundary) CATEGORY="${arg}" ;;
    --verbose|-v)           VERBOSE="--output-on-failure --verbose" ;;
    -h|--help)              sed -n '3,11p' "$0"; exit 0 ;;
    *)                      echo "Unknown option: ${arg}" >&2; exit 1 ;;
  esac
done

RUNNER="${BUILD_DIR}/bin/testrunner"
COMPILER="${BUILD_DIR}/bin/matrixdslc"

if [[ -x "${RUNNER}" && -x "${COMPILER}" ]]; then
  echo "MatrixDSL test suite"
  echo
  "${RUNNER}" "${COMPILER}" "${ROOT}/tests" ${CATEGORY}
  exit $?
fi

# Fall back to CTest when the runner has not been built (for example, an
# LLVM-less frontend build where matrixdslc does not exist yet).
echo "testrunner not built; falling back to CTest"
cd "${BUILD_DIR}"
if [[ -n "${CATEGORY}" ]]; then
  ctest -R "suite_${CATEGORY}" ${VERBOSE:---output-on-failure}
else
  ctest ${VERBOSE:---output-on-failure}
fi
