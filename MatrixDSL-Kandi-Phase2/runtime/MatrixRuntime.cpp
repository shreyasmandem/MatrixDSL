//===----------------------------------------------------------------------===//
//
// MatrixDSL - Runtime Support Library implementation
//
// Owner: Kandi Jeevitesh Reddy (24BCE0350)
//
// Allocation, output, and the reference implementations of every MatrixDSL
// operation. Deliberately free of LLVM and compiler dependencies so it links
// into both natively compiled programs and the MDT verification path.
//
// The reference implementations are the oracle for differential testing: MDT
// simulator output must agree with these element for element. They are written
// for obvious correctness, not for speed.
//
//===----------------------------------------------------------------------===//

#include "MatrixRuntime.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

extern "C" {

//===----------------------------------------------------------------------===//
// Allocation
//
// Storage is 64-byte aligned because one MDT matrix register holds exactly a
// 64-byte 4x4 FP32 tile. Aligning the base means a tile load never straddles
// an alignment boundary, so it stays a single aligned transfer.
//===----------------------------------------------------------------------===//

namespace {

constexpr size_t kTileAlignment = 64;

void *alignedAlloc(size_t bytes) {
  // Round up to a multiple of the alignment - required by aligned_alloc, and
  // harmless for the Windows path.
  const size_t rounded = (bytes + kTileAlignment - 1) / kTileAlignment * kTileAlignment;

#if defined(_WIN32)
  return _aligned_malloc(rounded, kTileAlignment);
#else
  return std::aligned_alloc(kTileAlignment, rounded);
#endif
}

void alignedFree(void *p) {
#if defined(_WIN32)
  _aligned_free(p);
#else
  std::free(p);
#endif
}

} // namespace

MDMatrix *matrix_alloc(int32_t rows, int32_t cols) {
  if (rows <= 0 || cols <= 0)
    return nullptr;

  MDMatrix *m = static_cast<MDMatrix *>(std::malloc(sizeof(MDMatrix)));
  if (!m)
    return nullptr;

  const size_t elements = static_cast<size_t>(rows) * static_cast<size_t>(cols);
  m->data = static_cast<float *>(alignedAlloc(elements * sizeof(float)));
  if (!m->data) {
    std::free(m);
    return nullptr;
  }

  std::memset(m->data, 0, elements * sizeof(float));
  m->rows = rows;
  m->cols = cols;
  return m;
}

MDMatrix *matrix_alloc_from(int32_t rows, int32_t cols, const float *values) {
  MDMatrix *m = matrix_alloc(rows, cols);
  if (!m || !values)
    return m;

  const size_t elements = static_cast<size_t>(rows) * static_cast<size_t>(cols);
  std::memcpy(m->data, values, elements * sizeof(float));
  return m;
}

void matrix_free(MDMatrix *m) {
  if (!m)
    return;
  alignedFree(m->data);
  std::free(m);
}

/// Zero-padded copy with both dimensions rounded up to a multiple of 4.
///
/// A 5x7 matrix does not divide evenly into 4x4 tiles, so the tiling path pads
/// it to 8x8. The logical dimensions are preserved in the original descriptor,
/// so print and shape checking still observe the true size - only the tile
/// computation sees the padded copy.
MDMatrix *matrix_pad_to_tile(const MDMatrix *m) {
  if (!m)
    return nullptr;

  const int32_t paddedRows = (m->rows + 3) / 4 * 4;
  const int32_t paddedCols = (m->cols + 3) / 4 * 4;

  MDMatrix *out = matrix_alloc(paddedRows, paddedCols);
  if (!out)
    return nullptr;

  // Copy row by row - the source and destination row strides differ.
  for (int32_t i = 0; i < m->rows; ++i)
    std::memcpy(out->data + static_cast<size_t>(i) * paddedCols,
                m->data + static_cast<size_t>(i) * m->cols,
                static_cast<size_t>(m->cols) * sizeof(float));

  return out;
}

//===----------------------------------------------------------------------===//
// Output
//===----------------------------------------------------------------------===//

void matrix_print(const MDMatrix *m) {
  if (!m) {
    std::printf("  (null matrix)\n");
    return;
  }

  for (int32_t i = 0; i < m->rows; ++i) {
    std::printf("  [");
    for (int32_t j = 0; j < m->cols; ++j)
      std::printf("%9.2f", m->data[static_cast<size_t>(i) * m->cols + j]);
    std::printf(" ]\n");
  }
}

void matrix_print_named(const char *name, const MDMatrix *m) {
  if (!m) {
    std::printf("%s = (null)\n", name ? name : "?");
    return;
  }
  std::printf("%s = (%dx%d)\n", name ? name : "?", m->rows, m->cols);
  matrix_print(m);
}

//===----------------------------------------------------------------------===//
// Reference implementations
//
// Straightforward and obviously correct. These define what "right" means for
// every MatrixDSL operation; the compiled MDT path is checked against them.
//===----------------------------------------------------------------------===//

void matrix_add_ref(MDMatrix *dst, const MDMatrix *a, const MDMatrix *b) {
  if (!dst || !a || !b)
    return;
  const size_t n = static_cast<size_t>(a->rows) * a->cols;
  for (size_t i = 0; i < n; ++i)
    dst->data[i] = a->data[i] + b->data[i];
}

void matrix_sub_ref(MDMatrix *dst, const MDMatrix *a, const MDMatrix *b) {
  if (!dst || !a || !b)
    return;
  const size_t n = static_cast<size_t>(a->rows) * a->cols;
  for (size_t i = 0; i < n; ++i)
    dst->data[i] = a->data[i] - b->data[i];
}

void matrix_scalar_mul_ref(MDMatrix *dst, float s, const MDMatrix *a) {
  if (!dst || !a)
    return;
  const size_t n = static_cast<size_t>(a->rows) * a->cols;
  for (size_t i = 0; i < n; ++i)
    dst->data[i] = s * a->data[i];
}

/// dst = a x b, where a is MxK, b is KxN, dst is MxN.
///
/// The k loop is innermost so the accumulation order matches the naive
/// definition exactly. The tiled MDT path reorders this, which is why
/// comparison against it uses a tolerance rather than bitwise equality.
void matrix_matmul_ref(MDMatrix *dst, const MDMatrix *a, const MDMatrix *b) {
  if (!dst || !a || !b)
    return;

  const int32_t M = a->rows;
  const int32_t K = a->cols;
  const int32_t N = b->cols;

  for (int32_t i = 0; i < M; ++i)
    for (int32_t j = 0; j < N; ++j) {
      float sum = 0.0f;
      for (int32_t k = 0; k < K; ++k)
        sum += a->data[static_cast<size_t>(i) * K + k] *
               b->data[static_cast<size_t>(k) * N + j];
      dst->data[static_cast<size_t>(i) * N + j] = sum;
    }
}

void matrix_transpose_ref(MDMatrix *dst, const MDMatrix *a) {
  if (!dst || !a)
    return;
  for (int32_t i = 0; i < a->rows; ++i)
    for (int32_t j = 0; j < a->cols; ++j)
      dst->data[static_cast<size_t>(j) * a->rows + i] =
          a->data[static_cast<size_t>(i) * a->cols + j];
}

void matrix_relu_ref(MDMatrix *dst, const MDMatrix *a) {
  if (!dst || !a)
    return;
  const size_t n = static_cast<size_t>(a->rows) * a->cols;
  for (size_t i = 0; i < n; ++i)
    dst->data[i] = a->data[i] > 0.0f ? a->data[i] : 0.0f;
}

//===----------------------------------------------------------------------===//
// Comparison
//===----------------------------------------------------------------------===//

/// Element-wise comparison with an absolute tolerance.
///
/// Tiling changes the order in which products are accumulated, and FP32
/// addition is not associative, so a tiled multiply legitimately differs from
/// the reference in the low bits. Exact equality would produce false failures.
int matrix_equal(const MDMatrix *a, const MDMatrix *b, float tolerance) {
  if (!a || !b)
    return 0;
  if (a->rows != b->rows || a->cols != b->cols)
    return 0;

  const size_t n = static_cast<size_t>(a->rows) * a->cols;
  for (size_t i = 0; i < n; ++i) {
    const float d = a->data[i] - b->data[i];
    if ((d < 0.0f ? -d : d) > tolerance)
      return 0;
  }
  return 1;
}

} // extern "C"
