//===----------------------------------------------------------------------===//
//
// MatrixDSL - source-annotated diagnostics (Phase 3 / Review 3)
//
// Owner: Vinay A (24BCB0131)
//
// Renders every lexical, syntax and semantic error the same way, with the
// offending source line and a caret under the column:
//
//   layer.mtx:5:5: error: matmul inner dimensions differ: 4x3 * 4x4
//       5 | Y = matmul(A, B);
//         |     ^
//     note: columns of the left operand must equal rows of the right
//
//===----------------------------------------------------------------------===//

#ifndef MATRIXDSL_DIAGNOSTICS_H
#define MATRIXDSL_DIAGNOSTICS_H

#include <string>

namespace matrixdsl {

std::string renderDiagnostic(const std::string &source,
                             const std::string &filename, int line, int column,
                             const std::string &severity,
                             const std::string &message,
                             const std::string &note = "");

} // namespace matrixdsl

#endif // MATRIXDSL_DIAGNOSTICS_H
