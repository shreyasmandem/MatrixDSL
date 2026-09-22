//===----------------------------------------------------------------------===//
//
// MatrixDSL - Recursive-Descent Parser implementation
//
// Owner: Vinay A (24BCB0131)
//
// Review 2 deliverable, promised in docs/contribution.md's Review 2 targets
// at Review 1. One method per grammar production (docs/grammar.md), so the
// code and the grammar can be read side by side.
//
// Phase 2 additions, both confined to the productions that actually changed
// (docs/grammar.md section 2a): parseMatrixDecl() now accepts either
// `matrix` or `tensor`, with `tensor` additionally accepting a third
// bracketed dimension (the batch count); parseFunctionCall() now accepts
// `softmax` alongside matmul/transpose/relu. Every other production is
// character-for-character what it was in Phase 1.
//
//===----------------------------------------------------------------------===//

#include "Parser.h"

#include <cstdlib>
#include <sstream>

namespace matrixdsl {

namespace {

bool isNumericToken(const Token &t) {
  return t.type == TokenType::NUMBER || t.type == TokenType::INT;
}

} // namespace

//===----------------------------------------------------------------------===//
// ParseError
//===----------------------------------------------------------------------===//

std::string ParseError::format(const std::string &filename) const {
  std::ostringstream os;
  os << filename << ":" << found.line << ":" << found.column
     << ": error: " << message;
  if (!expected.empty())
    os << " (expected " << expected << ")";
  return os.str();
}

//===----------------------------------------------------------------------===//
// Token stream helpers
//===----------------------------------------------------------------------===//

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {
  // parseProgram assumes there is always a current token (END_OF_FILE at
  // minimum) - the lexer's tokenize() contract guarantees the stream always
  // ends with exactly one END_OF_FILE token, so this never runs on an empty
  // vector in practice, but the check turns a violated assumption into an
  // early, obvious failure instead of an out-of-bounds read in current().
  if (tokens_.empty())
    tokens_.emplace_back(TokenType::END_OF_FILE, "", 0, 0);
}

const Token &Parser::current() const { return tokens_[pos_]; }

const Token &Parser::peek(size_t offset) const {
  const size_t idx = pos_ + offset;
  return idx < tokens_.size() ? tokens_[idx] : tokens_.back();
}

bool Parser::check(TokenType type) const { return current().is(type); }

bool Parser::match(TokenType type) {
  if (check(type)) {
    advance();
    return true;
  }
  return false;
}

Token Parser::advance() {
  const Token t = current();
  // Never advance past END_OF_FILE - every caller can keep asking for
  // "the current token" without a separate end-of-stream check.
  if (!check(TokenType::END_OF_FILE))
    ++pos_;
  return t;
}

Token Parser::expect(TokenType type, const std::string &context) {
  if (check(type))
    return advance();
  recordError("unexpected token '" + current().text + "'", context);
  // Deliberately does NOT consume the unexpected token: returning it as-is
  // lets the caller keep building a partial tree, and leaves the token where
  // synchronize() can find a statement boundary from it later.
  return current();
}

void Parser::recordError(const std::string &message,
                         const std::string &expected) {
  errors_.push_back({message, expected, current()});
}

void Parser::synchronize() {
  // Discard tokens until one that could start a fresh statement, or until
  // input is exhausted. This is what stops a single missing semicolon from
  // cascading into a wall of unrelated errors: the parser gives up on the
  // current statement but picks up cleanly at the next one.
  while (!check(TokenType::END_OF_FILE)) {
    if (current().startsStatement())
      return;
    advance();
  }
}

//===----------------------------------------------------------------------===//
// program ::= statement* EOF
//===----------------------------------------------------------------------===//

ProgramPtr Parser::parseProgram() {
  auto program = std::make_unique<Program>();

  while (!check(TokenType::END_OF_FILE)) {
    StmtPtr stmt = parseStatement();
    if (stmt) {
      program->addStatement(std::move(stmt));
    } else {
      // parseStatement already recorded an error and left the stream
      // pointed at the offending token; synchronize() finds the next
      // recoverable position so the loop makes forward progress.
      synchronize();
    }
  }

  return program;
}

//===----------------------------------------------------------------------===//
// statement ::= matrix_decl | tensor_decl | assignment | print_stmt
//===----------------------------------------------------------------------===//

StmtPtr Parser::parseStatement() {
  if (check(TokenType::MATRIX) || check(TokenType::TENSOR))
    return parseMatrixDecl();
  if (check(TokenType::PRINT))
    return parsePrintStmt();
  if (check(TokenType::IDENTIFIER))
    return parseAssignment();

  recordError("expected a declaration, assignment or print statement",
             "'matrix', 'tensor', 'print', or an identifier");
  return nullptr;
}

//===----------------------------------------------------------------------===//
// matrix_decl ::= "matrix" IDENTIFIER "[" INT "]" "[" INT "]" ";"
// tensor_decl ::= "tensor" IDENTIFIER "[" INT "]" "[" INT "]" ("[" INT "]")? ";"
//
// One AST node, MatrixDecl, represents both surface forms - see AST.h's
// class comment. `tensor` with only two brackets is identical to `matrix`
// (batch defaults to 1); `tensor` with three brackets makes the FIRST
// bracketed value the batch count.
//===----------------------------------------------------------------------===//

StmtPtr Parser::parseMatrixDecl() {
  const Token keyword = current();
  const SourceLocation loc(keyword.line, keyword.column);
  const bool isTensor = check(TokenType::TENSOR);
  advance(); // consume 'matrix' or 'tensor'

  const std::string name = expect(TokenType::IDENTIFIER, "a declaration name").text;

  expect(TokenType::LBRACKET, "'['");
  const int dim1 = std::atoi(expect(TokenType::INT, "an integer dimension").text.c_str());
  expect(TokenType::RBRACKET, "']'");

  expect(TokenType::LBRACKET, "'['");
  const int dim2 = std::atoi(expect(TokenType::INT, "an integer dimension").text.c_str());
  expect(TokenType::RBRACKET, "']'");

  StmtPtr decl;
  if (isTensor && check(TokenType::LBRACKET)) {
    // Three dimensions given: tensor X[B][R][C] - the first is the batch.
    advance(); // '['
    const int dim3 = std::atoi(
        expect(TokenType::INT, "an integer dimension").text.c_str());
    expect(TokenType::RBRACKET, "']'");
    decl = std::make_unique<MatrixDecl>(name, /*batch=*/dim1, /*rows=*/dim2,
                                        /*cols=*/dim3, loc);
  } else {
    decl = std::make_unique<MatrixDecl>(name, /*rows=*/dim1, /*cols=*/dim2, loc);
  }

  expect(TokenType::SEMICOLON, "';'");
  return decl;
}

//===----------------------------------------------------------------------===//
// assignment ::= IDENTIFIER "=" expression ";"
//===----------------------------------------------------------------------===//

StmtPtr Parser::parseAssignment() {
  const Token nameTok = expect(TokenType::IDENTIFIER, "an identifier");
  const SourceLocation loc(nameTok.line, nameTok.column);

  expect(TokenType::ASSIGN, "'='");
  ExprPtr value = parseExpression();
  expect(TokenType::SEMICOLON, "';'");

  return std::make_unique<Assignment>(nameTok.text, std::move(value), loc);
}

//===----------------------------------------------------------------------===//
// print_stmt ::= "print" "(" IDENTIFIER ")" ";"
//===----------------------------------------------------------------------===//

StmtPtr Parser::parsePrintStmt() {
  const Token keyword = expect(TokenType::PRINT, "'print'");
  const SourceLocation loc(keyword.line, keyword.column);

  expect(TokenType::LPAREN, "'('");
  const Token nameTok = expect(TokenType::IDENTIFIER, "an identifier");
  expect(TokenType::RPAREN, "')'");
  expect(TokenType::SEMICOLON, "';'");

  return std::make_unique<PrintStmt>(nameTok.text, loc);
}

//===----------------------------------------------------------------------===//
// expression ::= term (("+" | "-") term)*
//===----------------------------------------------------------------------===//

ExprPtr Parser::parseExpression() {
  ExprPtr left = parseTerm();

  while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
    const BinaryOp op = check(TokenType::PLUS) ? BinaryOp::Add : BinaryOp::Sub;
    const Token opTok = advance();
    const SourceLocation loc(opTok.line, opTok.column);
    ExprPtr right = parseTerm();
    left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right),
                                        loc);
  }

  return left;
}

//===----------------------------------------------------------------------===//
// term ::= factor ("*" factor)*
//===----------------------------------------------------------------------===//

ExprPtr Parser::parseTerm() {
  ExprPtr left = parseFactor();

  while (check(TokenType::STAR)) {
    const Token opTok = advance();
    const SourceLocation loc(opTok.line, opTok.column);
    ExprPtr right = parseFactor();
    left = std::make_unique<BinaryExpr>(BinaryOp::Mul, std::move(left),
                                        std::move(right), loc);
  }

  return left;
}

//===----------------------------------------------------------------------===//
// factor ::= IDENTIFIER | NUMBER | function_call | matrix_literal
//          | "(" expression ")"
//===----------------------------------------------------------------------===//

ExprPtr Parser::parseFactor() {
  const Token tok = current();
  const SourceLocation loc(tok.line, tok.column);

  if (check(TokenType::IDENTIFIER)) {
    advance();
    return std::make_unique<VariableExpr>(tok.text, loc);
  }

  if (isNumericToken(tok)) {
    advance();
    return std::make_unique<NumberExpr>(std::atof(tok.text.c_str()), loc);
  }

  if (check(TokenType::MATMUL) || check(TokenType::TRANSPOSE) ||
      check(TokenType::RELU) || check(TokenType::SOFTMAX)) {
    return parseFunctionCall();
  }

  if (check(TokenType::LBRACKET))
    return parseMatrixLiteral();

  if (match(TokenType::LPAREN)) {
    ExprPtr inner = parseExpression();
    expect(TokenType::RPAREN, "')'");
    return inner;
  }

  recordError("expected an expression",
             "an identifier, a number, a function call, a matrix literal, or '('");
  // A well-formed placeholder keeps the tree walkable for any later stage
  // that runs on partially-erroneous input, rather than requiring every
  // caller of parseFactor() to handle a null ExprPtr specially.
  return std::make_unique<NumberExpr>(0.0, loc);
}

//===----------------------------------------------------------------------===//
// function_call ::= "matmul" "(" expression "," expression ")"
//                 | "transpose" "(" expression ")"
//                 | "relu" "(" expression ")"
//                 | "softmax" "(" expression ")"          -- Phase 2
//===----------------------------------------------------------------------===//

ExprPtr Parser::parseFunctionCall() {
  const Token tok = current();
  const SourceLocation loc(tok.line, tok.column);

  BuiltinFunc callee;
  int arity = 1;

  if (check(TokenType::MATMUL)) {
    callee = BuiltinFunc::MatMul;
    arity = 2;
  } else if (check(TokenType::TRANSPOSE)) {
    callee = BuiltinFunc::Transpose;
  } else if (check(TokenType::RELU)) {
    callee = BuiltinFunc::Relu;
  } else if (check(TokenType::SOFTMAX)) {
    callee = BuiltinFunc::Softmax;
  } else {
    recordError("expected a builtin function name",
               "'matmul', 'transpose', 'relu', or 'softmax'");
    return std::make_unique<NumberExpr>(0.0, loc);
  }
  advance();

  expect(TokenType::LPAREN, "'('");

  std::vector<ExprPtr> args;
  args.push_back(parseExpression());
  if (arity == 2) {
    expect(TokenType::COMMA, "','");
    args.push_back(parseExpression());
  }

  expect(TokenType::RPAREN, "')'");

  return std::make_unique<CallExpr>(callee, std::move(args), loc);
}

//===----------------------------------------------------------------------===//
// matrix_literal ::= "[" row ("," row)* "]"
// row            ::= "[" NUMBER ("," NUMBER)* "]"
//===----------------------------------------------------------------------===//

std::vector<double> Parser::parseRow() {
  std::vector<double> values;

  expect(TokenType::LBRACKET, "'['");

  if (!check(TokenType::RBRACKET)) {
    for (;;) {
      const Token tok = current();
      if (!isNumericToken(tok)) {
        recordError("expected a number in a matrix literal row", "a number");
        break;
      }
      values.push_back(std::atof(tok.text.c_str()));
      advance();

      if (!match(TokenType::COMMA))
        break;
    }
  }

  expect(TokenType::RBRACKET, "']'");
  return values;
}

ExprPtr Parser::parseMatrixLiteral() {
  const Token tok = current();
  const SourceLocation loc(tok.line, tok.column);

  expect(TokenType::LBRACKET, "'['");

  std::vector<std::vector<double>> rows;
  if (!check(TokenType::RBRACKET)) {
    rows.push_back(parseRow());
    while (match(TokenType::COMMA))
      rows.push_back(parseRow());
  }

  expect(TokenType::RBRACKET, "']'");

  // Row-length consistency is deliberately NOT checked here - see
  // MatrixLiteralExpr's class comment in AST.h. A ragged literal parses
  // successfully and is rejected by semantic analysis instead, which can
  // report it as a proper diagnostic with a source span.
  return std::make_unique<MatrixLiteralExpr>(std::move(rows), loc);
}

} // namespace matrixdsl
