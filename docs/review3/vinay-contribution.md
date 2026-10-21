# Individual Contribution: Vinay A (Phase 3 / Final Review)

| | |
|---|---|
| **Register Number** | 24BCB0131 |
| **Branch** | `Vinay-A` |
| **Role** | Lexer, parser, AST, compiler driver |

## What's new in Phase 3

**`matrixdslc`, the compiler** (`tools/mtxc/main.cpp`):

* `--emit-tokens`, `--emit-ast`, `--emit-tir`, `--emit-llvm`, `-S`
* `--no-fusion`, `--no-tiling`, `-O0`
* `--run`, `--perf`, `--verify`, `--stats`
* Exit codes the test driver relies on: 0 ok, 1 compile error,
  2 internal/usage error, 3 verification failure.

**`Pipeline`** (`compiler/driver/Pipeline.{h,cpp}`) is the single object that
runs lexer → parser → semantic analysis → lowering → passes → code generation
→ simulator. `matrixdslc`, `mtxverify` and `mtxbench` all share it, so every
tool compiles in exactly the same way.

**Caret diagnostics** (`compiler/frontend/Diagnostics.{h,cpp}`): every
lexical, syntax and semantic error shows `file:line:col`, the source line, a
caret and a note.

```
matmul_mismatch.mtx:6:5: error: cannot multiply matrix 4x3 with matrix 4x4
  6 | C = matmul(A, B);
    |     ^
  note: matmul requires A.cols == B.rows, but 3 != 4
```

**Front-end fixes found by end-to-end testing:**

* The Phase 2 keywords `tensor` and `softmax` were handled by the parser but
  never produced by the lexer. They are now wired into the keyword table.
* Matrix literals now accept negative elements (`[[-1, 2], ...]`).
* The branch's `AST.h` and `Lexer.h` are synced with the frozen Phase 2 headers.

## Evidence

`ctest -L lexer -L parser` (28 tests).
`matrixdslc examples/ai_layer.mtx --emit-tokens -o -`.
