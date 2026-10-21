//===----------------------------------------------------------------------===//
//
// MatrixDSL - Tensor IR optimisation passes (Phase 3 / Review 3)
//
// Owner: Kandi Jeevitesh Reddy (24BCE0350)
//
//   runDeadCodeElimination  - drops every op that cannot reach a Print
//   runTilingPass           - marks multi-tile matmuls TiledMatMul and stages
//                             reused operands in the 64 KiB scratchpad
//
// Pass order used by the driver (compiler/driver/Pipeline.cpp):
//   lower -> fusion (FusionPass.h) -> DCE -> tiling -> MDT code generation
//
//===----------------------------------------------------------------------===//

#ifndef MATRIXDSL_PASSES_H
#define MATRIXDSL_PASSES_H

#include "TensorIR.h"

#include <string>
#include <vector>

namespace matrixdsl {
namespace tensorir {

constexpr int kTileBytes = 64;                 // one 4x4 FP32 tile
constexpr int kScratchpadBytes = 64 * 1024;    // docs/memory-hierarchy.md

/// Removes ops with no path to a Print. Returns the number removed.
int runDeadCodeElimination(TensorFunction &fn);

/// One staging decision, kept for --stats and the final report.
struct StagingDecision {
  int consumerId = -1;   // the matmul-family op that benefits
  int operandIndex = 0;  // 0 = A (left), 1 = B (right)
  int tiles = 0;         // tiles copied DRAM -> scratchpad, once
  int usesPerTile = 0;   // how many times each tile is read afterwards
  long long dramBytesSaved = 0;
};

struct TilingReport {
  int tiledMatMuls = 0;
  std::vector<StagingDecision> staged;
  std::vector<std::string> notes; // operands that did not fit, etc.
};

/// For every matmul-family op, an operand tile read more than once is copied
/// into the scratchpad once (ScratchStore) and re-read from there
/// (ScratchLoad) - 1 cycle instead of 8 per reuse. Operands are chosen
/// greedily by DRAM bytes saved, subject to the scratchpad capacity.
TilingReport runTilingPass(TensorFunction &fn,
                           int scratchpadBytes = kScratchpadBytes);

/// Tile-grid helpers shared with the code generator.
inline int tilesOf(int n) { return (n + 3) / 4; }
inline bool isMatMulFamily(TensorOpKind k) {
  return k == TensorOpKind::MatMul || k == TensorOpKind::TiledMatMul ||
         k == TensorOpKind::FusedMatMulRelu || k == TensorOpKind::FusedMatMulAcc;
}

} // namespace tensorir
} // namespace matrixdsl

#endif // MATRIXDSL_PASSES_H
