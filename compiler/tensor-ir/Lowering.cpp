//===----------------------------------------------------------------------===//
// MatrixDSL - AST -> Tensor IR lowering implementation
// Owner: Kandi Jeevitesh Reddy (24BCE0350)
//===----------------------------------------------------------------------===//

#include "Lowering.h"

#include <unordered_map>

namespace matrixdsl {
namespace tensorir {

namespace {

TensorShape shapeOf(const MatrixType &t) {
  return TensorShape(t.batch(), t.rows(), t.cols());
}

class Lowerer {
public:
  Lowerer(TensorFunction &fn, LoweringResult &result)
      : fn_(fn), result_(result) {}

  void lowerProgram(Program &program) {
    for (StmtPtr &stmt : program.statements)
      lowerStmt(*stmt);
  }

private:
  TensorFunction &fn_;
  LoweringResult &result_;
  std::unordered_map<std::string, TensorShape> declared_;
  std::unordered_map<std::string, TensorOp *> env_; // name -> current value

  void fail(const ASTNode &node, const std::string &message) {
    result_.ok = false;
    result_.errors.push_back(std::to_string(node.location().line) + ":" +
                             std::to_string(node.location().column) + ": " +
                             message);
  }

  /// Current value of a variable; the first read of a never-assigned
  /// variable creates the Load op that makes it a program input.
  TensorOp *valueOf(const std::string &name, const ASTNode &at) {
    auto it = env_.find(name);
    if (it != env_.end())
      return it->second;
    auto decl = declared_.find(name);
    if (decl == declared_.end()) {
      fail(at, "internal: '" + name + "' reached lowering undeclared");
      return nullptr;
    }
    TensorOp *load = fn_.addOp(TensorOpKind::Load, decl->second);
    load->refName = name;
    env_[name] = load;
    result_.inputs.push_back(name);
    return load;
  }

  void lowerStmt(Stmt &stmt) {
    switch (stmt.kind()) {
    case NodeKind::MatrixDecl: {
      auto &decl = static_cast<MatrixDecl &>(stmt);
      declared_[decl.name] = shapeOf(decl.declaredType());
      break;
    }
    case NodeKind::Assignment: {
      auto &assign = static_cast<Assignment &>(stmt);
      if (TensorOp *value = lowerExpr(*assign.value))
        env_[assign.target] = value; // SSA rebinding - no copy
      break;
    }
    case NodeKind::PrintStmt: {
      auto &print = static_cast<PrintStmt &>(stmt);
      if (TensorOp *value = valueOf(print.name, print)) {
        TensorOp *op = fn_.addOp(TensorOpKind::Print, value->shape, {value});
        op->refName = print.name;
      }
      break;
    }
    default:
      fail(stmt, "internal: unexpected statement kind");
    }
  }

  /// A 1x1 operand of `*` must be a compile-time constant: MDT scales a
  /// matrix by multiplying with a constant s*I tile, built at compile time.
  bool constantScalar(const TensorOp *op, double &value) const {
    if (op->kind == TensorOpKind::Const && op->constData.size() == 1) {
      value = op->constData[0];
      return true;
    }
    return false;
  }

  TensorOp *lowerExpr(Expr &expr) {
    if (!expr.hasResolvedType()) {
      fail(expr, "internal: expression has no resolved type");
      return nullptr;
    }
    const TensorShape shape = shapeOf(expr.resultType);

    switch (expr.kind()) {
    case NodeKind::NumberExpr: {
      auto &num = static_cast<NumberExpr &>(expr);
      TensorOp *op = fn_.addOp(TensorOpKind::Const, shape);
      op->constValue = num.value;
      op->constData = {num.value};
      return op;
    }
    case NodeKind::MatrixLiteralExpr: {
      auto &lit = static_cast<MatrixLiteralExpr &>(expr);
      TensorOp *op = fn_.addOp(TensorOpKind::Const, shape);
      for (const std::vector<double> &row : lit.values)
        op->constData.insert(op->constData.end(), row.begin(), row.end());
      return op;
    }
    case NodeKind::VariableExpr:
      return valueOf(static_cast<VariableExpr &>(expr).name, expr);

    case NodeKind::BinaryExpr: {
      auto &bin = static_cast<BinaryExpr &>(expr);
      TensorOp *lhs = lowerExpr(*bin.lhs);
      TensorOp *rhs = lowerExpr(*bin.rhs);
      if (!lhs || !rhs)
        return nullptr;

      if (bin.op == BinaryOp::Add)
        return fn_.addOp(TensorOpKind::Add, shape, {lhs, rhs});
      if (bin.op == BinaryOp::Sub)
        return fn_.addOp(TensorOpKind::Sub, shape, {lhs, rhs});

      // `*`: scalar multiplication when either side is 1x1, else matmul
      // (MatrixType::checkScalarMul falls back to checkMatMul the same way).
      const bool lhsScalar = bin.lhs->resultType.isScalar();
      const bool rhsScalar = bin.rhs->resultType.isScalar();
      if (!lhsScalar && !rhsScalar)
        return fn_.addOp(TensorOpKind::MatMul, shape, {lhs, rhs});

      double s = 0.0;
      TensorOp *matrix = nullptr;
      if (lhsScalar && constantScalar(lhs, s))
        matrix = rhs;
      else if (rhsScalar && constantScalar(rhs, s))
        matrix = lhs;
      if (!matrix) {
        fail(bin, "scalar factor of '*' must be a numeric constant "
                  "(the MDT back end scales by a compile-time s*I tile)");
        return nullptr;
      }
      TensorOp *op = fn_.addOp(TensorOpKind::ScalarMul, shape, {matrix});
      op->constValue = s;
      return op;
    }

    case NodeKind::CallExpr: {
      auto &call = static_cast<CallExpr &>(expr);
      std::vector<TensorOp *> args;
      for (ExprPtr &arg : call.args) {
        TensorOp *value = lowerExpr(*arg);
        if (!value)
          return nullptr;
        args.push_back(value);
      }
      switch (call.callee) {
      case BuiltinFunc::MatMul:
        return fn_.addOp(TensorOpKind::MatMul, shape, args);
      case BuiltinFunc::Transpose:
        return fn_.addOp(TensorOpKind::Transpose, shape, args);
      case BuiltinFunc::Relu:
        return fn_.addOp(TensorOpKind::Relu, shape, args);
      case BuiltinFunc::Softmax:
        return fn_.addOp(TensorOpKind::Softmax, shape, args);
      }
      fail(call, "internal: unknown builtin");
      return nullptr;
    }

    default:
      fail(expr, "internal: unexpected expression kind");
      return nullptr;
    }
  }
};

} // namespace

LoweringResult lowerToTensorIR(Program &program, TensorFunction &fn) {
  LoweringResult result;
  Lowerer(fn, result).lowerProgram(program);
  return result;
}

} // namespace tensorir
} // namespace matrixdsl
