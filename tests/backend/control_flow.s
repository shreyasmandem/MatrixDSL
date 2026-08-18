; MDT backend test - branches and loop termination
; Counts from 0 to 5.
; Expect: R1 == 5, R2 == 5 (loop ran exactly 5 times)
;
; Run: mdtsim tests/backend/control_flow.s --dump-scalar

.text
.global _start

_start:
    ADD   R1, R0, 0         ; counter = 0
    ADD   R2, R0, 0         ; iterations = 0
    ADD   R3, R0, 5         ; limit = 5

loop:
    BEQ   R1, R3, done      ; while counter != limit
    ADD   R1, R1, 1         ;   counter++
    ADD   R2, R2, 1         ;   iterations++
    BR    loop

done:
    HALT
