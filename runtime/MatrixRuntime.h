//===----------------------------------------------------------------------===//
//
// MatrixDSL - Runtime Support Library
//
// Owner: Kandi Jeevitesh Reddy (24BCE0350)
//
// Allocation and output support for compiled MatrixDSL programs. Deliberately
// free of LLVM and compiler dependencies so it can be linked into both
// natively compiled programs and the MDT simulator.
//
//===----------------------------------------------------------------------===//

#ifndef MATRIXDSL_MATRIXRUNTIME_H
#define MATRIXDSL_MATRIXRUNTIME_H

#include <cstddef>
#include <cstdint>

extern "C" {

/// Matrix descriptor. Layout must match the LLVM IR struct exactly:
///
///     %struct.Matrix = type { float*, i32, i32 }
///
/// Data is row-major and contiguous: data[i * cols + j].
typedef struct {
  float *data;
  int32_t rows;
  int32_t cols;
} MDMatrix;

//===----------------------------------------------------------------------===//
// Allocation
//===----------------------------------------------------------------------===//

/// Allocate a zero-initialised rows x cols matrix.
///
/// Storage is 64-byte aligned so that a 4x4 FP32 tile (exactly 64 bytes) never
/// straddles an alignment boundary - this keeps MDT tile loads to a single
/// aligned access.
MDMatrix *matrix_alloc(int32_t rows, int32_t cols);

/// Allocate and copy from an existing row-major buffer.
MDMatrix *matrix_alloc_from(int32_t rows, int32_t cols, const float *values);

void matrix_free(MDMatrix *m);

/// Zero-padded copy with dimensions rounded up to a multiple of 4, used by the
/// tiling path for matrices whose dimensions are not tile-aligned.
MDMatrix *matrix_pad_to_tile(const MDMatrix *m);

//===----------------------------------------------------------------------===//
// Element access
//===----------------------------------------------------------------------===//

static inline float matrix_get(const MDMatrix *m, int32_t i, int32_t j) {
  return m->data[i * m->cols + j];
}

static inline void matrix_set(MDMatrix *m, int32_t i, int32_t j, float v) {
  m->data[i * m->cols + j] = v;
}

//===----------------------------------------------------------------------===//
// Output
//===----------------------------------------------------------------------===//

/// Print in the standard MatrixDSL format - fixed width, two decimals:
///
///   [    1.00     2.00 ]
///   [    3.00     4.00 ]
void matrix_print(const MDMatrix *m);

void matrix_print_named(const char *name, const MDMatrix *m);

//===----------------------------------------------------------------------===//
// Reference implementations
//
// Straightforward scalar implementations used as the oracle for differential
// testing: MDT simulator output must agree with these element for element.
//===----------------------------------------------------------------------===//

void matrix_add_ref(MDMatrix *dst, const MDMatrix *a, const MDMatrix *b);
void matrix_sub_ref(MDMatrix *dst, const MDMatrix *a, const MDMatrix *b);
void matrix_scalar_mul_ref(MDMatrix *dst, float s, const MDMatrix *a);
void matrix_matmul_ref(MDMatrix *dst, const MDMatrix *a, const MDMatrix *b);
void matrix_transpose_ref(MDMatrix *dst, const MDMatrix *a);
void matrix_relu_ref(MDMatrix *dst, const MDMatrix *a);

/// Compare with tolerance. Tiling changes the order of accumulation, so exact
/// bitwise equality is not expected for multiplies; 1e-5 relative error is.
int matrix_equal(const MDMatrix *a, const MDMatrix *b, float tolerance);

} // extern "C"

#endif // MATRIXDSL_MATRIXRUNTIME_H
