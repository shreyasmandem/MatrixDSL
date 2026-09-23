# LLVM IR Generation and Optimization — Kandi Jeevitesh Reddy (24BCE0350)

Individual contribution branch for **MatrixDSL**, Team 13, BCSE307P Compiler Design.

This branch contains **only my own work**: LLVM IR generation, the optimization
pipeline, and the runtime library. Team-level deliverables (Review 1 report and
presentation, timeline, responsibility matrix) and the other members' modules live on
[`main`](../../tree/main) and are not duplicated here.

---

## Role

| | |
|---|---|
| **Member** | 3 of 4 |
| **Primary responsibility** | LLVM IR generation + optimization |
| **Supporting responsibility** | Runtime library, cross-module integration |

My stage sits between the front end and the MDT backend. It consumes a shape-annotated
AST from semantic analysis and produces optimized LLVM IR for code generation, so I
share an interface with two of the other three members.

---

## Contents

```
docs/
├── ir-design.md                 Matrix representation, lowering, optimization
└── contribution.md              Review 1 evidence and viva preparation

compiler/llvm/
├── LLVMCodeGen.h                IR generator interface + intrinsic contract
└── LLVMOptimizer.h              Pass pipeline wrapper and statistics

runtime/
├── MatrixRuntime.h              Allocation, output, reference implementations
└── MatrixRuntime.cpp            Working implementation

tools/mtxref/main.cpp            Reference operation runner

tests/llvm/                      Hand-written expected IR per construct
```

---

## The central design decision

Matrix operations are emitted as **opaque intrinsic calls**, not as loop nests:

```llvm
declare void @mdt.matmul.4x4(float* %dst, float* %a, float* %b)
declare void @mdt.relu.4x4(float* %dst, float* %src)
declare void @mdt.transpose.4x4(float* %dst, float* %src)
```

So `C = matmul(A, B)` becomes exactly one instruction:

```llvm
call void @mdt.matmul.4x4(float* %C.data, float* %A.data, float* %B.data)
```

**Why not a loop nest.** The obvious alternative is to emit the textbook triple loop
and let the backend recognise it. That fails in practice: loop-idiom recognition breaks
on reordering, non-constant bounds and possible aliasing — and worse, it is defeated by
*the optimizer's own transformations*. By the time the backend runs, `-O2` may have
unrolled, fused or vectorised the nest into something no pattern matcher will identify.

An opaque call survives `-O2` completely intact. The compiler never has to *recover*
information the front end already had.

Element-wise operations (`A + B`, `s * A`) do lower to ordinary loops, because there
the optimizer's freedom to unroll and vectorise is exactly what we want. The contrast
between `tests/llvm/matmul_4x4.ll` and `tests/llvm/addition_4x4.ll` shows both.

Full detail: [`docs/ir-design.md`](docs/ir-design.md)

---

## Matrix representation

```llvm
%struct.Matrix = type { float*, i32, i32 }   ; data, rows, cols
```

Row-major, contiguous, 64-byte aligned:

```
A[i][j] == A.data[i * A.cols + j]
```

Row-major makes each matrix *row* contiguous, which is what allows a 4×4 tile to be
extracted as four fixed-stride reads — precisely what the MDT `LOADT` instruction does.
The 64-byte alignment matches one MDT matrix register exactly, so a tile load never
straddles an alignment boundary.

---

## Building and running

No LLVM required for this branch — the runtime and reference runner are pure C++17.

```bash
cmake -S . -B build
cmake --build build -j
```

```bash
./build/bin/mtxref all
```

Expected output (abbreviated):

```
MatrixDSL reference implementation runner
=========================================

Matrix multiplication
  PASS  A x I == A
  PASS  rectangular (2x3)x(3x2) element [0][0] == 22

Transpose
  PASS  transpose of 3x4 is 4x3
  PASS  T[i][j] == A[j][i]
  PASS  transpose(transpose(A)) == A

Tile padding
  PASS  5x7 pads to 8x8
  PASS  padding region is zero
  PASS  original logical shape unchanged

-----------------------------------------
  19 checks, 0 failed
-----------------------------------------
```

Run one operation at a time with `mtxref matmul`, `mtxref tiling`, and so on.

---

## Tests

```bash
ctest --test-dir build --output-on-failure
```

The reference checks are written as **algebraic identities**, so they need no
hand-computed expected values and cannot rot:

| Identity | Catches |
|---|---|
| `A × I == A` | Any error in the multiply loop |
| `A - A == 0` | Sign and indexing errors |
| `transpose(transpose(A)) == A` | Row/column confusion |
| `relu(relu(A)) == relu(A)` | Idempotence violations |
| 5×7 pads to 8×8, padding is zero, logical shape preserved | Tiling errors on non-aligned dimensions |

This matters because these implementations are the **oracle** for differential testing:
MDT simulator output is checked against them element for element. If the oracle is
wrong, every downstream verification result is meaningless.

---

## Interface contract with the MDT backend

Fixed with Shreyas before either of us started implementation:

| Guarantee | Provided by |
|---|---|
| Module passes `llvm::verifyModule` | Me |
| Matrix data row-major, contiguous, 64-byte aligned | Me |
| Matrix ops appear only as `@mdt.*` calls, never loop nests | Me |
| Intrinsic operands are `(dst, srcA[, srcB])`, all `float*` | Me |
| Anything larger than 4×4 is already tiled | Me |
| Intrinsics survive `-O2` unmodified | Me |
| Each `@mdt.*` call becomes exactly one MDT instruction | Shreyas |

Agreeing this table up front is the mitigation for an integration failure at week 8,
when there would be very little schedule left to recover.

---

## Status

| Item | Status |
|---|---|
| Matrix representation in LLVM IR | Designed |
| Intrinsic contract with the backend | Agreed and documented |
| Row-major memory layout | Specified |
| IR generator interface | Complete |
| Optimizer interface and statistics | Complete |
| Runtime library | Complete — **not yet compiled** (no C++ toolchain on the authoring machine) |
| Reference operation runner | Complete |
| Expected IR per construct | 4 hand-written files |
| IR generator implementation | Review 2 |

Compiling `mtxref` and running the reference checks is the first task before the
Review 1 demonstration.
