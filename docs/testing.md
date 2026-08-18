# MatrixDSL Testing Strategy

Every stage of the pipeline is tested independently, and the whole pipeline is tested
end to end. Tests are classified as **valid** (positive), **invalid** (negative) and
**boundary**, as required by the project review criteria.

## 1. Test organisation

```
tests/
├── lexer/          Token-level tests
├── parser/         Grammar and AST tests
├── semantic/       Shape and type checking tests
├── llvm/           LLVM IR generation tests
├── backend/        MDT assembly generation tests
├── integration/    Full pipeline tests
├── valid/          Programs that must compile and run correctly
├── invalid/        Programs that must be rejected with a specific diagnostic
└── boundary/       Edge-case dimensions and limits
```

## 2. Test categories

| Category | Meaning | Pass criterion |
|---|---|---|
| Valid | Well-formed program | Compiles, runs, produces expected output |
| Invalid | Ill-formed program | Rejected with the expected error class and message |
| Boundary | Extreme but legal input | Compiles and runs correctly, no crash or overflow |

A negative test that fails for the *wrong reason* is a failing test. Invalid-program
tests assert on the diagnostic, not merely on non-zero exit status.

## 3. Lexer tests

| Test | Input | Expected |
|---|---|---|
| Keywords | `matrix print matmul transpose relu` | Five keyword tokens |
| Identifiers | `A result_1 _tmp` | Three `IDENTIFIER` tokens |
| Numbers | `0 42 3.14` | `INT INT NUMBER` |
| Operators | `+ - * =` | `PLUS MINUS STAR ASSIGN` |
| Brackets | `( ) [ ] , ;` | Six punctuation tokens |
| Comments | `// ignored` | Skipped, no token |
| Keyword prefix | `matrixx` | One `IDENTIFIER`, not `MATRIX` |
| Invalid character | `A = B @ C;` | `INVALID` token, error at column of `@` |
| Empty input | `` | Single `END_OF_FILE` |
| Line tracking | multi-line input | Correct line and column on every token |

## 4. Parser tests

| Test | Input | Expected |
|---|---|---|
| Declaration | `matrix A[4][4];` | `MatrixDecl(A, 4, 4)` |
| Assignment | `C = A;` | `Assignment(Var C, Var A)` |
| Binary expression | `C = A + B;` | `BinaryExpr(+, Var A, Var B)` |
| Precedence | `C = A + B * D;` | `+` at root, `*` as right child |
| Left associativity | `C = A - B - D;` | `((A-B)-D)`, not `(A-(B-D))` |
| Function call | `C = matmul(A, B);` | `CallExpr(matmul, [A, B])` |
| Nested call | `C = relu(matmul(A,B));` | Nested `CallExpr` |
| Matrix literal | `A = [[1,2],[3,4]];` | `MatrixLiteralExpr` 2 by 2 |
| Parentheses | `C = (A + B) * D;` | `*` at root |
| Print | `print(A);` | `PrintStmt(A)` |
| Missing semicolon | `C = A + B` | Syntax error, reports expected `;` |
| Unbalanced bracket | `A = [[1,2],[3,4];` | Syntax error, reports expected `]` |
| Empty program | `` | Empty program node, no error |

## 5. Semantic tests

### 5.1 Valid

| Test | Program | Expected |
|---|---|---|
| Matching addition | `A[4][4] + B[4][4]` into `C[4][4]` | Accepted, result 4 by 4 |
| Valid multiply | `matmul(A[4][3], B[3][5])` into `C[4][5]` | Accepted, result 4 by 5 |
| Transpose | `transpose(A[4][3])` into `C[3][4]` | Accepted, result 3 by 4 |
| ReLU | `relu(A[4][4])` into `C[4][4]` | Accepted, shape preserved |
| Scalar multiply | `2.0 * A[4][4]` into `C[4][4]` | Accepted, shape preserved |
| Chained | `relu(matmul(A,B))` | Accepted, shapes propagate |
| Square literal | `A[2][2] = [[1,2],[3,4]]` | Accepted |

### 5.2 Invalid

| Test | Program | Expected diagnostic |
|---|---|---|
| Addition mismatch | `A[4][4] + B[3][3]` | dimensions must match |
| Multiply mismatch | `matmul(A[4][3], B[7][5])` | `A.cols == B.rows` required, 3 != 7 |
| Result mismatch | `C[4][4] = matmul(A[4][3], B[3][5])` | result 4x5 cannot assign to 4x4 |
| Undeclared use | `C = A + Z;` | undeclared identifier `Z` |
| Redeclaration | `matrix A[4][4]; matrix A[2][2];` | `A` already declared |
| Ragged literal | `A = [[1,2],[3,4,5]];` | rows have differing lengths |
| Literal shape | `A[2][2] = [[1,2,3],[4,5,6]];` | literal is 2x3, `A` is 2x2 |
| Transpose assign | `C[4][4] = transpose(A[4][3]);` | result 3x4 cannot assign to 4x4 |
| Print undeclared | `print(Q);` | undeclared identifier `Q` |

### 5.3 Boundary

| Test | Program | Expected |
|---|---|---|
| Minimum size | `matrix A[1][1];` | Accepted |
| Maximum size | `matrix A[256][256];` | Accepted |
| Over maximum | `matrix A[257][257];` | Rejected, exceeds 256 limit |
| Zero dimension | `matrix A[0][4];` | Rejected, dimension must be >= 1 |
| Negative dimension | `matrix A[-1][4];` | Rejected at parse or semantic stage |
| Single row | `matrix A[1][256];` | Accepted |
| Single column | `matrix A[256][1];` | Accepted |
| Non-multiple of 4 | `matrix A[5][7];` | Accepted, padded during tiling |
| Vector-like multiply | `matmul(A[1][4], B[4][1])` | Accepted, result 1 by 1 |

## 6. LLVM IR tests

| Test | Check |
|---|---|
| Module validity | `llvm::verifyModule` reports no errors |
| Descriptor type | `%struct.Matrix = type { float*, i32, i32 }` emitted |
| Allocation | Each declaration produces storage of `rows * cols * 4` bytes |
| Zero initialisation | Declared matrices are zeroed |
| Matrix intrinsic | `matmul` emits `call void @mdt.matmul.4x4(...)` |
| Row-major indexing | Element access computes `i * cols + j` |
| Constant folding | Literal-only expressions folded by the optimizer |
| Dead code | Unused matrix computation removed at `-O2` |
| IR stability | Same source produces identical IR across runs |

## 7. Backend tests

| Test | Check |
|---|---|
| Scalar arithmetic | `add i32` selects `ADD`, likewise `SUB` and `MUL` |
| Memory | `load` / `store` select `LOAD` / `STORE` with correct offsets |
| Branches | Conditional branch selects `BEQ` or `BNE` correctly |
| Call and return | Function call emits `CALL`, return emits `RET` |
| Vector add | `fadd <4 x float>` selects `VADD` |
| Vector multiply | `fmul <4 x float>` selects `VMUL` |
| Matrix multiply | Matrix intrinsic selects a single `MATMUL` |
| ReLU | Matrix intrinsic selects `RELU` |
| Transpose | Matrix intrinsic selects `TRANSPOSE` |
| Register classes | Scalar values never allocated to V or M registers |
| Assembly syntax | Output parses cleanly in the MDT simulator |
| Matrix spill | More than 8 live tiles spills correctly to stack |

## 8. Integration tests

| Test | Program | Expected |
|---|---|---|
| Identity multiply | 4x4 times identity | Result equals input matrix |
| Addition | 4x4 plus 4x4 | Element-wise sum |
| ReLU | Matrix with negatives | All negatives become zero |
| Transpose | 4x4 transpose twice | Result equals original |
| Chained ops | `relu(matmul(A,B))` | Matches reference computation |
| 16x16 tiling | 16x16 multiply | 4x4 tile grid, correct result |
| 32x32 tiling | 32x32 multiply | 8x8 tile grid, correct result |
| Non-multiple of 4 | 5x7 times 7x3 | Zero-padded, logical shape preserved |
| Large workload | 128x128 multiply | Correct against reference |

## 9. Differential testing

The strongest correctness check available to this project, because it requires no
hand-written expected values:

```
MatrixDSL program
      |
      +---> compile to host (x86-64) ---> run natively ---> result A
      |
      +---> compile to MDT ------------> run in simulator -> result B

assert A == B, element for element
```

Any divergence indicates a bug in the MDT backend, the tiling pass, or the simulator.
Because the frontend and IR generation are shared by both paths, a divergence
localises the fault to the backend half of the compiler.

FP32 comparison uses an exact bitwise check where the operation order is identical,
and a tolerance of 1e-5 relative error where tiling changes accumulation order.

## 10. Running the tests

```bash
./scripts/test.sh              # everything
./scripts/test.sh lexer        # one stage
./scripts/test.sh --category invalid
```

Expected output:

```
[ lexer      ]  10 passed,  0 failed
[ parser     ]  13 passed,  0 failed
[ semantic   ]  25 passed,  0 failed
[ llvm       ]   9 passed,  0 failed
[ backend    ]  12 passed,  0 failed
[ integration]   9 passed,  0 failed
------------------------------------
   TOTAL         78 passed,  0 failed
```

## 11. Coverage targets

| Stage | Target | Rationale |
|---|---|---|
| Lexer | 100% of token types | Small, fully enumerable |
| Parser | 100% of grammar productions | Every production exercised at least once |
| Semantic | 100% of shape rules, both valid and invalid | This is where the DSL earns its keep |
| IR generation | Every operation kind | One test per language operation |
| Backend | 100% of the 17 instructions | Every instruction selected at least once |
| Integration | Every operation, plus tiling paths | End-to-end confidence |

## 12. Continuous verification

Every merge to `main` must pass the full suite. A member branch may contain failing
tests for work in progress; `main` may not.
