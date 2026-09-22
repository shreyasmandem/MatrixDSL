//===----------------------------------------------------------------------===//
//
// MatrixDSL - Matrix Type System implementation
//
// Owner: Mudpe Parth Tulsidas (24BDS0353)
//
// In MatrixDSL, shape IS the type. Every shape rule below returns the result
// type on success, or an invalid MatrixType (0x0x0) when the operation is not
// permitted.
//
// These functions only DECIDE. They never emit diagnostics - that is the
// caller's job. Keeping the decision separate from the message means the same
// rule can be reused by the analyzer, by tests, and by any future tooling
// without dragging diagnostic formatting along with it.
//
// Phase 2: every binary rule now also checks batch compatibility
// (isBatchCompatible/resolveBatch, defined inline in MatrixType.h) before its
// Phase 1 row/col rule. A Phase 1 caller that never constructs a batch != 1
// type sees byte-for-byte the same results as before, since resolveBatch(1,1)
// == 1 and isBatchCompatible(1,1) is trivially true.
//
//===----------------------------------------------------------------------===//

#include "MatrixType.h"

#include <string>

namespace matrixdsl {

std::string MatrixType::toString() const {
  if (!isValid())
    return "<unresolved>";
  if (batch_ > 1)
    return std::to_string(batch_) + "x" + std::to_string(rows_) + "x" +
           std::to_string(cols_);
  return std::to_string(rows_) + "x" + std::to_string(cols_);
}

//===----------------------------------------------------------------------===//
// Shape rules
//===----------------------------------------------------------------------===//

/// A + B and A - B require identical rows/cols and batch-compatible batch
/// counts; the result has the resolved batch and the shared rows/cols.
MatrixType MatrixType::checkAddition(const MatrixType &lhs,
                                     const MatrixType &rhs) {
  // An unresolved operand means an earlier error already reported. Return
  // invalid without deciding, so the caller can suppress a cascade rather than
  // reporting a second, meaningless error about the same expression.
  if (!lhs.isValid() || !rhs.isValid())
    return MatrixType();

  if (!isBatchCompatible(lhs.batch_, rhs.batch_))
    return MatrixType();

  if (lhs.rows_ != rhs.rows_ || lhs.cols_ != rhs.cols_)
    return MatrixType();

  return MatrixType(resolveBatch(lhs.batch_, rhs.batch_), lhs.rows_, lhs.cols_);
}

/// matmul(A, B) requires A.cols == B.rows and batch-compatible batch counts;
/// the result is (resolvedBatch, A.rows, B.cols).
///
/// This is the rule the entire language exists to enforce at compile time.
MatrixType MatrixType::checkMatMul(const MatrixType &lhs,
                                   const MatrixType &rhs) {
  if (!lhs.isValid() || !rhs.isValid())
    return MatrixType();

  if (!isBatchCompatible(lhs.batch_, rhs.batch_))
    return MatrixType();

  if (lhs.cols_ != rhs.rows_)
    return MatrixType();

  return MatrixType(resolveBatch(lhs.batch_, rhs.batch_), lhs.rows_, rhs.cols_);
}

/// s * A preserves A's shape (including its batch), where exactly one
/// operand is a scalar (batch == 1, 1x1).
///
/// Scalars are typed 1x1x1, which creates a genuine ambiguity: a 1x1x1 times
/// another 1x1x1 is both a scalar product and a legal matrix multiply. They
/// agree on the result, so the ambiguity is harmless and is resolved in
/// favour of the scalar interpretation.
MatrixType MatrixType::checkScalarMul(const MatrixType &lhs,
                                      const MatrixType &rhs) {
  if (!lhs.isValid() || !rhs.isValid())
    return MatrixType();

  if (lhs.isScalar())
    return MatrixType(rhs.batch_, rhs.rows_, rhs.cols_);

  if (rhs.isScalar())
    return MatrixType(lhs.batch_, lhs.rows_, lhs.cols_);

  // Neither side is a scalar, so this is not scalar multiplication. The caller
  // falls back to checkMatMul.
  return MatrixType();
}

/// transpose(A) turns (b, r, c) into (b, c, r). Batch passes through
/// unchanged - transpose acts within each batch element independently.
MatrixType MatrixType::checkTranspose(const MatrixType &operand) {
  if (!operand.isValid())
    return MatrixType();

  return MatrixType(operand.batch_, operand.cols_, operand.rows_);
}

/// relu(A) preserves shape, including batch - it is element-wise.
MatrixType MatrixType::checkRelu(const MatrixType &operand) {
  if (!operand.isValid())
    return MatrixType();

  return MatrixType(operand.batch_, operand.rows_, operand.cols_);
}

/// Phase 2: softmax(A) preserves shape, including batch - row-wise over the
/// trailing (rows, cols) dimensions, applied independently per batch
/// element. Structurally identical to checkRelu; kept as a separate function
/// because the two operations are conceptually distinct (softmax's row-wise
/// normalisation has real internal structure - see docs/tensor-ir.md section
/// 4 - even though neither one changes the shape).
MatrixType MatrixType::checkSoftmax(const MatrixType &operand) {
  if (!operand.isValid())
    return MatrixType();

  return MatrixType(operand.batch_, operand.rows_, operand.cols_);
}

} // namespace matrixdsl
