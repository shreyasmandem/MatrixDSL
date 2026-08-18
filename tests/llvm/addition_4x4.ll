; Expected LLVM IR for:
;
;   matrix A[4][4];
;   matrix B[4][4];
;   matrix C[4][4];
;   C = A + B;
;
; Element-wise operations DO lower to a loop, unlike matrix operations. The
; trip count is a compile-time constant, so LLVM can unroll or vectorise it
; without any help from us - and on MDT it maps onto VADD.
;
; Owner: Kandi Jeevitesh Reddy (24BCE0350)

target datalayout = "e-m:e-p:32:32-i64:64-n32-S64"
target triple = "mdt-unknown-none"

%struct.Matrix = type { float*, i32, i32 }

define void @matrix_add(float* %C, float* %A, float* %B) {
entry:
  br label %loop

loop:
  %i = phi i32 [ 0, %entry ], [ %i.next, %loop ]

  %a.ptr = getelementptr inbounds float, float* %A, i32 %i
  %b.ptr = getelementptr inbounds float, float* %B, i32 %i
  %c.ptr = getelementptr inbounds float, float* %C, i32 %i

  %a.val = load float, float* %a.ptr, align 4
  %b.val = load float, float* %b.ptr, align 4
  %sum   = fadd float %a.val, %b.val
  store float %sum, float* %c.ptr, align 4

  ; nuw nsw is safe: the trip count is a known constant well inside i32 range
  %i.next = add nuw nsw i32 %i, 1
  %done = icmp eq i32 %i.next, 16
  br i1 %done, label %exit, label %loop

exit:
  ret void
}
