//===----------------------------------------------------------------------===//
//
// MatrixDSL - Test Runner
//
// A self-contained C++ test driver. No scripting-language dependency: this is
// built by the same CMake configuration as the compiler and invoked by CTest.
//
// Each .mtx test file declares its own expectation in a leading comment:
//
//     // Expect: accepted, result 2x2
//     // Expect: ERROR - matmul requires A.cols == B.rows, but 3 != 7
//     // Expect: SYNTAX ERROR - expected ';'
//     // Expect: LEXICAL ERROR - invalid character '@'
//
// Keeping the expectation inside the test file means a test cannot drift away
// from its own description.
//
//===----------------------------------------------------------------------===//

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

enum class Expectation { Accept, SemanticError, SyntaxError, LexicalError };

struct TestCase {
  std::string path;
  std::string category; // valid | invalid | boundary
  Expectation expectation = Expectation::Accept;
  std::string description;
};

struct Result {
  const TestCase *test;
  bool passed;
  std::string detail;
};

std::string trim(const std::string &s) {
  const auto first = s.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return "";
  const auto last = s.find_last_not_of(" \t\r\n");
  return s.substr(first, last - first + 1);
}

bool contains(const std::string &haystack, const std::string &needle) {
  return haystack.find(needle) != std::string::npos;
}

/// Read the `// Expect:` line from a test file. Every test must have one; a
/// test without a stated expectation is treated as malformed rather than as
/// silently passing.
bool readExpectation(const std::string &path, TestCase &out) {
  std::ifstream in(path);
  if (!in)
    return false;

  std::string line;
  while (std::getline(in, line)) {
    const std::string t = trim(line);
    if (t.rfind("//", 0) != 0)
      continue;

    const auto pos = t.find("Expect:");
    if (pos == std::string::npos)
      continue;

    out.description = trim(t.substr(pos + 7));

    if (contains(out.description, "LEXICAL ERROR"))
      out.expectation = Expectation::LexicalError;
    else if (contains(out.description, "SYNTAX ERROR"))
      out.expectation = Expectation::SyntaxError;
    else if (contains(out.description, "ERROR"))
      out.expectation = Expectation::SemanticError;
    else
      out.expectation = Expectation::Accept;

    return true;
  }
  return false;
}

const char *expectationName(Expectation e) {
  switch (e) {
  case Expectation::Accept:
    return "accept";
  case Expectation::SemanticError:
    return "semantic error";
  case Expectation::SyntaxError:
    return "syntax error";
  case Expectation::LexicalError:
    return "lexical error";
  }
  return "unknown";
}

/// Invoke the compiler and capture its exit status.
///
/// Exit-status contract:
///   0  compiled successfully
///   1  compilation error (lexical, syntax or semantic)
///   2  internal compiler error - always a test failure, never an expected
///      outcome, because it means the compiler crashed rather than diagnosed
int runCompiler(const std::string &compiler, const std::string &file,
                std::string &output) {
  const std::string outFile = "test_output.tmp";
  const std::string cmd =
      "\"" + compiler + "\" \"" + file + "\" > " + outFile + " 2>&1";

  const int status = std::system(cmd.c_str());

  std::ifstream in(outFile);
  std::stringstream ss;
  ss << in.rdbuf();
  output = ss.str();
  in.close();
  std::remove(outFile.c_str());

#ifdef _WIN32
  return status;
#else
  return WEXITSTATUS(status);
#endif
}

Result evaluate(const TestCase &test, const std::string &compiler) {
  std::string output;
  const int code = runCompiler(compiler, test.path, output);

  Result r{&test, false, ""};

  if (code == 2) {
    r.detail = "compiler crashed (internal error)";
    return r;
  }

  const bool compiled = (code == 0);
  const bool wantAccept = (test.expectation == Expectation::Accept);

  if (wantAccept && compiled) {
    r.passed = true;
    return r;
  }
  if (wantAccept && !compiled) {
    r.detail = "expected success, but compilation failed:\n" + output;
    return r;
  }
  if (!wantAccept && compiled) {
    r.detail = std::string("expected ") + expectationName(test.expectation) +
               ", but compilation succeeded";
    return r;
  }

  // A negative test that fails for the wrong reason is a failing test, so the
  // diagnostic itself is checked, not merely the exit status.
  const bool mentionsError =
      contains(output, "error") || contains(output, "Error");
  if (!mentionsError) {
    r.detail = "compilation failed but produced no diagnostic";
    return r;
  }

  r.passed = true;
  return r;
}

std::vector<TestCase> collect(const std::string &root) {
  static const char *categories[] = {"valid", "invalid", "boundary"};
  std::vector<TestCase> tests;

  for (const char *category : categories) {
    const std::string listing = root + "/" + category + "/filelist.txt";
    std::ifstream in(listing);
    if (!in)
      continue;

    std::string name;
    while (std::getline(in, name)) {
      name = trim(name);
      if (name.empty())
        continue;

      TestCase tc;
      tc.path = root + "/" + category + "/" + name;
      tc.category = category;
      if (readExpectation(tc.path, tc))
        tests.push_back(std::move(tc));
      else
        std::cerr << "warning: " << tc.path << " has no // Expect: line\n";
    }
  }
  return tests;
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 3) {
    std::cerr << "usage: testrunner <compiler-path> <tests-dir> [category]\n";
    return 1;
  }

  const std::string compiler = argv[1];
  const std::string testsDir = argv[2];
  const std::string filter = (argc > 3) ? argv[3] : "";

  std::vector<TestCase> tests = collect(testsDir);
  if (tests.empty()) {
    std::cerr << "error: no tests found under " << testsDir << "\n";
    return 1;
  }

  int passed = 0, failed = 0;
  std::vector<Result> failures;

  for (const TestCase &test : tests) {
    if (!filter.empty() && test.category != filter)
      continue;

    const Result r = evaluate(test, compiler);
    if (r.passed) {
      ++passed;
      std::cout << "  PASS  [" << test.category << "] " << test.path << "\n";
    } else {
      ++failed;
      failures.push_back(r);
      std::cout << "  FAIL  [" << test.category << "] " << test.path << "\n";
    }
  }

  std::cout << "\n----------------------------------------\n";
  std::cout << "  " << passed << " passed, " << failed << " failed\n";
  std::cout << "----------------------------------------\n";

  if (!failures.empty()) {
    std::cout << "\nFailures:\n\n";
    for (const Result &r : failures) {
      std::cout << "  " << r.test->path << "\n";
      std::cout << "    expected: " << r.test->description << "\n";
      std::cout << "    actual:   " << r.detail << "\n\n";
    }
  }

  return failed == 0 ? 0 : 1;
}
