//===----------------------------------------------------------------------===//
//
// MatrixDSL - Matrix Type System
//
// SHARED INTERFACE - FROZEN FOR REVIEW 1, EXTENDED FOR REVIEW 2 (PHASE 2)
// Owner: Mudpe Parth Tulsidas (24BDS0353)
//
// In MatrixDSL, shape IS the type. A matrix's dimensions are compile-time
// constants carried through the type system, so dimension errors are type
// errors and are caught before code generation.
//
// Phase 2 extension: a batch dimension. `tensor X[8][4][4]` is a batch of 8
// independent 4x4 matrices; `tensor W[4][4]` (or `matrix W[4][4]`, unchanged)
// is the batch=1 case. See docs/review2/Review2_Report.md section 2.1 for the
// rationale, and docs/tensor-ir.md for how batch shapes flow into the Tensor
// IR layer.
//
//===----------------------------------------------------------------------===//

#ifndef MATRIXDSL_MATRIXTYPE_H
#define MATRIXDSL_MATRIXTYPE_H

#include <string>

namespace matrixdsl {

/// Project scope boundary. Dimensions outside [1, 256] are rejected.
constexpr int kMinDimension = 1;
constexpr int kMaxDimension = 256;

/// Phase 2: batch count bounds. Batch is "how many independent matrices",
/// deliberately allowed a wider range than rows/cols since a batch of small
/// matrices is a common, cheap-to-declare shape (see the attention benchmark).
constexpr int kMinBatch = 1;
constexpr int kMaxBatch = 1024;

/// The MDT hardware matrix tile is 4x4 FP32. Matrices larger than this are
/// decomposed by the tiling pass in the backend. Batch is looped by the
/// tiling pass; it is not, itself, tiled.
constexpr int kTileSize = 4;

/// The type of a MatrixDSL value.
///
/// v1.0/Phase 1 had a single element type (FP32) and two dimensions, so the
/// type was fully described by rows and cols. Phase 2 adds a batch dimension,
/// defaulted to 1 everywhere a Phase 1 caller does not mention it, so every
/// existing two-argument construction, comparison and shape rule keeps its
/// Phase 1 meaning exactly. A default-constructed MatrixType (0x0x0) means
/// "not yet resolved" - the parser leaves expression types in this state and
/// the semantic analyzer fills them in.
class MatrixType {
public:
  MatrixType() = default;

  /// Phase 1 constructor - unchanged signature, batch defaults to 1.
  MatrixType(int rows, int cols) : batch_(1), rows_(rows), cols_(cols) {}

  /// Phase 2 constructor - explicit batch.
  MatrixType(int batch, int rows, int cols)
      : batch_(batch), rows_(rows), cols_(cols) {}

  int batch() const { return batch_; }
  int rows() const { return rows_; }
  int cols() const { return cols_; }

  /// True once the semantic analyzer has resolved this type.
  bool isValid() const { return batch_ > 0 && rows_ > 0 && cols_ > 0; }

  /// A 1x1x1 matrix is how scalar literals are typed, so that scalar
  /// multiplication does not need a separate type in the system.
  bool isScalar() const { return batch_ == 1 && rows_ == 1 && cols_ == 1; }

  bool isSquare() const { return rows_ == cols_; }

  /// True when this type carries more than one matrix instance.
  bool isBatched() const { return batch_ > 1; }

  /// True when the 2-D shape maps exactly onto MDT matrix registers with no
  /// zero-padding required. Batch does not affect tile alignment - each
  /// batch element is tiled independently by the same rule.
  bool isTileAligned() const {
    return rows_ % kTileSize == 0 && cols_ % kTileSize == 0;
  }

  /// Element count of a SINGLE matrix instance (unchanged from Phase 1 - does
  /// NOT multiply by batch). Existing Phase 1 callers see identical values,
  /// since Phase 1 never constructed a batch != 1 type.
  int elementCount() const { return rows_ * cols_; }

  /// Storage size in bytes of a single matrix instance (unchanged from
  /// Phase 1, per-slice, FP32 elements).
  int byteSize() const { return elementCount() * 4; }

  /// Phase 2: element count / byte size across the WHOLE batch. int is still
  /// sufficient at the project's bounds: 1024 * 256 * 256 fits comfortably.
  int totalElementCount() const { return batch_ * elementCount(); }
  int totalByteSize() const { return totalElementCount() * 4; }

  /// Tile grid dimensions after padding up to a multiple of kTileSize. Per
  /// matrix instance; the tiling pass loops this over the batch dimension
  /// (docs/memory-hierarchy.md section 6).
  int tileRows() const { return (rows_ + kTileSize - 1) / kTileSize; }
  int tileCols() const { return (cols_ + kTileSize - 1) / kTileSize; }

  bool operator==(const MatrixType &other) const {
    return batch_ == other.batch_ && rows_ == other.rows_ &&
           cols_ == other.cols_;
  }
  bool operator!=(const MatrixType &other) const { return !(*this == other); }

  /// "4x4" when batch == 1 (identical to Phase 1's format, unchanged so
  /// existing golden-output tests keep passing), "8x4x4" when batched, or
  /// "<unresolved>" when not yet typed.
  std::string toString() const;

  //===--------------------------------------------------------------------===//
  // Shape rules
  //
  // Each rule returns the result type on success, or an invalid MatrixType
  // (0x0x0) when the operation is not permitted. The caller is responsible
  // for producing the diagnostic - these functions only decide.
  //
  // Phase 2 batch rule, applied by every binary rule below before the
  // Phase 1 row/col rule: two operands are batch-compatible if their batch
  // counts are equal, or if one of them has batch == 1 (broadcast across the
  // batch). This is the ONLY broadcasting MatrixDSL performs - general
  // NumPy-style broadcasting across arbitrary dimensions is out of scope
  // (docs/review2/Review2_Report.md section 1.4).
  //===--------------------------------------------------------------------===//

  /// True if `a` and `b` may appear together in a binary operation under the
  /// batch-broadcast rule above.
  static bool isBatchCompatible(int a, int b) {
    return a == b || a == 1 || b == 1;
  }

  /// The batch count of a binary operation's result, given two
  /// batch-compatible operand counts (caller must check isBatchCompatible
  /// first; behaviour is undefined - returns 0 - otherwise, so a bug here
  /// cannot silently produce a plausible-looking wrong shape).
  static int resolveBatch(int a, int b) {
    if (!isBatchCompatible(a, b))
      return 0;
    return a == 1 ? b : a;
  }

  /// A + B and A - B require identical rows/cols and batch-compatible batch
  /// counts; the result has the resolved batch and the shared rows/cols.
  static MatrixType checkAddition(const MatrixType &lhs, const MatrixType &rhs);

  /// matmul(A, B) requires A.cols == B.rows and batch-compatible batch
  /// counts; the result is (resolvedBatch, A.rows, B.cols).
  static MatrixType checkMatMul(const MatrixType &lhs, const MatrixType &rhs);

  /// s * A preserves A's shape, where exactly one operand is a scalar
  /// (batch == 1, rows == cols == 1). The scalar side never contributes a
  /// batch count to the result.
  static MatrixType checkScalarMul(const MatrixType &lhs,
                                   const MatrixType &rhs);

  /// transpose(A) turns (b, r, c) into (b, c, r). Batch passes through
  /// unchanged - transpose acts within each batch element independently.
  static MatrixType checkTranspose(const MatrixType &operand);

  /// relu(A) preserves shape, including batch.
  static MatrixType checkRelu(const MatrixType &operand);

  /// Phase 2: softmax(A) preserves shape, including batch. Row-wise over the
  /// trailing (rows, cols) dimensions, applied independently per batch
  /// element - see docs/tensor-ir.md section 4 for the decomposition this
  /// shape rule corresponds to downstream.
  static MatrixType checkSoftmax(const MatrixType &operand);

  /// Dimension bounds check, used at declaration sites.
  static bool isValidDimension(int dim) {
    return dim >= kMinDimension && dim <= kMaxDimension;
  }

  /// Phase 2: batch bounds check, used at declaration sites.
  static bool isValidBatch(int batch) {
    return batch >= kMinBatch && batch <= kMaxBatch;
  }

private:
  int batch_ = 0;
  int rows_ = 0;
  int cols_ = 0;
};

} // namespace matrixdsl

#endif // MATRIXDSL_MATRIXTYPE_H
