# Individual Contribution — Kandi Jeevitesh Reddy

| | |
|---|---|
| **Name** | Kandi Jeevitesh Reddy |
| **Register Number** | 24BCE0350 |
| **Role** | LLVM IR Generation + Optimization |
| **Branch** | `Jeevitesh` |
| **Review** | Review 1 |

---

## 1. Scope of my component

I own the middle of the compiler: converting the shape-annotated AST into LLVM IR,
running LLVM's optimization pipeline over it, and providing the runtime library that
compiled programs link against.

My stage is the only one with an interface on *both* sides — it consumes Parth's
annotated AST and produces the IR that Shreyas's backend consumes. That makes the two
interface contracts (AST shape annotation in, matrix intrinsics out) the most important
things I had to get agreed before writing any code.

---

## 2. What I completed for Review 1

### 2.1 Matrix representation in LLVM IR

`docs/ir-design.md` §2

```llvm
%struct.Matrix = type { float*, i32, i32 }   ; data, rows, cols
```

Row-major, contiguous, 64-byte aligned. `A[i][j] == A.data[i * A.cols + j]`.

**Design decision — descriptor rather than a bare array.** Shape is known at compile
time, so the dimensions could in principle be baked into every access. The descriptor
is kept because `print` needs true logical dimensions at runtime, the tiling path
produces padded copies whose physical shape differs from the logical shape, and it
matches the runtime's `MDMatrix` struct exactly so a matrix crosses the
compiled/runtime boundary with no marshalling.

**Design decision — row-major.** Makes each matrix *row* contiguous, which is what
allows a 4×4 tile to be extracted as four fixed-stride reads. That is exactly what MDT's
`LOADT` does, so the storage choice and the ISA agree rather than fighting each other.

**Design decision — 64-byte alignment.** One MDT matrix register holds exactly a
64-byte 4×4 FP32 tile. Aligning the base means a tile load never straddles an alignment
boundary and stays a single aligned transfer.

### 2.2 The matrix intrinsic contract — the decision this module turns on

`docs/ir-design.md` §5, `compiler/llvm/LLVMCodeGen.h`

```llvm
declare void @mdt.matmul.4x4(float* %dst, float* %a, float* %b)
declare void @mdt.relu.4x4(float* %dst, float* %src)
declare void @mdt.transpose.4x4(float* %dst, float* %src)
```

Matrix operations are emitted as **opaque calls**, not as loop nests.

**Why.** The alternative is to emit the textbook triple loop and let the backend
recognise it as a matrix multiply. That fails for two reasons. First, loop-idiom
recognition is fragile — it breaks on loop reordering, on bounds that are not
compile-time constants, and on possible aliasing between operands. Second and worse, it
is defeated by *the optimizer's own transformations*: by the time the backend sees the
code, `-O2` may have unrolled, fused, interchanged or vectorised the nest into something
no pattern matcher will identify.

An opaque call survives `-O2` completely intact and lowers to exactly one MDT
instruction. The compiler never has to recover information the front end already had.

**Consequence for the optimizer configuration.** These calls are declared *without*
`readnone` or `speculatable`. Marking them pure would let the optimizer sink, duplicate
or eliminate them, breaking the one-call-one-instruction guarantee.

**Consequence for naming.** The tile geometry (`4x4`) is part of the name, so the
backend never has to infer it — by the time an intrinsic is emitted, tiling has already
decomposed anything larger.

Element-wise operations are the deliberate opposite: `A + B` lowers to an ordinary loop
with a constant trip count, because there the optimizer's freedom to unroll and
vectorise is exactly what we want.

### 2.3 IR generator and optimizer interfaces

`compiler/llvm/LLVMCodeGen.h`, `LLVMOptimizer.h`

The generator assumes any AST reaching it has passed semantic analysis — every
`Expr::resultType` populated, every identifier resolved, every shape rule verified. It
does **no** shape re-checking. Duplicating those checks would mean two places to keep in
sync and two places for them to disagree.

`OptStats` captures instruction, basic-block and function counts either side of the
pipeline, with `instructionReduction()` reporting the percentage change. That is the
"generated LLVM IR before and after optimization" evidence the project evaluation asks
for at Review 3.

### 2.4 Optimization pipeline plan

`docs/ir-design.md` §7 — `-O2` via `PassBuilder`, with the passes that actually matter
for MatrixDSL output identified: `sroa`/`mem2reg` to promote descriptor fields out of
memory, `gvn` for repeated element loads, `licm` to hoist address computation out of
tiling loops, `loop-unroll` for constant-trip-count element-wise loops, and `dce` for
matrices computed but never printed.

### 2.5 Runtime library — working implementation

`runtime/MatrixRuntime.h`, `MatrixRuntime.cpp`

Allocation, formatted output, tile padding, and reference implementations of all six
operations.

Implementation points worth defending:

- **64-byte aligned allocation**, with the platform split handled (`_aligned_malloc` on
  Windows, `std::aligned_alloc` elsewhere) and the size rounded up as `aligned_alloc`
  requires.
- **`matrix_pad_to_tile` copies row by row**, because source and destination row strides
  differ — a single `memcpy` would be wrong for any non-aligned width.
- **Logical shape is preserved** when padding: only the padded copy sees 8×8, while the
  original descriptor still reports 5×7 so `print` and shape checking are unaffected.
- **`matrix_equal` uses a tolerance**, because tiling reorders accumulation and FP32
  addition is not associative. Exact equality would produce false failures on correct
  code.

### 2.6 Reference operation runner — verifiable output

`tools/mtxref/main.cpp`

Exercises every reference implementation and checks the algebraic identities that must
hold. The checks are **identities rather than hand-computed constants**, so they cannot
rot:

| Identity | Catches |
|---|---|
| `A × I == A` | Any error in the multiply loop |
| `A - A == 0` | Sign and indexing errors |
| `transpose(transpose(A)) == A` | Row/column confusion |
| `relu(relu(A)) == relu(A)` | Idempotence violations |
| 5×7 pads to 8×8, padding zero, logical shape preserved | Tiling errors on non-aligned input |

This matters more than it might look. These implementations are the **oracle** for
differential testing — MDT simulator output is checked against them element for
element. If the oracle is wrong, every downstream verification result is meaningless.

### 2.7 Expected IR files

`tests/llvm/` — four hand-written reference IR files.

They serve two purposes: they pin down the IR contract *before* the generator exists,
so Shreyas can develop instruction selection against real IR instead of waiting for me;
and they become golden-file comparisons once the generator lands in Review 2.

The contrast between `matmul_4x4.ll` (one call) and `addition_4x4.ll` (a loop) is the
clearest single statement of this module's design.

---

## 3. Evidence

| Evidence | Location |
|---|---|
| IR design and lowering | `docs/ir-design.md` |
| IR generator interface + intrinsic contract | `compiler/llvm/LLVMCodeGen.h` |
| Optimizer interface and statistics | `compiler/llvm/LLVMOptimizer.h` |
| Runtime interface | `runtime/MatrixRuntime.h` |
| Runtime implementation | `runtime/MatrixRuntime.cpp` |
| Reference runner | `tools/mtxref/main.cpp` |
| Expected IR | `tests/llvm/*.ll` |
| Commits | branch `Jeevitesh` |

---

## 4. Build status

```bash
cmake -S . -B build
cmake --build build -j
./build/bin/mtxref all
ctest --test-dir build --output-on-failure
```

**Note for the review:** the runtime has not yet been compiled, because no C++ toolchain
was installed on the machine used to author it. Compiling `mtxref` and running the
reference checks is the first task before the Review 1 demonstration, and any resulting
fixes will be committed to this branch.

---

## 5. Review 2 targets

| # | Target |
|---|---|
| 1 | `mtxref` compiled, all reference checks passing |
| 2 | IR generation for all 8 MatrixDSL operations |
| 3 | Every generated module passing `llvm::verifyModule` |
| 4 | `-O2` pipeline running, before/after statistics reported |
| 5 | Generated IR matching the hand-written files in `tests/llvm/` |

---

## 6. What I can be questioned on

- Why matrix operations are opaque calls instead of loop nests, and what breaks otherwise
- Why the intrinsics must not be marked `readnone` or `speculatable`
- Why element-wise operations *are* loops when matrix operations are not
- Why the matrix descriptor is kept when shape is known at compile time
- Why row-major, and how it relates to MDT's tile loads
- Why allocation is 64-byte aligned
- Why `matrix_equal` uses a tolerance instead of exact comparison
- What `llvm::verifyModule` failing would mean, and why it is always a compiler bug
- Which optimization passes actually affect MatrixDSL output, and why
