; MDT backend test - MATMULRELU (fused epilogue)
; Expect: M0 == relu(A) exactly, matching relu.s's known output
;
; MATMULRELU Md, Ma, Mb computes Md = relu(Ma x Mb). Multiplying by the
; identity matrix means Ma x I = Ma exactly, so MATMULRELU M0, A, I reduces
; to relu(A) - the SAME matrix and SAME expected output as tests/backend/
; relu.s, computed through the fused instruction instead of a separate
; MATMUL + RELU pair. Any mismatch against relu.s's known-good output means
; MATMULRELU's fusion, not its arithmetic, is wrong.
;
; Expect:
;   M0 = [  0.00   2.00   0.00   4.00 ]
;        [  5.00   0.00   7.00   0.00 ]
;        [  0.00  10.00   0.00  12.00 ]
;        [ 13.00   0.00  15.00   0.00 ]
;
; Run: mdtsim tests/backend/matmulrelu.s --dump-matrix M0

.data
matrix_a:
    .float  -1.0,   2.0,  -3.0,   4.0
    .float   5.0,  -6.0,   7.0,  -8.0
    .float  -9.0,  10.0, -11.0,  12.0
    .float  13.0, -14.0,  15.0, -16.0
identity:
    .float 1.0, 0.0, 0.0, 0.0
    .float 0.0, 1.0, 0.0, 0.0
    .float 0.0, 0.0, 1.0, 0.0
    .float 0.0, 0.0, 0.0, 1.0

.text
.global _start

_start:
    ADD        R1, R0, 0           ; address of matrix_a
    ADD        R2, R0, 64          ; address of identity

    LOAD       M1, [R1 + 0]        ; M1 = A
    LOAD       M2, [R2 + 0]        ; M2 = I

    MATMULRELU M0, M1, M2          ; M0 = relu(A x I) = relu(A)

    HALT
