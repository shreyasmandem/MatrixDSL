# MDT ISA and LLVM Backend — Shreyas Mandem (24BCE0381)

Individual contribution branch for **MatrixDSL**, Team 13, BCSE307P Compiler Design.

This branch contains **only my own work**: the MDT target architecture, its Phase 2
extensions, and the custom LLVM backend that targets it. Team-level deliverables —
the Review 1 and Review 2 reports, grammar, language specification, frontend
interfaces, timeline and responsibility matrix — live on [`main`](../../tree/main)
and are deliberately not duplicated here.

---

## Role

| | |
|---|---|
| **Member** | 4 of 4 |
| **Primary responsibility** | MDT ISA definition + custom LLVM backend |
| **Phase 2 responsibility** | ISA extensions, memory hierarchy, performance model |
| **Supporting responsibility** | Backend tests and assembly verification |

---

## Contents

```
docs/
├── isa.md                    MDT instruction set architecture (Phase 1, frozen)
├── isa-extensions.md         Phase 2: 7 new instructions, encoding, rationale
├── memory-hierarchy.md       Phase 2: scratchpad, addressing, performance model
├── llvm-backend.md           Backend design, lowering, TableGen, risk
├── contribution.md           Review 1 evidence and viva preparation
└── review2/
    └── shreyas-contribution.md   Review 2 evidence and viva preparation

llvm-backend/MDT/             TableGen target description (Phase 1)
├── MDT.td                    Top-level target, subtarget features
├── MDTRegisterInfo.td        R0-R15, V0-V7, M0-M7 and register classes
├── MDTInstrInfo.td           17 instructions, formats, selection patterns
├── MDTCallingConv.td         Argument, return and callee-saved assignment
├── MDTISelLowering.h         MDTISD nodes and lowering interface
├── CMakeLists.txt
└── MCTargetDesc/CMakeLists.txt

tools/mdtsim/                 MDT instruction set simulator (working, tested)
├── MDTSim.h                  Machine state, 24 instructions, performance model
├── Assembler.cpp             Two-pass assembler
├── MDTSim.cpp                Fetch-decode-execute + performance model
└── main.cpp                  Command-line driver (--perf and friends)

tests/backend/                17 MDT assembly test programs (10 + 7 Phase 2)
```

---

## The MDT target in one screen

| Class | Registers | Phase 1 instructions | Phase 2 additions |
|---|---|---|---|
| Scalar | R0–R15 (32-bit; R13 SP, R14 FP, R15 RA) | `ADD` `SUB` `MUL` `LOAD` `STORE` `BR` `BEQ` `BNE` `JMP` `CALL` `RET` | `CVT.F32.BF16` `CVT.BF16.F32` |
| Vector | V0–V7 (128-bit = 4×FP32) | `VADD` `VMUL` | `VREDSUM` |
| Matrix | M0–M7 (4×4 FP32 = 64 bytes) | `MATMUL` `TRANSPOSE` `RELU` | `MATMULACC` `MATMULRELU` `MROWMAX` `MROWSUM` |

**24 instructions** (17 + 7), fixed 32-bit width, load/store, little-endian. Plus a
new scratchpad memory tier reached via one address-space bit reused from Phase 1's
reserved encoding space — no opcode spent on it (`docs/memory-hierarchy.md` §3).

**Why a 4×4 tile.** 16 FP32 × 4 bytes = 64 bytes per matrix register. One `MATMUL`
is 64 multiply-accumulate operations — real work per instruction — while staying
small enough that the register file is plausible and the simulator stays simple.

**Why `MATMULRELU`.** XLA's real epilogue fusion merges an elementwise op onto the
end of a reduction kernel like matmul, because the result already sits in the
accumulator before the store — clamping it there costs nothing extra. `MATMULRELU`
is MDT's hardware-level implementation of exactly that idea, and the instruction
the Phase 2 fusion pass targets for the `matmul → relu` pattern.

Full specifications: [`docs/isa.md`](docs/isa.md), [`docs/isa-extensions.md`](docs/isa-extensions.md)

---

## Building and running the simulator

Standalone — no LLVM dependency:

```bash
cmake -S . -B build
cmake --build build -j
```

```bash
./build/bin/mdtsim tests/backend/matmul_4x4.s --dump-matrix M0
```

Expected output — A × Aᵀ, which is symmetric, so the test checks itself:

```
M0 =
  [    30.00    70.00   110.00   150.00 ]
  [    70.00   174.00   278.00   382.00 ]
  [   110.00   278.00   446.00   614.00 ]
  [   150.00   382.00   614.00   846.00 ]
```

The Phase 2 performance model — instruction count, bytes moved, and two cycle
estimates (sequential vs. overlap-aware) — is one flag away:

```bash
./build/bin/mdtsim tests/backend/tiling_scratchpad.s --perf --quiet
```

```
Performance model (not cycle-accurate - see docs/memory-hierarchy.md section 4)
------------------------------------------------------------------------
  Instructions executed        : 10
  Bytes moved (DRAM)           : 64
  Bytes moved (scratchpad)     : 256
  Estimated cycles, sequential : 26
  Estimated cycles, overlap-aware: 22
  Overlap reduction            : 15.4%
```

Compare against `tiling_naive.s` (37 sequential / 28 overlap-aware, 192 bytes from
DRAM instead of 64) to see the whole point of the memory hierarchy in two numbers.

---

## Test suite

### Phase 1 (re-verified — see Status below)

| Test | Covers |
|---|---|
| `scalar_arith.s` | `ADD`, `SUB`, `MUL` |
| `memory.s` | `LOAD` / `STORE` round-trip |
| `control_flow.s` | `BEQ`, `BR`, loop termination |
| `call_return.s` | `CALL`, `RET`, return address |
| `vector_ops.s` | `VADD`, `VMUL`, 4-lane semantics |
| `matmul_4x4.s` | `MATMUL` + `TRANSPOSE` (self-checking: result must be symmetric) |
| `transpose.s` | `TRANSPOSE` as an involution (self-checking: transposing twice returns the input) |
| `relu.s` | `RELU` |
| `ai_layer.s` | Full `relu(W × X)` layer |
| `tile_strided.s` | Strided 4×4 tile extraction from an 8×8 matrix |

### Phase 2

| Test | Covers |
|---|---|
| `matmulacc.s` | `MATMULACC` — result depends on the destination's prior value, not just A and B |
| `matmulrelu.s` | `MATMULRELU` (self-checking: matches `relu.s`'s known output via `A × I`) |
| `bf16_convert.s` | `CVT.F32.BF16` / `CVT.BF16.F32`, exact bit-pattern check |
| `reductions.s` | `MROWMAX`, `MROWSUM`, `VREDSUM` (self-checking: sum-of-row-sums equals the grand total) |
| `scratchpad_roundtrip.s` | Scalar and matrix-tile round trip through DRAM → register → scratchpad → register → DRAM |
| `tiling_naive.s` / `tiling_scratchpad.s` | Matched ablation-demo pair — identical computation, only the memory path differs; run both with `--perf` and compare |

---

## Status

| Item | Status |
|---|---|
| MDT ISA specification (Phase 1) | Frozen |
| MDT ISA extensions (Phase 2) | Frozen, implemented |
| Memory hierarchy + performance model (Phase 2) | Implemented |
| Backend design and risk analysis | Complete |
| TableGen description | Complete (Phase 1 instructions; Phase 2 extensions not yet added to `.td`) |
| MDT simulator | **Built and tested** — MinGW-W64 GCC 16.1.0, zero warnings |
| Backend tests | **17/17 passing**, every one checked against an independently computed expected value, not just exit code |

Review 1 closed with "not yet compiled — no C++ toolchain on the authoring
machine." A toolchain became available before Review 2 work started, and the very
first thing done with it was building and running everything already on this
branch — which found and fixed two real bugs in the Phase 1 assembler (see
`docs/review2/shreyas-contribution.md` §2). Every test below, Phase 1 and Phase 2
alike, is now verified, not merely written.
