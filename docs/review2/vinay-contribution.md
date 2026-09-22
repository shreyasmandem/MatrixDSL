# Individual Contribution — Vinay A (Phase 2 / Review 2)

| | |
|---|---|
| **Name** | Vinay A |
| **Register Number** | 24BCB0131 |
| **Role** | Lexer + Parser + AST |
| **Branch** | `Vinay-A` |
| **Review** | Review 2 |

---

## 1. Scope of my Phase 2 component

Same module boundary as Review 1: everything from source text to a validated
AST. Review 1 shipped a working lexer with `Parser.h` and `AST.h` as
interfaces only, and named their implementation as an explicit Review 2
target. Both are implemented now, plus the two Phase 2 grammar additions
(`docs/review2/Review2_Report.md` §2): a batch dimension on declarations, and
`softmax` as a fourth builtin.

---

## 2. What I completed for Review 2

### 2.1 The Review 1 promise, delivered: `Parser.cpp` and `AST.cpp`

`compiler/frontend/parser/Parser.cpp`, `compiler/frontend/ast/AST.cpp`

Full recursive-descent implementation, one method per grammar production
(`docs/grammar.md`), plus the visitor `accept()` overrides and a `dumpAST()`
that renders an indented tree — the same rendering the parser test suite now
compares against directly, via the new `mtxparse` tool.

**Design decision — panic-mode recovery, not backtracking.** `expect()`
deliberately does not consume an unexpected token; it records the error and
leaves the stream positioned on it, so `synchronize()` (unchanged from its
Review 1 interface) can find the next statement boundary. A single missing
semicolon produces one error, not a cascade — verified by
`missing_semicolon.mtx` and `unbalanced_bracket.mtx`, both of which report
exactly one diagnostic each.

**Design decision — a well-formed placeholder on expression errors.**
`parseFactor()` and `parseFunctionCall()` return a `NumberExpr(0.0)` rather
than `nullptr` when they cannot parse an expression, so nothing downstream of
the parser has to special-case a null `ExprPtr`. The tree stays walkable even
around a syntax error.

### 2.2 Grammar extended: batch dimension and `softmax`

`docs/grammar.md` §2a, `Parser.cpp::parseMatrixDecl`, `parseFunctionCall`

`tensor X[8][4][4];` and `matrix W[4][4];` both produce the *same* AST node,
`MatrixDecl` — the parser looks ahead for a third bracket only when the
keyword was `tensor`, and only the batch field differs. No new AST node type
was needed, matching the frozen-interface design already committed on `main`.

`softmax` slots into `parseFunctionCall` exactly like `transpose`/`relu` — a
one-argument builtin, no new grammar shape.

### 2.3 `mtxparse` — verifiable output, this review's proof of start

`tools/mtxparse/main.cpp`

Runs the full lexer → parser pipeline and prints the AST, or every syntax
error with panic-mode recovery already applied. This is the Phase 2
counterpart to Review 1's `mtxlex`: where that proved the token
specification executes, this proves the full production set does — including
the two new ones.

### 2.4 Test suite — 28 tests, all passing, all verified by content not just exit code

`tests/parser/`, `CMakeLists.txt`

12 original parser tests, plus 3 new for Phase 2:

| Test | Verifies |
|---|---|
| `tensor_declaration.mtx` | `tensor A[4][4];` parses identically to `matrix A[4][4];` (batch=1) |
| `tensor_batch_declaration.mtx` | `tensor X[8][4][4];` → `MatrixDecl(X, 8x4x4)` |
| `softmax_call.mtx` | `softmax(A)` → `CallExpr(softmax, [VariableExpr(A)])` |

Every test in this suite was checked against its **actual printed AST**, not
merely a zero exit code — including re-verifying the *existing* Review 1
tests (`precedence.mtx`, `left_associativity.mtx`, `nested_call.mtx`), since
this is the first time `mtxparse` has existed to check them against. All
three produce exactly their documented expected tree shape:
`precedence.mtx` puts `+` at the root with `*` as its right child;
`left_associativity.mtx` produces `((A-B)-D)`, not `(A-(B-D))`;
`nested_call.mtx` produces `CallExpr(relu)` wrapping `CallExpr(matmul)`.

28/28 tests pass, `ctest --test-dir build --output-on-failure`.

---

## 3. Evidence

| Evidence | Location |
|---|---|
| Grammar, Phase 2 additions | `docs/grammar.md` §2a |
| Parser implementation | `compiler/frontend/parser/Parser.cpp` |
| AST implementation | `compiler/frontend/ast/AST.cpp` |
| Lexer, extended for `tensor`/`softmax` | `compiler/frontend/lexer/Lexer.cpp` |
| Parser/AST dumper | `tools/mtxparse/main.cpp` |
| Test suite | `tests/parser/` (28 CTest cases) |
| Commits | branch `Vinay-A` |

---

## 4. Build and test status

```bash
cmake -S . -B build && cmake --build build -j
ctest --test-dir build --output-on-failure
```

**28/28 tests pass**, built with MinGW-W64 GCC 16.1.0, zero warnings. Every
grammar-shape claim above (precedence, associativity, nesting, the batch
dimension, `softmax`) was checked against `mtxparse`'s actual printed AST,
not assumed from the code.

---

## 5. Review 3 targets

| # | Target |
|---|---|
| 1 | Hand off the AST to Parth's `SemanticAnalyzer` for end-to-end shape checking |
| 2 | Source-span-accurate error messages (currently line:column of the offending token, not a full span) |
| 3 | A parser fuzz/property test: any token stream either parses or produces at least one diagnostic, never crashes |

---

## 6. What I can be questioned on

- Why `expect()` does not consume the unexpected token, and how that keeps
  `synchronize()` correct
- Why `matrix`/`tensor` share one AST node instead of two
- How the parser decides whether `tensor X[8][4][4]` is 2D or 3D — the
  lookahead is exactly one token, after the two mandatory dimensions
- Why a malformed expression returns a placeholder node instead of `nullptr`
- What `mtxparse` actually proves that `mtxlex` alone did not
