//===----------------------------------------------------------------------===//
//
// MatrixDSL - Tensor IR reference interpreter (Phase 3 / Review 3)
//
// Owner: Kandi Jeevitesh Reddy (24BCE0350)
//
// The oracle every compiled program is checked against. It evaluates a
// TensorFunction directly in FP32 on the host - no tiles, no registers, no
// scratchpad - so a disagreement with the MDT simulator's output points at
// the passes or the code generator, never at the oracle's own scheduling.
//
// Program inputs (declared, never-assigned matrices) are filled by
// makeInputs() from a seeded generator, with values that are multiples of
// 1/8 in [-2, 2]: products and sums of such values are exact in FP32 for
// every size MatrixDSL allows, so most comparisons are bit-exact and the
// tolerance only matters after softmax.
//
//===----------------------------------------------------------------------===//

#ifndef MATRIXDSL_INTERPRETER_H
#define MATRIXDSL_INTERPRETER_H

#include "TensorIR.h"

#include <map>
#include <string>
#include <vector>

namespace matrixdsl {
namespace tensorir {

/// Input name -> batch*rows*cols values, row-major, no padding.
using InputMap = std::map<std::string, std::vector<float>>;

/// One print(X), in program order.
struct PrintValue {
  std::string name;
  int batch = 1, rows = 0, cols = 0;
  std::vector<float> values;
};

/// Deterministic inputs for every Load op in `fn`.
InputMap makeInputs(const TensorFunction &fn, unsigned seed = 2026);

/// Evaluate `fn`. Returns false (with `error` set) on an unsupported op.
bool interpret(const TensorFunction &fn, const InputMap &inputs,
               std::vector<PrintValue> &prints, std::string &error);

/// Compare two print streams; `report` describes the first mismatch.
bool samePrints(const std::vector<PrintValue> &expected,
                const std::vector<PrintValue> &actual, double tolerance,
                std::string &report);

} // namespace tensorir
} // namespace matrixdsl

#endif // MATRIXDSL_INTERPRETER_H
