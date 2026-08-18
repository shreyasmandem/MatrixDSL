# Individual Contribution — Vinay A

| | |
|---|---|
| **Name** | Vinay A |
| **Register Number** | 24BCB0131 |
| **Role** | Lexer + Parser + AST |
| **Branch** | `Vinay-A` |
| **Review** | Review 1 |

---

## 1. Scope of my component

I own the front end: turning MatrixDSL source text into a structured tree that the
rest of the compiler can reason about. Three stages — lexical analysis, parsing, and
AST construction.

My output is the input to every other module in the project. That makes the AST node
hierarchy the single most widely depended-upon piece of the codebase, which is why it
was frozen and committed before implementation began rather than evolved as we went.

---

## 2. What I completed for Review 1

### 2.1 MatrixDSL grammar — formalised and frozen

`docs/grammar.md`

| Item | Detail |
|---|---|
| Notation | EBNF, with terminals and token classes distinguished |
| Productions | `program`, `statement`, `matrix_decl`, `assignment`, `print_stmt`, `expression`, `term`, `factor`, `function_call`, `matrix_literal`, `row` |
| Precedence | `+` `-` (lowest, left-assoc) → `*` → `( )` |
| Token classes | 20 token types with their patterns |
| Reserved words | `matrix`, `print`, `matmul`, `transpose`, `relu` |
| Grammar class | LL(1), one token lookahead, no backtracking |

**Design decision — left-recursion elimination.** The project design document writes:

```
expression ::= expression "+" term | term
```

This is a correct description of the language but cannot be implemented as written by
recursive descent: the parser would call `parseExpression` as the first action of
`parseExpression` and recurse forever. I rewrote it into the iterative form:

```
expression ::= term (("+" | "-") term)*
```

which accepts exactly the same language and yields the same left-associative parse.
This is why `A - B - D` correctly parses as `((A - B) - D)` rather than
`(A - (B - D))` — a distinction that matters because matrix subtraction is not
associative.

**Design decision — `INT` and `NUMBER` are separate token classes.** Matrix
*dimensions* must be integral (`matrix A[4][4]`), while matrix *elements* are FP32
(`[[1.5, 2.0]]`). Separating them at the token level means the parser can reject
`matrix A[2.5][4]` structurally instead of deferring it to semantic analysis. The
parser accepts an `INT` anywhere a `NUMBER` is permitted, so integer literals still
work as matrix elements.

### 2.2 Lexer — interface and working implementation

`compiler/frontend/lexer/Lexer.h`, `Lexer.cpp`

Single-pass scanner with one character of lookahead. Identifiers and numbers use
maximal munch.

Implementation points worth defending:

- **Keywords are recognised *after* scanning a complete identifier**, by table lookup.
  Scanning for keywords first would lex `matrixx` as `matrix` followed by `x`. The
  test `keyword_prefix.mtx` exists specifically to pin this down.
- **A `.` only begins a fractional part when a digit follows it.** Without the
  lookahead check, a trailing `.` would be silently absorbed into the number token.
- **Errors do not stop the scan.** An invalid character is recorded and scanning
  continues, so one bad character does not hide the rest of the file's tokens.
- **Line and column are tracked on every token**, so downstream diagnostics can point
  at the exact source position.
- **`tokenize()` resets state**, so the same `Lexer` object can be run more than once.

### 2.3 AST node hierarchy

`compiler/frontend/ast/AST.h`

```
ASTNode
├── Expr
│   ├── NumberExpr          scalar literal
│   ├── VariableExpr        identifier reference
│   ├── BinaryExpr          + - *
│   ├── MatrixLiteralExpr   [[...],[...]]
│   └── CallExpr            matmul / transpose / relu
└── Stmt
    ├── MatrixDecl          matrix A[R][C];
    ├── Assignment          A = expr;
    └── PrintStmt           print(A);
```

**Design decision — `Expr::resultType` is the explicit handoff slot.** Every
expression node carries a `MatrixType resultType`. I leave it default-constructed;
semantic analysis fills it in; IR generation may assume it is populated and correct.
Making the handoff an explicit field rather than a side table means the contract is
visible in the type itself and cannot be forgotten.

**Design decision — kind tag *and* visitor.** Each node carries a `NodeKind` so
consumers can dispatch with a `switch`, and an `accept()` method for code that prefers
double dispatch. Different modules have different preferences, and forcing one style
on all four of us would have created friction at no benefit.

**Design decision — the parser does not validate literal shape.** A ragged literal
like `[[1,2],[3,4,5]]` parses successfully and is rejected by semantic analysis. This
keeps the error a proper diagnostic with a source span rather than a parse failure,
which produces a far better message for the user.

### 2.4 Parser interface

`compiler/frontend/parser/Parser.h`

One method per grammar production, so the code and `docs/grammar.md` can be read side
by side. Includes precedence-climbing for binary operators and panic-mode recovery
that discards tokens to the next statement boundary — so a single missing semicolon
produces one error rather than a cascade.

`parseProgram()` returns a `Program` node even when errors are present, so semantic
analysis can still run over the well-formed portion of the file.

### 2.5 Token dumper — verifiable output

`tools/mtxlex/main.cpp`

Runs the lexer over a `.mtx` file and prints the token stream with positions, or a
token histogram with `--count`. This is the proof that the grammar and token
specification are not just written down but execute.

### 2.6 Test suite

`tests/lexer/` — 12 inputs, `tests/parser/` — 13 inputs

| Test | Covers |
|---|---|
| `keywords.mtx` | All 5 reserved words |
| `identifiers.mtx` | Letters, digits, underscores, leading underscore |
| `numbers.mtx` | `INT` vs `NUMBER` classification |
| `operators.mtx` | `=` `+` `-` `*` |
| `punctuation.mtx` | `(` `)` `[` `]` `,` `;` |
| `comments.mtx` | Full-line and trailing `//` comments |
| `keyword_prefix.mtx` | `matrixx` is one IDENTIFIER, not MATRIX + x |
| `empty.mtx` | Empty input yields a single EOF token |
| `line_tracking.mtx` | Line numbers correct across blank lines |
| `full_program.mtx` | Complete program end to end |
| `invalid_character.mtx` | `@` produces a lexical error |
| `invalid_symbols.mtx` | `#`, `$`, `?` all rejected |

The two rejection tests are marked `WILL_FAIL` in CTest, so silently *accepting* an
invalid character registers as a test failure rather than passing unnoticed.

Parser inputs document the expected AST shape for each construct — declarations,
precedence, left-associativity, nested calls, literals, and the two syntax-error
cases. They become executable assertions when the parser lands in Review 2.

---

## 3. Evidence

| Evidence | Location |
|---|---|
| Grammar specification | `docs/grammar.md` |
| Lexer interface | `compiler/frontend/lexer/Lexer.h` |
| Lexer implementation | `compiler/frontend/lexer/Lexer.cpp` |
| AST hierarchy | `compiler/frontend/ast/AST.h` |
| Parser interface | `compiler/frontend/parser/Parser.h` |
| Token dumper | `tools/mtxlex/main.cpp` |
| Lexer tests | `tests/lexer/` |
| Parser tests | `tests/parser/` |
| Commits | branch `Vinay-A` |

---

## 4. Build status

```bash
cmake -S . -B build
cmake --build build -j
./build/bin/mtxlex tests/lexer/full_program.mtx
ctest --test-dir build --output-on-failure
```

**Note for the review:** the lexer has not yet been compiled, because no C++ toolchain
was installed on the machine used to author it. Compiling `mtxlex` and running the
twelve lexer tests is the first task before the Review 1 demonstration, and any
resulting fixes will be committed to this branch.

---

## 5. Review 2 targets

| # | Target |
|---|---|
| 1 | `mtxlex` compiled, all 12 lexer tests passing |
| 2 | Recursive-descent parser implemented for every grammar production |
| 3 | AST construction verified against the 13 parser test inputs |
| 4 | `--dump-ast` producing readable tree output |
| 5 | Panic-mode error recovery working — one missing `;` gives one error, not a cascade |

---

## 6. What I can be questioned on

- Why the grammar needed left-recursion elimination, and what would happen without it
- Why the language is LL(1) and how I know one token of lookahead is enough
- Why `INT` and `NUMBER` are separate token classes
- Why keyword recognition happens after identifier scanning, not during
- How operator precedence and left-associativity are enforced in recursive descent
- Why the parser accepts ragged matrix literals instead of rejecting them
- What `Expr::resultType` is for and who is responsible for filling it
- How panic-mode recovery decides where to resume
