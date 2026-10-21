# Individual Contribution: Mudpe Parth Tulsidas (Phase 3 / Final Review)

| | |
|---|---|
| **Register Number** | 24BDS0353 |
| **Branch** | `parth` |
| **Role** | Semantic analysis, symbol table, verification |

## What's new in Phase 3

**`mtxverify`, the differential tester** (`tools/mtxverify/main.cpp`). The
semantic analyser proves a program is *well-shaped*; `mtxverify` proves the
compiled program computes the right *values*.

* Each positive program is compiled under all four configurations (baseline,
  fusion, tiling, both). Each is run on `mdtsim`, and every `print` is compared
  with the reference interpreter's output on the unoptimised Tensor IR.
* Negative programs (`// expect: error`) must be rejected, and the
  diagnostics must contain every `// expect-error:` string.

**End-to-end corpus** (`tests/e2e/`):

* 11 positive programs, each chosen to break one assumption: ragged shapes
  and padding, batch broadcast, the second-use fusion guard, softmax padding
  columns, named temporaries, scaling from either side.
* 6 negative programs: matmul, add and batch mismatches, an undeclared name,
  a missing semicolon, and a non-constant scalar factor.

Result: **100/100 verified compiled runs, 6/6 rejections.**

**Symbol-table fix.** `popScope` now also removes the popped scope's names
from the declaration order. Previously a shadowed name was listed twice after
its scope closed; `mtxcheck symbols` caught it.

**Test-driver fix.** `tests/TestRunner.cpp` now quotes its command correctly
for Windows `cmd.exe`, so the accept/reject suites run on every platform.

## Evidence

`ctest -L semantic -L e2e -L driver`, and
`mtxverify tests/e2e examples benchmarks/workloads`.
