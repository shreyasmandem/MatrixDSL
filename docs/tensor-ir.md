# Tensor IR — Phase 2

Owner: **Kandi Jeevitesh Reddy (24BCE0350)**. Status: **frozen for Review 2**
(interface); implementation is Kandi's individual Phase 2 contribution.

## 1. Why this layer exists

`docs/review2/Review2_Report.md` §3 recommends a hand-rolled Tensor IR between
semantic analysis and LLVM IR generation, deliberately inspired by MLIR's
progressive-lowering idea without depending on the MLIR framework. This document
is that layer's frozen interface: the operation catalogue every other module
builds against.

## 2. Position in the pipeline

```
Shape-annotated AST (batch-aware, from semantic analysis)
        |
        v
   Tensor IR construction        1:1 lowering from AST nodes
        |
        v
   Operator fusion pass          pattern-match -> Tier 2 ops
        |
        v
   Hierarchical tiling pass      batch/tile loop + scratchpad staging
        |
        v
   LLVM IR generation            Tier 2 ops -> extended intrinsic calls
```

## 3. The operation catalogue

The Phase 2 report estimated "~30 op kinds" before the concrete design was worked
out. The design that was actually worked out converges on a tighter set: **13
Tier 1 (source-level) kinds and 6 Tier 2 (pass-produced) kinds — 19 total.** A
smaller, fully-justified catalogue is preferred over padding to a round number;
the catalogue is expected to grow only as new source-level operations are added
in a later phase, not for its own sake.

### 3.1 Tier 1 — source-level, ~1:1 with the AST

| Kind | Arity | Shape rule | Notes |
|---|---|---|---|
| `Const` | 0 | scalar (1,1,1) | A compile-time scalar literal |
| `Load` | 0 | as declared | Reference to a declared tensor/matrix value |
| `Add` | 2 | batch-broadcast, rows/cols must match | |
| `Sub` | 2 | batch-broadcast, rows/cols must match | |
| `ScalarMul` | 2 | one operand must be scalar | |
| `MatMul` | 2 | batch-broadcast, `lhs.cols == rhs.rows` | The op the whole fusion story targets |
| `Transpose` | 1 | `(b,r,c) -> (b,c,r)` | |
| `Relu` | 1 | shape-preserving | |
| `Softmax` | 1 | shape-preserving | Decomposed into `RowMax`/`Sub`/`Exp`/`RowSum`/`Divide` before lowering — see §4 |
| `RowMax` | 1 | `(b,r,c) -> (b,r,1)` | Softmax decomposition primitive |
| `RowSum` | 1 | `(b,r,c) -> (b,r,1)` | Softmax decomposition primitive |
| `Exp` | 1 | shape-preserving | Softmax decomposition primitive; always lowers to a runtime call, never a hardware instruction (`docs/isa-extensions.md` §5) |
| `Print` | 1 | — (no result) | |

### 3.2 Tier 2 — produced only by the fusion and tiling passes

| Kind | Produced by | Lowers to (see `docs/isa-extensions.md` §6) |
|---|---|---|
| `FusedMatMulRelu` | Fusion pass, pattern `MatMul -> Relu` | `MATMULRELU` |
| `FusedMatMulAcc` | Fusion pass, pattern `MatMul -> Add(bias)` | `MATMULACC` |
| `TiledMatMul` | Tiling pass | A loop of `MATMUL`/`MATMULACC` over the tile grid |
| `ScratchLoad` | Tiling pass | `SCRATCHLOAD` |
| `ScratchStore` | Tiling pass | `SCRATCHSTORE` |
| `ConvertPrecision` | Tiling pass (optional, BF16-storage path) | `CVT.F32.BF16` / `CVT.BF16.F32` |

A Tier 1 op is never lowered directly to MDT assembly. Every Tier 1 op is first
rewritten — trivially, as itself, if no pattern matches — into a form that is
either a Tier 2 op or a Tier 1 op the LLVM IR generator already knows how to
lower unfused (exactly as Phase 1's `matmul`/`relu` lowered to separate
`@mdt.matmul.4x4`/`@mdt.relu.4x4` calls when nothing fused them).

## 4. `softmax` decomposition

`softmax` is a single Tensor IR node at construction time (matching the single
`CallExpr(softmax, ...)` in the AST), and is expanded into primitives by a
canonicalisation step that runs before fusion:

```
softmax(X):
    m = RowMax(X)
    s = Sub(X, m)          ; broadcast subtract, numerical stability
    e = Exp(s)              ; runtime call
    d = RowSum(e)
    Y = Divide(e, d)        ; broadcast divide
```

`Sub` and `Divide` here reuse the Tier 1 `Add`/`Sub`/`ScalarMul`-family shape
rules with row-broadcast (an `(b,r,1)` operand broadcasts across all `c`
columns) — a narrower, explicitly-scoped broadcast than the batch-broadcast rule
in `docs/review2/Review2_Report.md` §2.1, added specifically because softmax
cannot be expressed without it.

## 5. Fusion pattern table

| Source pattern | Fused to | Rationale |
|---|---|---|
| `MatMul -> Relu` (Relu's only input is the MatMul, MatMul has no other use) | `FusedMatMulRelu` | Highest-frequency pattern in both Phase 2 benchmarks |
| `MatMul -> Add` where the `Add`'s other operand is not itself derived from the same `MatMul` | `FusedMatMulAcc` | Bias/residual pattern |
| `MatMul -> Add -> Relu` | `FusedMatMulAcc` followed by a separate `Relu` (2 ops, not 3) | No dedicated 3-way opcode exists; fusion still removes one operation. See Review 2 report §6 for why this is a deliberate ISA-growth discipline, not an oversight. |

A pattern only fuses when the intermediate value has **no other use** — fusing a
value that is also consumed elsewhere would require materialising it anyway,
which defeats the purpose. The fusion pass must check use-count, not just
adjacency, before rewriting.

## 6. Testing contract

Every fusion rule needs both a positive test (the pattern is present, fusion
happens) and a negative test (a superficially similar but non-fusible shape does
**not** fuse — e.g. the intermediate value has a second use, or the two operands
of `Add` are unrelated to the `MatMul`). A fusion pass that over-fuses is worse
than one that under-fuses: it silently changes what a program computes rather
than only how fast it runs.
