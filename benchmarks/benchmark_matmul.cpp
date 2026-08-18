//===----------------------------------------------------------------------===//
//
// MatrixDSL - Matrix multiplication benchmark
//
// Establishes the reference implementation and the instruction-count baseline
// that MDT code generation is measured against.
//
// Note on what is and is not claimed: MDT has no silicon and the simulator is
// functional rather than cycle-accurate, so no wall-clock comparison against a
// real CPU is meaningful. What IS meaningful, and what this benchmark
// measures, is the instruction count required to express each workload -
// scalar operations versus MDT MATMUL instructions.
//
//===----------------------------------------------------------------------===//

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

constexpr int kTileSize = 4;

using Matrix = std::vector<float>;

Matrix makeMatrix(int rows, int cols, unsigned seed) {
  Matrix m(static_cast<size_t>(rows) * cols);
  unsigned state = seed;
  for (auto &v : m) {
    // xorshift - deterministic across platforms, so benchmark inputs are
    // reproducible without depending on the standard library's RNG.
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    v = static_cast<float>(state % 100) / 10.0f - 5.0f;
  }
  return m;
}

/// Reference triple-nested multiply. This is the shape a general-purpose
/// compiler sees, and the shape from which a matrix instruction would have to
/// be recovered by loop-idiom recognition.
void matmulReference(Matrix &dst, const Matrix &a, const Matrix &b, int n) {
  for (int i = 0; i < n; ++i)
    for (int j = 0; j < n; ++j) {
      float sum = 0.0f;
      for (int k = 0; k < n; ++k)
        sum += a[i * n + k] * b[k * n + j];
      dst[i * n + j] = sum;
    }
}

/// Tiled multiply, matching the decomposition the MDT backend performs. Each
/// innermost 4x4 block corresponds to exactly one MATMUL instruction.
void matmulTiled(Matrix &dst, const Matrix &a, const Matrix &b, int n) {
  std::fill(dst.begin(), dst.end(), 0.0f);

  for (int ii = 0; ii < n; ii += kTileSize)
    for (int jj = 0; jj < n; jj += kTileSize)
      for (int kk = 0; kk < n; kk += kTileSize)
        // One MDT MATMUL instruction per iteration of this innermost block.
        for (int i = ii; i < ii + kTileSize && i < n; ++i)
          for (int j = jj; j < jj + kTileSize && j < n; ++j) {
            float sum = 0.0f;
            for (int k = kk; k < kk + kTileSize && k < n; ++k)
              sum += a[i * n + k] * b[k * n + j];
            dst[i * n + j] += sum;
          }
}

/// Scalar operations a general-purpose target needs: n^3 multiply-accumulates,
/// each costing a load, a load, a multiply, an add and a store.
long long scalarInstructionCount(int n) {
  const long long macs = 1LL * n * n * n;
  return macs * 5;
}

/// MDT instruction count: one MATMUL per tile triple, plus tile loads/stores.
long long mdtInstructionCount(int n) {
  const long long tiles = (n + kTileSize - 1) / kTileSize;
  const long long matmuls = tiles * tiles * tiles;
  return matmuls * 3 /* 2 loads + 1 matmul */ + tiles * tiles /* stores */;
}

double timeMs(void (*fn)(Matrix &, const Matrix &, const Matrix &, int),
              Matrix &dst, const Matrix &a, const Matrix &b, int n,
              int repeats) {
  const auto start = std::chrono::high_resolution_clock::now();
  for (int r = 0; r < repeats; ++r)
    fn(dst, a, b, n);
  const auto end = std::chrono::high_resolution_clock::now();
  return std::chrono::duration<double, std::milli>(end - start).count() /
         repeats;
}

bool matricesAgree(const Matrix &x, const Matrix &y, float tolerance) {
  if (x.size() != y.size())
    return false;
  for (size_t i = 0; i < x.size(); ++i) {
    const float d = x[i] - y[i];
    if ((d < 0 ? -d : d) > tolerance)
      return false;
  }
  return true;
}

} // namespace

int main(int argc, char **argv) {
  const int sizes[] = {4, 8, 16, 32, 64, 128};
  const int repeats = (argc > 1) ? std::atoi(argv[1]) : 10;

  std::printf("MatrixDSL matrix multiplication benchmark\n");
  std::printf("Tile size: %dx%d FP32, repeats: %d\n\n", kTileSize, kTileSize,
              repeats);

  std::printf("%6s %12s %12s %14s %14s %10s\n", "N", "ref (ms)", "tiled (ms)",
              "scalar instrs", "MDT instrs", "ratio");
  std::printf("%6s %12s %12s %14s %14s %10s\n", "------", "------------",
              "------------", "--------------", "--------------",
              "----------");

  bool allAgree = true;

  for (const int n : sizes) {
    const Matrix a = makeMatrix(n, n, 12345u);
    const Matrix b = makeMatrix(n, n, 67890u);
    Matrix refOut(static_cast<size_t>(n) * n);
    Matrix tiledOut(static_cast<size_t>(n) * n);

    const double refMs = timeMs(matmulReference, refOut, a, b, n, repeats);
    const double tiledMs = timeMs(matmulTiled, tiledOut, a, b, n, repeats);

    // Tiling reorders accumulation, so exact equality is not expected; a
    // relative tolerance is.
    if (!matricesAgree(refOut, tiledOut, 1e-3f)) {
      std::printf("  MISMATCH at N=%d\n", n);
      allAgree = false;
    }

    const long long scalarInstrs = scalarInstructionCount(n);
    const long long mdtInstrs = mdtInstructionCount(n);

    std::printf("%6d %12.4f %12.4f %14lld %14lld %9.1fx\n", n, refMs, tiledMs,
                scalarInstrs, mdtInstrs,
                (double)scalarInstrs / (double)mdtInstrs);
  }

  std::printf("\nTiled results %s the reference implementation.\n",
              allAgree ? "agree with" : "DISAGREE with");

  std::printf("\nNote: instruction counts are static estimates from the\n");
  std::printf("decomposition, not measurements of real hardware. The MDT\n");
  std::printf("column counts MATMUL and tile transfer instructions; the\n");
  std::printf("scalar column counts the load/multiply/add/store sequence a\n");
  std::printf("general-purpose target requires for the same work.\n");

  return allAgree ? 0 : 1;
}
