//===----------------------------------------------------------------------===//
//
// mtxref - MatrixDSL reference operation runner
//
// Owner: Kandi Jeevitesh Reddy (24BCE0350)
//
// Exercises every reference implementation in the runtime and checks the
// algebraic identities that must hold. This is the oracle the compiled MDT
// path will be verified against, so it has to be right before anything else
// can be trusted.
//
// Usage:
//   mtxref [operation]
//
// Operations: add, sub, scalar, matmul, transpose, relu, layer, tiling, all
//
//===----------------------------------------------------------------------===//

#include "../../runtime/MatrixRuntime.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace {

int gChecks = 0;
int gFailures = 0;

void check(bool condition, const char *what) {
  ++gChecks;
  if (condition) {
    std::printf("  PASS  %s\n", what);
  } else {
    ++gFailures;
    std::printf("  FAIL  %s\n", what);
  }
}

MDMatrix *makeCounting(int32_t rows, int32_t cols) {
  MDMatrix *m = matrix_alloc(rows, cols);
  for (int32_t i = 0; i < rows; ++i)
    for (int32_t j = 0; j < cols; ++j)
      matrix_set(m, i, j, static_cast<float>(i * cols + j + 1));
  return m;
}

MDMatrix *makeIdentity(int32_t n) {
  MDMatrix *m = matrix_alloc(n, n);
  for (int32_t i = 0; i < n; ++i)
    matrix_set(m, i, i, 1.0f);
  return m;
}

//===--------------------------------------------------------------------===//

void testAdd() {
  std::printf("\nAddition\n");
  MDMatrix *a = makeCounting(2, 3);
  MDMatrix *b = makeCounting(2, 3);
  MDMatrix *c = matrix_alloc(2, 3);

  matrix_add_ref(c, a, b);
  // Each element of a doubled.
  check(matrix_get(c, 0, 0) == 2.0f && matrix_get(c, 1, 2) == 12.0f,
        "A + A doubles every element");

  matrix_print_named("C", c);
  matrix_free(a); matrix_free(b); matrix_free(c);
}

void testSub() {
  std::printf("\nSubtraction\n");
  MDMatrix *a = makeCounting(2, 3);
  MDMatrix *b = makeCounting(2, 3);
  MDMatrix *c = matrix_alloc(2, 3);

  matrix_sub_ref(c, a, b);
  bool allZero = true;
  for (int32_t i = 0; i < 2; ++i)
    for (int32_t j = 0; j < 3; ++j)
      if (matrix_get(c, i, j) != 0.0f)
        allZero = false;

  check(allZero, "A - A is the zero matrix");
  matrix_free(a); matrix_free(b); matrix_free(c);
}

void testScalar() {
  std::printf("\nScalar multiplication\n");
  MDMatrix *a = makeCounting(2, 2);
  MDMatrix *c = matrix_alloc(2, 2);

  matrix_scalar_mul_ref(c, 3.0f, a);
  check(matrix_get(c, 0, 0) == 3.0f && matrix_get(c, 1, 1) == 12.0f,
        "3 * A scales every element");
  matrix_free(a); matrix_free(c);
}

void testMatMul() {
  std::printf("\nMatrix multiplication\n");

  // A x I == A. Needs no hand-computed expected values.
  MDMatrix *a = makeCounting(4, 4);
  MDMatrix *identity = makeIdentity(4);
  MDMatrix *c = matrix_alloc(4, 4);

  matrix_matmul_ref(c, a, identity);
  check(matrix_equal(a, c, 1e-6f) == 1, "A x I == A");

  // Rectangular: (2x3) x (3x2) -> 2x2
  MDMatrix *r1 = makeCounting(2, 3);
  MDMatrix *r2 = makeCounting(3, 2);
  MDMatrix *r3 = matrix_alloc(2, 2);
  matrix_matmul_ref(r3, r1, r2);

  // Row 0 of r1 is [1 2 3]; column 0 of r2 is [1 3 5].
  // 1*1 + 2*3 + 3*5 = 22.
  check(matrix_get(r3, 0, 0) == 22.0f, "rectangular (2x3)x(3x2) element [0][0] == 22");
  matrix_print_named("R", r3);

  matrix_free(a); matrix_free(identity); matrix_free(c);
  matrix_free(r1); matrix_free(r2); matrix_free(r3);
}

void testTranspose() {
  std::printf("\nTranspose\n");
  MDMatrix *a = makeCounting(3, 4);
  MDMatrix *t = matrix_alloc(4, 3);
  MDMatrix *tt = matrix_alloc(3, 4);

  matrix_transpose_ref(t, a);
  check(t->rows == 4 && t->cols == 3, "transpose of 3x4 is 4x3");
  check(matrix_get(t, 2, 1) == matrix_get(a, 1, 2), "T[i][j] == A[j][i]");

  // Transposing twice is the identity - self-checking.
  matrix_transpose_ref(tt, t);
  check(matrix_equal(a, tt, 1e-6f) == 1, "transpose(transpose(A)) == A");

  matrix_free(a); matrix_free(t); matrix_free(tt);
}

void testRelu() {
  std::printf("\nReLU\n");
  MDMatrix *a = matrix_alloc(2, 2);
  matrix_set(a, 0, 0, -1.0f); matrix_set(a, 0, 1,  2.0f);
  matrix_set(a, 1, 0,  0.0f); matrix_set(a, 1, 1, -4.0f);

  MDMatrix *r = matrix_alloc(2, 2);
  matrix_relu_ref(r, a);

  check(matrix_get(r, 0, 0) == 0.0f, "negative becomes zero");
  check(matrix_get(r, 0, 1) == 2.0f, "positive passes through");
  check(matrix_get(r, 1, 0) == 0.0f, "zero stays zero");
  check(matrix_get(r, 1, 1) == 0.0f, "negative becomes zero");

  // ReLU is idempotent: relu(relu(A)) == relu(A).
  MDMatrix *rr = matrix_alloc(2, 2);
  matrix_relu_ref(rr, r);
  check(matrix_equal(r, rr, 1e-6f) == 1, "relu is idempotent");

  matrix_free(a); matrix_free(r); matrix_free(rr);
}

void testLayer() {
  std::printf("\nNeural network layer: Y = relu(W x X)\n");
  MDMatrix *w = matrix_alloc(4, 4);
  MDMatrix *x = makeCounting(4, 4);
  MDMatrix *h = matrix_alloc(4, 4);
  MDMatrix *y = matrix_alloc(4, 4);

  const float weights[16] = {
       0.5f, -0.2f,  0.3f,  0.1f,
      -0.4f,  0.6f, -0.1f,  0.2f,
       0.2f,  0.3f, -0.5f,  0.4f,
      -0.3f,  0.1f,  0.2f, -0.6f};
  std::memcpy(w->data, weights, sizeof(weights));

  matrix_matmul_ref(h, w, x);
  matrix_relu_ref(y, h);

  bool noNegatives = true;
  for (int32_t i = 0; i < 4; ++i)
    for (int32_t j = 0; j < 4; ++j)
      if (matrix_get(y, i, j) < 0.0f)
        noNegatives = false;

  check(noNegatives, "layer output has no negative elements");
  matrix_print_named("Y", y);

  matrix_free(w); matrix_free(x); matrix_free(h); matrix_free(y);
}

void testTiling() {
  std::printf("\nTile padding\n");

  // 5x7 is not tile-aligned; it must pad to 8x8.
  MDMatrix *a = makeCounting(5, 7);
  MDMatrix *padded = matrix_pad_to_tile(a);

  check(padded->rows == 8 && padded->cols == 8, "5x7 pads to 8x8");
  check(matrix_get(padded, 0, 0) == matrix_get(a, 0, 0),
        "padded copy preserves original values");
  check(matrix_get(padded, 4, 6) == matrix_get(a, 4, 6),
        "last original element lands in the right place");
  check(matrix_get(padded, 7, 7) == 0.0f, "padding region is zero");
  check(a->rows == 5 && a->cols == 7, "original logical shape unchanged");

  // An already-aligned matrix must be unchanged in shape.
  MDMatrix *b = makeCounting(8, 8);
  MDMatrix *bPadded = matrix_pad_to_tile(b);
  check(bPadded->rows == 8 && bPadded->cols == 8, "8x8 needs no padding");

  matrix_free(a); matrix_free(padded);
  matrix_free(b); matrix_free(bPadded);
}

} // namespace

int main(int argc, char **argv) {
  const std::string op = (argc > 1) ? argv[1] : "all";

  std::printf("MatrixDSL reference implementation runner\n");
  std::printf("=========================================\n");

  const bool all = (op == "all");
  if (all || op == "add")       testAdd();
  if (all || op == "sub")       testSub();
  if (all || op == "scalar")    testScalar();
  if (all || op == "matmul")    testMatMul();
  if (all || op == "transpose") testTranspose();
  if (all || op == "relu")      testRelu();
  if (all || op == "layer")     testLayer();
  if (all || op == "tiling")    testTiling();

  if (gChecks == 0) {
    std::fprintf(stderr, "\nUnknown operation '%s'\n", op.c_str());
    std::fprintf(stderr,
                 "Valid: add sub scalar matmul transpose relu layer tiling all\n");
    return 1;
  }

  std::printf("\n-----------------------------------------\n");
  std::printf("  %d checks, %d failed\n", gChecks, gFailures);
  std::printf("-----------------------------------------\n");

  return gFailures == 0 ? 0 : 1;
}
