; Expected LLVM IR for:
;
;   B = transpose(A);
;   C = relu(B);
;
; Both are unary matrix operations: (dst, src). Note the operand order is
; always destination first, matching the MDT assembly convention.
;
; Owner: Kandi Jeevitesh Reddy (24BCE0350)

target datalayout = "e-m:e-p:32:32-i64:64-n32-S64"
target triple = "mdt-unknown-none"

declare void @mdt.transpose.4x4(float* %dst, float* %src)
declare void @mdt.relu.4x4(float* %dst, float* %src)

define void @transpose_then_relu(float* %C, float* %B, float* %A) {
entry:
  call void @mdt.transpose.4x4(float* %B, float* %A)
  call void @mdt.relu.4x4(float* %C, float* %B)
  ret void
}
