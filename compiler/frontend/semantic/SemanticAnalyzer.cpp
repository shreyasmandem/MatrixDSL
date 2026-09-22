//===----------------------------------------------------------------------===//
//
// MatrixDSL - Semantic Analyzer implementation
//
// Owner: Mudpe Parth Tulsidas (24BDS0353)
//
// The Review 2 deliverable named in docs/contribution.md at Review 1: the
// AST walk that applies the shape rules already implemented and tested in
// MatrixType.cpp, now that Vinay's AST.h/Parser.cpp exist to walk.
//
// A single post-order traversal (docs/shape-rules.md section 3): each
// expression node computes its result shape from its already-computed
// children and reports it upward via Expr::resultType. Bottom-up is correct
// because every shape rule is a pure function of its operands' shapes - no
// expected type needs to propagate downward, so one pass with no fixpoint
// iteration suffices.
//
//===----------------------------------------------------------------------===//

#include "SemanticAnalyzer.h"

#include <sstream>

namespace matrixdsl {

//===----------------------------------------------------------------------===//
// Diagnostic::format
//===----------------------------------------------------------------------===//

std::string Diagnostic::format(const std::string &filename) const {
  std::ostringstream os;
  const char *label = (severity == DiagnosticSeverity::Error) ? "error"
                      : (severity == DiagnosticSeverity::Warning) ? "warning"
                                                                   : "note";
  os << label << ": " << message << "\n";
  os << "  --> " << filename << ":" << location.line << ":" << location.column
     << "\n";
  if (!note.empty())
    os << "   = note: " << note << "\n";
  return os.str();
}

//===----------------------------------------------------------------------===//
// Public interface
//===----------------------------------------------------------------------===//

bool SemanticAnalyzer::analyze(Program &program) {
  symbols_ = SymbolTable();
  diagnostics_.clear();
  program.accept(*this);
  return !hasErrors();
}

bool SemanticAnalyzer::hasErrors() const {
  for (const Diagnostic &d : diagnostics_)
    if (d.severity == DiagnosticSeverity::Error)
      return true;
  return false;
}

size_t SemanticAnalyzer::errorCount() const {
  size_t n = 0;
  for (const Diagnostic &d : diagnostics_)
    if (d.severity == DiagnosticSeverity::Error)
      ++n;
  return n;
}

size_t SemanticAnalyzer::warningCount() const {
  size_t n = 0;
  for (const Diagnostic &d : diagnostics_)
    if (d.severity == DiagnosticSeverity::Warning)
      ++n;
  return n;
}

void SemanticAnalyzer::error(DiagnosticKind kind, SourceLocation loc,
                             std::string message, std::string note) {
  diagnostics_.push_back({DiagnosticSeverity::Error, kind, std::move(message),
                          std::move(note), loc});
}

void SemanticAnalyzer::warn(DiagnosticKind kind, SourceLocation loc,
                            std::string message) {
  diagnostics_.push_back(
      {DiagnosticSeverity::Warning, kind, std::move(message), "", loc});
}

//===----------------------------------------------------------------------===//
// Expressions - each visit() computes resultType from its children,
// bottom-up, and suppresses a cascading error when a child is already
// invalid (MatrixType.h's shape rules already return invalid silently in
// that case; visit() only needs to skip diagnosing it a second time).
//===----------------------------------------------------------------------===//

void SemanticAnalyzer::visit(NumberExpr &node) {
  // A scalar literal is always a valid 1x1x1 MatrixType - see MatrixType.h's
  // isScalar(), which every binary-operator rule checks against.
  node.resultType = MatrixType(1, 1, 1);
}

void SemanticAnalyzer::visit(VariableExpr &node) {
  const std::optional<MatrixType> declared = symbols_.lookup(node.name);
  if (!declared) {
    error(DiagnosticKind::UndeclaredIdentifier, node.location(),
         "undeclared identifier '" + node.name + "'");
    node.resultType = MatrixType(); // stays invalid - suppresses cascades
    return;
  }
  symbols_.markRead(node.name);
  node.resultType = *declared;
}

void SemanticAnalyzer::visit(BinaryExpr &node) {
  node.lhs->accept(*this);
  node.rhs->accept(*this);

  const MatrixType &lhs = node.lhs->resultType;
  const MatrixType &rhs = node.rhs->resultType;

  if (node.op == BinaryOp::Add || node.op == BinaryOp::Sub) {
    node.resultType = MatrixType::checkAddition(lhs, rhs);
    if (!node.resultType.isValid() && lhs.isValid() && rhs.isValid()) {
      std::ostringstream note;
      note << "addition/subtraction requires both operands to have "
              "identical shapes, but "
           << lhs.toString() << " != " << rhs.toString();
      error(DiagnosticKind::ShapeMismatchAdd, node.location(),
           "cannot add or subtract matrices of shape " + lhs.toString() +
               " and " + rhs.toString(),
           note.str());
    }
    return;
  }

  // BinaryOp::Mul is overloaded: scalar*matrix or matrix*matrix (matmul via
  // the `*` spelling, docs/grammar.md section 3). Try the scalar rule first;
  // it deliberately returns invalid (not an error) when neither operand is a
  // scalar, which is exactly the signal to fall back to the matmul rule.
  MatrixType scalarResult = MatrixType::checkScalarMul(lhs, rhs);
  if (scalarResult.isValid() || !lhs.isValid() || !rhs.isValid()) {
    node.resultType = scalarResult;
    return;
  }

  node.resultType = MatrixType::checkMatMul(lhs, rhs);
  if (!node.resultType.isValid()) {
    std::ostringstream note;
    note << "matmul requires A.cols == B.rows, but " << lhs.cols() << " != "
         << rhs.rows();
    error(DiagnosticKind::ShapeMismatchMatMul, node.location(),
         "cannot multiply matrix of shape " + lhs.toString() +
             " with matrix of shape " + rhs.toString(),
         note.str());
  }
}

void SemanticAnalyzer::visit(MatrixLiteralExpr &node) {
  if (!node.isRectangular()) {
    error(DiagnosticKind::RaggedMatrixLiteral, node.location(),
         "matrix literal rows have differing lengths");
    node.resultType = MatrixType();
    return;
  }

  const int rows = static_cast<int>(node.rowCount());
  const int cols = static_cast<int>(node.colCount());

  if (!MatrixType::isValidDimension(rows) ||
      !MatrixType::isValidDimension(cols)) {
    error(DiagnosticKind::DimensionOutOfRange, node.location(),
         "matrix literal dimension out of range [" +
             std::to_string(kMinDimension) + ", " +
             std::to_string(kMaxDimension) + "]");
    node.resultType = MatrixType();
    return;
  }

  // A literal is always batch 1 - see docs/grammar.md section 2a: a batched
  // tensor is populated from a batch-producing expression, never a literal.
  node.resultType = MatrixType(1, rows, cols);
}

void SemanticAnalyzer::visit(CallExpr &node) {
  for (const ExprPtr &arg : node.args)
    arg->accept(*this);

  switch (node.callee) {
  case BuiltinFunc::MatMul: {
    const MatrixType &lhs = node.args[0]->resultType;
    const MatrixType &rhs = node.args[1]->resultType;
    node.resultType = MatrixType::checkMatMul(lhs, rhs);
    if (!node.resultType.isValid() && lhs.isValid() && rhs.isValid()) {
      std::ostringstream note;
      note << "matmul requires A.cols == B.rows, but " << lhs.cols()
           << " != " << rhs.rows();
      error(DiagnosticKind::ShapeMismatchMatMul, node.location(),
           "cannot multiply matrix " + lhs.toString() + " with matrix " +
               rhs.toString(),
           note.str());
    }
    return;
  }

  case BuiltinFunc::Transpose:
    node.resultType = MatrixType::checkTranspose(node.args[0]->resultType);
    return;

  case BuiltinFunc::Relu:
    node.resultType = MatrixType::checkRelu(node.args[0]->resultType);
    return;

  case BuiltinFunc::Softmax: // Phase 2
    node.resultType = MatrixType::checkSoftmax(node.args[0]->resultType);
    return;
  }
}

//===----------------------------------------------------------------------===//
// Statements
//===----------------------------------------------------------------------===//

void SemanticAnalyzer::visit(MatrixDecl &node) {
  if (!MatrixType::isValidDimension(node.rows) ||
      !MatrixType::isValidDimension(node.cols)) {
    error(DiagnosticKind::DimensionOutOfRange, node.location(),
         "dimension of '" + node.name + "' is out of range [" +
             std::to_string(kMinDimension) + ", " +
             std::to_string(kMaxDimension) + "]");
    return;
  }

  // Phase 2: batch bounds, only meaningful when a batch was actually given
  // (node.batch defaults to 1 for a plain `matrix` declaration, which is
  // always in range).
  if (!MatrixType::isValidBatch(node.batch)) {
    error(DiagnosticKind::DimensionOutOfRange, node.location(),
         "batch count of '" + node.name + "' is out of range [" +
             std::to_string(kMinBatch) + ", " + std::to_string(kMaxBatch) +
             "]");
    return;
  }

  const bool declared = symbols_.declare(node.name, node.declaredType(),
                                         node.location().line,
                                         node.location().column);
  if (!declared) {
    const Symbol *existing = symbols_.lookupSymbol(node.name);
    std::string note;
    if (existing)
      note = "'" + node.name + "' first declared at line " +
             std::to_string(existing->declaredLine);
    error(DiagnosticKind::Redeclaration, node.location(),
         "'" + node.name + "' is already declared", note);
  }
}

void SemanticAnalyzer::visit(Assignment &node) {
  node.value->accept(*this);

  const std::optional<MatrixType> target = symbols_.lookup(node.target);
  if (!target) {
    error(DiagnosticKind::UndeclaredIdentifier, node.location(),
         "undeclared identifier '" + node.target + "'");
    return;
  }

  symbols_.markAssigned(node.target);

  const MatrixType &valueType = node.value->resultType;
  if (!valueType.isValid())
    return; // an earlier error already explains why; don't cascade

  if (valueType != *target) {
    std::ostringstream note;
    note << "expression produces " << valueType.toString()
         << ", but '" << node.target << "' is declared " << target->toString();
    error(DiagnosticKind::ShapeMismatchAssign, node.location(),
         "cannot assign to '" + node.target + "'", note.str());
  }
}

void SemanticAnalyzer::visit(PrintStmt &node) {
  const std::optional<MatrixType> declared = symbols_.lookup(node.name);
  if (!declared) {
    error(DiagnosticKind::UndeclaredIdentifier, node.location(),
         "undeclared identifier '" + node.name + "'");
    return;
  }
  symbols_.markRead(node.name);
}

void SemanticAnalyzer::visit(Program &node) {
  for (const StmtPtr &stmt : node.statements)
    stmt->accept(*this);

  // A declared-but-unread variable is a warning, not an error - it does not
  // affect analyze()'s return value (hasErrors() only counts Error-severity
  // diagnostics).
  for (const Symbol *symbol : symbols_.unusedSymbols())
    warn(DiagnosticKind::UnusedVariable, {symbol->declaredLine, symbol->declaredColumn},
        "'" + symbol->name + "' is declared but never used");
}

} // namespace matrixdsl
