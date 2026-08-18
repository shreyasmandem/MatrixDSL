; Expected LLVM IR for:
;
;   matrix A[4][4];
;   matrix B[4][4];
;   matrix C[4][4];
;   C = matmul(A, B);
;
; The whole point of this file: matmul is ONE call, not a triple loop nest.
; An opaque call survives -O2 intact and lowers to a single MDT MATMUL.
;
; Owner: Kandi Jeevitesh Reddy (24BCE0350)

target datalayout = "e-m:e-p:32:32-i64:64-n32-S64"
target triple = "mdt-unknown-none"

%struct.Matrix = type { float*, i32, i32 }

declare void @mdt.matmul.4x4(float* %dst, float* %a, float* %b)
declare void @llvm.memset.p0i8.i64(i8*, i8, i64, i1)

define i32 @main() {
entry:
  ; storage - 64-byte aligned so a 4x4 tile never straddles an alignment boundary
  %A.data = alloca [16 x float], align 64
  %B.data = alloca [16 x float], align 64
  %C.data = alloca [16 x float], align 64

  %A.raw = bitcast [16 x float]* %A.data to i8*
  %B.raw = bitcast [16 x float]* %B.data to i8*
  %C.raw = bitcast [16 x float]* %C.data to i8*
  call void @llvm.memset.p0i8.i64(i8* align 64 %A.raw, i8 0, i64 64, i1 false)
  call void @llvm.memset.p0i8.i64(i8* align 64 %B.raw, i8 0, i64 64, i1 false)
  call void @llvm.memset.p0i8.i64(i8* align 64 %C.raw, i8 0, i64 64, i1 false)

  %A.ptr = getelementptr inbounds [16 x float], [16 x float]* %A.data, i32 0, i32 0
  %B.ptr = getelementptr inbounds [16 x float], [16 x float]* %B.data, i32 0, i32 0
  %C.ptr = getelementptr inbounds [16 x float], [16 x float]* %C.data, i32 0, i32 0

  ; C = matmul(A, B)  ->  one instruction
  call void @mdt.matmul.4x4(float* %C.ptr, float* %A.ptr, float* %B.ptr)

  ret i32 0
}
