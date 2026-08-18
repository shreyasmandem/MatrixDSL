# Semantic Analysis and Symbol Table — Mudpe Parth Tulsidas (24BDS0353)

Individual contribution for **MatrixDSL**, Team 13, BCSE307P Compiler Design.

This package contains **only my own work**: semantic analysis, the symbol table, and
the matrix type system. Team-level deliverables (Review 1 report and presentation,
timeline, responsibility matrix) and the other members' modules live on
[`main`](https://github.com/shreyasmandem/MatrixDSL/tree/main).

> **This is a git repository, ready to push.** See [PUSH.md](PUSH.md) — it takes two
> commands.

---

## Role

| | |
|---|---|
| **Member** | 2 of 4 |
| **Primary responsibility** | Semantic analysis, symbol table, matrix type system |
| **Supporting responsibility** | Negative and boundary test suites |

---

## Contents

```
docs/
├── shape-rules.md               Shape rules, checking algorithm, diagnostics
└── contribution.md              Review 1 evidence and viva preparation

compiler/frontend/semantic/
├── MatrixType.h                 Shape-as-type interface
├── MatrixType.cpp               Working implementation of every shape rule
├── SymbolTable.h                Scoped name-to-shape binding interface
├── SymbolTable.cpp              Working implementation
└── SemanticAnalyzer.h           Analyzer interface and diagnostic catalogue

tools/mtxcheck/main.cpp          Shape rule and symbol table checker

tests/semantic/
├── valid/                       7 programs that must be accepted
├── invalid/                     9 programs that must be rejected
└── boundary/                    9 edge-case programs
```

---

## The central idea: shape *is* the type

In most languages a matrix is an array and its dimensions are runtime data, so a
dimension error surfaces at runtime — as a wrong answer, or as memory corruption.

In MatrixDSL, dimensions are part of the type. Two matrices have the same type only if
both dimensions agree, which turns every dimension error into a **compile-time type
error**.

This is the strongest single argument for MatrixDSL existing: a general-purpose
compiler cannot make this check, because it does not know the program is doing matrix
algebra.

| Operation | Requirement | Result |
|---|---|---|
| `A + B` | `A.rows == B.rows` and `A.cols == B.cols` | `A.rows × A.cols` |
| `matmul(A, B)` | `A.cols == B.rows` | `A.rows × B.cols` |
| `transpose(A)` | none | `A.cols × A.rows` |
| `relu(A)` | none | `A.rows × A.cols` |
| `s * A` | exactly one operand is scalar | shape of the other |
| `C = expr` | `expr` shape equals `C`'s declared shape | — |

Full detail: [`docs/shape-rules.md`](docs/shape-rules.md)

---

## Building and running

No LLVM required — pure C++17.

```bash
cmake -S . -B build
cmake --build build -j
```

```bash
./build/bin/mtxcheck all
```

Expected output (abbreviated):

```
MatrixDSL shape rule and symbol table checker
============================================

Matrix multiplication - A.cols must equal B.rows
  PASS  matmul(4x4, 4x4) -> 4x4
  PASS  matmul(4x3, 3x5) -> 4x5
  PASS  matmul(1x4, 4x1) -> 1x1 (dot product)
  PASS  matmul(4x1, 1x4) -> 4x4 (outer product)
  PASS  matmul(4x3, 7x5) rejected (3 != 7)
  PASS  matmul is not shape-commutative: AB is 2x2 but BA is 3x3

Symbol table
  PASS  declare A as 4x4
  PASS  redeclaring A in the same scope is rejected
  PASS  shadowing A in an inner scope is allowed
  PASS  symbols enumerate in declaration order

--------------------------------------------
  46 checks, 0 failed
--------------------------------------------
```

Run one section at a time: `mtxcheck matmul`, `mtxcheck symbols`, `mtxcheck diag`.

`mtxcheck diag` renders the diagnostic format:

```
error: cannot multiply matrix A (4x3) with matrix B (7x5)
  --> program.mtx:12:5
   |
12 |     C = matmul(A, B);
   |         ^^^^^^^^^^^^
   = note: matmul requires A.cols == B.rows, but 3 != 7
```

---

## Why this runs without the parser

`mtxcheck` verifies my module's entire core logic **before Vinay's parser exists**.

That is not an accident. The shape rules are pure functions over `MatrixType`, and the
symbol table is independent of the AST, so neither needs a parsed program to be tested.
Only `SemanticAnalyzer` — the tree walk that applies the rules — depends on `AST.h`, and
that lands in Review 2.

This is what freezing the shared interfaces on `main` bought the team: four people
working in parallel from week 3 instead of three waiting on one.

---

## Tests

```bash
ctest --test-dir build --output-on-failure
```

25 MatrixDSL programs in `tests/semantic/`, each declaring its expectation in a leading
comment:

| Category | Count | Examples |
|---|---|---|
| Valid | 7 | Matching addition, `matmul(4×3, 3×5)`, chained `relu(matmul(A,B))` |
| Invalid | 9 | Shape mismatches, undeclared identifier, redeclaration, ragged literal |
| Boundary | 9 | 1×1, 256×256, 257×257 rejected, zero/negative dims, 5×7 non-aligned |

A negative test that fails for the *wrong reason* is a failing test, so invalid cases
assert on the diagnostic kind rather than merely on a non-zero exit status.

---

## Status

| Item | Status |
|---|---|
| Shape rules specified | Complete |
| `MatrixType` interface and implementation | Complete |
| `SymbolTable` interface and implementation | Complete |
| Diagnostic catalogue and format | Complete |
| Shape rule checker (`mtxcheck`) | Complete |
| 25 semantic test programs | Complete |
| `SemanticAnalyzer` interface | Complete |
| `SemanticAnalyzer` implementation (AST walk) | Review 2 — needs `AST.h` |

**Not yet compiled** — no C++ toolchain was installed on the machine used to author
this. Building `mtxcheck` and running the checks is the first task before the Review 1
demonstration.
