; MDT backend test - 4x4 matrix multiplication
;
; Computes M0 = A x transpose(A), where
;
;   A = [  1  2  3  4 ]
;       [  5  6  7  8 ]
;       [  9 10 11 12 ]
;       [ 13 14 15 16 ]
;
; The result is A x A' , which is symmetric - so the output is self-checking:
; if M0 is not symmetric, either MATMUL or TRANSPOSE is wrong.
;
; Expect:
;   M0 = [   30.00    70.00   110.00   150.00 ]
;        [   70.00   174.00   278.00   382.00 ]
;        [  110.00   278.00   446.00   614.00 ]
;        [  150.00   382.00   614.00   846.00 ]
;
; Row 0 col 0 = 1*1 + 2*2 + 3*3 + 4*4 = 30, and so on.
;
; Run: mdtsim tests/backend/matmul_4x4.s --dump-matrix M0

.data
matrix_a:
    .float  1.0,  2.0,  3.0,  4.0
    .float  5.0,  6.0,  7.0,  8.0
    .float  9.0, 10.0, 11.0, 12.0
    .float 13.0, 14.0, 15.0, 16.0

.text
.global _start

_start:
    ADD       R1, R0, 0         ; R1 = address of matrix_a

    LOAD      M1, [R1 + 0]      ; M1 = A          (one 64-byte tile)
    TRANSPOSE M2, M1            ; M2 = transpose(A)
    MATMUL    M0, M1, M2        ; M0 = A x A'

    HALT
