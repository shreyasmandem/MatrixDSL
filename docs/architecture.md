# MatrixDSL System Architecture

## 1. Overview

MatrixDSL is a four-stage compiler with a custom code generator. Each stage consumes
a well-defined representation and produces the next one, so the four team members can
work against interfaces rather than against each other's in-progress code.

```
                         MatrixDSL source (.mtx)
                                   |
                                   v
                    +---------------------------+
                    |    Lexer / Parser         |   Owner: Vinay A
                    +-------------+-------------+
                                  |
                                  v
                                 AST
                                  |
                                  v
                    +---------------------------+
                    |    Semantic Analysis      |   Owner: Parth
                    |    Shape + Type Check     |
                    +-------------+-------------+
                                  |
                                  v
                        shape-annotated AST
                                  |
                                  v
                    +---------------------------+
                    |    LLVM IR Generation     |   Owner: Kandi
                    +-------------+-------------+
                                  |
                                  v
                              LLVM IR
                                  |
                                  v
                    +---------------------------+
                    |    LLVM Optimization      |   Owner: Kandi
                    +-------------+-------------+
                                  |
                                  v
                        optimized LLVM IR
                                  |
                                  v
                    +---------------------------+
                    |    MDT LLVM Backend       |   Owner: Shreyas
                    +-------------+-------------+
                                  |
              +-------------------+-------------------+
              v                   v                   v
           Scalar              Vector              Matrix
           R0-R15              V0-V7               M0-M7
              |                   |                   |
              v                   v                   v
        ADD/SUB/MUL           VADD/VMUL            MATMUL
        LOAD/STORE                                 RELU
        BRANCHES                                   TRANSPOSE
              |                   |                   |
              +-------------------+-------------------+
                                  |
                                  v
                            MDT Assembly (.s)
                                  |
                                  v
                            MDT Simulator
                                  |
                                  v
                          Program output
```

## 2. Stage responsibilities

| Stage | Input | Output | Owner |
|---|---|---|---|
| Lexer | source text | token stream | Vinay A |
| Parser | token stream | AST | Vinay A |
| Semantic Analyzer | AST | shape-annotated AST + symbol table | Parth |
| LLVM IR Generator | annotated AST | LLVM IR module | Kandi |
| LLVM Optimizer | LLVM IR | optimized LLVM IR | Kandi |
| MDT Backend | optimized LLVM IR | MDT assembly | Shreyas |
| MDT Simulator | MDT assembly | execution result | Shreyas |

## 3. Module interfaces

The interfaces below are frozen on `main` before implementation starts. This is the
key decision that allows parallel work: each member codes against a fixed contract
and can test with hand-constructed inputs rather than waiting for the upstream stage.

### 3.1 Lexer to Parser

```cpp
enum class TokenType { MATRIX, PRINT, MATMUL, TRANSPOSE, RELU,
                       IDENTIFIER, NUMBER, INT,
                       ASSIGN, PLUS, MINUS, STAR,
                       LPAREN, RPAREN, LBRACKET, RBRACKET,
                       COMMA, SEMICOLON, END_OF_FILE, INVALID };

struct Token {
    TokenType   type;
    std::string text;
    int         line;
    int         column;
};

class Lexer {
public:
    explicit Lexer(const std::string& source);
    Token nextToken();
};
```

### 3.2 Parser to Semantic Analyzer

The AST node hierarchy (see `compiler/frontend/ast/AST.h`):

```
ASTNode
|
+-- Expr
|   +-- NumberExpr          literal scalar
|   +-- VariableExpr        identifier reference
|   +-- BinaryExpr          + - *
|   +-- MatrixLiteralExpr   [[...],[...]]
|   +-- CallExpr            matmul / transpose / relu
|
+-- Stmt
    +-- MatrixDecl          matrix A[R][C];
    +-- Assignment          A = expr;
    +-- PrintStmt           print(A);
```

Every `Expr` carries a `MatrixType resultType` field, left empty by the parser and
filled in by the semantic analyzer.

### 3.3 Semantic Analyzer to IR Generator

```cpp
struct MatrixType {
    int rows = 0;
    int cols = 0;
    bool isScalar() const { return rows == 1 && cols == 1; }
};

class SymbolTable {
public:
    bool declare(const std::string& name, MatrixType type);
    std::optional<MatrixType> lookup(const std::string& name) const;
};
```

The IR generator may assume that any AST reaching it has passed semantic analysis:
every `Expr::resultType` is populated, every identifier resolves, and every shape
rule holds. No shape re-checking happens downstream.

### 3.4 IR Generator to Backend

The interface is LLVM IR itself. Matrices are represented by a descriptor:

```llvm
%struct.Matrix = type { float*, i32, i32 }   ; data, rows, cols
```

Matrix operations are emitted as calls to target intrinsic-style functions that the
backend recognises and lowers to MDT matrix instructions:

```llvm
declare void @mdt.matmul.4x4(float* %dst, float* %a, float* %b)
declare void @mdt.relu.4x4(float* %dst, float* %src)
declare void @mdt.transpose.4x4(float* %dst, float* %src)
```

Keeping the tile size in the name (`4x4`) means the backend never has to infer tile
geometry — the tiling pass has already decomposed larger matrices by this point.

## 4. Data flow for a single statement

Source:

```
C = matmul(A, B);      // A is 4x4, B is 4x4
```

| Stage | Representation |
|---|---|
| Tokens | `IDENT(C) ASSIGN MATMUL LPAREN IDENT(A) COMMA IDENT(B) RPAREN SEMI` |
| AST | `Assignment(VariableExpr(C), CallExpr(matmul, [Var(A), Var(B)]))` |
| After sema | same tree, with `resultType = 4x4` on the `CallExpr`, verified against `C` |
| LLVM IR | `call void @mdt.matmul.4x4(float* %C.data, float* %A.data, float* %B.data)` |
| After opt | unchanged (single call, nothing to fold) |
| MDT asm | `LOAD M1, [R1+0]` / `LOAD M2, [R2+0]` / `MATMUL M0, M1, M2` / `STORE M0, [R0+0]` |

## 5. Tiling

MDT matrix registers hold exactly one 4 by 4 FP32 tile. Matrices larger than that are
decomposed by a compiler pass before instruction selection.

```
128x128 matrix
      |
      v
divide into 4x4 tiles
      |
      v
32x32 tile grid
      |
      v
load required tiles into M registers
      |
      v
MATMUL per tile triple (i, j, k)
      |
      v
accumulate and store result tiles
```

For a tiled multiply of two N by N matrices, the pass emits the loop nest:

```
for i in 0 .. N/4:
    for j in 0 .. N/4:
        acc = 0
        for k in 0 .. N/4:
            load tile A[i][k] into M1
            load tile B[k][j] into M2
            MATMUL M3, M1, M2
            acc = acc + M3
        store acc to C[i][j]
```

Dimensions that are not multiples of four are zero-padded up to the next multiple for
the tile computation; the logical matrix dimensions are preserved in the descriptor
so that `print` and shape checking still see the true size.

The first implementation prioritises correctness and simple tiling. Cache-aware or
auto-tuned tile scheduling is explicitly out of scope.

## 6. Why LLVM IR in the middle

Placing LLVM IR between the frontend and the MDT backend buys three things that
matter for this project:

1. **A stable interface.** The frontend team and the backend team develop against LLVM
   IR rather than against each other's code, so the two halves progress in parallel.
2. **Free optimization.** Constant folding, dead code elimination, common
   subexpression elimination and loop simplification come from LLVM's existing pass
   pipeline rather than being reimplemented.
3. **Retargetability.** The same frontend can emit x86 or ARM for validation by
   swapping the backend, which gives a reference implementation to check MDT output
   against.

Point 3 is what makes correctness testing tractable: the same MatrixDSL program can
be compiled both to the host machine and to MDT, and the two results compared.

## 7. Build and dependency structure

```
                +------------------+
                |     runtime/     |   no dependencies
                +--------+---------+
                         ^
                         |
+-------------+  +-------+--------+  +------------------+
|  frontend/  |->|  compiler/llvm |->|  llvm-backend/   |
|  (no LLVM)  |  |  (needs LLVM)  |  |  (needs LLVM)    |
+-------------+  +----------------+  +------------------+
                                              |
                                              v
                                     +------------------+
                                     |  tools/mdtsim/   |  no LLVM dependency
                                     +------------------+
```

The frontend has no LLVM dependency, so Vinay and Parth can build and test their
modules without an LLVM installation. The simulator is likewise standalone. Only the
IR generator and backend require LLVM development headers.
