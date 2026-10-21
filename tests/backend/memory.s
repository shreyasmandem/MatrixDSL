; MDT backend test - load and store round-trip
; Expect: R2 == R1 == 42, and memory[64] == 42
;
; Run: mdtsim tests/backend/memory.s --dump-scalar

.text
.global _start

_start:
    ADD   R1, R0, 42        ; R1 = 42
    ADD   R10, R0, 64       ; R10 = base address 64
    STORE R1, [R10 + 0]     ; memory[64] = 42
    LOAD  R2, [R10 + 0]     ; R2 = memory[64] = 42
    SUB   R3, R2, R1        ; R3 = 0 if the round-trip worked
    HALT
