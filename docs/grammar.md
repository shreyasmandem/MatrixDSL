# MatrixDSL Grammar

Formal grammar for MatrixDSL, expressed in EBNF. The grammar is deliberately small
and free of left recursion in its implemented form so that a hand-written
recursive-descent parser is practical.

**Phase 2 status: frozen for Review 2.** Section 2 is the Phase 1 grammar,
unchanged. Section 2a adds exactly the two Phase 2 productions —
`tensor_decl` (batched declarations) and the `softmax` builtin — per
`docs/review2/Review2_Report.md` section 2. Nothing else in the grammar
changes: `matrix_decl`, `assignment`, `print_stmt`, `expression`, `term` and
`factor` all keep their Phase 1 form exactly.

## 1. Notation

| Symbol | Meaning |
|---|---|
| `X*` | zero or more repetitions of X |
| `X?` | X is optional |
| <code>X &#124; Y</code> | X or Y |
| `"literal"` | terminal token |
| `UPPERCASE` | terminal token class produced by the lexer |

## 2. Grammar

```ebnf
program        ::= statement* EOF

statement      ::= matrix_decl
                 | assignment
                 | print_stmt

matrix_decl    ::= "matrix" IDENTIFIER "[" INT "]" "[" INT "]" ";"

assignment     ::= IDENTIFIER "=" expression ";"

print_stmt     ::= "print" "(" IDENTIFIER ")" ";"

expression     ::= term (("+" | "-") term)*

term           ::= factor ("*" factor)*

factor         ::= IDENTIFIER
                 | NUMBER
                 | function_call
                 | matrix_literal
                 | "(" expression ")"

function_call  ::= "matmul"     "(" expression "," expression ")"
                 | "transpose"  "(" expression ")"
                 | "relu"       "(" expression ")"

matrix_literal ::= "[" row ("," row)* "]"

row            ::= "[" NUMBER ("," NUMBER)* "]"
```

## 2a. Phase 2 additions

```ebnf
statement      ::= matrix_decl
                 | tensor_decl        (* NEW *)
                 | assignment
                 | print_stmt

tensor_decl    ::= "tensor" IDENTIFIER "[" INT "]" "[" INT "]" ("[" INT "]")? ";"

function_call  ::= "matmul"     "(" expression "," expression ")"
                 | "transpose"  "(" expression ")"
                 | "relu"       "(" expression ")"
                 | "softmax"    "(" expression ")"        (* NEW *)
```

`tensor_decl` accepts either two or three bracketed dimensions. With two, it is
exactly `matrix_decl`'s shape (batch defaults to 1) — `tensor W[4][4];` and
`matrix W[4][4];` declare the identical type, and both keywords remain valid so
that every Phase 1 program still parses without modification. With three, the
**first** bracketed dimension is the batch count: `tensor X[8][4][4];` declares
a batch of 8 independent 4x4 matrices.

Both `matrix_decl` and `tensor_decl` construct the same AST node,
`MatrixDecl` (`compiler/frontend/ast/AST.h`), which now carries a `batch`
field (default 1). Nothing downstream of parsing needs to know which keyword
the source used, only the resulting `(batch, rows, cols)` triple.

`softmax` follows the identical one-argument shape as `transpose` and `relu`:
it is shape-preserving at the grammar and AST level. Its row-max/subtract/
exp/row-sum/divide decomposition happens later, in the Tensor IR
canonicalisation step (`docs/tensor-ir.md` section 4) — the parser and AST
never see that structure.

**Explicitly out of scope for Phase 2** (`docs/review2/Review2_Report.md`
section 1.4 and section 2.3): batched matrix literals (a `tensor` value is
populated by assignment from a batch-producing expression, not a literal),
general N-dimensional shapes beyond the one batch dimension, and dynamic
(runtime-determined) dimensions anywhere in `tensor_decl`.

### Note on left recursion

Section 12 of the design document presents `expression` and `term` using left
recursion:

```
expression ::= expression "+" term | term
```

This is correct as a description of the language but cannot be implemented directly
by recursive descent, because it recurses infinitely on the leftmost symbol. The
grammar above is the standard left-recursion-eliminated form using iteration. It
accepts exactly the same language and yields the same left-associative parse.

## 3. Operator precedence and associativity

From lowest to highest binding:

| Level | Operators | Associativity | Meaning |
|---|---|---|---|
| 1 | `+` `-` | left | matrix addition / subtraction |
| 2 | `*` | left | scalar-matrix, or matrix-matrix product |
| 3 | `( )` | n/a | grouping |

`*` between two matrices is accepted by the grammar and resolved during semantic
analysis. `matmul(A, B)` is the preferred explicit spelling; `A * B` where both
operands are matrices is treated as an alias for it. Where one operand is a scalar,
`*` is element-wise scalar multiplication.

## 4. Token classes

| Token | Pattern | Example |
|---|---|---|
| `IDENTIFIER` | `[A-Za-z_][A-Za-z0-9_]*` | `A`, `result_1` |
| `INT` | `[0-9]+` | `4`, `128` |
| `NUMBER` | `[0-9]+ ("." [0-9]+)?` | `1`, `2.5` |
| `MATRIX` | keyword `matrix` | |
| `PRINT` | keyword `print` | |
| `MATMUL` | keyword `matmul` | |
| `TRANSPOSE` | keyword `transpose` | |
| `RELU` | keyword `relu` | |
| `ASSIGN` | `=` | |
| `PLUS` `MINUS` `STAR` | `+` `-` `*` | |
| `LPAREN` `RPAREN` | `(` `)` | |
| `LBRACKET` `RBRACKET` | `[` `]` | |
| `COMMA` `SEMICOLON` | `,` `;` | |
| `EOF` | end of input | |

Whitespace and newlines are insignificant apart from separating tokens.
Comments run from `//` to end of line.

`INT` is a distinct token class from `NUMBER` because matrix *dimensions* must be
integral, while matrix *elements* are FP32. The lexer emits `INT` for a digit
sequence with no fractional part; the parser accepts an `INT` anywhere a `NUMBER`
is permitted.

## 5. Reserved words

```
matrix   tensor   print   matmul   transpose   relu   softmax
```

Reserved words may not be used as identifiers. `tensor` and `softmax` are the
two Phase 2 additions (section 2a).

## 6. Worked derivation

Input:

```
C = matmul(A, B);
```

Derivation:

```
program
└── statement
    └── assignment
        ├── IDENTIFIER(C)
        ├── ASSIGN
        └── expression
            └── term
                └── factor
                    └── function_call
                        ├── MATMUL
                        ├── expression -> term -> factor -> IDENTIFIER(A)
                        └── expression -> term -> factor -> IDENTIFIER(B)
```

Resulting AST:

```
Assignment
├── target: VariableExpr(C)
└── value:  CallExpr(matmul)
             ├── VariableExpr(A)
             └── VariableExpr(B)
```

## 7. Grammar properties

| Property | Value |
|---|---|
| Class | LL(1) after left-recursion elimination |
| Lookahead required | 1 token |
| Parsing method | Recursive descent |
| Ambiguity | None |
| Backtracking | Not required |

`statement` is distinguishable on its first token alone: `matrix` or `tensor`
starts a declaration, `print` starts a print statement, and `IDENTIFIER` starts
an assignment. Phase 2's `tensor_decl` needed one additional lookahead
decision internally — two brackets versus three — but that decision is made
*after* the keyword has already selected the production, so the grammar
remains LL(1) with the Phase 2 additions included, not just the Phase 1
core. No backtracking is required anywhere in the grammar.
