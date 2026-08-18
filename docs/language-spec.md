# MatrixDSL Language Specification

Version 1.0 — frozen for Review 1.

## 1. Design goals

MatrixDSL exists to express matrix and elementary AI operations directly, so that the
compiler can reason about matrix *shape* as a first-class property rather than
recovering it from loop nests. Three goals follow:

1. **Shape is part of the type.** A matrix carries its dimensions in its type, known
   at compile time, so dimension errors become compile-time errors.
2. **Operations are named, not open-coded.** `matmul(A, B)` is a single AST node, not
   a triple-nested loop, so the backend can map it onto a target matrix instruction
   instead of trying to recognise a loop idiom.
3. **The language stays small.** One data type, eight operations. The engineering
   effort belongs in the compiler and backend, not in the surface syntax.

## 2. Types

MatrixDSL v1.0 has exactly two types.

| Type | Description |
|---|---|
| `matrix[R][C]` | R by C matrix of FP32 elements; R and C are compile-time constants |
| scalar | FP32 literal, valid only as an operand to scalar multiplication |

There are no integers, booleans, strings, pointers, structs or user-defined types,
and there is no type inference: every matrix is declared with explicit dimensions.

### Type representation

```
MatrixType {
    elementType = FP32     // fixed in v1.0
    rows        : int      // 1 .. 256
    cols        : int      // 1 .. 256
}
```

### Bounds

| Property | Minimum | Maximum |
|---|---|---|
| Rows | 1 | 256 |
| Columns | 1 | 256 |

Dimensions outside this range are rejected during semantic analysis. The 256 by 256
ceiling is a project scope boundary, not a fundamental limitation of the design.

## 3. Declarations

```
matrix A[4][4];
matrix W[128][64];
```

A declaration introduces a name with a fixed shape into the current scope. Shapes are
immutable: a matrix declared `A[4][4]` is 4 by 4 for the whole program.

- Redeclaring an existing name is an error.
- Using a name that has not been declared is an error.
- Declared matrices are zero-initialised.

## 4. Matrix literals

```
A = [
    [1, 2, 3, 4],
    [5, 6, 7, 8],
    [9, 10, 11, 12],
    [13, 14, 15, 16]
];
```

Every row of a literal must have the same number of columns, and the literal's shape
must match the declared shape of the assignment target. Elements are FP32; integer
literals are promoted.

## 5. Operations

| Operation | Syntax | Shape rule | Result shape |
|---|---|---|---|
| Addition | `A + B` | `A.rows == B.rows` and `A.cols == B.cols` | `A.rows` by `A.cols` |
| Subtraction | `A - B` | `A.rows == B.rows` and `A.cols == B.cols` | `A.rows` by `A.cols` |
| Scalar multiply | `2.0 * A` | none | `A.rows` by `A.cols` |
| Matrix multiply | `matmul(A, B)` | `A.cols == B.rows` | `A.rows` by `B.cols` |
| Transpose | `transpose(A)` | none | `A.cols` by `A.rows` |
| ReLU | `relu(A)` | none | `A.rows` by `A.cols` |
| Print | `print(A);` | none | none |

### Semantics

```
(A + B)[i][j]        = A[i][j] + B[i][j]
(A - B)[i][j]        = A[i][j] - B[i][j]
(s * A)[i][j]        = s * A[i][j]
matmul(A,B)[i][j]    = sum over k of A[i][k] * B[k][j]
transpose(A)[i][j]   = A[j][i]
relu(A)[i][j]        = max(0, A[i][j])
```

## 6. Assignment

```
C = <expression>;
```

The expression's computed shape must equal the declared shape of the target. This is
the central check performed by the semantic analyser:

```
matrix A[4][3];
matrix B[3][5];
matrix C[4][4];

C = matmul(A, B);      // ERROR: matmul yields 4x5, but C is 4x4
```

Shape checking is a bottom-up computation over the expression tree: each node reports
its result shape to its parent, and the assignment compares the root's shape against
the declared target.

## 7. Statements and program structure

A program is a sequence of statements executed top to bottom.

```
program   ::= statement*
statement ::= declaration | assignment | print
```

There is no control flow in v1.0 — no conditionals, loops, or user-defined functions
at the source level. Control-flow instructions exist in the MDT ISA because the
compiler generates them for tiling loops, but they are not exposed in the surface
language.

## 8. Memory model

Matrices are stored row-major in contiguous FP32 memory:

```
A = [[1, 2, 3],
     [4, 5, 6]]

memory: 1 2 3 4 5 6

A[i][j] == A.data[i * A.cols + j]
```

At the LLVM IR level a matrix is represented by a descriptor:

```llvm
%struct.Matrix = type { float*, i32, i32 }   ; data, rows, cols
```

Row-major order is chosen to match C conventions and to make each matrix row
contiguous, which keeps the 4 by 4 tile extraction in the backend simple.

## 9. Errors

All errors are compile-time. The compiler reports the first error with a source
location and continues collecting further errors where recovery is possible.

| Class | Example |
|---|---|
| Lexical | invalid character in source |
| Syntax | missing semicolon, unbalanced bracket |
| Semantic — undeclared | use of a name that was never declared |
| Semantic — redeclaration | two declarations of the same name |
| Semantic — shape mismatch | `matmul` where `A.cols != B.rows` |
| Semantic — literal shape | literal rows of differing length |
| Semantic — bounds | dimension outside 1..256 |

Diagnostic format:

```
error: cannot multiply matrix A (4x3) with matrix B (7x5)
  --> program.mtx:12:5
   |
12 |     C = matmul(A, B);
   |         ^^^^^^^^^^^^
   = note: matmul requires A.cols == B.rows, but 3 != 7
```

## 10. Explicitly out of scope for v1.0

Deliberately excluded to keep the language finishable within the project timeline:

- Integer and boolean types
- User-defined functions
- Source-level control flow (`if`, `while`, `for`)
- Strings, non-matrix arrays, structs, pointers
- Matrix inverse, determinant, decompositions (LU, QR, SVD)
- Convolution, pooling, softmax, batch normalisation
- Automatic differentiation
- Dynamic (runtime-determined) matrix shapes
- Sparse matrix representations

Convolution, softmax and additional activation functions are natural extensions once
the compilation path is proven end to end, and are recorded as future work rather
than as gaps.

## 11. Complete example

```
// 4x4 identity multiply followed by activation
matrix A[4][4];
matrix B[4][4];
matrix C[4][4];

A = [
    [1, 2, 3, 4],
    [5, 6, 7, 8],
    [9, 10, 11, 12],
    [13, 14, 15, 16]
];

B = [
    [1, 0, 0, 0],
    [0, 1, 0, 0],
    [0, 0, 1, 0],
    [0, 0, 0, 1]
];

C = matmul(A, B);
C = relu(C);
print(C);
```
