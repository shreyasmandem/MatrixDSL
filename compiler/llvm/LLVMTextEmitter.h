//===----------------------------------------------------------------------===//
//
// MatrixDSL - Tensor IR -> LLVM IR (textual) emitter (Phase 3 / Review 3)
//
// Owner: Kandi Jeevitesh Reddy (24BCE0350)
//
// Produces a self-contained .ll module without linking against LLVM, so
// `matrixdslc --emit-llvm` works on any machine. Matrix work is expressed as
// calls to the reserved @mdt.* tile intrinsics fixed in docs/llvm-backend.md,
// one call per 4x4 tile, with explicit leading dimensions (in floats) because
// MatrixDSL buffers are row-major with tile-padded rows and columns:
//
//   @mdt.matmul.4x4      (ptr d, ptr a, ptr b, i32 ldd, i32 lda, i32 ldb)
//   @mdt.matmul_acc.4x4  (ptr d, ptr a, ptr b, i32 ldd, i32 lda, i32 ldb)
//   @mdt.matmul_relu.4x4 (ptr d, ptr a, ptr b, i32 ldd, i32 lda, i32 ldb)
//   @mdt.add.4x4 / @mdt.sub.4x4 (ptr d, ptr a, ptr b, i32 ldd, i32 lda, i32 ldb)
//   @mdt.scale.4x4       (ptr d, ptr a, float s, i32 ldd, i32 lda)
//   @mdt.relu.4x4 / @mdt.transpose.4x4 / @mdt.copy.4x4 (ptr d, ptr a, i32 ldd, i32 lda)
//   @mdt_softmax / @mdt_print - runtime calls (docs/runtime-abi.md)
//
// Scratchpad staging is an MDT memory-placement decision; ScratchStore /
// ScratchLoad are treated as aliases of their source here and appear only
// as comments.
//
//===----------------------------------------------------------------------===//

#ifndef MATRIXDSL_LLVMTEXTEMITTER_H
#define MATRIXDSL_LLVMTEXTEMITTER_H

#include "tensor-ir/Interpreter.h"
#include "tensor-ir/TensorIR.h"

#include <string>

namespace matrixdsl {

std::string emitLLVMText(const tensorir::TensorFunction &fn,
                         const tensorir::InputMap &inputs,
                         const std::string &sourceName);

} // namespace matrixdsl

#endif // MATRIXDSL_LLVMTEXTEMITTER_H
