# Team Responsibility Matrix

**Project:** MatrixDSL — A Domain-Specific Language and LLVM-Based Compiler for Matrix and AI Workloads
**Team Number:** 13 | **Project ID:** A3 | **Course:** BCSE307P — Compiler Design

Tasks are divided by module and measurable output, not by generic activity such as
"documentation" or "coding". Every member owns at least one technical component and
supports at least one integration, testing or documentation activity.

---

## Matrix

| Member | Primary Responsibility | Supporting Responsibility | Review 1 Evidence | Review 2 Target | Review 3 Target |
|---|---|---|---|---|---|
| **Vinay A**<br>24BCB0131<br>Branch: `vinay` | Lexer, Parser, AST construction | Frontend integration; grammar documentation; parser test suite | EBNF grammar (`docs/grammar.md`); token specification; AST node hierarchy; `Lexer.h`, `AST.h` interface headers | Working lexer and recursive-descent parser producing correct AST for all language constructs; 23 frontend tests passing | Error recovery, full diagnostics with source locations, frontend fully integrated |
| **Mudpe Parth Tulsidas**<br>24BDS0353<br>Branch: `parth` | Semantic Analysis, Symbol Table, Matrix Type System | Negative and boundary test suites; shape rule documentation | Shape rule specification (`docs/language-spec.md` §5); `MatrixType.h`, `SymbolTable.h` interfaces; 25-case semantic test plan | Shape checking operational; all 9 invalid-shape programs correctly rejected | Complete diagnostics with source spans; all 25 semantic tests passing |
| **Kandi Jeevitesh Reddy**<br>24BCE0350<br>Branch: `kandi` | LLVM IR Generation, Optimization Pipeline | Runtime library; cross-module integration | Matrix IR representation design; matrix intrinsic naming contract (`@mdt.matmul.4x4`); optimization pass plan | Valid, verifiable LLVM IR for all 8 operations; `-O2` pipeline running | Before/after optimization analysis; runtime library complete; IR tests passing |
| **Shreyas Mandem**<br>24BCE0381<br>Branch: `shreyas` | MDT ISA Definition, Custom LLVM Backend | Backend test suite; assembly verification; simulator | Frozen MDT ISA (`docs/isa.md`); backend design with risk mitigation (`docs/llvm-backend.md`); TableGen register and instruction descriptions; **working MDT simulator** | MDT registered as an LLVM target; scalar and memory instructions selecting correctly | Vector and matrix instructions; 4x4 tiling; end-to-end assembly generation |

---

## Dependency map

Understanding who is blocked on whom, and how those blocks were removed:

```
Vinay (AST) ──────┬──────> Parth (Semantic)
                  │              │
                  │              v
                  └──────> Kandi (LLVM IR)
                                 │
                                 v
                          Shreyas (Backend)
```

**The problem.** As drawn, three members are blocked behind the parser and the project
becomes serial.

**The resolution.** Two interfaces were frozen and committed to `main` before
implementation started:

| Frozen interface | Unblocks | File |
|---|---|---|
| Token enum + AST node hierarchy | Parth and Kandi can build against a known AST and test with hand-constructed fixtures | `compiler/frontend/ast/AST.h` |
| Matrix intrinsic naming (`@mdt.matmul.4x4`) | Shreyas can develop instruction selection against known IR without waiting for the IR generator | `docs/llvm-backend.md` §5.1 |

With these frozen, all four members work in parallel from Week 3.

---

## Integration responsibilities

| Integration point | Owner | Secondary | Week |
|---|---|---|---|
| Lexer to Parser | Vinay | — | 4 |
| Parser to Semantic Analyzer | Parth | Vinay | 5 |
| Semantic Analyzer to IR Generator | Kandi | Parth | 6 |
| IR Generator to Backend | Shreyas | Kandi | 8 |
| Backend to Simulator | Shreyas | — | 9 |
| Full pipeline | All | — | 11 |

The IR-to-backend interface (Week 8) is the highest-risk integration because it
crosses the two most technically demanding modules. It is mitigated by the intrinsic
naming contract being agreed in Week 1 rather than negotiated at integration time.

---

## Testing and documentation ownership

| Artifact | Owner |
|---|---|
| Lexer and parser tests | Vinay A |
| Semantic tests (valid, invalid, boundary) | Mudpe Parth Tulsidas |
| LLVM IR tests | Kandi Jeevitesh Reddy |
| Backend and simulator tests | Shreyas Mandem |
| Integration and differential tests | Shreyas Mandem (lead), all contributing |
| Repository management, branch policy | Shreyas Mandem |
| Grammar and language specification | Vinay A |
| Architecture documentation | Kandi Jeevitesh Reddy |
| ISA and backend documentation | Shreyas Mandem |
| Review report consolidation | All |

---

## Acknowledgement

Each member confirms that the allocation above reflects the agreed division of work
and that they understand both their own component and the overall project.

| Member | Register Number | Signature | Date |
|---|---|---|---|
| Vinay A | 24BCB0131 | ____________________ | ____________ |
| Mudpe Parth Tulsidas | 24BDS0353 | ____________________ | ____________ |
| Kandi Jeevitesh Reddy | 24BCE0350 | ____________________ | ____________ |
| Shreyas Mandem | 24BCE0381 | ____________________ | ____________ |
