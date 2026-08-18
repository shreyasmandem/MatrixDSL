# MDT ISA and LLVM Backend — Shreyas Mandem (24BCE0381)

Individual contribution branch for **MatrixDSL**, Team 13, BCSE307P Compiler Design.

This branch contains **only my own work**: the MDT target architecture and the custom
LLVM backend that targets it. Team-level deliverables — the Review 1 report and
presentation, grammar, language specification, frontend interfaces, timeline and
responsibility matrix — live on [`main`](../../tree/main) and are deliberately not
duplicated here.

---

## Role

| | |
|---|---|
| **Member** | 4 of 4 |
| **Primary responsibility** | MDT ISA definition + custom LLVM backend |
| **Supporting responsibility** | Backend tests and assembly verification |

Per the project design document, Member 4 owns *"MDT ISA + Custom LLVM Backend"* with
supporting responsibility for *"Backend tests and assembly"*. Everything in this
branch falls under that scope.

---

## Contents

```
docs/
├── isa.md                    MDT instruction set architecture (frozen)
├── llvm-backend.md           Backend design, lowering, TableGen, risk
└── contribution.md           Review 1 evidence and viva preparation

llvm-backend/MDT/             TableGen target description
├── MDT.td                    Top-level target, subtarget features
├── MDTRegisterInfo.td        R0-R15, V0-V7, M0-M7 and register classes
├── MDTInstrInfo.td           17 instructions, formats, selection patterns
├── MDTCallingConv.td         Argument, return and callee-saved assignment
├── MDTISelLowering.h         MDTISD nodes and lowering interface
├── CMakeLists.txt
└── MCTargetDesc/CMakeLists.txt

tools/mdtsim/                 MDT instruction set simulator (working)
├── MDTSim.h                  Machine state and interfaces
├── Assembler.cpp             Two-pass assembler
├── MDTSim.cpp                Fetch-decode-execute, all 17 instructions
└── main.cpp                  Command-line driver

tests/backend/                10 MDT assembly test programs
```

---

## The MDT target in one screen

| Class | Registers | Instructions |
|---|---|---|
| Scalar | R0–R15 (32-bit; R13 SP, R14 FP, R15 RA) | `ADD` `SUB` `MUL` `LOAD` `STORE` `BR` `BEQ` `BNE` `JMP` `CALL` `RET` |
| Vector | V0–V7 (128-bit = 4×FP32) | `VADD` `VMUL` |
| Matrix | M0–M7 (4×4 FP32 = 64 bytes) | `MATMUL` `TRANSPOSE` `RELU` |

17 instructions, fixed 32-bit width, load/store, little-endian.

**Why a 4×4 tile.** 16 FP32 × 4 bytes = 64 bytes per matrix register. One `MATMUL` is
64 multiply-accumulate operations — real work per instruction — while staying small
enough that the register file is plausible and the simulator stays simple. Matrices
larger than one tile are decomposed by compiler-level tiling.

Full specification: [`docs/isa.md`](docs/isa.md)

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

---

## Test suite

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

---

## Status

| Item | Status |
|---|---|
| MDT ISA specification | Frozen |
| Backend design and risk analysis | Complete |
| TableGen description | Complete |
| MDT simulator | Written; **not yet compiled** — no C++ toolchain on the authoring machine |
| Backend tests | 10 programs written |

Compiling the simulator and running the ten backend tests is the first task before the
Review 1 demonstration.
