# Responsibility Matrix: Final Review (Phase 3)

| Stage / artefact | Owner | Phase 3 files |
|---|---|---|
| Lexer, parser, AST, diagnostics, compiler driver | Vinay A | `compiler/frontend/{lexer,parser,ast}`, `compiler/frontend/Diagnostics.*`, `compiler/driver/Pipeline.*`, `tools/mtxc` |
| Semantic analysis, symbol table, differential testing | Mudpe Parth Tulsidas | `compiler/frontend/semantic`, `tools/mtxverify`, `tests/e2e`, `tests/TestRunner.cpp` |
| Tensor IR, lowering, passes, oracle, LLVM IR | Kandi Jeevitesh Reddy | `compiler/tensor-ir/*`, `compiler/llvm/LLVMTextEmitter.*` |
| MDT code generator, simulator, runtime ABI, ablation study | Shreyas Mandem | `compiler/backend`, `tools/mdtsim`, `tools/mtxbench`, `benchmarks/workloads`, `docs/runtime-abi.md` |
| Integrated build, final report | Team (integrated by Shreyas Mandem) | `CMakeLists.txt`, `tests/CMakeLists.txt`, `docs/review3/Final_Report.*`, `README.md` |
