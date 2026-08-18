//===----------------------------------------------------------------------===//
//
// MatrixDSL - Lexical Analyzer implementation
//
// Owner: Vinay A (24BCB0131)
//
// Single-pass scanner with one character of lookahead. Identifiers and numbers
// use maximal munch; keywords are recognised by table lookup *after* a full
// identifier has been scanned, so `matrixx` lexes as one IDENTIFIER rather
// than as MATRIX followed by `x`.
//
//===----------------------------------------------------------------------===//

#include "Lexer.h"

#include <cctype>
#include <sstream>

namespace matrixdsl {

//===----------------------------------------------------------------------===//
// Token type names
//===----------------------------------------------------------------------===//

const char *tokenTypeToString(TokenType type) {
  switch (type) {
  case TokenType::MATRIX:      return "MATRIX";
  case TokenType::PRINT:       return "PRINT";
  case TokenType::MATMUL:      return "MATMUL";
  case TokenType::TRANSPOSE:   return "TRANSPOSE";
  case TokenType::RELU:        return "RELU";
  case TokenType::IDENTIFIER:  return "IDENTIFIER";
  case TokenType::INT:         return "INT";
  case TokenType::NUMBER:      return "NUMBER";
  case TokenType::ASSIGN:      return "ASSIGN";
  case TokenType::PLUS:        return "PLUS";
  case TokenType::MINUS:       return "MINUS";
  case TokenType::STAR:        return "STAR";
  case TokenType::LPAREN:      return "LPAREN";
  case TokenType::RPAREN:      return "RPAREN";
  case TokenType::LBRACKET:    return "LBRACKET";
  case TokenType::RBRACKET:    return "RBRACKET";
  case TokenType::COMMA:       return "COMMA";
  case TokenType::SEMICOLON:   return "SEMICOLON";
  case TokenType::END_OF_FILE: return "EOF";
  case TokenType::INVALID:     return "INVALID";
  }
  return "UNKNOWN";
}

bool Token::startsStatement() const {
  return type == TokenType::MATRIX || type == TokenType::PRINT ||
         type == TokenType::IDENTIFIER;
}

//===----------------------------------------------------------------------===//
// Keyword table
//
// Kept as a lookup applied after identifier scanning. Adding a keyword is a
// one-line change here and nowhere else.
//===----------------------------------------------------------------------===//

namespace {

struct KeywordEntry {
  const char *text;
  TokenType type;
};

const KeywordEntry kKeywords[] = {
    {"matrix",    TokenType::MATRIX},
    {"print",     TokenType::PRINT},
    {"matmul",    TokenType::MATMUL},
    {"transpose", TokenType::TRANSPOSE},
    {"relu",      TokenType::RELU},
};

TokenType keywordOrIdentifier(const std::string &text) {
  for (const KeywordEntry &entry : kKeywords)
    if (text == entry.text)
      return entry.type;
  return TokenType::IDENTIFIER;
}

bool isIdentifierStart(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool isIdentifierPart(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

} // namespace

//===----------------------------------------------------------------------===//
// Construction and character access
//===----------------------------------------------------------------------===//

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

char Lexer::currentChar() const {
  return pos_ < source_.size() ? source_[pos_] : '\0';
}

char Lexer::peekChar() const {
  return pos_ + 1 < source_.size() ? source_[pos_ + 1] : '\0';
}

void Lexer::advance() {
  if (pos_ >= source_.size())
    return;

  if (source_[pos_] == '\n') {
    ++line_;
    column_ = 1;
  } else {
    ++column_;
  }
  ++pos_;
}

void Lexer::recordError(const std::string &message) {
  std::ostringstream os;
  os << line_ << ":" << column_ << ": " << message;
  errors_.push_back(os.str());
}

//===----------------------------------------------------------------------===//
// Whitespace and comments
//
// Comments run from `//` to end of line and produce no token. The loop is a
// single pass so that a comment followed by whitespace followed by another
// comment is consumed in one call.
//===----------------------------------------------------------------------===//

void Lexer::skipWhitespaceAndComments() {
  for (;;) {
    const char c = currentChar();

    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      advance();
      continue;
    }

    if (c == '/' && peekChar() == '/') {
      while (currentChar() != '\n' && currentChar() != '\0')
        advance();
      continue;
    }

    return;
  }
}

//===----------------------------------------------------------------------===//
// Scanners
//===----------------------------------------------------------------------===//

Token Lexer::scanIdentifierOrKeyword() {
  const int startLine = line_;
  const int startColumn = column_;

  std::string text;
  while (isIdentifierPart(currentChar())) {
    text += currentChar();
    advance();
  }

  return Token(keywordOrIdentifier(text), text, startLine, startColumn);
}

/// Scan a numeric literal.
///
/// INT and NUMBER are separate token classes because matrix *dimensions* must
/// be integral while matrix *elements* are FP32. A digit sequence with no
/// fractional part is emitted as INT; the parser accepts INT anywhere a NUMBER
/// is permitted.
Token Lexer::scanNumber() {
  const int startLine = line_;
  const int startColumn = column_;

  std::string text;
  bool isFloat = false;

  while (std::isdigit(static_cast<unsigned char>(currentChar()))) {
    text += currentChar();
    advance();
  }

  // A '.' only begins a fractional part if a digit follows it. Without this
  // check, a trailing '.' would be silently swallowed into the number.
  if (currentChar() == '.' &&
      std::isdigit(static_cast<unsigned char>(peekChar()))) {
    isFloat = true;
    text += currentChar();
    advance();

    while (std::isdigit(static_cast<unsigned char>(currentChar()))) {
      text += currentChar();
      advance();
    }
  }

  return Token(isFloat ? TokenType::NUMBER : TokenType::INT, text, startLine,
               startColumn);
}

Token Lexer::scanOperatorOrPunctuation() {
  const int startLine = line_;
  const int startColumn = column_;
  const char c = currentChar();

  TokenType type = TokenType::INVALID;
  switch (c) {
  case '=': type = TokenType::ASSIGN;    break;
  case '+': type = TokenType::PLUS;      break;
  case '-': type = TokenType::MINUS;     break;
  case '*': type = TokenType::STAR;      break;
  case '(': type = TokenType::LPAREN;    break;
  case ')': type = TokenType::RPAREN;    break;
  case '[': type = TokenType::LBRACKET;  break;
  case ']': type = TokenType::RBRACKET;  break;
  case ',': type = TokenType::COMMA;     break;
  case ';': type = TokenType::SEMICOLON; break;
  default:  type = TokenType::INVALID;   break;
  }

  const std::string text(1, c);
  advance();

  if (type == TokenType::INVALID)
    recordError("invalid character '" + text + "'");

  return Token(type, text, startLine, startColumn);
}

//===----------------------------------------------------------------------===//
// Main entry points
//===----------------------------------------------------------------------===//

Token Lexer::nextToken() {
  skipWhitespaceAndComments();

  if (pos_ >= source_.size())
    return Token(TokenType::END_OF_FILE, "", line_, column_);

  const char c = currentChar();

  if (isIdentifierStart(c))
    return scanIdentifierOrKeyword();

  if (std::isdigit(static_cast<unsigned char>(c)))
    return scanNumber();

  return scanOperatorOrPunctuation();
}

std::vector<Token> Lexer::tokenize() {
  // Reset so that tokenize() is callable more than once on the same Lexer.
  pos_ = 0;
  line_ = 1;
  column_ = 1;
  errors_.clear();

  std::vector<Token> tokens;
  for (;;) {
    Token token = nextToken();
    const bool done = token.is(TokenType::END_OF_FILE);
    tokens.push_back(std::move(token));
    if (done)
      break;
  }
  return tokens;
}

} // namespace matrixdsl
