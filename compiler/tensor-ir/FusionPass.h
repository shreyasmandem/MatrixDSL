//===----------------------------------------------------------------------===//
// MatrixDSL - Operator fusion pass
// Owner: Kandi Jeevitesh Reddy (24BCE0350)
//
// Pattern-matches Tier 1 sequences into Tier 2 fused ops - docs/tensor-ir.md
// section 5. A pattern only fuses when the intermediate value has no other
// use; the fusion pass checks use-count, not just adjacency.
//===----------------------------------------------------------------------===//

#ifndef MATRIXDSL_FUSIONPASS_H
#define MATRIXDSL_FUSIONPASS_H

#include "TensorIR.h"

namespace matrixdsl {
namespace tensorir {

/// Rewrites `fn` in place. Returns the number of ops fused.
int runFusionPass(TensorFunction &fn);

} // namespace tensorir
} // namespace matrixdsl

#endif // MATRIXDSL_FUSIONPASS_H
