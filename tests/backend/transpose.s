; MDT backend test - transpose is an involution
;
; Transposing twice must return the original matrix, so M3 == M1.
; This needs no hand-computed expected values.
;
; Expect: M3 identical to M1
;
; Run: mdtsim tests/backend/transpose.s --dump-matrix

.data
matrix_a:
    .float  1.0,  2.0,  3.0,  4.0
    .float  5.0,  6.0,  7.0,  8.0
    .float  9.0, 10.0, 11.0, 12.0
    .float 13.0, 14.0, 15.0, 16.0

.text
.global _start

_start:
    ADD       R1, R0, 0
    LOAD      M1, [R1 + 0]      ; M1 = A
    TRANSPOSE M2, M1            ; M2 = A'
    TRANSPOSE M3, M2            ; M3 = (A')' = A
    HALT
