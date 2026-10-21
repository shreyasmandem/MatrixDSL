; MDT backend test - scalar arithmetic
; Expect: R3 = 30, R4 = 10, R5 = 200
;
; Run: mdtsim tests/backend/scalar_arith.s --dump-scalar

.text
.global _start

_start:
    ADD   R1, R0, 20        ; R1 = 0 + 20  = 20
    ADD   R2, R0, 10        ; R2 = 0 + 10  = 10
    ADD   R3, R1, R2        ; R3 = 20 + 10 = 30
    SUB   R4, R1, R2        ; R4 = 20 - 10 = 10
    MUL   R5, R1, R2        ; R5 = 20 * 10 = 200
    HALT
