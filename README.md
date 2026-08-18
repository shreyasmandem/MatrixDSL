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

## Repository layout

```
MatrixDSL/
├── docs/                  Language spec, grammar, architecture, ISA, backend, testing
│   └── review1/           Review 1 report, matrices, timeline, contribution log
├── compiler/
│   ├── frontend/          lexer/ parser/ ast/ semantic/
│   ├── llvm/              LLVM IR generation + optimization
│   └── driver/            Compiler driver / CLI
├── llvm-backend/MDT/      Custom LLVM target (TableGen + C++)
├── runtime/               Matrix runtime support library
├── tools/mdtsim/          MDT instruction-set simulator
├── examples/              Sample MatrixDSL programs
├── tests/                 valid / invalid / boundary / integration
├── benchmarks/            Baseline comparison workloads
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
| `vinay` | Vinay A | Lexer, parser, AST implementation |
| `parth` | Mudpe Parth Tulsidas | Semantic analyzer, symbol table, matrix type system |
| `kandi` | Kandi Jeevitesh Reddy | LLVM IR codegen, optimization pipeline |
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
| [`docs/review1/Review1_Report.md`](docs/review1/Review1_Report.md) | Full Review 1 report |

## Status

Review 1 — initial design, planning and feasibility. Language and ISA frozen;
shared interfaces defined; MDT simulator prototype operational.

## License

MIT — see [LICENSE](LICENSE).
