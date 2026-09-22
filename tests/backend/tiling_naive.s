; MDT backend test / ablation demo - no scratchpad reuse (baseline)
;
; Repeats the same DRAM tile load 3 times, once per use, the way naive
; (non-tiled) code generation would if it never staged data through the
; scratchpad. Pairs with tiling_scratchpad.s below for the docs/review2/
; Review2_Report.md section 8 ablation study: same computation, only the
; memory path differs.
;
; Hand-derived expected performance model (docs/isa-extensions.md latency
; table: scalar ops = 1, DRAM LOAD/STORE = 8, MATMUL family = 4; HALT is
; never counted - step() returns before recordPerfInstruction runs for it).
;
; Executed trace and its PerfClass runs (ADD is Compute, per MDTSim.cpp's
; classifyForPerf):
;   ADD(C,1) LOAD(T,8) MATMUL(C,4) LOAD(T,8) MATMUL(C,4) LOAD(T,8) MATMUL(C,4)
;   -> 7 runs, none merge (no two adjacent instructions share a class)
;
;   sequential    = 1+8+4+8+4+8+4 = 37
;   overlap-aware = max(1,8) + max(4,8) + max(4,8) + [4, unpaired, last run]
;                 = 8 + 8 + 8 + 4 = 28
;
; Run: mdtsim tests/backend/tiling_naive.s --perf --quiet

.data
counting:
    .float  1.0,  2.0,  3.0,  4.0
    .float  5.0,  6.0,  7.0,  8.0
    .float  9.0, 10.0, 11.0, 12.0
    .float 13.0, 14.0, 15.0, 16.0

.text
.global _start

_start:
    ADD    R1, R0, 0

    LOAD   M1, [R1 + 0]      ; DRAM load, use 1
    MATMUL M2, M1, M1

    LOAD   M1, [R1 + 0]      ; DRAM load, use 2 - repeated, no reuse
    MATMUL M2, M1, M1

    LOAD   M1, [R1 + 0]      ; DRAM load, use 3 - repeated, no reuse
    MATMUL M2, M1, M1

    HALT
