//===----------------------------------------------------------------------===//
//
// matrixdslc - the MatrixDSL compiler (Phase 3 / Review 3)
//
// Owner: Vinay A (24BCB0131)
//
// Usage:
//   matrixdslc <file.mtx> [options]
//
// Output (default: MDT assembly written to <file>.s):
//   -o <path>          write the output to <path> ("-" = stdout)
//   --emit-tokens      token stream             --emit-ast   syntax tree
//   --emit-tir         Tensor IR before/after the passes
//   --emit-llvm        LLVM IR (text, @mdt.* intrinsics)
//   -S                 MDT assembly (the default)
//
// Optimisation:
//   --no-fusion        disable operator fusion
//   --no-tiling        disable scratchpad staging
//   -O0                both of the above
//
// Execution:
//   --run              assemble and run on the MDT simulator, print output
//   --perf             with --run: print the performance model
//   --verify           with --run: check the output against the reference
//                      interpreter (exit 3 on mismatch)
//   --stats            pass and code-generation statistics
//
// Exit status: 0 success, 1 compile error, 2 internal/usage error,
// 3 verification failure.
//
//===----------------------------------------------------------------------===//

#include "driver/Pipeline.h"
#include "llvm/LLVMTextEmitter.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

using namespace matrixdsl;

namespace {

void usage() {
  std::printf(
      "matrixdslc - MatrixDSL compiler for the MDT matrix ISA\n\n"
      "Usage: matrixdslc <file.mtx> [options]\n\n"
      "  -o <path>        output file ('-' for stdout; default <file>.s)\n"
      "  -S               emit MDT assembly (default)\n"
      "  --emit-tokens    emit the token stream\n"
      "  --emit-ast       emit the syntax tree\n"
      "  --emit-tir       emit Tensor IR before and after optimisation\n"
      "  --emit-llvm      emit LLVM IR (text)\n"
      "  --no-fusion      disable operator fusion\n"
      "  --no-tiling      disable scratchpad tiling\n"
      "  -O0              disable both\n"
      "  --run            run the program on the MDT simulator\n"
      "  --perf           with --run, report the performance model\n"
      "  --verify         with --run, compare against the reference interpreter\n"
      "  --stats          print optimisation statistics\n");
}

const char *onOff(bool b) { return b ? "on" : "off"; }

bool writeOut(const std::string &path, const std::string &text) {
  if (path == "-") {
    std::fwrite(text.data(), 1, text.size(), stdout);
    return true;
  }
  std::ofstream out(path, std::ios::binary);
  if (!out)
    return false;
  out << text;
  return true;
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 2 || !std::strcmp(argv[1], "-h") || !std::strcmp(argv[1], "--help")) {
    usage();
    return argc < 2 ? 2 : 0;
  }

  std::string input, output;
  enum { Asm, Tokens, Ast, Tir, Llvm } emit = Asm;
  PipelineOptions opts;
  bool run = false, perf = false, verify = false, stats = false;

  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "-o" && i + 1 < argc) output = argv[++i];
    else if (a == "-S") emit = Asm;
    else if (a == "--emit-tokens") emit = Tokens;
    else if (a == "--emit-ast") emit = Ast;
    else if (a == "--emit-tir") emit = Tir;
    else if (a == "--emit-llvm") emit = Llvm;
    else if (a == "--no-fusion") opts.fusion = false;
    else if (a == "--no-tiling") opts.tiling = false;
    else if (a == "-O0") opts.fusion = opts.tiling = false;
    else if (a == "--run") run = true;
    else if (a == "--perf") perf = true;
    else if (a == "--verify") verify = true;
    else if (a == "--stats") stats = true;
    else if (!a.empty() && a[0] == '-') {
      std::fprintf(stderr, "matrixdslc: unknown option '%s'\n", a.c_str());
      return 2;
    } else input = a;
  }
  if (input.empty()) {
    usage();
    return 2;
  }

  std::string source;
  if (!readFile(input, source)) {
    std::fprintf(stderr, "matrixdslc: cannot read '%s'\n", input.c_str());
    return 2;
  }
  opts.filename = input;

  Compilation c = compile(source, opts);
  std::fputs(c.diagnostics.c_str(), stderr);
  if (!c.ok)
    return 1;

  const std::string stem = input.substr(0, input.find_last_of('.'));
  std::string text;
  switch (emit) {
  case Tokens:
    for (const Token &t : c.tokens)
      text += std::to_string(t.line) + ":" + std::to_string(t.column) + "\t" +
              tokenTypeToString(t.type) + "\t" + t.text + "\n";
    break;
  case Ast:
    text = dumpAST(*c.ast);
    break;
  case Tir:
    text = "; Tensor IR after lowering\n" + c.irBefore +
           "\n; Tensor IR after passes (fusion " + onOff(opts.fusion) +
           ", scratchpad tiling " + onOff(opts.tiling) + ")\n" + c.irAfter;
    break;
  case Llvm:
    text = emitLLVMText(c.ir, c.inputs, input);
    break;
  case Asm:
    text = c.assembly;
    break;
  }

  if (emit != Asm || !output.empty() || !run) {
    std::string path = output;
    if (path.empty())
      path = (emit == Asm) ? stem + ".s" : (emit == Llvm ? stem + ".ll" : "-");
    if (!writeOut(path, text)) {
      std::fprintf(stderr, "matrixdslc: cannot write '%s'\n", path.c_str());
      return 2;
    }
  }

  if (stats) {
    std::printf("fusion: %s, scratchpad tiling: %s\n", onOff(opts.fusion),
                onOff(opts.tiling));
    std::printf("  ops fused            %d\n", c.fused);
    std::printf("  dead ops removed     %d\n", c.deadOps);
    std::printf("  tiled matmuls        %d\n", c.tiling.tiledMatMuls);
    for (const auto &s : c.tiling.staged)
      std::printf("  staged %s of %%%d: %d tiles, each reused %dx, saves %lld DRAM bytes\n",
                  s.operandIndex == 0 ? "A" : "B", s.consumerId, s.tiles,
                  s.usesPerTile, s.dramBytesSaved);
    for (const auto &n : c.tiling.notes)
      std::printf("  note: %s\n", n.c_str());
    std::printf("  MDT instructions     %lld (static)\n", c.codegen.instructions);
    std::printf("  data image           %lld bytes in %d buffers\n",
                c.codegen.dataBytes, c.codegen.buffers);
  }

  if (!run)
    return 0;

  Execution x = simulate(c.assembly, /*echoPrints=*/true);
  if (!x.ok) {
    std::fprintf(stderr, "matrixdslc: %s\n", x.error.c_str());
    return 2;
  }
  if (perf)
    std::printf("\n%s", x.perf.summary().c_str());

  if (verify) {
    std::vector<tensorir::PrintValue> expected;
    std::string error;
    if (!referenceOutput(source, expected, error)) {
      std::fprintf(stderr, "matrixdslc: reference: %s\n", error.c_str());
      return 2;
    }
    std::string report;
    if (!tensorir::samePrints(expected, x.prints, 1e-5, report)) {
      std::printf("\nverify: FAIL - %s\n", report.c_str());
      return 3;
    }
    std::printf("\nverify: PASS - %zu print(s) match the reference interpreter\n",
                expected.size());
  }
  return 0;
}
