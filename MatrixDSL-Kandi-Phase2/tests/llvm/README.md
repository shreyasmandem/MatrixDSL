# Expected LLVM IR

Hand-written reference IR showing what the generator must emit for each
MatrixDSL construct. These files serve two purposes at Review 1:

1. They pin down the IR contract with the MDT backend **before** the generator
   exists, so Shreyas can develop and test instruction selection against real
   IR rather than waiting for code generation to be finished.
2. They become golden-file comparisons once the generator lands in Review 2.

| File | Construct | Key point |
|---|---|---|
| `matmul_4x4.ll` | `C = matmul(A, B)` | One call, **not** a loop nest |
| `addition_4x4.ll` | `C = A + B` | Element-wise ops **do** use a loop |
| `transpose_relu.ll` | `transpose` then `relu` | Unary form, destination first |
| `ai_layer.ll` | `relu(matmul(W, X))` | Two statements, two instructions |

The distinction between the first two files is the central design decision of
this module: matrix operations stay opaque so they survive `-O2` and lower to a
single MDT instruction, while element-wise operations are ordinary loops the
optimizer is free to unroll and vectorise.
