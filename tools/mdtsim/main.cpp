//===----------------------------------------------------------------------===//
//
// mdtsim - MDT Instruction Set Simulator, command-line driver
//
// Owner: Shreyas Mandem (24BCE0381)
//
// Usage:
//   mdtsim <program.s> [options]
//
// Options:
//   --trace              print each instruction as it executes
//   --dump-scalar        dump R0-R15 on completion
//   --dump-vector        dump V0-V7 on completion
//   --dump-matrix [N]    dump M0-M7, or just MN
//   --dump-all           dump every register file
//   --perf               print the Phase 2 performance model report
//                        (docs/memory-hierarchy.md section 4)
//   --max-instr N        instruction limit (default 10000000)
//   --quiet              suppress the banner
//
//===----------------------------------------------------------------------===//

#include "MDTSim.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

void printUsage(const char *program) {
  std::printf("mdtsim - MDT Instruction Set Simulator\n\n");
  std::printf("Usage: %s <program.s> [options]\n\n", program);
  std::printf("Options:\n");
  std::printf("  --trace              print each instruction as it runs\n");
  std::printf("  --dump-scalar        dump R0-R15 on completion\n");
  std::printf("  --dump-vector        dump V0-V7 on completion\n");
  std::printf("  --dump-matrix [N]    dump all matrix registers, or just MN\n");
  std::printf("  --dump-all           dump every register file\n");
  std::printf("  --perf               print the performance model report\n");
  std::printf("  --max-instr N        instruction limit\n");
  std::printf("  --quiet              suppress the banner\n");
  std::printf("  -h, --help           this message\n");
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
  mdt::SimulatorOptions options;
  bool quiet = false;
  bool dumpAll = false;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];

    if (arg == "-h" || arg == "--help") {
      printUsage(argv[0]);
      return 0;
    } else if (arg == "--trace") {
      options.trace = true;
    } else if (arg == "--quiet") {
      quiet = true;
    } else if (arg == "--dump-scalar") {
      options.dumpScalar = true;
    } else if (arg == "--dump-vector") {
      options.dumpVector = true;
    } else if (arg == "--dump-all") {
      dumpAll = true;
    } else if (arg == "--perf") {
      options.perf = true;
    } else if (arg == "--dump-matrix") {
      options.dumpMatrix = true;
      // An optional register operand may follow, e.g. "--dump-matrix M0".
      if (i + 1 < argc && argv[i + 1][0] != '-') {
        const char *next = argv[++i];
        const char *digits = (next[0] == 'M' || next[0] == 'm') ? next + 1 : next;
        options.dumpMatrixIndex = std::atoi(digits);
      }
    } else if (arg == "--max-instr") {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "mdtsim: --max-instr requires a value\n");
        return 1;
      }
      options.maxInstructions =
          static_cast<size_t>(std::strtoull(argv[++i], nullptr, 10));
    } else if (!arg.empty() && arg[0] == '-') {
      std::fprintf(stderr, "mdtsim: unknown option '%s'\n", arg.c_str());
      return 1;
    } else {
      inputPath = arg;
    }
  }

  if (inputPath.empty()) {
    std::fprintf(stderr, "mdtsim: no input file\n");
    return 1;
  }

  std::string source;
  if (!readFile(inputPath, source)) {
    std::fprintf(stderr, "mdtsim: cannot open '%s'\n", inputPath.c_str());
    return 1;
  }

  if (!quiet)
    std::printf("MDT Simulator v1.0\n");

  //=== Assemble ===//

  mdt::Assembler assembler;
  if (!assembler.assemble(source)) {
    std::fprintf(stderr, "\nAssembly failed:\n\n");
    for (const mdt::AssemblyError &e : assembler.errors()) {
      std::fprintf(stderr, "  %s:%d: error: %s\n", inputPath.c_str(), e.line,
                   e.message.c_str());
      if (!e.text.empty())
        std::fprintf(stderr, "      %s\n", e.text.c_str());
    }
    return 1;
  }

  const auto &code = assembler.instructions();
  if (!quiet)
    std::printf("Loaded %zu instructions.\n\n", code.size());

  //=== Execute ===//

  mdt::Simulator sim(options);
  sim.load(code, assembler.dataSection());

  if (options.trace)
    std::printf("Trace:\n");

  const bool ok = sim.run();

  if (!ok) {
    std::fprintf(stderr, "\nRuntime error:\n");
    for (const mdt::RuntimeError &e : sim.errors()) {
      std::fprintf(stderr, "  pc=%zu", e.pc);
      if (e.sourceLine > 0)
        std::fprintf(stderr, " (%s:%d)", inputPath.c_str(), e.sourceLine);
      std::fprintf(stderr, ": %s\n", e.message.c_str());
    }
    return 1;
  }

  //=== Report ===//

  if (dumpAll) {
    sim.dumpScalarRegisters();
    std::printf("\n");
    sim.dumpVectorRegisters();
    std::printf("\n");
    sim.dumpAllMatrixRegisters();
  } else {
    if (options.dumpScalar) {
      sim.dumpScalarRegisters();
      std::printf("\n");
    }
    if (options.dumpVector) {
      sim.dumpVectorRegisters();
      std::printf("\n");
    }
    if (options.dumpMatrix) {
      if (options.dumpMatrixIndex >= 0)
        sim.dumpMatrixRegister(options.dumpMatrixIndex);
      else
        sim.dumpAllMatrixRegisters();
    }
  }

  if (options.perf) {
    std::printf("\n%s", sim.performanceModel().summary().c_str());
  }

  if (!quiet)
    std::printf("\nExecuted %zu instructions.\n", sim.executedCount());

  return 0;
}
