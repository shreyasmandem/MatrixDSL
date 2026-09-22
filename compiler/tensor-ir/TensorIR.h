//===----------------------------------------------------------------------===//
//
// MatrixDSL - Tensor IR
//
// SHARED INTERFACE - FROZEN FOR REVIEW 2 (PHASE 2)
// Owner: Kandi Jeevitesh Reddy (24BCE0350)
//
// The layer between semantic analysis and LLVM IR generation recommended by
// docs/review2/Review2_Report.md section 3: a small, hand-rolled IR inspired
// by MLIR's progressive-lowering idea, not built on the MLIR framework. Full
// design and rationale: docs/tensor-ir.md.
//
// This header is the frozen contract everyone else builds against:
//   - Vinay/Parth's AST -> Tensor IR lowering targets these op kinds
//   - Shreyas's docs/isa-extensions.md section 6 lowering table is keyed by
//     the Tier 2 kinds defined here
// The construction helpers, the fusion pass, and the tiling pass are Kandi's
// individual Phase 2 implementation (TensorIR.cpp, FusionPass.cpp,
// TilingPass.cpp), not part of this frozen header.
//
// Design note: TensorOp is one tagged class rather than a subclass hierarchy
// per kind, following the same style already used by mdtsim's Instruction/
// Operand types (tools/mdtsim/MDTSim.h). This is an internal IR with one
// consumer (Kandi's own passes and, downstream, the LLVM IR generator), not a
// four-way shared contract like AST.h, so the simpler representation is a
// deliberate choice, not an oversight.
//
//===----------------------------------------------------------------------===//

#ifndef MATRIXDSL_TENSORIR_H
#define MATRIXDSL_TENSORIR_H

#include <memory>
#include <string>
#include <vector>

namespace matrixdsl {
namespace tensorir {

//===----------------------------------------------------------------------===//
// Shape
//===----------------------------------------------------------------------===//

/// A Tensor IR value's shape. Mirrors MatrixType's (batch, rows, cols) triple
/// exactly - see compiler/frontend/semantic/MatrixType.h - so that lowering
/// from a shape-annotated AST is a direct field copy, not a reinterpretation.
struct TensorShape {
  int batch = 0;
  int rows = 0;
  int cols = 0;

  TensorShape() = default;
  TensorShape(int batch, int rows, int cols)
      : batch(batch), rows(rows), cols(cols) {}

  bool isValid() const { return batch > 0 && rows > 0 && cols > 0; }
  bool isBatched() const { return batch > 1; }

  bool operator==(const TensorShape &other) const {
    return batch == other.batch && rows == other.rows && cols == other.cols;
  }
  bool operator!=(const TensorShape &other) const { return !(*this == other); }

  /// "4x4" when batch == 1, "8x4x4" when batched - same format convention as
  /// MatrixType::toString().
  std::string toString() const;
};

//===----------------------------------------------------------------------===//
// Operation kinds - docs/tensor-ir.md section 3
//===----------------------------------------------------------------------===//

enum class TensorOpKind {
  // Tier 1 - source-level, ~1:1 with the AST (docs/tensor-ir.md 3.1)
  Const,
  Load,
  Add,
  Sub,
  ScalarMul,
  MatMul,
  Transpose,
  Relu,
  Softmax,
  RowMax,
  RowSum,
  Exp,
  Print,

  // Tier 2 - produced only by the fusion and tiling passes (3.2)
  FusedMatMulRelu,
  FusedMatMulAcc,
  TiledMatMul,
  ScratchLoad,
  ScratchStore,
  ConvertPrecision,
};

/// Debug name for a kind, e.g. "MatMul". Implemented in TensorIR.cpp.
const char *tensorOpKindName(TensorOpKind kind);

/// True for the six Tier 2 kinds - a Tier 1 op should never reach LLVM IR
/// generation still tagged Tier 1 if a fusible pattern matched it; this
/// distinguishes "already lowered/fused" ops from "still source-level" ones
/// for passes and for debug dumps.
bool isTier2(TensorOpKind kind);

//===----------------------------------------------------------------------===//
// Operation
//===----------------------------------------------------------------------===//

/// A single Tensor IR operation. Every field is present on every op; only the
/// fields relevant to `kind` are meaningful, following the same tagged-struct
/// convention as mdtsim's Instruction type. `operands` are non-owning raw
/// pointers into the owning TensorFunction's op list (see TensorFunction::
/// addOp) - valid for the function's lifetime, since growing the owning
/// vector of unique_ptr moves the smart pointers, never the pointees.
struct TensorOp {
  TensorOpKind kind;
  TensorShape shape;
  int id = -1;

  /// Operand ops, in a kind-dependent order documented alongside each kind
  /// below. Empty for Const, Load, and any 0-ary op.
  std::vector<TensorOp *> operands;

  /// Const only: the literal value.
  double constValue = 0.0;

  /// Load, Print, ScratchLoad, ScratchStore only: the source/destination
  /// variable name as it appears in MatrixDSL source.
  std::string refName;

  /// False for Load/Const/Print and for any op the fusion pass has already
  /// consumed into a Tier 2 op - a pass must not attempt to fuse a
  /// non-fusible op or an op that has already been rewritten away.
  bool fusible = true;

  /// ConvertPrecision only: true converts FP32 storage to BF16, false
  /// converts BF16 back to FP32 (docs/isa-extensions.md section 2.3).
  bool toBF16 = false;

  //===------------------------------------------------------------------===//
  // Per-kind operand order (documented, not enforced by the type system -
  // enforcing it structurally would mean the subclass hierarchy this header
  // deliberately avoids; the fusion/tiling pass tests in
  // tests/tensor-ir/ are what actually catch a violation)
  //===------------------------------------------------------------------===//
  //
  //   Add, Sub, MatMul, FusedMatMulRelu   : operands = { lhs, rhs }
  //   ScalarMul                            : operands = { scalar, tensor }
  //   FusedMatMulAcc                       : operands = { lhs, rhs, acc }
  //   Transpose, Relu, Softmax, RowMax,
  //     RowSum, Exp, Print, ScratchLoad,
  //     ScratchStore, ConvertPrecision     : operands = { operand }
  //   Const, Load                          : operands = {}  (0-ary)
  //   TiledMatMul                          : operands = { lhs, rhs }, plus
  //                                           shape carries the full
  //                                           (pre-tiling) result shape;
  //                                           tile-loop structure itself is
  //                                           not represented as Tensor IR
  //                                           nodes - see docs/tensor-ir.md
  //                                           and docs/memory-hierarchy.md
  //                                           section 6 for the loop the
  //                                           tiling pass emits directly
  //                                           into the LLVM IR generator's
  //                                           output instead.
};

//===----------------------------------------------------------------------===//
// Function - an ordered, owning list of operations
//===----------------------------------------------------------------------===//

/// One MatrixDSL program's Tensor IR, in construction order. There is no
/// control flow at this level (matching the source language, which has none)
/// and no basic blocks - a TensorFunction is a straight-line list of ops,
/// each referencing only earlier ops in the list (a standard SSA-like
/// invariant, checked by tests rather than the type system).
class TensorFunction {
public:
  /// Construct and append a new op, returning a non-owning pointer valid for
  /// the function's lifetime. `id` is assigned automatically.
  TensorOp *addOp(TensorOpKind kind, TensorShape shape,
                  std::vector<TensorOp *> operands = {});

  const std::vector<std::unique_ptr<TensorOp>> &ops() const { return ops_; }
  size_t size() const { return ops_.size(); }

  /// Render every op, one per line, in the format:
  ///   %3 = MatMul %1, %2 : 4x4
  /// Implemented in TensorIR.cpp; used by tests and by the --dump-tensor-ir
  /// mode of whichever demo tool exercises this function.
  std::string dump() const;

private:
  std::vector<std::unique_ptr<TensorOp>> ops_;
};

} // namespace tensorir
} // namespace matrixdsl

#endif // MATRIXDSL_TENSORIR_H
