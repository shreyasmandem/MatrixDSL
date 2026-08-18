# Individual Contribution — Shreyas Mandem

| | |
|---|---|
| **Name** | Shreyas Mandem |
| **Register Number** | 24BCE0381 |
| **Role** | MDT ISA Definition + Custom LLVM Backend |
| **Branch** | `shreyas` |
| **Review** | Review 1 |

---

## 1. Scope of my component

I own the half of the compiler below LLVM IR: the definition of the target
architecture itself, and the backend that translates optimized LLVM IR into MDT
machine instructions.

This is the component that makes the project a compiler-construction contribution
rather than a source-to-source translator. MDT does not exist in LLVM, so every piece
of target knowledge — registers, instructions, selection patterns, calling convention,
assembly syntax — has to be described from scratch.

---

## 2. What I completed for Review 1

### 2.1 MDT instruction set architecture — frozen

`docs/isa.md`

| Item | Detail |
|---|---|
| Register file | R0–R15 scalar (32-bit), V0–V7 vector (128-bit), M0–M7 matrix (4×4 FP32) |
| Instruction set | 17 instructions across scalar, memory, control-flow, vector and matrix classes |
| Instruction semantics | Exact behaviour specified for every instruction |
| Assembly syntax | Operand forms, labels, directives (`.text`, `.data`, `.float`, `.word`, `.space`) |
| Encoding | R-type, I-type and M-type formats; 32-bit fixed width; opcode assignments |
| Calling convention | Argument/return registers, caller/callee-saved split, stack direction |

**Design decision — why the matrix tile is 4×4.** A matrix register holds 4 × 4 = 16
FP32 values = 64 bytes. That is large enough that one `MATMUL` performs 64
multiply-accumulate operations — real work, not a token instruction — while remaining
small enough that the register file is plausible and the simulator stays simple. Fixed
matrix-tile compute units of exactly this kind are what NVIDIA tensor cores and the
TPU's systolic array implement; 4×4 is that idea at a scale appropriate to this
project.

**Design decision — 3-bit matrix register fields.** Matrix operand fields are 3 bits
wide because there are only 8 matrix registers, against 5 bits for scalar operands.
This keeps the M-type format comfortably inside 32 bits with room to spare for a
future accumulate-mode bit.

### 2.2 Backend architecture and lowering design

`docs/llvm-backend.md`

- Complete file organisation and the responsibility of each component
- Lowering pipeline from optimized IR through to assembly text
- Instruction selection table mapping every LLVM construct to its MDT instruction
- Register allocation analysis, including the matrix spill path
- Seven-phase implementation order, each phase independently demonstrable

**The central design decision.** Matrix operations reach the backend as *reserved
intrinsic calls*, not as loop nests:

```llvm
call void @mdt.matmul.4x4(float* %dst, float* %a, float* %b)
```

The alternative — emitting a triple-nested loop and recognising it in the backend —
fails in practice, because loop-idiom recognition is routinely defeated by the
optimizer's own transformations before the backend ever sees the code. An opaque call
survives `-O2` intact and lowers to exactly one MDT instruction. The tile geometry is
encoded in the name, so the backend never has to infer it.

This is also the interface I share with Kandi's IR generation, and it was fixed and
documented before either of us started writing code — which is the mitigation for
risk R4.

### 2.3 TableGen target description

`llvm-backend/MDT/`

| File | Contents |
|---|---|
| `MDT.td` | Top-level target, subtarget features, processor models |
| `MDTRegisterInfo.td` | All 32 registers and three disjoint register classes |
| `MDTInstrInfo.td` | Instruction formats (R/I/M), all 17 instructions, selection patterns, custom matrix SDNodes |
| `MDTCallingConv.td` | Argument, return and callee-saved register assignment |
| `MDTISelLowering.h` | `MDTISD` node types and the lowering interface |
| `CMakeLists.txt` | TableGen invocation and incremental source list |

**Design decision — matrix tiles typed as `v16f32`.** Representing a 4×4 tile as
`v16f32` reuses LLVM's existing vector type machinery. Introducing a genuinely new
MVT would require patching LLVM core, which would make the target impossible to build
against a stock LLVM release and would tie the project to a modified toolchain.

### 2.4 MDT instruction-set simulator — working prototype

`tools/mdtsim/`

| File | Contents |
|---|---|
| `MDTSim.h` | Machine state, instruction representation, assembler and simulator interfaces |
| `Assembler.cpp` | Two-pass assembler: label resolution, then instruction decoding; directives and data section |
| `MDTSim.cpp` | Fetch-decode-execute loop implementing all 17 instructions |
| `main.cpp` | Command-line driver with tracing and register dumps |

This is the verifiable technical output for Review 1. MDT has no silicon, so without a
simulator, "generated MDT assembly" would be text nobody can check. With it, the ISA
is executable and the backend has something to be verified against.

Notable implementation points:

- **Every memory access is bounds-checked.** An out-of-range access produces a
  diagnostic rather than undefined behaviour, so a backend bug surfaces as a clear
  error instead of silently corrupting simulator state.
- **`MATMUL` accumulates into a temporary** before writing the destination, because
  the destination register may alias a source (`MATMUL M0, M0, M1` is legal).
- **Transfer width is selected by destination register class** — 4 bytes for R, 16 for
  V, 64 for M — matching the ISA specification rather than requiring separate
  mnemonics.
- **Strided tile transfers** (`LOADT` / `STORET`) extract a 4×4 tile from a wider
  matrix, which is what the tiling pass needs for anything larger than 4×4.

### 2.5 Backend test suite

`tests/backend/` — 10 MDT assembly programs covering every instruction class:

| Test | Covers |
|---|---|
| `scalar_arith.s` | `ADD`, `SUB`, `MUL` |
| `memory.s` | `LOAD` / `STORE` round-trip |
| `control_flow.s` | `BEQ`, `BR`, loop termination |
| `call_return.s` | `CALL`, `RET`, return-address handling |
| `vector_ops.s` | `VADD`, `VMUL`, 4-lane semantics |
| `matmul_4x4.s` | `MATMUL` + `TRANSPOSE` |
| `transpose.s` | `TRANSPOSE` as an involution |
| `relu.s` | `RELU` |
| `ai_layer.s` | Full `relu(W × X)` layer |
| `tile_strided.s` | Strided 4×4 tile extraction from an 8×8 matrix |

Two of these are deliberately **self-checking**, which means they need no
hand-computed expected values and cannot rot:

- `matmul_4x4.s` computes A × Aᵀ. The result must be symmetric — if it is not, either
  `MATMUL` or `TRANSPOSE` is wrong.
- `transpose.s` transposes twice. The result must equal the input exactly.

### 2.6 Risk analysis for my component

`docs/llvm-backend.md` §9, and `docs/review1/risk-register.md` R1 on `main`

I identified the backend as the project's single largest risk and designed the
mitigation before starting, rather than waiting to discover the problem:

- **Risk.** A full SelectionDAG and TableGen target is the most demanding component
  here. Public tutorials for building a toy LLVM backend run to several hundred pages,
  and the calling-convention, frame-lowering and MC-layer plumbing each carry a real
  learning curve. If the scalar phases overrun, the matrix instructions — the actual
  novelty — never get reached.
- **Mitigation.** Seven independently demonstrable phases, plus a designed fallback
  (`MDTDirectEmitter`) that walks optimized LLVM IR, pattern matches directly, does
  linear-scan register assignment, and prints assembly — bypassing SelectionDAG,
  TableGen and the MC layer entirely. It still satisfies the project abstract and
  emits identical assembly, so the simulator, tests and benchmarks are unaffected.
- **Decision point: end of Week 8.** Stated criteria, so a slip becomes a planned
  engineering decision rather than an improvised retreat.

---

## 3. Evidence

| Evidence | Location |
|---|---|
| MDT ISA specification | `docs/isa.md` |
| Backend design | `docs/llvm-backend.md` |
| TableGen description | `llvm-backend/MDT/*.td` |
| Lowering interface | `llvm-backend/MDT/MDTISelLowering.h` |
| Simulator source | `tools/mdtsim/` |
| Backend tests | `tests/backend/*.s` |
| Risk analysis | `docs/llvm-backend.md` §9 (R1 on `main`) |
| Commits | branch `shreyas` |

---

## 4. Build status

The simulator is written to build standalone with no LLVM dependency:

```bash
cmake -S . -B build
cmake --build build -j
./build/bin/mdtsim tests/backend/matmul_4x4.s --dump-matrix M0
```

**Note for the review:** the simulator has not yet been compiled, because no C++
toolchain was installed on the machine used to author it. Compiling and running the
ten backend tests is the first task before the Review 1 demonstration, and any
resulting fixes will be committed to this branch.

---

## 5. Review 2 targets

| # | Target |
|---|---|
| 1 | Simulator compiled, all 10 backend tests passing |
| 2 | MDT registered as an LLVM target — `llc -march=mdt` accepts input |
| 3 | Scalar instructions (`ADD`/`SUB`/`MUL`) selecting from hand-written LLVM IR |
| 4 | `LOAD`/`STORE` selecting, verified by simulator round-trip |
| 5 | Branch and call/return lowering, so a loop compiles and terminates |

## 6. What I can be questioned on

- Why the matrix tile is 4×4, and what changes at 8×8
- Why matrix operations are intrinsic calls rather than recognised loop nests
- Why tiles are typed `v16f32` instead of a new MVT
- How instruction selection reaches `MATMUL` from LLVM IR
- Register allocation with only 8 matrix registers, and when tiles spill
- The instruction encoding, and why matrix operand fields are 3 bits
- What the fallback backend path does differently, and what it costs
