//===----------------------------------------------------------------------===//
//
// MatrixDSL - Matrix Type System implementation
//
// Owner: Mudpe Parth Tulsidas (24BDS0353)
//
// In MatrixDSL, shape IS the type. Every shape rule below returns the result
// type on success, or an invalid MatrixType (0x0) when the operation is not
// permitted.
//
// These functions only DECIDE. They never emit diagnostics - that is the
// caller's job. Keeping the decision separate from the message means the same
// rule can be reused by the analyzer, by tests, and by any future tooling
// without dragging diagnostic formatting along with it.
//
//===----------------------------------------------------------------------===//

#include "MatrixType.h"

#include <string>

namespace matrixdsl {

std::string MatrixType::toString() const {
  if (!isValid())
    return "<unresolved>";
  return std::to_string(rows_) + "x" + std::to_string(cols_);
}

//===----------------------------------------------------------------------===//
// Shape rules
//===----------------------------------------------------------------------===//

/// A + B and A - B require identical shapes; the result has that shape.
MatrixType MatrixType::checkAddition(const MatrixType &lhs,
                                     const MatrixType &rhs) {
  // An unresolved operand means an earlier error already reported. Return
  // invalid without deciding, so the caller can suppress a cascade rather than
  // reporting a second, meaningless error about the same expression.
  if (!lhs.isValid() || !rhs.isValid())
    return MatrixType();

  if (lhs.rows_ != rhs.rows_ || lhs.cols_ != rhs.cols_)
    return MatrixType();

  return MatrixType(lhs.rows_, lhs.cols_);
}

/// matmul(A, B) requires A.cols == B.rows; the result is A.rows x B.cols.
///
/// This is the rule the entire language exists to enforce at compile time.
MatrixType MatrixType::checkMatMul(const MatrixType &lhs,
                                   const MatrixType &rhs) {
  if (!lhs.isValid() || !rhs.isValid())
    return MatrixType();

  if (lhs.cols_ != rhs.rows_)
    return MatrixType();

  return MatrixType(lhs.rows_, rhs.cols_);
}

/// s * A preserves A's shape, where exactly one operand is a scalar.
///
/// Scalars are typed 1x1, which creates a genuine ambiguity: 1x1 * 1x1 is both
/// a scalar product and a legal 1x1 matrix multiply. They agree on the result
/// (1x1), so the ambiguity is harmless and is resolved in favour of the scalar
/// interpretation.
MatrixType MatrixType::checkScalarMul(const MatrixType &lhs,
                                      const MatrixType &rhs) {
  if (!lhs.isValid() || !rhs.isValid())
    return MatrixType();

  if (lhs.isScalar())
    return MatrixType(rhs.rows_, rhs.cols_);

  if (rhs.isScalar())
    return MatrixType(lhs.rows_, lhs.cols_);

  // Neither side is a scalar, so this is not scalar multiplication. The caller
  // falls back to checkMatMul.
  return MatrixType();
}

/// transpose(A) turns r x c into c x r.
MatrixType MatrixType::checkTranspose(const MatrixType &operand) {
  if (!operand.isValid())
    return MatrixType();

  return MatrixType(operand.cols_, operand.rows_);
}

/// relu(A) preserves shape - it is element-wise.
MatrixType MatrixType::checkRelu(const MatrixType &operand) {
  if (!operand.isValid())
    return MatrixType();

  return MatrixType(operand.rows_, operand.cols_);
}

} // namespace matrixdsl
