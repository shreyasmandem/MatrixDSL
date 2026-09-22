// mtxtensorir - Tensor IR + fusion pass demo. Owner: Kandi Jeevitesh Reddy.
// Builds hand-constructed Tensor IR graphs (no AST->TensorIR lowering yet)
// and runs the fusion pass, printing before/after - same "runs without the
// parser" pattern Parth's mtxcheck used at Review 1.
#include "../../compiler/tensor-ir/FusionPass.h"
#include "../../compiler/tensor-ir/TensorIR.h"

#include <cstdio>

using namespace matrixdsl::tensorir;

int main() {
  std::printf("MatrixDSL Tensor IR + fusion pass demo\n\n");

  // matmul(A,B) -> relu(...)  should fuse to one FusedMatMulRelu.
  {
    TensorFunction fn;
    TensorShape s(1, 4, 4);
    TensorOp *a = fn.addOp(TensorOpKind::Load, s);
    TensorOp *b = fn.addOp(TensorOpKind::Load, s);
    TensorOp *mm = fn.addOp(TensorOpKind::MatMul, s, {a, b});
    fn.addOp(TensorOpKind::Relu, s, {mm});

    std::printf("Before fusion:\n%s\n", fn.dump().c_str());
    const int fused = runFusionPass(fn);
    std::printf("After fusion (%d fused):\n%s\n", fused, fn.dump().c_str());

    const bool ok = fused == 1 && fn.ops().back()->kind == TensorOpKind::FusedMatMulRelu;
    std::printf("matmul->relu fusion: %s\n\n", ok ? "PASS" : "FAIL");
  }

  // Negative test: matmul's result used TWICE must NOT fuse.
  {
    TensorFunction fn;
    TensorShape s(1, 4, 4);
    TensorOp *a = fn.addOp(TensorOpKind::Load, s);
    TensorOp *b = fn.addOp(TensorOpKind::Load, s);
    TensorOp *mm = fn.addOp(TensorOpKind::MatMul, s, {a, b});
    fn.addOp(TensorOpKind::Relu, s, {mm});
    fn.addOp(TensorOpKind::Print, s, {mm}); // second use of mm

    const int fused = runFusionPass(fn);
    const bool ok = (fused == 0);
    std::printf("matmul-with-second-use does NOT fuse: %s (fused=%d)\n",
               ok ? "PASS" : "FAIL", fused);
  }

  return 0;
}
