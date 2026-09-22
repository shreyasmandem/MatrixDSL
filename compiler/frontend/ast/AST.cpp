//===----------------------------------------------------------------------===//
//
// MatrixDSL - Abstract Syntax Tree implementation
//
// Owner: Vinay A (24BCB0131)
//
// Review 2 deliverable, promised in docs/contribution.md's Review 2 targets
// at Review 1: the visitor accept() overrides, the toString() helpers, and
// dumpAST() - all declared in AST.h since Review 1, implemented here now
// that the whole team is building against a stable AST.
//
//===----------------------------------------------------------------------===//

#include "AST.h"

#include <sstream>

namespace matrixdsl {

//===----------------------------------------------------------------------===//
// Debug names
//===----------------------------------------------------------------------===//

const char *toString(BinaryOp op) {
  switch (op) {
  case BinaryOp::Add: return "+";
  case BinaryOp::Sub: return "-";
  case BinaryOp::Mul: return "*";
  }
  return "?"; // unreachable for a valid BinaryOp; never silently misdumps
}

const char *toString(BuiltinFunc fn) {
  switch (fn) {
  case BuiltinFunc::MatMul:    return "matmul";
  case BuiltinFunc::Transpose: return "transpose";
  case BuiltinFunc::Relu:      return "relu";
  case BuiltinFunc::Softmax:   return "softmax"; // Phase 2
  }
  return "?";
}

//===----------------------------------------------------------------------===//
// MatrixLiteralExpr::isRectangular
//
// The parser deliberately accepts a ragged literal (docs/grammar.md) so that
// semantic analysis can report it as a proper diagnostic with a source span,
// rather than the parser rejecting it as a syntax error. This is the check
// semantic analysis calls.
//===----------------------------------------------------------------------===//

bool MatrixLiteralExpr::isRectangular() const {
  if (values.empty())
    return true;
  const size_t width = values.front().size();
  for (const auto &row : values)
    if (row.size() != width)
      return false;
  return true;
}

//===----------------------------------------------------------------------===//
// Visitor double dispatch
//===----------------------------------------------------------------------===//

void NumberExpr::accept(ASTVisitor &visitor) { visitor.visit(*this); }
void VariableExpr::accept(ASTVisitor &visitor) { visitor.visit(*this); }
void BinaryExpr::accept(ASTVisitor &visitor) { visitor.visit(*this); }
void MatrixLiteralExpr::accept(ASTVisitor &visitor) { visitor.visit(*this); }
void CallExpr::accept(ASTVisitor &visitor) { visitor.visit(*this); }
void MatrixDecl::accept(ASTVisitor &visitor) { visitor.visit(*this); }
void Assignment::accept(ASTVisitor &visitor) { visitor.visit(*this); }
void PrintStmt::accept(ASTVisitor &visitor) { visitor.visit(*this); }
void Program::accept(ASTVisitor &visitor) { visitor.visit(*this); }

//===----------------------------------------------------------------------===//
// dumpAST - an indented-tree ASTVisitor, kept private to this file since
// nothing outside dumpAST() itself needs to name it.
//===----------------------------------------------------------------------===//

namespace {

class DumpVisitor : public ASTVisitor {
public:
  explicit DumpVisitor(std::ostringstream &out) : out_(out) {}

  void visit(NumberExpr &node) override {
    indent();
    out_ << "NumberExpr(" << node.value << ")\n";
  }

  void visit(VariableExpr &node) override {
    indent();
    out_ << "VariableExpr(" << node.name << ")\n";
  }

  void visit(BinaryExpr &node) override {
    indent();
    out_ << "BinaryExpr(" << matrixdsl::toString(node.op) << ")\n";
    ++depth_;
    node.lhs->accept(*this);
    node.rhs->accept(*this);
    --depth_;
  }

  void visit(MatrixLiteralExpr &node) override {
    indent();
    out_ << "MatrixLiteralExpr(" << node.rowCount() << "x" << node.colCount();
    if (!node.isRectangular())
      out_ << ", RAGGED";
    out_ << ")\n";
  }

  void visit(CallExpr &node) override {
    indent();
    out_ << "CallExpr(" << matrixdsl::toString(node.callee) << ")\n";
    ++depth_;
    for (const ExprPtr &arg : node.args)
      arg->accept(*this);
    --depth_;
  }

  void visit(MatrixDecl &node) override {
    indent();
    out_ << "MatrixDecl(" << node.name << ", ";
    // Phase 2: only mention the batch dimension when it is not the
    // Phase-1-default 1, so a plain `matrix A[4][4];` still dumps exactly
    // as it did before Phase 2 existed.
    if (node.batch > 1)
      out_ << node.batch << "x";
    out_ << node.rows << "x" << node.cols << ")\n";
  }

  void visit(Assignment &node) override {
    indent();
    out_ << "Assignment(" << node.target << ")\n";
    ++depth_;
    node.value->accept(*this);
    --depth_;
  }

  void visit(PrintStmt &node) override {
    indent();
    out_ << "PrintStmt(" << node.name << ")\n";
  }

  void visit(Program &node) override {
    indent();
    out_ << "Program\n";
    ++depth_;
    for (const StmtPtr &stmt : node.statements)
      stmt->accept(*this);
    --depth_;
  }

private:
  void indent() {
    for (int i = 0; i < depth_; ++i)
      out_ << "  ";
  }

  std::ostringstream &out_;
  int depth_ = 0;
};

} // namespace

std::string dumpAST(ASTNode &node) {
  std::ostringstream out;
  DumpVisitor visitor(out);
  node.accept(visitor);
  return out.str();
}

} // namespace matrixdsl
