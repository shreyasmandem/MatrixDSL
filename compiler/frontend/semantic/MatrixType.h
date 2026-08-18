//===----------------------------------------------------------------------===//
//
// MatrixDSL - Matrix Type System
//
// SHARED INTERFACE - FROZEN FOR REVIEW 1
// Owner: Mudpe Parth Tulsidas (24BDS0353)
//
// In MatrixDSL, shape IS the type. A matrix's dimensions are compile-time
// constants carried through the type system, so dimension errors are type
// errors and are caught before code generation.
//
//===----------------------------------------------------------------------===//

#ifndef MATRIXDSL_MATRIXTYPE_H
#define MATRIXDSL_MATRIXTYPE_H

#include <string>

namespace matrixdsl {

/// Project scope boundary. Dimensions outside [1, 256] are rejected.
constexpr int kMinDimension = 1;
constexpr int kMaxDimension = 256;

/// The MDT hardware matrix tile is 4x4 FP32. Matrices larger than this are
/// decomposed by the tiling pass in the backend.
constexpr int kTileSize = 4;

/// The type of a MatrixDSL value.
///
/// v1.0 has a single element type (FP32), so the type is fully described by
/// its dimensions. A default-constructed MatrixType (0x0) means "not yet
/// resolved" - the parser leaves expression types in this state and the
/// semantic analyzer fills them in.
class MatrixType {
public:
  MatrixType() = default;
  MatrixType(int rows, int cols) : rows_(rows), cols_(cols) {}

  int rows() const { return rows_; }
  int cols() const { return cols_; }

  /// True once the semantic analyzer has resolved this type.
  bool isValid() const { return rows_ > 0 && cols_ > 0; }

  /// A 1x1 matrix is how scalar literals are typed, so that scalar
  /// multiplication does not need a separate type in the system.
  bool isScalar() const { return rows_ == 1 && cols_ == 1; }

  bool isSquare() const { return rows_ == cols_; }

  /// True when the shape maps exactly onto MDT matrix registers with no
  /// zero-padding required.
  bool isTileAligned() const {
    return rows_ % kTileSize == 0 && cols_ % kTileSize == 0;
  }

  /// Number of elements. int is sufficient: 256 * 256 == 65536.
  int elementCount() const { return rows_ * cols_; }

  /// Storage size in bytes (FP32 elements).
  int byteSize() const { return elementCount() * 4; }

  /// Tile grid dimensions after padding up to a multiple of kTileSize.
  int tileRows() const { return (rows_ + kTileSize - 1) / kTileSize; }
  int tileCols() const { return (cols_ + kTileSize - 1) / kTileSize; }

  bool operator==(const MatrixType &other) const {
    return rows_ == other.rows_ && cols_ == other.cols_;
  }
  bool operator!=(const MatrixType &other) const { return !(*this == other); }

  /// "4x4", or "<unresolved>" when not yet typed.
  std::string toString() const;

  //===--------------------------------------------------------------------===//
  // Shape rules
  //
  // Each rule returns the result type on success, or an invalid MatrixType
  // (0x0) when the operation is not permitted. The caller is responsible for
  // producing the diagnostic - these functions only decide.
  //===--------------------------------------------------------------------===//

  /// A + B and A - B require identical shapes; the result has that shape.
  static MatrixType checkAddition(const MatrixType &lhs, const MatrixType &rhs);

  /// matmul(A, B) requires A.cols == B.rows; the result is A.rows x B.cols.
  static MatrixType checkMatMul(const MatrixType &lhs, const MatrixType &rhs);

  /// s * A preserves A's shape, where exactly one operand is a scalar.
  static MatrixType checkScalarMul(const MatrixType &lhs,
                                   const MatrixType &rhs);

  /// transpose(A) turns r x c into c x r.
  static MatrixType checkTranspose(const MatrixType &operand);

  /// relu(A) preserves shape.
  static MatrixType checkRelu(const MatrixType &operand);

  /// Dimension bounds check, used at declaration sites.
  static bool isValidDimension(int dim) {
    return dim >= kMinDimension && dim <= kMaxDimension;
  }

private:
  int rows_ = 0;
  int cols_ = 0;
};

} // namespace matrixdsl

#endif // MATRIXDSL_MATRIXTYPE_H
