; MDT backend test - scratchpad address space round trip
;
; Moves a scalar and a matrix tile: DRAM -> register -> scratchpad ->
; register -> DRAM, verifying the value survives every hop unchanged.
; docs/memory-hierarchy.md section 1: there is no direct DRAM<->scratchpad
; instruction, so this is genuinely two register-mediated hops each way,
; exactly as generated tiling code would perform it.
;
; DRAM layout: `counting` occupies bytes [0, 64). The scalar round trip
; deliberately stages through DRAM bytes [64, 72) instead, so it cannot
; corrupt the matrix the second half of this test still needs to read.
;
; Expect:
;   R1 = R2 = R3 = R4 = 42             (scalar round trip)
;   M1 == M2 == the counting matrix     (tile round trip)
;
; Run: mdtsim tests/backend/scratchpad_roundtrip.s --dump-scalar --dump-matrix

.data
counting:
    .float  1.0,  2.0,  3.0,  4.0
    .float  5.0,  6.0,  7.0,  8.0
    .float  9.0, 10.0, 11.0, 12.0
    .float 13.0, 14.0, 15.0, 16.0

.text
.global _start

_start:
    ; --- scalar round trip (uses DRAM [64,72) - past `counting`) ---
    ADD          R1, R0, 42
    ADD          R10, R0, 64         ; DRAM scratch address, clear of `counting`
    ADD          R11, R0, 0          ; scratchpad staging address

    STORE        R1, [R10 + 0]       ; R1 -> DRAM
    LOAD         R2, [R10 + 0]       ; DRAM -> R2  (R2 should be 42)
    SCRATCHSTORE R2, [R11 + 0]       ; R2 -> scratchpad
    SCRATCHLOAD  R3, [R11 + 0]       ; scratchpad -> R3  (R3 should be 42)
    STORE        R3, [R10 + 4]       ; R3 -> DRAM (a fresh address)
    LOAD         R4, [R10 + 4]       ; DRAM -> R4  (R4 should be 42)

    ; --- matrix tile round trip (reads `counting` at DRAM [0,64) intact) ---
    ADD          R5, R0, 0          ; address of `counting`
    LOAD         M1, [R5 + 0]       ; DRAM -> M1
    SCRATCHSTORE M1, [R11 + 64]      ; M1 -> scratchpad (offset 64, clear of the scalar's use of 0-3)
    SCRATCHLOAD  M2, [R11 + 64]      ; scratchpad -> M2

    HALT
