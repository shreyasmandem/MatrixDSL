; MDT backend test - ReLU activation
;
; Every negative element becomes zero; positives pass through unchanged.
;
;   A = [ -1   2  -3   4 ]
;       [  5  -6   7  -8 ]
;       [ -9  10 -11  12 ]
;       [ 13 -14  15 -16 ]
;
; Expect:
;   M1 = [  0.00   2.00   0.00   4.00 ]
;        [  5.00   0.00   7.00   0.00 ]
;        [  0.00  10.00   0.00  12.00 ]
;        [ 13.00   0.00  15.00   0.00 ]
;
; Run: mdtsim tests/backend/relu.s --dump-matrix M1

.data
matrix_a:
    .float  -1.0,   2.0,  -3.0,   4.0
    .float   5.0,  -6.0,   7.0,  -8.0
    .float  -9.0,  10.0, -11.0,  12.0
    .float  13.0, -14.0,  15.0, -16.0

.text
.global _start

_start:
    ADD   R1, R0, 0
    LOAD  M0, [R1 + 0]          ; M0 = A
    RELU  M1, M0                ; M1 = max(0, A)
    HALT
