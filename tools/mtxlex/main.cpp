//===----------------------------------------------------------------------===//
//
// mtxlex - MatrixDSL token dumper
//
// Owner: Vinay A (24BCB0131)
//
// Runs the lexer over a .mtx file and prints the token stream. This is the
// verifiable output for the frontend at Review 1: the grammar and token
// specification are not just written down, they execute.
//
// Usage:
//   mtxlex <file.mtx> [options]
//
// Options:
//   --count      print only a token count summary
//   --no-pos     omit line:column information
//   --quiet      suppress the banner
//
//===----------------------------------------------------------------------===//

#include "../../compiler/frontend/lexer/Lexer.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

namespace {

void printUsage(const char *program) {
  std::printf("mtxlex - MatrixDSL token dumper\n\n");
  std::printf("Usage: %s <file.mtx> [options]\n\n", program);
  std::printf("Options:\n");
  std::printf("  --count      print only a token count summary\n");
  std::printf("  --no-pos     omit line:column information\n");
  std::printf("  --quiet      suppress the banner\n");
  std::printf("  -h, --help   this message\n");
}

bool readFile(const std::string &path, std::string &out) {
  std::ifstream in(path);
  if (!in)
    return false;
  std::ostringstream ss;
  ss << in.rdbuf();
  out = ss.str();
  return true;
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    printUsage(argv[0]);
    return 1;
  }

  std::string inputPath;
  bool countOnly = false;
  bool showPosition = true;
  bool quiet = false;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      printUsage(argv[0]);
      return 0;
    } else if (arg == "--count") {
      countOnly = true;
    } else if (arg == "--no-pos") {
      showPosition = false;
    } else if (arg == "--quiet") {
      quiet = true;
    } else if (!arg.empty() && arg[0] == '-') {
      std::fprintf(stderr, "mtxlex: unknown option '%s'\n", arg.c_str());
      return 1;
    } else {
      inputPath = arg;
    }
  }

  if (inputPath.empty()) {
    std::fprintf(stderr, "mtxlex: no input file\n");
    return 1;
  }

  std::string source;
  if (!readFile(inputPath, source)) {
    std::fprintf(stderr, "mtxlex: cannot open '%s'\n", inputPath.c_str());
    return 1;
  }

  if (!quiet)
    std::printf("MatrixDSL Lexer v1.0 - %s\n\n", inputPath.c_str());

  matrixdsl::Lexer lexer(source);
  const std::vector<matrixdsl::Token> tokens = lexer.tokenize();

  if (countOnly) {
    std::map<std::string, int> histogram;
    for (const matrixdsl::Token &t : tokens)
      ++histogram[matrixdsl::tokenTypeToString(t.type)];

    for (const auto &entry : histogram)
      std::printf("  %-12s %4d\n", entry.first.c_str(), entry.second);
    std::printf("  %-12s %4zu\n", "TOTAL", tokens.size());
  } else {
    for (const matrixdsl::Token &t : tokens) {
      if (t.is(matrixdsl::TokenType::END_OF_FILE)) {
        std::printf("  %-12s\n", "EOF");
        continue;
      }

      if (showPosition)
        std::printf("  %3d:%-3d  %-12s %s\n", t.line, t.column,
                    matrixdsl::tokenTypeToString(t.type), t.text.c_str());
      else
        std::printf("  %-12s %s\n", matrixdsl::tokenTypeToString(t.type),
                    t.text.c_str());
    }
  }

  //=== Diagnostics ===//
  //
  // An invalid character is a lexical error. It is reported and the scan
  // continues, so one bad character does not hide the rest of the file.

  if (lexer.hasErrors()) {
    std::fprintf(stderr, "\n%zu lexical error(s):\n", lexer.errors().size());
    for (const std::string &e : lexer.errors())
      std::fprintf(stderr, "  %s:%s\n", inputPath.c_str(), e.c_str());
    return 1;
  }

  if (!quiet)
    std::printf("\n%zu tokens, no errors.\n", tokens.size());

  return 0;
}
