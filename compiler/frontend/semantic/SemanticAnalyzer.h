//===----------------------------------------------------------------------===//
//
// MatrixDSL - Semantic Analyzer
//
// SHARED INTERFACE - FROZEN FOR REVIEW 1
// Owner: Mudpe Parth Tulsidas (24BDS0353)
//
// Validates names and matrix shapes, and annotates every Expr node with its
// resolved `resultType`. The IR generator may assume that any AST which has
// passed analysis is fully typed and shape-correct - no shape re-checking
// happens downstream.
//
//===----------------------------------------------------------------------===//

#ifndef MATRIXDSL_SEMANTICANALYZER_H
#define MATRIXDSL_SEMANTICANALYZER_H

#include <string>
#include <vector>

#include "../ast/AST.h"
#include "MatrixType.h"
#include "SymbolTable.h"

namespace matrixdsl {

enum class DiagnosticSeverity { Error, Warning, Note };

enum class DiagnosticKind {
  UndeclaredIdentifier,
  Redeclaration,
  ShapeMismatchAdd,
  ShapeMismatchMatMul,
  ShapeMismatchAssign,
  RaggedMatrixLiteral,
  LiteralShapeMismatch,
  DimensionOutOfRange,
  UnusedVariable,
  ReadBeforeAssign
};

struct Diagnostic {
  DiagnosticSeverity severity;
  DiagnosticKind kind;
  std::string message;
  std::string note;
  SourceLocation location;

  /// Render in the standard format:
  ///
  ///   error: cannot multiply matrix A (4x3) with matrix B (7x5)
  ///     --> program.mtx:12:5
  ///      = note: matmul requires A.cols == B.rows, but 3 != 7
  std::string format(const std::string &filename) const;
};

/// Post-order traversal computing each expression's shape bottom-up.
///
/// Analysis does not stop at the first error. Where a shape cannot be
/// resolved, the node is left with an invalid type and analysis continues, so
/// that a single run reports as many genuine problems as possible. Cascading
/// errors from an already-invalid subexpression are suppressed.
class SemanticAnalyzer : public ASTVisitor {
public:
  SemanticAnalyzer() = default;

  /// Returns true when the program is free of errors. Warnings do not affect
  /// the return value.
  bool analyze(Program &program);

  const std::vector<Diagnostic> &diagnostics() const { return diagnostics_; }
  const SymbolTable &symbols() const { return symbols_; }

  bool hasErrors() const;
  size_t errorCount() const;
  size_t warningCount() const;

  void visit(NumberExpr &node) override;
  void visit(VariableExpr &node) override;
  void visit(BinaryExpr &node) override;
  void visit(MatrixLiteralExpr &node) override;
  void visit(CallExpr &node) override;
  void visit(MatrixDecl &node) override;
  void visit(Assignment &node) override;
  void visit(PrintStmt &node) override;
  void visit(Program &node) override;

private:
  void error(DiagnosticKind kind, SourceLocation loc, std::string message,
             std::string note = "");
  void warn(DiagnosticKind kind, SourceLocation loc, std::string message);

  SymbolTable symbols_;
  std::vector<Diagnostic> diagnostics_;
};

} // namespace matrixdsl

#endif // MATRIXDSL_SEMANTICANALYZER_H
