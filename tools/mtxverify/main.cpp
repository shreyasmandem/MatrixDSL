//===----------------------------------------------------------------------===//
//
// mtxverify - differential correctness testing (Phase 3 / Review 3)
//
// Owner: Mudpe Parth Tulsidas (24BDS0353)
//
// The semantic analyser proves a program is well-shaped; mtxverify proves
// the compiled program computes the right *values*. For every .mtx file:
//
//   * a file whose first line contains `expect: error` must be rejected at
//     compile time, and every `// expect-error: <text>` line must appear in
//     the diagnostics (negative tests);
//   * otherwise the program is compiled under all four optimisation
//     configurations, each run on the MDT simulator, and every print is
//     compared with the reference interpreter's output on the unoptimised
//     Tensor IR (positive tests).
//
// Usage: mtxverify [--quiet] <file.mtx | directory> ...
// Exit status: 0 if every check passes, 1 otherwise.
//
//===----------------------------------------------------------------------===//

#include "driver/Pipeline.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

using namespace matrixdsl;
namespace fs = std::filesystem;

namespace {

std::vector<std::string> collect(const std::vector<std::string> &args) {
  std::vector<std::string> files;
  for (const std::string &a : args) {
    if (fs::is_directory(a)) {
      for (const auto &e : fs::recursive_directory_iterator(a))
        if (e.path().extension() == ".mtx")
          files.push_back(e.path().generic_string());
    } else {
      files.push_back(a);
    }
  }
  std::sort(files.begin(), files.end());
  return files;
}

/// Negative test: returns "" on success, else the reason it failed.
std::string checkRejected(const std::string &file, const std::string &source) {
  PipelineOptions opts;
  opts.filename = file;
  Compilation c = compile(source, opts);
  if (c.ok)
    return "compiled, but an error was expected";
  std::istringstream in(source);
  std::string line;
  while (std::getline(in, line)) {
    const std::string tag = "expect-error:";
    const size_t p = line.find(tag);
    if (p == std::string::npos)
      continue;
    std::string want = line.substr(p + tag.size());
    want.erase(0, want.find_first_not_of(' '));
    while (!want.empty() && (want.back() == '\r' || want.back() == ' '))
      want.pop_back();
    if (!want.empty() && c.diagnostics.find(want) == std::string::npos)
      return "diagnostics do not mention \"" + want + "\"";
  }
  return "";
}

} // namespace

int main(int argc, char **argv) {
  bool quiet = false;
  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--quiet")
      quiet = true;
    else
      args.push_back(argv[i]);
  }
  if (args.empty()) {
    std::printf("Usage: mtxverify [--quiet] <file.mtx | directory> ...\n");
    return 1;
  }

  int passed = 0, failed = 0;
  std::printf("%-44s %-9s %-9s %-9s %-14s\n", "program", "baseline", "fusion",
              "tiling", "fusion+tiling");
  for (const std::string &file : collect(args)) {
    std::string source;
    if (!readFile(file, source)) {
      std::printf("%-44s cannot read\n", file.c_str());
      ++failed;
      continue;
    }
    const std::string name = fs::path(file).filename().string();
    const std::string firstLine = source.substr(0, source.find('\n'));

    if (firstLine.find("expect: error") != std::string::npos) {
      const std::string why = checkRejected(file, source);
      std::printf("%-44s %s\n", name.c_str(),
                  why.empty() ? "rejected as expected            PASS"
                              : ("FAIL: " + why).c_str());
      why.empty() ? ++passed : ++failed;
      continue;
    }

    std::vector<tensorir::PrintValue> expected;
    std::string error;
    if (!referenceOutput(source, expected, error)) {
      std::printf("%-44s FAIL: reference: %s\n", name.c_str(), error.c_str());
      ++failed;
      continue;
    }

    std::string line = name, failure;
    line.resize(45, ' ');
    for (const Config &cfg : kConfigs) {
      PipelineOptions opts;
      opts.filename = file;
      opts.fusion = cfg.fusion;
      opts.tiling = cfg.tiling;
      Compilation c = compile(source, opts);
      std::string verdict = "ok";
      if (!c.ok) {
        verdict = "COMPILE";
        failure = c.diagnostics;
      } else {
        Execution x = simulate(c.assembly, false);
        std::string report;
        if (!x.ok) {
          verdict = "RUN";
          failure = x.error;
        } else if (!tensorir::samePrints(expected, x.prints, 1e-5, report)) {
          verdict = "WRONG";
          failure = std::string(cfg.name) + ": " + report;
        }
      }
      std::string cell = verdict;
      cell.resize(cfg.fusion && cfg.tiling ? 14 : 10, ' ');
      line += cell;
    }
    const bool ok = failure.empty();
    std::printf("%s%s\n", line.c_str(), ok ? "PASS" : "FAIL");
    if (!ok && !quiet)
      std::printf("    %s\n", failure.c_str());
    ok ? ++passed : ++failed;
  }

  std::printf("\n%d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
