# LLVM IR Design for MatrixDSL

Owner: **Kandi Jeevitesh Reddy (24BCE0350)**

How MatrixDSL programs are represented in LLVM IR, how each operation is lowered, and
which optimizations are applied. This is the interface between the front end (Vinay,
Parth) and the MDT backend (Shreyas).

---

## 1. Why LLVM IR sits in the middle

Placing LLVM IR between the front end and the MDT backend buys three things:

1. **A stable module boundary.** The frontend team and the backend team develop against
   LLVM IR rather than against each other's in-progress code.
2. **Optimization for free.** Constant folding, dead code elimination, common
   subexpression elimination and loop simplification come from LLVM's existing pass
   pipeline instead of being reimplemented.
3. **Retargetability.** The same front end can emit host code by swapping the backend,
   which is what makes differential testing possible — the same program compiled to
   x86-64 and to MDT must produce identical numbers.

Point 3 is the one that matters most for verification: without it, checking MDT output
would require hand-computed expected values for every test.

---

## 2. Matrix representation

A matrix is a descriptor: a pointer to contiguous FP32 data plus its dimensions.

```llvm
%struct.Matrix = type { float*, i32, i32 }   ; data, rows, cols
```

Data is stored **row-major** and contiguously:

```
A = [[1, 2, 3],
     [4, 5, 6]]

memory: 1 2 3 4 5 6

A[i][j] == A.data[i * A.cols + j]
```

### Why a descriptor rather than a flat array

Shape is known at compile time, so in principle the dimensions could be baked into
every access and the descriptor dropped. It is kept for three reasons:

- `print` needs the true logical dimensions at runtime.
- The tiling path produces zero-padded copies whose physical shape differs from the
  logical shape; the descriptor keeps both available.
- It matches the runtime library's `MDMatrix` struct exactly, so a matrix can cross the
  compiled/runtime boundary with no marshalling.

### Why row-major

Matches C convention, and makes each matrix *row* contiguous. Row contiguity is what
makes a 4×4 tile extractable as four fixed-stride reads, which is exactly what the MDT
`LOADT` instruction does.

---

## 3. Declaration lowering

```
matrix A[4][4];
```

becomes an alloca of `rows * cols` floats, zero-initialised, plus a descriptor:

```llvm
%A.data = alloca [16 x float], align 64
%A = alloca %struct.Matrix, align 8

; zero the storage
%A.raw = bitcast [16 x float]* %A.data to i8*
call void @llvm.memset.p0i8.i64(i8* align 64 %A.raw, i8 0, i64 64, i1 false)

; fill in the descriptor
%A.dataptr = getelementptr inbounds %struct.Matrix, %struct.Matrix* %A, i32 0, i32 0
%A.first   = getelementptr inbounds [16 x float], [16 x float]* %A.data, i32 0, i32 0
store float* %A.first, float** %A.dataptr
%A.rowsptr = getelementptr inbounds %struct.Matrix, %struct.Matrix* %A, i32 0, i32 1
store i32 4, i32* %A.rowsptr
%A.colsptr = getelementptr inbounds %struct.Matrix, %struct.Matrix* %A, i32 0, i32 2
store i32 4, i32* %A.colsptr
```

`align 64` is deliberate: one MDT matrix register holds exactly a 64-byte 4×4 FP32 tile,
so a 64-byte-aligned base means a tile load never straddles an alignment boundary.

---

## 4. Element-wise operation lowering

`A + B`, `A - B` and `s * A` lower to a loop over the flat data array. Because the
element count is a compile-time constant, LLVM's optimizer can unroll or vectorise it
without any help from us.

```llvm
; C = A + B, 16 elements
br label %loop

loop:
  %i = phi i32 [ 0, %entry ], [ %i.next, %loop ]
  %a.ptr = getelementptr inbounds float, float* %A.data, i32 %i
  %b.ptr = getelementptr inbounds float, float* %B.data, i32 %i
  %c.ptr = getelementptr inbounds float, float* %C.data, i32 %i
  %a.val = load float, float* %a.ptr
  %b.val = load float, float* %b.ptr
  %sum   = fadd float %a.val, %b.val
  store float %sum, float* %c.ptr
  %i.next = add nuw nsw i32 %i, 1
  %done = icmp eq i32 %i.next, 16
  br i1 %done, label %exit, label %loop

exit:
```

`nuw nsw` on the induction variable is safe — the trip count is a known constant well
inside i32 range — and it lets the optimizer reason about the loop more aggressively.

---

## 5. Matrix operation lowering — the critical decision

Matrix operations are **not** emitted as loop nests. They are emitted as calls to
reserved intrinsic names:

```llvm
declare void @mdt.matmul.4x4(float* %dst, float* %a, float* %b)
declare void @mdt.relu.4x4(float* %dst, float* %src)
declare void @mdt.transpose.4x4(float* %dst, float* %src)
```

So `C = matmul(A, B)` with 4×4 operands becomes exactly one instruction:

```llvm
call void @mdt.matmul.4x4(float* %C.data, float* %A.data, float* %B.data)
```

### Why not a loop nest

The obvious alternative is to emit the textbook triple loop and let the backend
recognise it as a matrix multiply. That fails in practice:

- Loop-idiom recognition is fragile — it breaks on loop reordering, on bounds that are
  not compile-time constants, and on possible aliasing between operands.
- Worse, it is defeated by **the optimizer's own transformations**. By the time the
  backend sees the code, `-O2` may have unrolled, fused, interchanged or vectorised the
  nest into something no pattern matcher will identify.

An opaque call survives `-O2` completely intact and lowers to a single MDT instruction.
The compiler never has to *recover* information the front end already had.

This is the same principle behind Triton and Halide: preserve domain semantics down to
the code generator rather than reconstructing them from scalar code.

### Intrinsic attributes

These calls are declared **without** `readnone` or `speculatable`. Marking them pure
would let the optimizer sink, duplicate or eliminate them, which would break the
guarantee that each call becomes exactly one target instruction.

### Tile geometry in the name

`4x4` is part of the intrinsic name, so the backend never has to infer tile geometry.
By the time an intrinsic call is emitted, the tiling pass has already decomposed
anything larger.

---

## 6. Tiling for larger matrices

Matrices larger than 4×4 are decomposed before the intrinsic calls are emitted:

```
for i in 0 .. M/4:
    for j in 0 .. N/4:
        zero accumulator tile
        for k in 0 .. K/4:
            call @mdt.matmul.4x4(tmp, A_tile[i][k], B_tile[k][j])
            accumulate tmp into the accumulator
        store accumulator to C_tile[i][j]
```

Dimensions that are not multiples of four are zero-padded up to the next multiple via
`matrix_pad_to_tile`. The logical dimensions stay in the descriptor, so `print` and
shape checking still see the true size.

The accumulation order differs from the naive definition, and FP32 addition is not
associative, so a tiled result may differ from the reference in the low bits. Verification
therefore uses a tolerance rather than bitwise equality.

---

## 7. Optimization pipeline

`-O2` via `PassBuilder`, using LLVM's default pipeline. The passes that actually matter
for MatrixDSL output:

| Pass | Effect on MatrixDSL code |
|---|---|
| `instcombine` | Folds redundant GEP and pointer arithmetic |
| `sroa` / `mem2reg` | Promotes descriptor fields out of memory into registers |
| `gvn` | Removes repeated loads of the same element |
| `licm` | Hoists loop-invariant address computation out of tiling loops |
| `loop-unroll` | Unrolls constant-trip-count element-wise loops |
| `dce` / `adce` | Removes matrices computed but never printed |
| `simplifycfg` | Cleans up the branch structure of generated loops |

**What is measured for Review 3:** instruction count, basic block count and function
count before and after the pipeline. `OptStats::instructionReduction()` reports the
percentage change, which is the "generated LLVM IR before and after optimization"
evidence the project evaluation asks for.

---

## 8. Verification

Every generated module is checked with `llvm::verifyModule` before it leaves this
stage. A verifier failure is always a compiler bug, never a user error, so it aborts
rather than producing a diagnostic.

Beyond that, the IR is verified structurally:

| Check | What it catches |
|---|---|
| Module verifies | Malformed IR |
| Descriptor type present and correctly shaped | Representation drift |
| Allocation size equals `rows * cols * 4` | Off-by-one in size computation |
| Declared matrices are zeroed | Uninitialised reads |
| Matrix ops emit intrinsic calls, not loops | Accidental fallback to loop lowering |
| Element access computes `i * cols + j` | Column-major confusion |
| Same source yields identical IR across runs | Non-determinism, e.g. iteration over a hash map |

The last one is easy to overlook and unpleasant to debug later: iterating an unordered
container while emitting IR produces output that differs between runs and makes
golden-file tests flaky.

---

## 9. Interface contract with the backend

This is the agreement with Shreyas, fixed before either of us started implementation:

| Guarantee | Provided by |
|---|---|
| Module passes `llvm::verifyModule` | IR generation |
| Matrix data is row-major, contiguous, 64-byte aligned | IR generation |
| Matrix ops appear only as `@mdt.*` calls, never as loop nests | IR generation |
| Intrinsic operands are `(dst, srcA[, srcB])`, all `float*` | IR generation |
| Anything larger than 4×4 is already tiled | IR generation |
| Intrinsics survive `-O2` unmodified | Optimizer configuration |
| Each `@mdt.*` call becomes exactly one MDT instruction | MDT backend |

If either side needs to change this table, both sides have to agree. That is the
mitigation for the risk of an integration failure at week 8, when there would be very
little schedule left to recover.
