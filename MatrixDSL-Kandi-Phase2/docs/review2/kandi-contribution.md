# Individual Contribution — Kandi Jeevitesh Reddy (Phase 2 / Review 2)

| | |
|---|---|
| **Name** | Kandi Jeevitesh Reddy |
| **Register Number** | 24BCE0350 |
| **Branch** | `Jeevitesh` |
| **Review** | Review 2 |

## What's new

`compiler/tensor-ir/TensorIR.cpp` implements the frozen `TensorIR.h` contract
(`docs/tensor-ir.md`): `TensorFunction::addOp`, `dump()`, `tensorOpKindName`,
`isTier2`.

`compiler/tensor-ir/FusionPass.{h,cpp}` implements the operator fusion pass —
`docs/tensor-ir.md` §5's pattern table: `MatMul → Relu` fuses to
`FusedMatMulRelu`; `MatMul → Add(bias)` fuses to `FusedMatMulAcc`. Both checks
use-count, not just adjacency — a value with a second use is never fused away,
verified by `mtxtensorir`'s negative test.

`tools/mtxtensorir/main.cpp` is the "runs without the parser" demo, same
pattern as Parth's `mtxcheck` at Review 1: hand-built Tensor IR graphs, since
AST→TensorIR lowering is a Review 3 target, not this one.

## Evidence

| File | Contents |
|---|---|
| `compiler/tensor-ir/TensorIR.cpp` | Frozen interface implementation |
| `compiler/tensor-ir/FusionPass.h`/`.cpp` | The fusion pass |
| `tools/mtxtensorir/main.cpp` | Positive (`matmul→relu` fuses) and negative (second-use blocks fusion) demo |

Syntax-checked (`g++ -fsyntax-only`), not yet fully built/linked in this final
packaging pass — build and run before Review 2:

```bash
cmake -S . -B build && cmake --build build -j && ./build/bin/mtxtensorir
```

## Review 3 targets

AST → Tensor IR lowering; the hierarchical tiling pass (`docs/memory-hierarchy.md`
§6); wiring fused ops to Shreyas's `MATMULACC`/`MATMULRELU` intrinsics.
