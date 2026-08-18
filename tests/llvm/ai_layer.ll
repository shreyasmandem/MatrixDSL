; Expected LLVM IR for a full neural network layer:
;
;   H = matmul(W, X);
;   Y = relu(H);
;
; Two MatrixDSL statements, two IR calls, two MDT instructions. This is the
; workload the project exists to compile, and it is the clearest demonstration
; that domain semantics survive all the way to code generation.
;
; Owner: Kandi Jeevitesh Reddy (24BCE0350)

target datalayout = "e-m:e-p:32:32-i64:64-n32-S64"
target triple = "mdt-unknown-none"

declare void @mdt.matmul.4x4(float* %dst, float* %a, float* %b)
declare void @mdt.relu.4x4(float* %dst, float* %src)

define void @ai_layer(float* %Y, float* %H, float* %W, float* %X) {
entry:
  call void @mdt.matmul.4x4(float* %H, float* %W, float* %X)
  call void @mdt.relu.4x4(float* %Y, float* %H)
  ret void
}
