# Individual Contribution — Mudpe Parth Tulsidas

| | |
|---|---|
| **Name** | Mudpe Parth Tulsidas |
| **Register Number** | 24BDS0353 |
| **Role** | Semantic Analysis + Symbol Table + Matrix Type System |
| **Branch** | `parth` |
| **Review** | Review 1 |

---

## 1. Scope of my component

I own the stage that decides whether a syntactically valid MatrixDSL program actually
*makes sense*: name resolution, shape checking, and the diagnostics reported when it
does not.

My module is the one that justifies the project's premise. A general-purpose compiler
cannot tell you that `matmul(A, B)` is wrong when A is 4×3 and B is 7×5, because it does
not know the program is doing matrix algebra. MatrixDSL can, because shape is part of
the type.

---

## 2. What I completed for Review 1

### 2.1 The shape rules — specified and implemented

`docs/shape-rules.md`, `compiler/frontend/semantic/MatrixType.cpp`

| Operation | Requirement | Result shape |
|---|---|---|
| `A + B`, `A - B` | `A.rows == B.rows` and `A.cols == B.cols` | `A.rows × A.cols` |
| `s * A` | exactly one operand is scalar | shape of the non-scalar operand |
| `matmul(A, B)` | `A.cols == B.rows` | `A.rows × B.cols` |
| `transpose(A)` | none | `A.cols × A.rows` |
| `relu(A)` | none | `A.rows × A.cols` |
| `C = expr` | `expr` shape equals `C`'s declared shape | — |

**Design decision — shape is the type, not a runtime property.** Two matrices have the
same type only if both dimensions agree. This is what converts a whole class of runtime
failures into compile-time errors.

**Design decision — rules decide, they do not diagnose.** Every rule returns the result
type on success or an invalid `MatrixType` (0×0) on failure, and never emits a message.
Keeping the decision separate from the wording means the same rule is reusable by the
analyzer, by tests, and by future tooling without dragging diagnostic formatting along.

**Design decision — scalars are typed 1×1.** This avoids introducing a separate scalar
type. It creates one real ambiguity: `1×1 * 1×1` is both a scalar product and a legal
1×1 matrix multiply. They agree on the result, so the ambiguity is harmless, and it is
resolved in favour of the scalar interpretation.

Three consequences I should be able to defend:

- **`4×3 + 3×4` is rejected** even though both have 12 elements and are transposes of
  one another. Element-wise addition between them is undefined.
- **Multiplication is not shape-commutative.** `matmul(A,B)` with A 2×3 and B 3×2 gives
  2×2; `matmul(B,A)` gives 3×3. Both legal, different types.
- **Degenerate cases fall out for free.** `matmul(1×4, 4×1)` is a dot product yielding
  1×1; `matmul(4×1, 1×4)` is an outer product yielding 4×4. No special-casing.

### 2.2 Checking algorithm

A single **post-order traversal**. Each node computes its result shape from its
children's already-computed shapes and reports upward; the assignment at the root
compares against the declared target shape.

```
C = relu(matmul(A, B));       // A is 4x3, B is 3x5, C is 4x5

     Assignment(C)                 check 4x5 == declared C (4x5)  OK
          |
     CallExpr(relu)                relu preserves shape  -> 4x5
          |
     CallExpr(matmul)              A.cols(3) == B.rows(3)  -> 4x5
        /      \
   Var(A)     Var(B)               4x3         3x5
```

Bottom-up is correct because every rule is a function of its operands' shapes — no
expected type needs to propagate downward. That keeps the analyzer to one pass with no
fixpoint iteration.

The computed shape is written into `Expr::resultType`. That field is the explicit
handoff to Kandi's IR generator, which may assume it is populated and correct and does
**no** shape re-checking. Duplicating the rules downstream would mean two places to keep
in sync and two places for them to disagree.

### 2.3 Cascade suppression

Analysis does not stop at the first error — one run should report as many genuine
problems as possible. That creates a risk: an invalid subexpression makes its parent
invalid, producing a second error about the same underlying mistake.

Every rule therefore opens with:

```cpp
if (!lhs.isValid() || !rhs.isValid())
    return MatrixType();
```

returning invalid *without deciding*, so the caller knows to stay silent. The user sees
one error per actual mistake, not one per level of nesting.

### 2.4 Symbol table

`compiler/frontend/semantic/SymbolTable.h`, `SymbolTable.cpp`

Scoped name-to-shape binding on a scope stack. v1.0 has one global scope — no functions
or blocks — but the stack is there so adding them later does not require reworking every
caller.

| Behaviour | Rule |
|---|---|
| Declare | Fails only if the name exists **in the innermost scope** |
| Shadowing | Legal — an inner scope may redeclare an outer name |
| Lookup | Innermost outward |
| Enumeration | **Declaration order**, never hash order |

**Design decision — redeclaration is checked per-scope, not globally.** `declare()`
inspects the innermost map directly rather than calling `lookup()`, because shadowing an
outer name is legitimate while redeclaring in the same scope is not.

**Design decision — declaration order is recorded separately.** Iterating an
`unordered_map` produces output that differs between runs, which makes golden-file tests
flaky and the failures very unpleasant to diagnose. Worth the extra vector.

Each symbol records its declaration line and column specifically so a redeclaration
error can point at the original with `note: A declared here`.

### 2.5 Diagnostics

Ten diagnostic kinds, with a format that states what was found, what was required, and
why:

```
error: cannot multiply matrix A (4x3) with matrix B (7x5)
  --> program.mtx:12:5
   |
12 |     C = matmul(A, B);
   |         ^^^^^^^^^^^^
   = note: matmul requires A.cols == B.rows, but 3 != 7
```

Naming the actual dimensions is what separates a message a user can act on from one they
cannot. Compare what a general-purpose language gives for equivalent code: either
nothing, or a complaint about pointer types that never mentions matrices.

### 2.6 Shape rule checker — verifiable output

`tools/mtxcheck/main.cpp`

Exercises every shape rule and every symbol table behaviour, and renders the diagnostic
format.

**It runs without the parser.** That is the point worth making. The shape rules are pure
functions over `MatrixType` and the symbol table is independent of the AST, so my
module's entire core logic is verifiable before Vinay's parser exists. Only
`SemanticAnalyzer` — the tree walk that applies the rules — needs `AST.h`, and that lands
in Review 2.

This is exactly what freezing the shared interfaces bought the team: four people working
in parallel from week 3 instead of three waiting on one.

Several checks are self-verifying identities rather than hand-written constants —
`transpose(transpose(A))` recovers A's shape, `AB` and `BA` differ — so they cannot rot.

### 2.7 Test suite — 25 programs

`tests/semantic/`

| Category | Count | Coverage |
|---|---|---|
| Valid | 7 | Matching addition, rectangular matmul, transpose, ReLU, scalar multiply, chained ops, literal |
| Invalid | 9 | Addition mismatch, matmul mismatch, assignment mismatch, undeclared, redeclaration, ragged literal, literal shape, transpose assign, print undeclared |
| Boundary | 9 | 1×1, 256×256, 257×257, zero dim, negative dim, row vector, column vector, 5×7 non-aligned, degenerate dot product |

A negative test that fails for the *wrong reason* is a failing test, so invalid cases
assert on the diagnostic kind rather than merely on non-zero exit status.

The ragged-literal case is worth noting: it **parses** fine and is rejected here. That
is deliberate — catching it in semantic analysis produces a proper diagnostic with a
source span instead of a confusing syntax error.

---

## 3. Evidence

| Evidence | Location |
|---|---|
| Shape rules and algorithm | `docs/shape-rules.md` |
| Matrix type interface | `compiler/frontend/semantic/MatrixType.h` |
| Matrix type implementation | `compiler/frontend/semantic/MatrixType.cpp` |
| Symbol table interface | `compiler/frontend/semantic/SymbolTable.h` |
| Symbol table implementation | `compiler/frontend/semantic/SymbolTable.cpp` |
| Analyzer interface + diagnostics | `compiler/frontend/semantic/SemanticAnalyzer.h` |
| Shape rule checker | `tools/mtxcheck/main.cpp` |
| Test programs | `tests/semantic/` |
| Commits | branch `parth` |

---

## 4. Build status

```bash
cmake -S . -B build
cmake --build build -j
./build/bin/mtxcheck all
ctest --test-dir build --output-on-failure
```

**Note for the review:** not yet compiled — no C++ toolchain was installed on the
machine used to author this. Building `mtxcheck` and running the checks is the first
task before the Review 1 demonstration, and any resulting fixes will be committed.

---

## 5. Review 2 targets

| # | Target |
|---|---|
| 1 | `mtxcheck` compiled, all shape rule and symbol table checks passing |
| 2 | `SemanticAnalyzer` implemented against Vinay's `AST.h` |
| 3 | All 7 valid programs accepted, all 9 invalid programs rejected |
| 4 | Diagnostics carrying real source locations from the AST |
| 5 | `Expr::resultType` populated on every node, verified by Kandi's IR generator |

---

## 6. What I can be questioned on

- Why shape is part of the type rather than runtime data
- Why `4×3 + 3×4` is rejected even though both have 12 elements
- Why `matmul(A,B)` and `matmul(B,A)` produce different shapes
- Why scalars are typed 1×1, and what ambiguity that creates
- Why checking is a single bottom-up pass with no fixpoint iteration
- Why every shape rule returns early on an invalid operand
- Why redeclaration is checked per-scope while lookup searches outward
- Why the symbol table enumerates in declaration order and not hash order
- Who fills in `Expr::resultType` and why IR generation does not re-check shapes
- Why a ragged matrix literal is a semantic error and not a syntax error
