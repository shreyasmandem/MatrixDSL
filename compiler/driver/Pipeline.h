//===----------------------------------------------------------------------===//
//
// MatrixDSL - the end-to-end compilation pipeline (Phase 3 / Review 3)
//
// Owner: Vinay A (24BCB0131)
//
// One object that runs every stage the team built, in order, and keeps each
// intermediate form so tools can show any of them:
//
//   source --Lexer--> tokens --Parser--> AST --SemanticAnalyzer--> typed AST
//          --lowerToTensorIR--> Tensor IR --fusion, DCE, tiling--> Tensor IR'
//          --generateMDT--> MDT assembly --mdt::Assembler/Simulator--> output
//
// matrixdslc (the compiler), mtxverify (differential testing) and mtxbench
// (the ablation study) are thin front ends over this file.
//
//===----------------------------------------------------------------------===//

#ifndef MATRIXDSL_PIPELINE_H
#define MATRIXDSL_PIPELINE_H

#include "backend/MDTCodeGen.h"
#include "frontend/ast/AST.h"
#include "frontend/lexer/Lexer.h"
#include "tensor-ir/Interpreter.h"
#include "tensor-ir/Passes.h"
#include "tensor-ir/TensorIR.h"

#include "MDTSim.h"

#include <string>
#include <vector>

namespace matrixdsl {

struct PipelineOptions {
  bool fusion = true;  // FusionPass + dead-code elimination
  bool tiling = true;  // scratchpad staging (TilingPass)
  std::string filename = "<input>";
};

/// The four configurations of the ablation study.
struct Config {
  const char *name;
  bool fusion, tiling;
};
extern const Config kConfigs[4];

struct Compilation {
  bool ok = false;
  std::string diagnostics;      // rendered errors and warnings
  int errorCount = 0, warningCount = 0;

  std::vector<Token> tokens;
  ProgramPtr ast;
  tensorir::TensorFunction ir;  // after the enabled passes
  std::string irBefore, irAfter;
  tensorir::InputMap inputs;

  int fused = 0, deadOps = 0;
  tensorir::TilingReport tiling;
  backend::CodeGenStats codegen;
  std::string assembly;
};

/// Run the front end and middle end, and generate MDT assembly.
Compilation compile(const std::string &source, const PipelineOptions &options);

/// Reference output: the *unoptimised* Tensor IR, interpreted on the host.
bool referenceOutput(const std::string &source,
                     std::vector<tensorir::PrintValue> &prints,
                     std::string &error);

struct Execution {
  bool ok = false;
  std::string error;
  std::vector<tensorir::PrintValue> prints;
  mdt::PerformanceModel perf;
  /// Transfer / Compute / Other runs (0 / 1 / 2) in execution order.
  std::vector<std::pair<int, long long>> runs;
};

/// Assemble and run on the MDT simulator.
Execution simulate(const std::string &assembly, bool echoPrints);

/// Convenience for the tools: read a whole file.
bool readFile(const std::string &path, std::string &out);

} // namespace matrixdsl

#endif // MATRIXDSL_PIPELINE_H
