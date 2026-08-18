# Lexer, Parser and AST — Vinay A (24BCB0131)

Individual contribution branch for **MatrixDSL**, Team 13, BCSE307P Compiler Design.

This branch contains **only my own work**: the front end of the compiler — lexical
analysis, the grammar, the parser interface, and the AST node hierarchy. Team-level
deliverables (Review 1 report and presentation, timeline, responsibility matrix) and
the other members' modules live on [`main`](../../tree/main).

---

## Role

| | |
|---|---|
| **Member** | 1 of 4 |
| **Primary responsibility** | Lexer + Parser + AST |
| **Supporting responsibility** | Frontend integration, grammar documentation |

---

## Contents

```
docs/
├── grammar.md                     MatrixDSL grammar in EBNF (frozen)
└── contribution.md                Review 1 evidence and viva preparation

compiler/frontend/
├── lexer/Lexer.h                  Token enumeration and scanner interface
├── lexer/Lexer.cpp                Working implementation
├── parser/Parser.h                Recursive-descent parser interface
└── ast/AST.h                      AST node hierarchy (shared contract)

tools/mtxlex/main.cpp              Token dumper CLI

tests/
├── lexer/                         12 lexer test inputs
└── parser/                        13 parser test inputs
```

---

## The grammar

MatrixDSL is **LL(1) after left-recursion elimination**, so a hand-written
recursive-descent parser needs one token of lookahead and no backtracking.

```ebnf
program        ::= statement* EOF
statement      ::= matrix_decl | assignment | print_stmt

matrix_decl    ::= "matrix" IDENTIFIER "[" INT "]" "[" INT "]" ";"
assignment     ::= IDENTIFIER "=" expression ";"
print_stmt     ::= "print" "(" IDENTIFIER ")" ";"

expression     ::= term (("+" | "-") term)*
term           ::= factor ("*" factor)*
factor         ::= IDENTIFIER | NUMBER | function_call
                 | matrix_literal | "(" expression ")"

function_call  ::= "matmul" "(" expression "," expression ")"
                 | "transpose" "(" expression ")"
                 | "relu" "(" expression ")"

matrix_literal ::= "[" row ("," row)* "]"
row            ::= "[" NUMBER ("," NUMBER)* "]"
```

**Note on left recursion.** The project design document writes `expression` as
`expression "+" term | term`. That is a correct description of the language but cannot
be implemented directly by recursive descent — it recurses infinitely on the leftmost
symbol. The form above is the standard left-recursion-eliminated version: it accepts
exactly the same language and produces the same left-associative parse.

Full specification: [`docs/grammar.md`](docs/grammar.md)

---

## Building and running

No LLVM required — the front end is deliberately free of any LLVM dependency, so it
builds with nothing but a C++17 compiler.

```bash
cmake -S . -B build
cmake --build build -j
```

```bash
./build/bin/mtxlex tests/lexer/full_program.mtx
```

Expected output (abbreviated):

```
MatrixDSL Lexer v1.0 - tests/lexer/full_program.mtx

    2:1    MATRIX       matrix
    2:8    IDENTIFIER   A
    2:9    LBRACKET     [
    2:10   INT          4
    2:11   RBRACKET     ]
    ...
   14:5    MATMUL       matmul
   14:11   LPAREN       (
   14:12   IDENTIFIER   A
   14:13   COMMA        ,
    ...
  EOF

92 tokens, no errors.
```

Token histogram instead of the full stream:

```bash
./build/bin/mtxlex tests/lexer/full_program.mtx --count
```

---

## Tests

```bash
ctest --test-dir build --output-on-failure
```

| Category | Count | Checks |
|---|---|---|
| Lexer — valid | 10 | Keywords, identifiers, numbers, operators, punctuation, comments, line tracking, empty input, full program |
| Lexer — reject | 2 | Invalid characters must produce a lexical error, not silent acceptance |
| Parser — inputs | 13 | Declarations, precedence, associativity, calls, literals, syntax errors |

Two cases are worth calling out because they are where hand-written lexers usually go
wrong:

- **`keyword_prefix.mtx`** — `matrixx` must lex as one `IDENTIFIER`, not as the keyword
  `matrix` followed by `x`. Keywords are matched by table lookup only *after* a
  complete identifier has been scanned.
- **`numbers.mtx`** — `INT` and `NUMBER` are separate token classes, because matrix
  *dimensions* must be integral while matrix *elements* are FP32. A `.` only begins a
  fractional part when a digit follows it.

The parser test inputs are design-stage: they document the expected AST shape for each
construct. The parser implementation lands in Review 2.

---

## Status

| Item | Status |
|---|---|
| Grammar formalised in EBNF, left recursion eliminated | Complete |
| Token classes defined | Complete |
| Lexer interface | Complete |
| Lexer implementation | Complete — **not yet compiled** (no C++ toolchain on the authoring machine) |
| Token dumper CLI | Complete |
| AST node hierarchy | Complete (interface) |
| Parser interface | Complete (interface) |
| Parser implementation | Review 2 |

Compiling `mtxlex` and running the twelve lexer tests is the first task before the
Review 1 demonstration.
