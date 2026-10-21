# Individual Contribution: Shreyas Mandem (Phase 3 / Final Review)

| | |
|---|---|
| **Register Number** | 24BCE0381 |
| **Branch** | `Shreyas` |
| **Role** | MDT ISA, back end, simulator, evaluation, integration |

## What's new in Phase 3

**MDT code generator** (`compiler/backend/MDTCodeGen.{h,cpp}`) takes Tensor IR to
MDT assembly. It is the stage that turns the compiler into an end-to-end system.

* **Memory layout.** Row-major, 4-padded, 64-byte-aligned buffers with a
  *zero-padding invariant*, so ragged shapes (3x5, 5x3·3x7) run exactly on a
  4x4 matrix unit with no masking.
* **Instruction selection** for every Tensor IR op. Fused ops use `MATMULACC`
  and `MATMULRELU`, and the Phase 3 epilogue-fused `relu(A·B+C)` applies `RELU`
  in register before a single store.
* **The identity trick.** MDT has no matrix add, so `A ± B` lowers to
  `MATMULACC` against a resident `±I` tile: 3 instructions per tile instead
  of 16.
* **Scratchpad lowering** of the tiling pass's decisions: `LOAD` +
  `SCRATCHSTORE` once, `SCRATCHLOAD` per reuse.

**Simulator runtime ABI** (`tools/mdtsim`, `docs/runtime-abi.md`): `CALL mdt_print`
and `CALL mdt_softmax` are host functions with register arguments and a modelled
cost. The assembler gains `.printname`, and DRAM grows to fit large images.

**Ablation study** (`tools/mtxbench`): 8 workloads × 4 configurations, every
run verified against the reference interpreter. Measured geo-mean speed-up is
**2.19x** with fusion + tiling, with **3.79x** less DRAM traffic
(`docs/review3/results.md`, `ablation.svg`).

**MatrixDSL Lab** (`tools/mtxserve`, `extension/`): a dependency-free C++ HTTP backend on 127.0.0.1 that exposes the real pipeline as a JSON API, plus a Manifest V3 Chrome extension and dashboard. It compares a traditional compile with MatrixDSL side by side (cards, animated race, simulator timeline, outputs) and reruns the full ablation study live.

**Integration.** All four members' work is merged into one CMake build: 70
CTests, static MinGW binaries, and the final report.

## Findings I own

* **The scratchpad capacity cliff.** Tiling gives 3.00x at 64x64 but only
  1.70x at 128x128, because each 128x128 operand fills the whole 64 KiB.
* **The softmax ISA gap.** `MROWMAX`/`MROWSUM` need a lane-wise `VMAX` and a
  row-broadcast divide before softmax can compile to plain MDT code.

## Evidence

`ctest -L backend` (17 hand-written programs), `ctest -L bench`,
`matrixdslc <file> --run --perf --verify`.
