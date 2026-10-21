# MDT Runtime ABI (Phase 3)

**Owner:** Shreyas Mandem (24BCE0381) · implemented in `tools/mdtsim/MDTSim.cpp`

MDT deliberately has no transcendental instruction (`docs/isa-extensions.md` §5)
and no I/O. Two operations therefore run as **runtime calls**: host functions
inside the simulator, reached with an ordinary `CALL`. The assembler resolves
the reserved names to negative call targets; the simulator dispatches them.

| Call | Purpose | Arguments (scalar registers) |
|---|---|---|
| `CALL mdt_print` | `print(X)` | R1 = address, R2 = batch, R3 = rows, R4 = cols, R5 = row stride (bytes), R6 = print id |
| `CALL mdt_softmax` | row-wise `softmax(X)` | R1 = source, R2 = destination, R3 = batch, R4 = rows, R5 = cols, R6 = row stride (bytes) |

Arguments are set up with `ADD Rn, R0, imm`, so the generated code pays for
the call sequence in the instruction count like any real ABI would.

## Conventions

* **Layout.** Tensors are row-major, rows and columns padded to a multiple of 4,
  batches stacked (`batch stride = padded rows x row stride`).
* **Zero padding.** `mdt_softmax` writes zeros into the destination's padding
  rows and columns, preserving the invariant every MDT instruction sequence
  relies on (`compiler/backend/MDTCodeGen.h`).
* **Numerics.** `y_j = exp(x_j - max x) / sum_k exp(x_k - max x)` in FP32, in the
  same operation order as the reference interpreter
  (`compiler/tensor-ir/Interpreter.cpp`), so the two agree bit for bit.
* **Names.** `.printname <id>, <name>` in the assembly lets `mdt_print` report
  the source variable name.

## Performance model

| Call | Modelled cost | Class |
|---|---|---|
| `mdt_print` | 0 cycles beyond the `CALL` (host I/O is excluded) | Other |
| `mdt_softmax` | 4 cycles per element (`kSoftmaxCyclesPerElement`), 8 DRAM bytes per element (read + write) | Other |

`Other` runs are never overlapped with transfers or compute by the
overlap-aware estimate, so a softmax is always charged in full.

## Why softmax is a runtime call: an ISA finding

The Phase 2 ISA added `MROWMAX` and `MROWSUM` to support softmax, and the
Phase 3 code generator shows they are **not sufficient on their own**. Both
reduce one 4x4 tile. A row wider than 4 columns spans several tiles, and
combining the per-tile maxima needs a lane-wise vector max, which MDT does not
have. Normalising also needs a row-broadcast divide. The smallest extension
that would let softmax compile to straight MDT code (with `exp` still a call)
is:

* `VMAX Vd, Va, Vb`: lane-wise maximum, to combine per-tile row maxima.
* `MBCASTDIV Md, Ma, Vb`: divide row *i* of a tile by lane *i* of a vector.

We record this as future ISA work instead of adding the instructions now.
Growing the ISA only when a compiler pass demonstrably needs it has been the
project's rule since Review 2.
