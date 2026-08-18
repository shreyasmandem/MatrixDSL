# Project Timeline and Milestones

**Project:** MatrixDSL | **Team 13** | **Duration:** 12 weeks

---

## 1. Twelve-week plan

| Weeks | Phase | Deliverable | Lead | Verification |
|---|---|---|---|---|
| 1–2 | Language and MDT design | Grammar, type rules, ISA, register model, instruction semantics | All | Grammar reviewed; ISA frozen |
| 3–4 | Lexer and parser | Token stream, recursive-descent parser, AST | Vinay | 23 frontend tests pass |
| 5–6 | Semantic analysis | Symbol table, matrix type system, shape diagnostics | Parth | 25 semantic tests pass |
| 6–7 | LLVM IR generation | AST to LLVM IR, matrix memory representation | Kandi | `verifyModule` clean |
| 7–8 | MDT backend skeleton | Target registration, register classes, TableGen | Shreyas | `llc -march=mdt` runs |
| 8–9 | Scalar backend | ADD/SUB/MUL, LOAD/STORE, branches | Shreyas | Scalar asm correct in simulator |
| 9–10 | Vector and matrix backend | VADD/VMUL, MATMUL/RELU/TRANSPOSE | Shreyas | 4x4 ops match reference |
| 10–11 | Tiling and integration | 4x4 tiling, end-to-end compilation | All | 16x16, 32x32 correct |
| 12 | Testing and reporting | Benchmarks, demonstration, final documentation | All | 78 tests pass |

---

## 2. Gantt chart

```
Week               1  2  3  4  5  6  7  8  9 10 11 12
                   |--|--|--|--|--|--|--|--|--|--|--|--|

Design phase       ######
  Grammar          ######
  ISA freeze       ######

Frontend                 ############
  Lexer                  ######
  Parser                    ######
  AST                    ############

Semantic                          ############
  Symbol table                    ######
  Shape checking                     ######

LLVM IR                                 ############
  IR generation                         ######
  Optimization                             ######

MDT Backend                                ##########################
  Registration                             ######
  Scalar                                      ######
  Vector                                         ######
  Matrix                                            ######

Tiling                                                  ############
Integration                                             ##################
Testing                  ..................................................
Documentation      ................................................########

Legend:  #  primary activity      .  continuous activity
```

---

## 3. Milestones

| # | Milestone | Week | Owner | Success criterion |
|---|---|---|---|---|
| M1 | Grammar and ISA frozen | 2 | All | Documents committed; no further changes without team agreement |
| M2 | Shared interfaces committed | 2 | All | `AST.h`, `Lexer.h`, `MatrixType.h`, `SymbolTable.h` on `main` |
| M3 | Lexer producing correct tokens | 3 | Vinay | 10 lexer tests pass |
| M4 | Parser producing correct AST | 4 | Vinay | 13 parser tests pass |
| M5 | Shape checking operational | 6 | Parth | All 9 invalid programs rejected correctly |
| M6 | First valid LLVM IR emitted | 7 | Kandi | `verifyModule` clean on all examples |
| M7 | **First end-to-end run (host target)** | 7 | Kandi | MatrixDSL program compiles and runs natively |
| M8 | MDT registered as an LLVM target | 8 | Shreyas | `llc -march=mdt` accepts input without error |
| M9 | Scalar MDT assembly correct | 9 | Shreyas | Simulator executes generated scalar code correctly |
| M10 | Matrix instructions emitted | 10 | Shreyas | `MATMUL` / `RELU` / `TRANSPOSE` selected from IR |
| M11 | Tiling correct for large matrices | 11 | All | 16x16 and 32x32 multiplies verified |
| M12 | Full suite passing | 12 | All | 78 tests pass; differential verification clean |

**M7 is the most important early milestone.** Compiling a MatrixDSL program to a
native executable exercises every frontend interface before the backend exists, so
interface defects surface while there is still time to fix them.

---

## 4. Critical path

```
Grammar freeze -> Lexer -> Parser -> AST -> Semantic -> LLVM IR -> Backend -> Tiling -> Integration
     W2           W3       W4       W4      W6         W7        W9        W11       W11
```

The backend (Weeks 7–10) is the longest single block and the least familiar work, so
it starts as early as its dependencies allow and runs in parallel with frontend
completion. Backend work does not wait for the frontend, because it develops against
hand-written LLVM IR.

**Slack analysis.** Frontend tasks carry roughly two weeks of slack. Backend tasks
carry approximately none — any slip there moves the integration date directly. This is
why the backend phases are ordered so that each is independently demonstrable, and why
a fallback implementation path exists (see `risk-register.md`, R1).

---

## 5. Decision points

| Week | Decision | Criteria | Fallback |
|---|---|---|---|
| 8 | Continue with TableGen backend or switch to direct emitter | Is scalar assembly being emitted correctly through SelectionDAG? | Switch to `MDTDirectEmitter`; keeps all downstream work unaffected |
| 10 | Full tiling or fixed 4x4 only | Are matrix instructions working and verified? | Restrict demonstration to 4x4 and document tiling as designed but not implemented |
| 11 | Include benchmark study | Is the pipeline complete end to end? | Report instruction counts only, omit timing study |

Naming these decision points in advance, with explicit criteria, means a slip becomes a
planned adjustment rather than an unplanned failure.

---

## 6. Review schedule

| Review | Week | Deliverables |
|---|---|---|
| **Review 1** | 2 | Design, planning, feasibility, frozen specifications, simulator prototype |
| **Review 2** | 7 | Working frontend, semantic analysis, LLVM IR generation, backend skeleton |
| **Review 3** | 12 | Complete pipeline, tiling, verification, benchmarks, final report |
