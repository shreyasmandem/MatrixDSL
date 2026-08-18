# MatrixDSL Grammar

Formal grammar for MatrixDSL, expressed in EBNF. The grammar is deliberately small
and free of left recursion in its implemented form so that a hand-written
recursive-descent parser is practical.

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
matrix   print   matmul   transpose   relu
```

Reserved words may not be used as identifiers.

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

`statement` is distinguishable on its first token alone: `matrix` starts a
declaration, `print` starts a print statement, and `IDENTIFIER` starts an
assignment. No backtracking is required anywhere in the grammar.
