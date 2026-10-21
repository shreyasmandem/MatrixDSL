//===----------------------------------------------------------------------===//
//
// MatrixDSL - AST -> Tensor IR lowering (Phase 3 / Review 3)
//
// Owner: Kandi Jeevitesh Reddy (24BCE0350)
//
// Turns a semantically-checked Program into a TensorFunction in SSA form.
// Variables are not storage: an assignment simply rebinds a name to the op
// that computed its value, so `T = matmul(A, B); Y = relu(T);` produces the
// same graph as `Y = relu(matmul(A, B));` - which is exactly what lets the
// fusion pass see through named temporaries.
//
// Observable behaviour is defined by `print`: each print becomes a Print op
// that pins its operand live; anything not reachable from a Print is dead.
// A declared matrix that is read before it is ever assigned is a program
// *input* and becomes a Load op carrying the variable's name.
//
//===----------------------------------------------------------------------===//

#ifndef MATRIXDSL_LOWERING_H
#define MATRIXDSL_LOWERING_H

#include "TensorIR.h"
#include "frontend/ast/AST.h"

#include <string>
#include <vector>

namespace matrixdsl {
namespace tensorir {

struct LoweringResult {
  bool ok = true;
  std::vector<std::string> errors; // lowering-time errors (not shape errors)
  std::vector<std::string> inputs; // names of program inputs, in first-use order
};

/// Precondition: `program` passed SemanticAnalyzer without errors, so every
/// Expr carries a valid resultType.
LoweringResult lowerToTensorIR(Program &program, TensorFunction &fn);

} // namespace tensorir
} // namespace matrixdsl

#endif // MATRIXDSL_LOWERING_H
