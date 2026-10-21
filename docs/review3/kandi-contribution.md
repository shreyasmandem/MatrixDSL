# Individual Contribution: Kandi Jeevitesh Reddy (Phase 3 / Final Review)

| | |
|---|---|
| **Register Number** | 24BCE0350 |
| **Branch** | `Jeevitesh` |
| **Role** | Tensor IR, optimisation passes, LLVM IR |

## What's new in Phase 3

**AST → Tensor IR lowering** (`compiler/tensor-ir/Lowering.{h,cpp}`) replaces
the hand-built graphs of Review 2.

* It produces SSA: assignments rebind names, so named temporaries never
  block fusion.
* `print` defines observable behaviour.
* Never-assigned matrices become `Load` inputs.
* `*` dispatches to scalar multiplication or matmul, exactly as the type
  checker does.

**Passes** (`compiler/tensor-ir/Passes.{h,cpp}`, `FusionPass.cpp`):

* **Epilogue fusion** (new pattern): `FusedMatMulAcc → Relu` becomes a
  three-operand `FusedMatMulRelu`, `relu(A·B + C)`, with no new opcode. This
  closes the Review 2 "two ops, not three" gap.
* **Dead-code elimination**: a reverse sweep from the `Print` roots.
* **Scratchpad tiling**:
  * computes per-operand tile reuse, including batch broadcast;
  * stages reused operands with `ScratchStore`/`ScratchLoad`;
  * chooses greedily by DRAM bytes saved under the 64 KiB capacity;
  * marks multi-tile matmuls `TiledMatMul`.

**Reference interpreter** (`compiler/tensor-ir/Interpreter.{h,cpp}`) is the
oracle behind all verification.

* It evaluates Tensor IR in FP32 on the host.
* It generates exact-arithmetic inputs (multiples of 1/8), so results are
  bit-comparable.
* It mirrors the hardware's summation order and the runtime's softmax.

**LLVM IR emitter** (`compiler/llvm/LLVMTextEmitter.{h,cpp}`): `--emit-llvm`
writes a textual module that calls the `@mdt.*` tile intrinsics fixed in
Review 1. It needs no LLVM installation.

**Tensor IR API** (`TensorIR.h`) gains `constData`, `scratchOffset`,
`insertOpBefore`, `eraseIf` and `useCount`. `dump()` now shows names, scale
factors and scratchpad offsets.

## Evidence

* `matrixdslc <file> --emit-tir -o -` shows the IR before and after the passes.
* `matrixdslc <file> --stats` shows the staging decisions.
* `ctest -L reference`.
