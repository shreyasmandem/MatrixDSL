//===----------------------------------------------------------------------===//
//
// MatrixDSL - LLVM Optimization Pipeline
//
// SHARED INTERFACE - FROZEN FOR REVIEW 1
// Owner: Kandi Jeevitesh Reddy (24BCE0350)
//
//===----------------------------------------------------------------------===//

#ifndef MATRIXDSL_LLVMOPTIMIZER_H
#define MATRIXDSL_LLVMOPTIMIZER_H

#include <string>
#include <vector>

namespace llvm {
class Module;
}

namespace matrixdsl {

enum class OptLevel { O0, O1, O2, O3, Os, Oz };

const char *toString(OptLevel level);

/// Statistics captured either side of the pipeline. These are what the Review 3
/// "before and after optimization" analysis is built from.
struct OptStats {
  size_t instructionsBefore = 0;
  size_t instructionsAfter = 0;
  size_t basicBlocksBefore = 0;
  size_t basicBlocksAfter = 0;
  size_t functionsBefore = 0;
  size_t functionsAfter = 0;

  double instructionReduction() const {
    if (instructionsBefore == 0)
      return 0.0;
    return 100.0 * (double)(instructionsBefore - instructionsAfter) /
           (double)instructionsBefore;
  }

  std::string summary() const;
};

/// Wrapper over LLVM's PassBuilder.
///
/// Note on matrix intrinsics: the calls emitted for matmul/relu/transpose must
/// survive optimization as opaque calls so the backend can still see them. They
/// are therefore declared without `readnone`/`speculatable`, which would let the
/// optimizer sink, duplicate, or eliminate them.
class LLVMOptimizer {
public:
  explicit LLVMOptimizer(OptLevel level = OptLevel::O2);

  /// Run the default pipeline for the configured level.
  bool run(llvm::Module &module);

  /// Run a specific pass pipeline, e.g. "instcombine,simplifycfg,dce".
  /// Used by the phase-ordering experiments in the benchmark suite.
  bool runCustomPipeline(llvm::Module &module, const std::string &pipeline);

  const OptStats &stats() const { return stats_; }

  void setLevel(OptLevel level) { level_ = level; }
  OptLevel level() const { return level_; }

  /// Passes explicitly enabled for this project, in pipeline order.
  static std::vector<std::string> defaultPasses();

private:
  void captureBefore(const llvm::Module &module);
  void captureAfter(const llvm::Module &module);

  OptLevel level_;
  OptStats stats_;
};

} // namespace matrixdsl

#endif // MATRIXDSL_LLVMOPTIMIZER_H
