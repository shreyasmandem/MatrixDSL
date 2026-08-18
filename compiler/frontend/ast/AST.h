//===----------------------------------------------------------------------===//
//
// MatrixDSL - Abstract Syntax Tree
//
// SHARED INTERFACE - FROZEN FOR REVIEW 1
//
// This header is the contract between all four compiler modules:
//
//   Vinay   (parser)   constructs these nodes
//   Parth   (semantic) walks them and fills in `resultType`
//   Kandi   (irgen)    walks them and emits LLVM IR
//   Shreyas (backend)  consumes the IR that results
//
// Changing anything here requires agreement from all four members, because
// every module compiles against it. Downstream modules build and test against
// hand-constructed AST fixtures before the parser is finished.
//
//===----------------------------------------------------------------------===//

#ifndef MATRIXDSL_AST_H
#define MATRIXDSL_AST_H

#include <memory>
#include <string>
#include <vector>

#include "../semantic/MatrixType.h"

namespace matrixdsl {

//===----------------------------------------------------------------------===//
// Source location
//===----------------------------------------------------------------------===//

struct SourceLocation {
  int line = 0;
  int column = 0;

  SourceLocation() = default;
  SourceLocation(int l, int c) : line(l), column(c) {}
};

//===----------------------------------------------------------------------===//
// Node kinds
//
// A kind tag is carried on every node so that consumers can dispatch with a
// switch rather than a chain of dynamic_casts. Visitors are provided below for
// code that prefers double dispatch.
//===----------------------------------------------------------------------===//

enum class NodeKind {
  // Expressions
  NumberExpr,
  VariableExpr,
  BinaryExpr,
  MatrixLiteralExpr,
  CallExpr,

  // Statements
  MatrixDecl,
  Assignment,
  PrintStmt,

  // Root
  Program
};

enum class BinaryOp { Add, Sub, Mul };

/// Built-in matrix functions. The language has no user-defined functions, so
/// this enumeration is closed.
enum class BuiltinFunc { MatMul, Transpose, Relu };

const char *toString(BinaryOp op);
const char *toString(BuiltinFunc fn);

//===----------------------------------------------------------------------===//
// Base node
//===----------------------------------------------------------------------===//

class ASTVisitor;

class ASTNode {
public:
  explicit ASTNode(NodeKind kind, SourceLocation loc = {})
      : kind_(kind), loc_(loc) {}
  virtual ~ASTNode() = default;

  NodeKind kind() const { return kind_; }
  SourceLocation location() const { return loc_; }

  virtual void accept(ASTVisitor &visitor) = 0;

private:
  NodeKind kind_;
  SourceLocation loc_;
};

using ASTNodePtr = std::unique_ptr<ASTNode>;

//===----------------------------------------------------------------------===//
// Expressions
//===----------------------------------------------------------------------===//

/// Base class for all expressions.
///
/// `resultType` is the shared slot between semantic analysis and IR
/// generation. The parser leaves it default-constructed (rows == cols == 0);
/// the semantic analyzer computes and stores the true shape; the IR generator
/// may assume it is populated and correct.
class Expr : public ASTNode {
public:
  using ASTNode::ASTNode;

  MatrixType resultType;

  bool hasResolvedType() const { return resultType.isValid(); }
};

using ExprPtr = std::unique_ptr<Expr>;

/// A scalar FP32 literal, e.g. `2.0` in `B = 2.0 * A;`
class NumberExpr : public Expr {
public:
  explicit NumberExpr(double value, SourceLocation loc = {})
      : Expr(NodeKind::NumberExpr, loc), value(value) {}

  double value;

  void accept(ASTVisitor &visitor) override;
};

/// A reference to a declared matrix, e.g. `A`.
class VariableExpr : public Expr {
public:
  explicit VariableExpr(std::string name, SourceLocation loc = {})
      : Expr(NodeKind::VariableExpr, loc), name(std::move(name)) {}

  std::string name;

  void accept(ASTVisitor &visitor) override;
};

/// A binary operation: `A + B`, `A - B`, `2.0 * A`, `A * B`.
class BinaryExpr : public Expr {
public:
  BinaryExpr(BinaryOp op, ExprPtr lhs, ExprPtr rhs, SourceLocation loc = {})
      : Expr(NodeKind::BinaryExpr, loc), op(op), lhs(std::move(lhs)),
        rhs(std::move(rhs)) {}

  BinaryOp op;
  ExprPtr lhs;
  ExprPtr rhs;

  void accept(ASTVisitor &visitor) override;
};

/// A matrix literal: `[[1, 2], [3, 4]]`.
///
/// Rows are stored outer-first; `values[i][j]` is row i, column j. The parser
/// does not verify that rows are of equal length - that is a semantic check,
/// so that the error is reported with a proper diagnostic rather than a parse
/// failure.
class MatrixLiteralExpr : public Expr {
public:
  explicit MatrixLiteralExpr(std::vector<std::vector<double>> values,
                             SourceLocation loc = {})
      : Expr(NodeKind::MatrixLiteralExpr, loc), values(std::move(values)) {}

  std::vector<std::vector<double>> values;

  size_t rowCount() const { return values.size(); }
  size_t colCount() const { return values.empty() ? 0 : values[0].size(); }
  bool isRectangular() const;

  void accept(ASTVisitor &visitor) override;
};

/// A built-in call: `matmul(A, B)`, `transpose(A)`, `relu(A)`.
class CallExpr : public Expr {
public:
  CallExpr(BuiltinFunc callee, std::vector<ExprPtr> args,
           SourceLocation loc = {})
      : Expr(NodeKind::CallExpr, loc), callee(callee), args(std::move(args)) {}

  BuiltinFunc callee;
  std::vector<ExprPtr> args;

  void accept(ASTVisitor &visitor) override;
};

//===----------------------------------------------------------------------===//
// Statements
//===----------------------------------------------------------------------===//

class Stmt : public ASTNode {
public:
  using ASTNode::ASTNode;
};

using StmtPtr = std::unique_ptr<Stmt>;

/// `matrix A[4][4];`
class MatrixDecl : public Stmt {
public:
  MatrixDecl(std::string name, int rows, int cols, SourceLocation loc = {})
      : Stmt(NodeKind::MatrixDecl, loc), name(std::move(name)), rows(rows),
        cols(cols) {}

  std::string name;
  int rows;
  int cols;

  MatrixType declaredType() const { return MatrixType(rows, cols); }

  void accept(ASTVisitor &visitor) override;
};

/// `C = <expression>;`
class Assignment : public Stmt {
public:
  Assignment(std::string target, ExprPtr value, SourceLocation loc = {})
      : Stmt(NodeKind::Assignment, loc), target(std::move(target)),
        value(std::move(value)) {}

  std::string target;
  ExprPtr value;

  void accept(ASTVisitor &visitor) override;
};

/// `print(A);`
class PrintStmt : public Stmt {
public:
  explicit PrintStmt(std::string name, SourceLocation loc = {})
      : Stmt(NodeKind::PrintStmt, loc), name(std::move(name)) {}

  std::string name;

  void accept(ASTVisitor &visitor) override;
};

//===----------------------------------------------------------------------===//
// Program
//===----------------------------------------------------------------------===//

/// Root of the AST: an ordered sequence of statements.
class Program : public ASTNode {
public:
  Program() : ASTNode(NodeKind::Program, {}) {}

  std::vector<StmtPtr> statements;

  void addStatement(StmtPtr stmt) { statements.push_back(std::move(stmt)); }

  void accept(ASTVisitor &visitor) override;
};

using ProgramPtr = std::unique_ptr<Program>;

//===----------------------------------------------------------------------===//
// Visitor
//===----------------------------------------------------------------------===//

class ASTVisitor {
public:
  virtual ~ASTVisitor() = default;

  virtual void visit(NumberExpr &node) = 0;
  virtual void visit(VariableExpr &node) = 0;
  virtual void visit(BinaryExpr &node) = 0;
  virtual void visit(MatrixLiteralExpr &node) = 0;
  virtual void visit(CallExpr &node) = 0;

  virtual void visit(MatrixDecl &node) = 0;
  virtual void visit(Assignment &node) = 0;
  virtual void visit(PrintStmt &node) = 0;

  virtual void visit(Program &node) = 0;
};

//===----------------------------------------------------------------------===//
// Debug printing
//===----------------------------------------------------------------------===//

/// Render the AST as an indented tree. Used by `matrixdslc --dump-ast` and by
/// the parser tests, which compare against golden output.
std::string dumpAST(ASTNode &node);

} // namespace matrixdsl

#endif // MATRIXDSL_AST_H
