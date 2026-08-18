# Contribution Log

**Project:** MatrixDSL | **Team 13** | **Course:** BCSE307P — Compiler Design

This log records meetings, task assignments, completion status and evidence for each
member. It is maintained continuously and forms the basis for individual contribution
assessment.

---

## Meeting record

| # | Date | Attendees | Agenda | Decisions |
|---|---|---|---|---|
| 1 | 2026-08-18 | All 4 | Project scope; role allocation | Roles assigned; agreed to keep the language small; C++ with LLVM C++ API confirmed as the implementation approach |
| 2 | 2026-08-18 | All 4 | Project direction | Adopted MatrixDSL + custom MDT target as the project topic; superseded earlier general-purpose language plan |
| 3 | 2026-08-18 | All 4 | Design freeze | Grammar frozen; MDT ISA frozen at 17 instructions and 3 register classes; shared interfaces agreed; repository created |

---

## Task assignment and completion — Review 1

### Vinay A (24BCB0131) — Lexer, Parser, AST

| Task | Assigned | Status | Evidence |
|---|---|---|---|
| Formalise MatrixDSL grammar in EBNF | Meeting 3 | Complete | `docs/grammar.md` |
| Eliminate left recursion for recursive descent | Meeting 3 | Complete | `docs/grammar.md` §2 |
| Define token classes | Meeting 3 | Complete | `docs/grammar.md` §4 |
| Design AST node hierarchy | Meeting 3 | Complete | `compiler/frontend/ast/AST.h` |
| Lexer interface | Meeting 3 | Complete | `compiler/frontend/lexer/Lexer.h` |
| Parser test plan | Meeting 3 | Complete | `docs/testing.md` §4 |

### Mudpe Parth Tulsidas (24BDS0353) — Semantic Analysis, Symbol Table

| Task | Assigned | Status | Evidence |
|---|---|---|---|
| Specify matrix shape rules | Meeting 3 | Complete | `docs/language-spec.md` §5 |
| Design `MatrixType` representation | Meeting 3 | Complete | `compiler/frontend/semantic/MatrixType.h` |
| Design symbol table interface | Meeting 3 | Complete | `compiler/frontend/semantic/SymbolTable.h` |
| Semantic analyzer interface | Meeting 3 | Complete | `compiler/frontend/semantic/SemanticAnalyzer.h` |
| Diagnostic message format | Meeting 3 | Complete | `docs/language-spec.md` §9 |
| Semantic test plan (25 cases) | Meeting 3 | Complete | `docs/testing.md` §5; `tests/invalid/`, `tests/boundary/` |

### Kandi Jeevitesh Reddy (24BCE0350) — LLVM IR, Optimization

| Task | Assigned | Status | Evidence |
|---|---|---|---|
| Design matrix representation in LLVM IR | Meeting 3 | Complete | `docs/architecture.md` §3.4 |
| Define matrix intrinsic naming contract | Meeting 3 | Complete | `docs/llvm-backend.md` §5.1 |
| Specify row-major memory layout | Meeting 3 | Complete | `docs/language-spec.md` §8 |
| IR generator interface | Meeting 3 | Complete | `compiler/llvm/LLVMCodeGen.h` |
| Optimization pass plan | Meeting 3 | Complete | `docs/architecture.md`; report §8.5 |
| Runtime library interface | Meeting 3 | Complete | `runtime/MatrixRuntime.h` |

### Shreyas Mandem (24BCE0381) — MDT ISA, LLVM Backend

| Task | Assigned | Status | Evidence |
|---|---|---|---|
| Define and freeze the MDT ISA | Meeting 3 | Complete | `docs/isa.md` |
| Specify register file (R/V/M classes) | Meeting 3 | Complete | `docs/isa.md` §2 |
| Specify all 17 instruction semantics | Meeting 3 | Complete | `docs/isa.md` §4 |
| Define assembly syntax and directives | Meeting 3 | Complete | `docs/isa.md` §5 |
| Design instruction encoding | Meeting 3 | Complete | `docs/isa.md` §6–7 |
| Specify calling convention | Meeting 3 | Complete | `docs/isa.md` §8 |
| Backend architecture and lowering design | Meeting 3 | Complete | `docs/llvm-backend.md` |
| TableGen register description | Meeting 3 | Complete | `llvm-backend/MDT/MDTRegisterInfo.td` |
| TableGen instruction description | Meeting 3 | Complete | `llvm-backend/MDT/MDTInstrInfo.td` |
| **MDT instruction-set simulator** | Meeting 3 | **Complete — working prototype** | `tools/mdtsim/` |
| Backend test suite | Meeting 3 | Complete | `tests/backend/` |
| Risk analysis and fallback design | Meeting 3 | Complete | `docs/review1/risk-register.md` R1 |
| Repository setup and branch policy | Meeting 3 | Complete | Repository structure; `README.md` |

---

## Team-level deliverables

| Deliverable | Contributors | Status | Evidence |
|---|---|---|---|
| Review 1 report | All | Complete | `docs/review1/Review1_Report.md` |
| Review 1 presentation | All | Complete | `docs/review1/Review1_Presentation.pptx` |
| Responsibility matrix | All | Complete | `docs/review1/responsibility-matrix.md` |
| Timeline and Gantt chart | All | Complete | `docs/review1/timeline.md` |
| Risk register | All | Complete | `docs/review1/risk-register.md` |
| System architecture | All | Complete | `docs/architecture.md` |
| Testing strategy | All | Complete | `docs/testing.md` |
| Repository and build system | All | Complete | `CMakeLists.txt`, `scripts/` |

---

## Branch and commit record

| Branch | Owner | Purpose |
|---|---|---|
| `main` | Team | Shared deliverables, documentation, frozen interfaces, build system |
| `shreyas` | Shreyas Mandem | MDT ISA implementation, TableGen descriptions, backend, simulator |
| `vinay` | Vinay A | Lexer, parser, AST implementation |
| `parth` | Mudpe Parth Tulsidas | Semantic analyzer, symbol table, matrix type system |
| `kandi` | Kandi Jeevitesh Reddy | LLVM IR generation, optimization, runtime |

Individual contributions are committed to per-member branches so that authorship is
identifiable in the git history, then merged to `main` after review.

---

## Notes for Review 2

| Member | Next deliverable | Target week |
|---|---|---|
| Vinay A | Working lexer and parser; 23 tests passing | 4 |
| Mudpe Parth Tulsidas | Shape checking operational; invalid programs rejected | 6 |
| Kandi Jeevitesh Reddy | Valid LLVM IR for all 8 operations | 7 |
| Shreyas Mandem | MDT registered; scalar instructions selecting | 8 |
