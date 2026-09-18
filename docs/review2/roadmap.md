# Phase 2 Implementation Roadmap

**Project:** MatrixDSL — Phase 2 | **Team 13** | **Duration:** Weeks 13–28 (16 weeks, continuing directly from the Phase 1 12-week plan)

---

## 1. Sixteen-week plan

| Weeks | Phase | Deliverable | Lead |
|---|---|---|---|
| 13–14 | Phase 2 design freeze | This report; Tensor IR op catalogue; ISA extension spec; memory hierarchy spec | All |
| 15–16 | Grammar and semantic extensions | Batch dimension parsing, `softmax` builtin, batch-compatibility shape rule | Vinay, Parth |
| 17–18 | Tensor IR construction | Tensor IR class hierarchy; AST → Tensor IR lowering; unit tests per op kind | Kandi |
| 19–20 | Operator fusion pass | Pattern matcher for `matmul→relu` and `matmul→add`; positive/negative tests | Kandi |
| 20–21 | New MDT instructions (compute) | `MATMULACC`, `MATMULRELU`, `VREDSUM`, `MROWMAX`, `MROWSUM` in TableGen + simulator | Shreyas |
| 21–22 | Mixed-precision storage | `CVT.F32.BF16` / `CVT.BF16.F32`; BF16 storage path in the runtime | Shreyas |
| 22–24 | Memory hierarchy | Scratchpad address space; `SCRATCHLOAD`/`SCRATCHSTORE`; hierarchical batch-aware tiling pass | Kandi (pass), Shreyas (backend + sim) |
| 24–25 | Performance model | Per-instruction latency table; analytical overlap estimator in `mdtsim` | Shreyas |
| 25–26 | Benchmark workloads | `ffn_block.mtx`, `attention_head.mtx`; end-to-end compilation and simulation | All |
| 26–27 | Ablation study | Run the 4-configuration fusion×tiling experiment; collect instruction count, bytes moved, estimated cycles | All (Shreyas leads measurement) |
| 27–28 | Testing, evaluation, reporting | Full test suite passing; Review 3 report and demonstration prep | All |

---

## 2. Gantt chart

```
Week                  13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28
                       |--|--|--|--|--|--|--|--|--|--|--|--|--|--|--|--|

Design freeze          ####
Grammar/semantic             ########
Tensor IR construction              ########
Fusion pass                               ########
New instructions (compute)                   ######
Mixed-precision storage                            ######
Memory hierarchy                                         ##########
Performance model                                                  ######
Benchmark workloads                                                      ######
Ablation study                                                                 ######
Testing + evaluation                                                                 ########
Report + demo prep     ..............................................................########

Legend:  #  primary activity      .  continuous activity
```

---

## 3. Milestones

| # | Milestone | Week | Owner | Success criterion |
|---|---|---|---|---|
| M1 | Tensor IR op catalogue and ISA extension frozen | 14 | All | Both documents committed; no further changes without team agreement |
| M2 | Batch syntax parses and shape-checks | 16 | Vinay, Parth | `tensor[B][R][C]` declarations accepted; batch mismatches rejected with a diagnostic |
| M3 | AST lowers to Tensor IR | 18 | Kandi | Every Phase 1 operation plus `softmax` has a Tensor IR equivalent, unit-tested |
| M4 | Fusion pass demonstrably fuses | 20 | Kandi | `matmul→relu` fuses to one op in Tensor IR; a deliberately non-adjacent pattern does **not** fuse |
| M5 | New instructions selecting and simulating correctly | 22 | Shreyas | All 7 new instructions covered by simulator tests, matching `docs/isa-extensions.md` semantics exactly |
| M6 | Scratchpad round-trips correctly | 24 | Shreyas | Differential test: tiled-through-scratchpad execution matches non-tiled execution, bit for bit |
| M7 | Performance model produces all three ablation columns | 25 | Shreyas | Instruction count, bytes moved, and estimated cycles all reported for a single compiled program |
| M8 | Both benchmarks compile and run end to end | 26 | All | `ffn_block.mtx` and `attention_head.mtx` produce correct output in the simulator, verified against the reference implementation |
| M9 | Ablation study complete | 27 | All | All 4 configurations × 2 benchmarks measured; results table drafted |
| M10 | Full suite passing, report ready | 28 | All | Tensor IR, fusion, ISA, memory-hierarchy and benchmark tests all green |

**M4 and M6 matter most.** M4 is the first proof the fusion story is real rather than aspirational. M6 is the correctness backbone for the entire memory-hierarchy claim — without it, nothing in the ablation study (M9) can be trusted, because a tiling bug that happens to look like a speedup is worse than no tiling at all.

---

## 4. Critical path

```
Grammar/semantic (batch) -> Tensor IR -> Fusion pass -> Memory hierarchy -> Performance model -> Ablation study
       W16                    W18          W20              W24                 W25                  W27
```

The Tensor IR (Kandi) is the new long pole: three downstream items — fusion, the tiling pass, and the benchmark end-to-end run — all depend on it existing and being stable. This mirrors the backend's role as the Phase 1 critical path, and the same mitigation applies: the Tensor IR's op catalogue is frozen at M1, before implementation starts, so downstream work (fusion pattern design, ISA instruction design) can proceed against the frozen shape of the IR rather than waiting for its implementation to finish.

---

## 5. Decision points

| Week | Decision | Criteria | Fallback |
|---|---|---|---|
| 20 | Is the fusion pass reliable enough to trust for the ablation study? | Zero false-positive fusions on the negative test set | Ship configuration (b)/(d) of the ablation study with fusion restricted to the single `matmul→relu` pattern only, drop `matmul→add` fusion from the report |
| 24 | Is the scratchpad model correct? | M6's differential test passes bit-for-bit | If not resolved by week 25, report configuration (a)/(b) only (no-tiling baseline and fusion-only) and describe hierarchical tiling as designed but not verified — same honest-limitation framing used for Phase 1's tiling-for-non-multiples-of-4 case |
| 26 | Include the BF16 storage-comparison stretch benchmark? | Is the pipeline stable end to end by week 26? | Report only the two core benchmarks in FP32; mention BF16 storage as implemented-but-not-benchmarked |

---

## 6. Continuity with the Phase 1 timeline

Phase 2 begins where the Phase 1 twelve-week plan ended (`docs/review1/timeline.md`), so weeks are numbered 13–28 rather than restarting at 1. The same discipline that made Phase 1 trackable is reused: every phase has one clearly named owner, every downstream dependency is named explicitly, and every milestone has a stated, checkable success criterion rather than a vague description of progress.
