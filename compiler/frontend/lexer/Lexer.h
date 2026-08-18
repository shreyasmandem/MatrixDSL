//===----------------------------------------------------------------------===//
//
// MatrixDSL - Lexical Analyzer
//
// SHARED INTERFACE - FROZEN FOR REVIEW 1
// Owner: Vinay A (24BCB0131)
//
//===----------------------------------------------------------------------===//

#ifndef MATRIXDSL_LEXER_H
#define MATRIXDSL_LEXER_H

#include <string>
#include <vector>

namespace matrixdsl {

//===----------------------------------------------------------------------===//
// Tokens
//===----------------------------------------------------------------------===//

enum class TokenType {
  // Keywords
  MATRIX,
  PRINT,
  MATMUL,
  TRANSPOSE,
  RELU,

  // Literals and names
  IDENTIFIER,
  INT,     // digit sequence with no fractional part - used for dimensions
  NUMBER,  // FP32 literal - used for matrix elements

  // Operators
  ASSIGN,  // =
  PLUS,    // +
  MINUS,   // -
  STAR,    // *

  // Punctuation
  LPAREN,    // (
  RPAREN,    // )
  LBRACKET,  // [
  RBRACKET,  // ]
  COMMA,     // ,
  SEMICOLON, // ;

  // Control
  END_OF_FILE,
  INVALID
};

const char *tokenTypeToString(TokenType type);

struct Token {
  TokenType type = TokenType::INVALID;
  std::string text;
  int line = 0;
  int column = 0;

  Token() = default;
  Token(TokenType type, std::string text, int line, int column)
      : type(type), text(std::move(text)), line(line), column(column) {}

  bool is(TokenType t) const { return type == t; }
  bool isOneOf(TokenType a, TokenType b) const { return is(a) || is(b); }

  /// True for tokens that may begin a statement: `matrix`, `print`, or an
  /// identifier. Used by the parser for panic-mode error recovery.
  bool startsStatement() const;
};

//===----------------------------------------------------------------------===//
// Lexer
//===----------------------------------------------------------------------===//

/// Single-pass scanner with one character of lookahead.
///
/// Identifiers and numbers use maximal munch; keywords are recognised by table
/// lookup after an identifier has been scanned, so `matrixx` lexes as one
/// IDENTIFIER rather than as MATRIX followed by an identifier.
///
/// Comments run from `//` to end of line and produce no token.
class Lexer {
public:
  explicit Lexer(std::string source);

  /// Produce the next token. Returns END_OF_FILE indefinitely once the input
  /// is exhausted.
  Token nextToken();

  /// Tokenize the entire input, terminated by a single END_OF_FILE token.
  std::vector<Token> tokenize();

  bool hasErrors() const { return !errors_.empty(); }
  const std::vector<std::string> &errors() const { return errors_; }

private:
  char currentChar() const;
  char peekChar() const;
  void advance();
  void skipWhitespaceAndComments();

  Token scanIdentifierOrKeyword();
  Token scanNumber();
  Token scanOperatorOrPunctuation();

  void recordError(const std::string &message);

  std::string source_;
  size_t pos_ = 0;
  int line_ = 1;
  int column_ = 1;
  std::vector<std::string> errors_;
};

} // namespace matrixdsl

#endif // MATRIXDSL_LEXER_H
