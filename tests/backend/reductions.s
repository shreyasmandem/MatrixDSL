; MDT backend test - VREDSUM, MROWMAX, MROWSUM
;
; Uses the same counting matrix (1..16) as matmul_4x4.s and matmulacc.s, so
; the expected values below can be checked by hand directly against source:
;
;   [  1  2  3  4 ]     row sums: 10, 26, 42, 58
;   [  5  6  7  8 ]     row maxes: 4, 8, 12, 16
;   [  9 10 11 12 ]
;   [ 13 14 15 16 ]
;
; The final VREDSUM is a genuine cross-check, not just a repeat of the row
; sums: summing all four row-sums must equal 1+2+...+16 = 16*17/2 = 136,
; which only holds if MROWSUM computed every row correctly.
;
; Expect:
;   V0 = [ 10.00  26.00  42.00  58.00 ]   (MROWSUM)
;   V1 = [  4.00   8.00  12.00  16.00 ]   (MROWMAX)
;   R1 = 136                              (VREDSUM of V0)
;
; Run: mdtsim tests/backend/reductions.s --dump-vector --dump-scalar

.data
counting:
    .float  1.0,  2.0,  3.0,  4.0
    .float  5.0,  6.0,  7.0,  8.0
    .float  9.0, 10.0, 11.0, 12.0
    .float 13.0, 14.0, 15.0, 16.0

.text
.global _start

_start:
    ADD     R2, R0, 0
    LOAD    M0, [R2 + 0]     ; M0 = counting matrix

    MROWSUM V0, M0            ; V0 = row sums
    MROWMAX V1, M0            ; V1 = row maxes
    VREDSUM R1, V0             ; R1 = sum of row sums = grand total = 136

    HALT
