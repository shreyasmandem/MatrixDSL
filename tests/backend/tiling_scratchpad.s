; MDT backend test / ablation demo - scratchpad staging + reuse (tiled)
;
; Loads the SAME tile from DRAM exactly once, stages it into the
; scratchpad, then reuses it 3 times via SCRATCHLOAD - what a tiling pass
; that recognises repeated access to the same block would generate.
; Companion to tiling_naive.s: identical computation (three MATMULs of the
; same tile against itself), only the memory path differs.
;
; Hand-derived expected performance model (scalar ops = 1, DRAM LOAD/STORE
; = 8, SCRATCHLOAD/SCRATCHSTORE = 1, MATMUL family = 4; HALT not counted,
; same as tiling_naive.s).
;
; Executed trace and its PerfClass runs - two ADDs merge into one Compute
; run, and LOAD+SCRATCHSTORE+SCRATCHLOAD merge into one Transfer run, since
; RLE collapses any run of CONSECUTIVE same-class instructions:
;   ADD(C,1) ADD(C,1)                              -> Compute run, 2
;   LOAD(T,8) SCRATCHSTORE(T,1) SCRATCHLOAD(T,1)    -> Transfer run, 10
;   MATMUL(C,4)                                     -> Compute run, 4
;   SCRATCHLOAD(T,1)                                -> Transfer run, 1
;   MATMUL(C,4)                                     -> Compute run, 4
;   SCRATCHLOAD(T,1)                                -> Transfer run, 1
;   MATMUL(C,4)                                     -> Compute run, 4  (last, unpaired)
;
;   sequential    = 2+10+4+1+4+1+4 = 26
;   overlap-aware = max(2,10) + max(4,1) + max(4,1) + [4, unpaired, last run]
;                 = 10 + 4 + 4 + 4 = 22
;
; Both figures are lower than tiling_naive.s's (37 sequential, 28
; overlap-aware) - staging through the scratchpad cuts DRAM traffic from
; 3 accesses (192 bytes) to 1 (64 bytes), which is the entire point of the
; memory hierarchy. Run both and compare `--perf` output directly.
;
; Run: mdtsim tests/backend/tiling_scratchpad.s --perf --quiet

.data
counting:
    .float  1.0,  2.0,  3.0,  4.0
    .float  5.0,  6.0,  7.0,  8.0
    .float  9.0, 10.0, 11.0, 12.0
    .float 13.0, 14.0, 15.0, 16.0

.text
.global _start

_start:
    ADD          R1, R0, 0
    ADD          R11, R0, 0          ; scratchpad staging address

    LOAD         M1, [R1 + 0]        ; ONE DRAM load
    SCRATCHSTORE M1, [R11 + 0]       ; stage into scratchpad

    SCRATCHLOAD  M1, [R11 + 0]       ; reuse 1 - from scratchpad, not DRAM
    MATMUL       M2, M1, M1

    SCRATCHLOAD  M1, [R11 + 0]       ; reuse 2
    MATMUL       M2, M1, M1

    SCRATCHLOAD  M1, [R11 + 0]       ; reuse 3
    MATMUL       M2, M1, M1

    HALT
