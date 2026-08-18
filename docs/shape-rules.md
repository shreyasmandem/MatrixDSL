# Shape Rules and Semantic Analysis

Owner: **Mudpe Parth Tulsidas (24BDS0353)**

How MatrixDSL validates that a program makes sense before any code is generated.

---

## 1. The central idea: shape *is* the type

In most languages, a matrix is an array and its dimensions are runtime data. A
dimension error therefore surfaces at runtime — as a wrong answer, or as memory
corruption.

In MatrixDSL, dimensions are part of the type:

```
MatrixType {
    elementType = FP32     // fixed in v1.0
    rows        : int      // 1 .. 256
    cols        : int      // 1 .. 256
}
```

Two matrices have the same type only if both dimensions agree. This turns every
dimension error into a **compile-time type error**, which is the single strongest
argument for MatrixDSL existing at all: a general-purpose language cannot make this
check because it does not know the program is doing matrix algebra.

---

## 2. The shape rules

| Operation | Requirement | Result shape |
|---|---|---|
| `A + B` | `A.rows == B.rows` and `A.cols == B.cols` | `A.rows × A.cols` |
| `A - B` | `A.rows == B.rows` and `A.cols == B.cols` | `A.rows × A.cols` |
| `s * A` | exactly one operand is a scalar | shape of the non-scalar operand |
| `matmul(A, B)` | `A.cols == B.rows` | `A.rows × B.cols` |
| `transpose(A)` | none | `A.cols × A.rows` |
| `relu(A)` | none | `A.rows × A.cols` |
| `C = expr` | `expr` shape equals `C`'s declared shape | — |

### Consequences worth stating explicitly

**Addition is stricter than it looks.** `4×3 + 3×4` is rejected. The two matrices have
the same element count and are transposes of one another, but element-wise addition is
undefined between them.

**Multiplication is not shape-commutative.** `matmul(A, B)` where A is 2×3 and B is 3×2
gives 2×2, while `matmul(B, A)` gives 3×3. Both are legal, and they are different
types. A program that swaps the operands compiles and silently computes something else
— which is exactly the class of bug this type system exists to catch when the shapes
*don't* line up.

**Degenerate cases are legal and useful.** `matmul(1×4, 4×1)` yields 1×1 — a dot
product. `matmul(4×1, 1×4)` yields 4×4 — an outer product. Both fall out of the general
rule with no special-casing.

**Scalars are typed 1×1.** This avoids introducing a separate scalar type into the
system. It creates one genuine ambiguity: `1×1 * 1×1` is both a scalar product and a
legal 1×1 matrix multiply. They agree on the result (1×1), so the ambiguity is harmless
and is resolved in favour of the scalar interpretation.

---

## 3. How checking works

A single **post-order traversal** of the expression tree. Each node computes its result
shape from its children's already-computed shapes and reports it upward. The assignment
at the root compares the final shape against the declared shape of the target.

```
C = relu(matmul(A, B));       // A is 4x3, B is 3x5, C is 4x5

     Assignment(C)                       check 4x5 == declared C (4x5)  OK
          |
     CallExpr(relu)                      relu preserves shape  -> 4x5
          |
     CallExpr(matmul)                    A.cols(3) == B.rows(3)  -> 4x5
        /      \
   Var(A)     Var(B)                     4x3         3x5
```

Bottom-up is the right direction because every rule is a function of its operands'
shapes. There is no need to propagate an expected type downward, which keeps the
analyzer to one pass with no fixpoint iteration.

The computed shape is written into `Expr::resultType` on each node. That field is the
explicit handoff to IR generation: the parser leaves it unresolved, I fill it in, and
Kandi's IR generator may assume it is populated and correct. **IR generation does no
shape re-checking** — duplicating the rules would mean two places to keep in sync and
two places for them to disagree.

---

## 4. Error recovery

Analysis does not stop at the first error. Where a shape cannot be resolved, the node
is left with an invalid type (0×0) and traversal continues, so one run reports as many
genuine problems as possible.

This creates a cascade risk: an invalid subexpression would make its parent invalid,
producing a second error about the same underlying mistake. Every shape rule therefore
begins with

```cpp
if (!lhs.isValid() || !rhs.isValid())
    return MatrixType();
```

which returns invalid **without deciding**, so the caller knows to stay silent. The
user sees one error per actual mistake, not one per level of nesting.

---

## 5. Diagnostics

A diagnostic must state what was found, what was required, and why. Naming the actual
dimensions is what separates a message the user can act on from one they cannot.

```
error: cannot multiply matrix A (4x3) with matrix B (7x5)
  --> program.mtx:12:5
   |
12 |     C = matmul(A, B);
   |         ^^^^^^^^^^^^
   = note: matmul requires A.cols == B.rows, but 3 != 7
```

Compare with what a general-purpose language would produce for the equivalent code:
either nothing at all, or a message about pointer types that says nothing about
matrices.

### Diagnostic catalogue

| Kind | Message |
|---|---|
| `UndeclaredIdentifier` | use of undeclared identifier `Z` |
| `Redeclaration` | `A` is already declared *(+ note: declared here)* |
| `ShapeMismatchAdd` | cannot add matrix A (4x4) and matrix B (3x3) |
| `ShapeMismatchMatMul` | cannot multiply matrix A (4x3) with matrix B (7x5) |
| `ShapeMismatchAssign` | cannot assign 4x5 result to matrix C (4x4) |
| `RaggedMatrixLiteral` | matrix literal rows have differing lengths (2 and 3) |
| `LiteralShapeMismatch` | literal is 2x3 but A is declared 2x2 |
| `DimensionOutOfRange` | dimension 257 exceeds the maximum of 256 |
| `UnusedVariable` | *(warning)* `B` is declared but never used |
| `ReadBeforeAssign` | *(warning)* `C` is read before being assigned |

The symbol table records each declaration's line and column specifically so
redeclaration errors can point at the original with a `note: A declared here`.

---

## 6. Symbol table

Name-to-shape binding on a scope stack.

MatrixDSL v1.0 has exactly one global scope — there are no functions or blocks — but
the table is built on a stack anyway so that adding them later does not require
reworking every caller.

| Behaviour | Rule |
|---|---|
| Declare | Fails if the name already exists **in the innermost scope** |
| Shadowing | Legal — an inner scope may redeclare an outer name |
| Lookup | Innermost scope outward |
| Enumeration | **Declaration order**, never hash order |

The last one is easy to overlook. Iterating an `unordered_map` produces output that
differs between runs, which makes any golden-file test flaky and the failure
maddening to diagnose. Declaration order is recorded separately for exactly this
reason.

---

## 7. Dimension bounds

| Property | Minimum | Maximum |
|---|---|---|
| Rows | 1 | 256 |
| Columns | 1 | 256 |

256 is a project scope boundary, not a fundamental limit. Zero and negative dimensions
are rejected outright.

Also tracked, for the backend's benefit:

| Query | Meaning |
|---|---|
| `isTileAligned()` | Both dimensions are multiples of 4, so no zero-padding is needed |
| `tileRows()`, `tileCols()` | Tile grid size after padding up — 5×7 needs a 2×2 grid |
| `byteSize()` | 4×4 FP32 is 64 bytes, exactly one MDT matrix register |

---

## 8. Test plan

25 cases across three categories.

**Valid (7).** Matching addition; `matmul(4×3, 3×5)`; transpose of a non-square matrix;
ReLU preserving shape; scalar multiplication; chained `relu(matmul(A,B))`; square
literal.

**Invalid (9).** Addition shape mismatch; matmul inner-dimension mismatch; result
assigned to the wrong shape; undeclared identifier; redeclaration; ragged literal;
literal shape mismatch; transpose result assigned to the wrong shape; print of an
undeclared name.

**Boundary (9).** 1×1 minimum; 256×256 maximum; 257×257 rejected; zero dimension
rejected; negative dimension rejected; 1×256 row vector; 256×1 column vector; 5×7
non-tile-aligned; `matmul(1×4, 4×1)` degenerate dot product.

A negative test that fails for the *wrong reason* is a failing test. Invalid cases
assert on the diagnostic kind, not merely on non-zero exit status.
