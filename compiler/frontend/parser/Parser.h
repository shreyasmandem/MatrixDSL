//===----------------------------------------------------------------------===//
//
// MatrixDSL - Recursive-Descent Parser
//
// SHARED INTERFACE - FROZEN FOR REVIEW 1
// Owner: Vinay A (24BCB0131)
//
// The grammar is LL(1) after left-recursion elimination (see docs/grammar.md),
// so one token of lookahead suffices and no backtracking is required.
//
//===----------------------------------------------------------------------===//

#ifndef MATRIXDSL_PARSER_H
#define MATRIXDSL_PARSER_H

#include <string>
#include <vector>

#include "../ast/AST.h"
#include "../lexer/Lexer.h"

namespace matrixdsl {

struct ParseError {
  std::string message;
  std::string expected;
  Token found;

  std::string format(const std::string &filename) const;
};

class Parser {
public:
  explicit Parser(std::vector<Token> tokens);

  /// Parse a complete program. Returns a Program node even when errors are
  /// present, so that semantic analysis can still run on the well-formed
  /// portion. Check `hasErrors()` before trusting the result.
  ProgramPtr parseProgram();

  bool hasErrors() const { return !errors_.empty(); }
  const std::vector<ParseError> &errors() const { return errors_; }

private:
  // Grammar productions - one method per nonterminal.
  StmtPtr parseStatement();
  StmtPtr parseMatrixDecl();
  StmtPtr parseAssignment();
  StmtPtr parsePrintStmt();

  ExprPtr parseExpression();     // term (("+"|"-") term)*
  ExprPtr parseTerm();           // factor ("*" factor)*
  ExprPtr parseFactor();         // identifier | number | call | literal | ( )
  ExprPtr parseFunctionCall();   // matmul | transpose | relu
  ExprPtr parseMatrixLiteral();  // "[" row ("," row)* "]"
  std::vector<double> parseRow();

  // Token stream helpers.
  const Token &current() const;
  const Token &peek(size_t offset = 1) const;
  bool check(TokenType type) const;
  bool match(TokenType type);
  Token advance();

  /// Consume a token of the expected type, or record an error.
  Token expect(TokenType type, const std::string &context);

  void recordError(const std::string &message, const std::string &expected);

  /// Panic-mode recovery: discard tokens until the next statement boundary, so
  /// that one syntax error does not cascade into a wall of spurious errors.
  void synchronize();

  std::vector<Token> tokens_;
  size_t pos_ = 0;
  std::vector<ParseError> errors_;
};

} // namespace matrixdsl

#endif // MATRIXDSL_PARSER_H
