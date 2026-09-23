# MatrixDSL

**A Domain-Specific Language and LLVM-Based Compiler for Matrix and AI Workloads**

| | |
|---|---|
| **Course** | BCSE307P — Compiler Design |
| **Project ID** | A3 |
| **Team Number** | 13 |
| **Target** | MDT (MatrixDSL Target) — custom matrix-oriented ISA |

---

## Team

| Member | Register No. | Primary Responsibility |
|---|---|---|
| Vinay A | 24BCB0131 | Lexer + Parser + AST |
| Mudpe Parth Tulsidas | 24BDS0353 | Semantic Analysis + Symbol Table (shape/type checking) |
| Kandi Jeevitesh Reddy | 24BCE0350 | LLVM IR Generation + Optimization |
| Shreyas Mandem | 24BCE0381 | MDT ISA + Custom LLVM Backend |

---

## What this project is

MatrixDSL is a small domain-specific language for expressing matrix and basic AI
operations directly:

```
matrix A[4][4];
matrix B[4][4];
matrix C[4][4];

C = matmul(A, B);
C = relu(C);
print(C);
```

The compiler lowers these programs to LLVM IR, runs LLVM's optimizer, and then emits
assembly for **MDT** — a custom matrix-oriented target architecture defined by this
project, rather than for x86 or ARM.

The contribution is not the language alone. It is the demonstration of a complete
compilation path from domain-specific matrix semantics down to a specialised
instruction set containing first-class matrix operations.

## Compilation pipeline

```
MatrixDSL source (.mtx)
        |
        v
    Lexer  ---------------->  tokens
        |
        v
    Parser  --------------->  AST
        |
        v
Semantic Analysis  -------->  shape + type checked AST
        |                     (matrix dimension validation)
        v
   LLVM IR Generation
        |
        v
   LLVM Optimization
        |
        v
   MDT LLVM Backend
        |
        v
     MDT ISA
        |
        v
   MDT Assembly (.s)
```

## The MDT target

MDT has three register/instruction domains:

| Domain | Registers | Instructions |
|---|---|---|
| Scalar | R0–R15 (32-bit) | `ADD` `SUB` `MUL` `LOAD` `STORE` `BR` `BEQ` `BNE` `JMP` `CALL` `RET` |
| Vector | V0–V7 (128-bit = 4×FP32) | `VADD` `VMUL` |
| Matrix | M0–M7 (4×4 FP32 = 64 bytes) | `MATMUL` `TRANSPOSE` `RELU` |

Matrices larger than the 4×4 hardware tile are handled by compiler-level tiling.
A 128×128 matrix becomes a 32×32 grid of 4×4 tiles.

Full specification: [`docs/isa.md`](docs/isa.md)

## Phase 2 — evolving toward a tensor / AI compiler

Phase 2 extends the project from a matrix DSL into a small tensor DSL, adds a
hand-rolled Tensor IR (MLIR-inspired, not MLIR itself) between semantic analysis and
LLVM IR, and extends MDT with 7 new instructions for operator fusion, mixed-precision
storage, and a scratchpad memory hierarchy — enough to compile and run a transformer
feed-forward block and a single-head attention kernel, and to measure the effect of
fusion and tiling with a controlled ablation study.

Full design: [`docs/review2/Review2_Report.pdf`](docs/review2/Review2_Report.pdf)

## Repository layout

```
MatrixDSL/
├── docs/                  Language spec, grammar, architecture, ISA, backend, testing
│   ├── review1/           Review 1 report, matrices, timeline, contribution log
│   └── review2/           Review 2 report, responsibility matrix, roadmap
├── compiler/
│   ├── frontend/          lexer/ parser/ ast/ semantic/
│   ├── tensor-ir/         Phase 2: Tensor IR, fusion pass, hierarchical tiling
│   ├── llvm/              LLVM IR generation + optimization
│   └── driver/            Compiler driver / CLI
├── llvm-backend/MDT/      Custom LLVM target (TableGen + C++)
├── runtime/               Matrix runtime support library
├── tools/mdtsim/          MDT instruction-set simulator + performance model
├── examples/              Sample MatrixDSL programs
├── benchmarks/            ffn_block.mtx, attention_head.mtx, baseline workloads
├── tests/                 valid / invalid / boundary / integration / tensor-ir / fusion
└── scripts/               build / test / benchmark helpers
```

## Branching model

`main` holds team-level deliverables and the **shared interface contracts** that all
four modules compile against (AST node definitions, token enum, symbol-table
interface). Implementation work happens on per-member branches and is merged after
review:

| Branch | Owner | Contents |
|---|---|---|
| `main` | Team | Docs, Review 1 deliverables, shared headers, build system, tests |
| `Vinay-A` | Vinay A | Lexer, parser, AST implementation |
| `parth` | Mudpe Parth Tulsidas | Semantic analyzer, symbol table, matrix type system |
| `Jeevitesh` | Kandi Jeevitesh Reddy | LLVM IR codegen, optimization pipeline |
| `shreyas` | Shreyas Mandem | MDT ISA, TableGen descriptions, backend, simulator |

Freezing the AST and token definitions on `main` first is deliberate: it lets all four
members work in parallel instead of waiting for the parser to be finished.

## Building

Requires **CMake ≥ 3.16**, a **C++17** compiler, and **LLVM 17 or 18** development
headers.

```bash
./scripts/build.sh
```

or manually:

```bash
cmake -S . -B build -DLLVM_DIR=/path/to/llvm/lib/cmake/llvm
cmake --build build -j
```

The MDT simulator builds standalone with no LLVM dependency:

```bash
cmake -S . -B build -DMATRIXDSL_BUILD_SIM_ONLY=ON && cmake --build build -j
```

## Testing

```bash
./scripts/test.sh
```

Test plan and case inventory: [`docs/testing.md`](docs/testing.md)

## Documentation

| Document | Contents |
|---|---|
| [`docs/language-spec.md`](docs/language-spec.md) | MatrixDSL semantics, types, operations |
| [`docs/grammar.md`](docs/grammar.md) | Formal EBNF grammar |
| [`docs/architecture.md`](docs/architecture.md) | System architecture and data flow |
| [`docs/isa.md`](docs/isa.md) | MDT instruction set and register model |
| [`docs/llvm-backend.md`](docs/llvm-backend.md) | Backend design, TableGen, instruction selection |
| [`docs/testing.md`](docs/testing.md) | Testing strategy |
| [`docs/review1/Review1_Report.pdf`](docs/review1/Review1_Report.pdf) | Review 1 report — PDF submission copy (23 pages) |
| [`docs/review1/Review1_Report.md`](docs/review1/Review1_Report.md) | Review 1 report — markdown source |
| [`docs/review1/Review1_Presentation.pptx`](docs/review1/Review1_Presentation.pptx) | Review 1 presentation — 14 slides |
| [`docs/review2/Review2_Report.pdf`](docs/review2/Review2_Report.pdf) | **Review 2 report — PDF submission copy** |
| [`docs/review2/Review2_Report.md`](docs/review2/Review2_Report.md) | Review 2 report — markdown source |
| [`docs/review2/responsibility-matrix.md`](docs/review2/responsibility-matrix.md) | Phase 2 team responsibility matrix |
| [`docs/review2/roadmap.md`](docs/review2/roadmap.md) | Phase 2 16-week roadmap and milestones |

## Status

Review 2 — Phase 2 architecture designed and frozen: Tensor IR, 7 new MDT
instructions, scratchpad memory hierarchy, fusion strategy, and the benchmark/ablation
plan are committed. Implementation against this design is in progress on individual
branches. Review 1 (initial design, planning and feasibility) is complete; see
[`docs/review1/`](docs/review1/).

## License

MIT — see [LICENSE](LICENSE).
