//===----------------------------------------------------------------------===//
// MatrixDSL - Operator fusion pass implementation
// Owner: Kandi Jeevitesh Reddy (24BCE0350)
//===----------------------------------------------------------------------===//

#include "FusionPass.h"

#include <unordered_map>

namespace matrixdsl {
namespace tensorir {

namespace {

/// Use-count per op, computed once over the whole function. A pattern only
/// fuses when its intermediate value has exactly one use (the op being
/// fused into) - fusing a value consumed elsewhere would require
/// materialising it anyway, defeating the point (docs/tensor-ir.md 5).
std::unordered_map<const TensorOp *, int> countUses(const TensorFunction &fn) {
  std::unordered_map<const TensorOp *, int> uses;
  for (const std::unique_ptr<TensorOp> &op : fn.ops())
    for (const TensorOp *operand : op->operands)
      ++uses[operand];
  return uses;
}

} // namespace

int runFusionPass(TensorFunction &fn) {
  const std::unordered_map<const TensorOp *, int> uses = countUses(fn);
  int fused = 0;

  for (const std::unique_ptr<TensorOp> &opPtr : fn.ops()) {
    TensorOp *op = opPtr.get();

    // Pattern: MatMul -> Relu, MatMul has no other use.
    if (op->kind == TensorOpKind::Relu && op->operands.size() == 1) {
      TensorOp *producer = op->operands[0];
      const auto it = uses.find(producer);
      const int useCount = (it != uses.end()) ? it->second : 0;

      if (producer->kind == TensorOpKind::MatMul && useCount == 1) {
        op->kind = TensorOpKind::FusedMatMulRelu;
        op->operands = producer->operands; // {lhs, rhs} of the original matmul
        producer->fusible = false; // the old MatMul op is dead code now
        ++fused;
        continue;
      }
    }

    // Pattern: MatMul -> Add(bias), where the Add's OTHER operand is not
    // itself derived from the same MatMul (the report's FusedMatMulAcc
    // pattern - docs/tensor-ir.md 5).
    if (op->kind == TensorOpKind::Add && op->operands.size() == 2) {
      for (int side = 0; side < 2; ++side) {
        TensorOp *producer = op->operands[side];
        TensorOp *bias = op->operands[1 - side];
        const auto it = uses.find(producer);
        const int useCount = (it != uses.end()) ? it->second : 0;

        if (producer->kind == TensorOpKind::MatMul && useCount == 1 &&
            bias != producer) {
          op->kind = TensorOpKind::FusedMatMulAcc;
          op->operands = {producer->operands[0], producer->operands[1], bias};
          producer->fusible = false;
          ++fused;
          break;
        }
      }
    }
  }

  return fused;
}

} // namespace tensorir
} // namespace matrixdsl
