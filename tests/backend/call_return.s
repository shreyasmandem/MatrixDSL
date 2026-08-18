; MDT backend test - CALL and RET
; Calls a subroutine that doubles R1.
; Expect: R1 == 14 (7 doubled), R15 holds the return address after CALL
;
; Run: mdtsim tests/backend/call_return.s --dump-scalar

.text
.global _start

_start:
    ADD   R1, R0, 7         ; R1 = 7
    CALL  double_it         ; R15 = return address
    HALT

double_it:
    ADD   R1, R1, R1        ; R1 = R1 * 2
    RET
