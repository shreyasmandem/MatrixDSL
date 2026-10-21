//===----------------------------------------------------------------------===//
//
// MatrixDSL - Compiler Driver
//
// SHARED INTERFACE - FROZEN FOR REVIEW 1
//
// Sequences the pipeline and owns the command-line interface. Every stage can
// be stopped at and dumped, which is what makes each member's module
// independently demonstrable.
//
//===----------------------------------------------------------------------===//

#ifndef MATRIXDSL_COMPILERDRIVER_H
#define MATRIXDSL_COMPILERDRIVER_H

#include <string>
#include <vector>

#include "../llvm/LLVMOptimizer.h"

namespace matrixdsl {

/// Where to stop. `--emit-tokens` through `--emit-asm` map onto these, so a
/// member can exercise their stage without the later stages existing yet.
enum class CompilationStage {
  Tokens,   // --emit-tokens
  AST,      // --emit-ast
  Semantic, // --emit-symbols
  LLVMIR,   // --emit-llvm
  Optimized,// --emit-llvm -O2
  Assembly  // default: MDT assembly
};

/// Which backend path generates assembly.
///
/// TableGen is the primary implementation. Direct is the documented fallback
/// described in docs/llvm-backend.md section 9 - it walks optimized IR and
/// emits assembly without SelectionDAG or the MC layer. Both must produce
/// identical output for the test suite.
enum class BackendMode { TableGen, Direct };

struct CompilerOptions {
  std::string inputFile;
  std::string outputFile;
  CompilationStage stage = CompilationStage::Assembly;
  OptLevel optLevel = OptLevel::O2;
  BackendMode backend = BackendMode::TableGen;

  bool verbose = false;
  bool dumpStats = false;
  bool colorDiagnostics = true;

  /// Emit for the host architecture instead of MDT. This is what makes
  /// differential testing possible: the same program compiled both ways must
  /// produce the same numbers.
  bool targetHost = false;
};

class CompilerDriver {
public:
  explicit CompilerDriver(CompilerOptions options);

  /// Run the pipeline. Returns 0 on success, non-zero on error.
  int run();

  static CompilerOptions parseArguments(int argc, char **argv);
  static void printUsage(const char *programName);
  static void printVersion();

private:
  int runToTokens();
  int runToAST();
  int runToSemantic();
  int runToLLVMIR();
  int runToOptimized();
  int runToAssembly();

  bool readSourceFile(std::string &contents);
  void reportError(const std::string &message);

  CompilerOptions options_;
  std::string source_;
};

} // namespace matrixdsl

#endif // MATRIXDSL_COMPILERDRIVER_H
