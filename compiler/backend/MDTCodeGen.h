//===----------------------------------------------------------------------===//
//
// MatrixDSL - Tensor IR -> MDT assembly code generator (Phase 3 / Review 3)
//
// Owner: Shreyas Mandem (24BCE0381)
//
// Instruction selection, memory layout and register assignment for the MDT
// target, producing assembly that tools/mdtsim assembles and runs unchanged.
//
// Memory layout. Every value that lives in DRAM gets one buffer in .data:
// row-major, rows and columns padded up to a multiple of 4, 64-byte aligned,
// batches stacked. Padding is always zero - inputs and constants are emitted
// that way and every instruction sequence below preserves it - which is what
// lets ragged shapes (e.g. 3x5) run on a 4x4 matrix unit without masks.
//
// Instruction selection (one 4x4 tile at a time):
//   MatMul / TiledMatMul  MATMUL, then MATMULACC for each further k tile
//   FusedMatMulAcc        bias tile loaded into the accumulator, MATMULACC x k
//   FusedMatMulRelu       MATMULRELU when k = 1, else accumulate + RELU
//   Add / Sub             MATMULACC against a resident +I / -I tile (M7 / M6):
//                         the matrix unit's accumulate path doubles as a tile
//                         adder - 3 instructions per tile instead of the 16
//                         a 4-lane VADD sequence would need
//   ScalarMul             MATMUL against a constant s*I tile (M5)
//   Transpose / Relu      TRANSPOSE / RELU
//   Softmax / Print       runtime calls mdt_softmax / mdt_print
//   ScratchStore          LOAD + SCRATCHSTORE per tile, once
//   ScratchLoad           no code: consumers read its tiles with SCRATCHLOAD
//
// Register plan: M0 accumulator/result, M1-M2 operands, M3 staging,
// M5 scale tile, M6 -I, M7 +I; R1-R3 operand base addresses, R4 result base,
// R1-R6 runtime-call arguments.
//
//===----------------------------------------------------------------------===//

#ifndef MATRIXDSL_MDTCODEGEN_H
#define MATRIXDSL_MDTCODEGEN_H

#include "tensor-ir/Interpreter.h"
#include "tensor-ir/TensorIR.h"

#include <string>

namespace matrixdsl {
namespace backend {

struct CodeGenStats {
  long long instructions = 0;  // static count, excluding HALT
  long long dataBytes = 0;     // size of the .data image
  int buffers = 0;
};

struct CodeGenResult {
  bool ok = false;
  std::string error;
  std::string assembly;
  CodeGenStats stats;
};

CodeGenResult generateMDT(const tensorir::TensorFunction &fn,
                          const tensorir::InputMap &inputs,
                          const std::string &header);

} // namespace backend
} // namespace matrixdsl

#endif // MATRIXDSL_MDTCODEGEN_H
