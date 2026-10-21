; MDT backend test - MATMULACC (fused accumulate)
; Expect: M0 = 2 x counting matrix (each element doubled)
;
; MATMULACC Md, Ma, Mb computes Md = Md + (Ma x Mb).
; Preload M0 with the counting matrix C, then MATMULACC M0, Identity, C
; computes M0 = C + (I x C) = C + C = 2C - genuinely exercises the
; "destination's PRIOR value contributes to the result" semantics, since a
; plain MATMUL into a zeroed register could never produce this.
;
; Run: mdtsim tests/backend/matmulacc.s --dump-matrix M0

.data
counting:
    .float  1.0,  2.0,  3.0,  4.0
    .float  5.0,  6.0,  7.0,  8.0
    .float  9.0, 10.0, 11.0, 12.0
    .float 13.0, 14.0, 15.0, 16.0
identity:
    .float 1.0, 0.0, 0.0, 0.0
    .float 0.0, 1.0, 0.0, 0.0
    .float 0.0, 0.0, 1.0, 0.0
    .float 0.0, 0.0, 0.0, 1.0

.text
.global _start

_start:
    ADD       R1, R0, 0            ; address of counting
    ADD       R2, R0, 64           ; address of identity

    LOAD      M0, [R1 + 0]         ; M0 = C  (the value MATMULACC will add to)
    LOAD      M1, [R2 + 0]         ; M1 = I
    LOAD      M2, [R1 + 0]         ; M2 = C

    MATMULACC M0, M1, M2           ; M0 = C + (I x C) = 2C

    HALT
