# Phase 2 Team Responsibility Matrix

**Project:** MatrixDSL — Phase 2: Tensor / AI Compiler and Accelerator
**Team Number:** 13 | **Project ID:** A3 | **Course:** BCSE307P — Compiler Design

Phase 2 responsibilities map directly onto each member's Phase 1 ownership — nobody
changes lane, everybody's scope grows in the direction their Phase 1 module already
pointed.

---

## Matrix

| Member | Phase 1 owned | Phase 2 addition | Phase 2 evidence expected |
|---|---|---|---|
| **Vinay A**<br>24BCB0131<br>Branch: `Vinay-A` | Lexer, Parser, AST | Grammar extension for the batch dimension (`tensor[B][R][C]`); `softmax` as a fourth builtin function; AST node updates so `MatrixType` carries a batch field | Updated `docs/grammar.md` diff; extended `AST.h`/`MatrixType.h`; parser tests for batch declarations and `softmax(...)` calls |
| **Mudpe Parth Tulsidas**<br>24BDS0353<br>Branch: `parth` | Semantic Analysis, Symbol Table, Matrix Type System | Batch-compatibility shape rule (equal batch, or one side batch=1); shape rule for `softmax` (shape-preserving, like `relu`); dtype compatibility checking for the new BF16-storage annotation | Extended `docs/shape-rules.md`; new batch/dtype test cases in `tests/semantic/` |
| **Kandi Jeevitesh Reddy**<br>24BCE0350<br>Branch: `Jeevitesh` | LLVM IR Generation, Optimization, Runtime | Owns the new **Tensor IR** layer end to end: ~30 op kinds, the operator-fusion pass, the batch-aware hierarchical tiling pass, and lowering fused Tensor IR ops to the extended LLVM intrinsic set | `docs/tensor-ir.md`; Tensor IR class hierarchy; fusion pass with positive and negative pattern-match tests |
| **Shreyas Mandem**<br>24BCE0381<br>Branch: `shreyas` | MDT ISA, Custom LLVM Backend, Simulator | 7 new MDT instructions (`MATMULACC`, `MATMULRELU`, `CVT.F32.BF16`, `CVT.BF16.F32`, `VREDSUM`, `MROWMAX`, `MROWSUM`); scratchpad address space and `SCRATCHLOAD`/`SCRATCHSTORE`; the lightweight analytical performance model in `mdtsim` | `docs/isa-extensions.md`; `docs/memory-hierarchy.md`; simulator extensions with per-instruction tests |

---

## Why this split holds up

Each Phase 2 addition sits at the exact boundary its owner already controlled in Phase 1:

- Vinay already owned "what MatrixDSL programs are allowed to say" — the batch dimension and `softmax` are additions to that surface, nothing more.
- Parth already owned "what shapes are allowed to combine" — batch compatibility is one more rule in the same rule set, using the same accept/reject/cascade-suppression machinery from Phase 1.
- Kandi already owned the LLVM IR boundary and the matrix-intrinsic contract with the backend — the Tensor IR is a new layer *above* that boundary, so it is a natural extension of "what gets handed to Shreyas," not a new responsibility area.
- Shreyas already owned everything below optimized LLVM IR — new instructions, a new memory address space, and a performance model are all backend-and-simulator work, same as Phase 1's ISA and `mdtsim`.

No member is asked to work outside the module boundary they have already built expertise in.

---

## Integration points for Phase 2

| Integration | Between | What must agree |
|---|---|---|
| Batch shape propagation | Vinay → Parth | AST batch field is populated by the parser, consumed and validated by semantic analysis |
| Tensor IR construction | Parth → Kandi | Every shape-annotated AST node must have a corresponding Tensor IR op; `Expr::resultType`'s batch field becomes the Tensor IR op's batch shape |
| Fused intrinsic contract | Kandi → Shreyas | New intrinsic names and signatures (`@mdt.matmul_relu.4x4`, `@mdt.matmul_acc.4x4`) fixed before either side implements, exactly as `@mdt.matmul.4x4` was fixed in Phase 1 |
| Scratchpad addressing | Kandi (tiling pass) → Shreyas (backend + simulator) | The address-space encoding bit and the block sizes the tiling pass assumes must match what the simulator's scratchpad model actually enforces |

---

## Testing and documentation ownership — Phase 2

| Artifact | Owner |
|---|---|
| Tensor IR unit tests | Kandi Jeevitesh Reddy |
| Fusion pass positive/negative tests | Kandi Jeevitesh Reddy |
| Batch/dtype semantic tests | Mudpe Parth Tulsidas |
| Parser tests for batch syntax and `softmax` | Vinay A |
| New-instruction simulator tests | Shreyas Mandem |
| Memory hierarchy / scratchpad tests | Shreyas Mandem |
| Benchmark programs (`ffn_block.mtx`, `attention_head.mtx`) | All — written jointly, verified individually against each member's own module |
| Ablation study (§8 of the Phase 2 report) | Shreyas Mandem (owns the performance model that produces the numbers), analysis written jointly |
| Review 2 report consolidation | All |
