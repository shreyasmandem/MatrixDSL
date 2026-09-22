//===----------------------------------------------------------------------===//
//
// MatrixDSL - Tensor IR implementation
//
// Owner: Kandi Jeevitesh Reddy (24BCE0350)
//
//===----------------------------------------------------------------------===//

#include "TensorIR.h"

#include <sstream>

namespace matrixdsl {
namespace tensorir {

std::string TensorShape::toString() const {
  if (!isValid())
    return "<invalid>";
  if (batch > 1)
    return std::to_string(batch) + "x" + std::to_string(rows) + "x" +
           std::to_string(cols);
  return std::to_string(rows) + "x" + std::to_string(cols);
}

const char *tensorOpKindName(TensorOpKind kind) {
  switch (kind) {
  case TensorOpKind::Const:            return "Const";
  case TensorOpKind::Load:             return "Load";
  case TensorOpKind::Add:              return "Add";
  case TensorOpKind::Sub:              return "Sub";
  case TensorOpKind::ScalarMul:        return "ScalarMul";
  case TensorOpKind::MatMul:           return "MatMul";
  case TensorOpKind::Transpose:        return "Transpose";
  case TensorOpKind::Relu:             return "Relu";
  case TensorOpKind::Softmax:          return "Softmax";
  case TensorOpKind::RowMax:           return "RowMax";
  case TensorOpKind::RowSum:           return "RowSum";
  case TensorOpKind::Exp:              return "Exp";
  case TensorOpKind::Print:            return "Print";
  case TensorOpKind::FusedMatMulRelu:  return "FusedMatMulRelu";
  case TensorOpKind::FusedMatMulAcc:   return "FusedMatMulAcc";
  case TensorOpKind::TiledMatMul:      return "TiledMatMul";
  case TensorOpKind::ScratchLoad:      return "ScratchLoad";
  case TensorOpKind::ScratchStore:     return "ScratchStore";
  case TensorOpKind::ConvertPrecision: return "ConvertPrecision";
  }
  return "?";
}

bool isTier2(TensorOpKind kind) {
  switch (kind) {
  case TensorOpKind::FusedMatMulRelu:
  case TensorOpKind::FusedMatMulAcc:
  case TensorOpKind::TiledMatMul:
  case TensorOpKind::ScratchLoad:
  case TensorOpKind::ScratchStore:
  case TensorOpKind::ConvertPrecision:
    return true;
  default:
    return false;
  }
}

TensorOp *TensorFunction::addOp(TensorOpKind kind, TensorShape shape,
                                std::vector<TensorOp *> operands) {
  auto op = std::make_unique<TensorOp>();
  op->kind = kind;
  op->shape = shape;
  op->operands = std::move(operands);
  op->id = static_cast<int>(ops_.size());
  TensorOp *raw = op.get();
  ops_.push_back(std::move(op));
  return raw;
}

std::string TensorFunction::dump() const {
  std::ostringstream os;
  for (const std::unique_ptr<TensorOp> &op : ops_) {
    os << "%" << op->id << " = " << tensorOpKindName(op->kind);
    for (const TensorOp *operand : op->operands)
      os << " %" << operand->id;
    os << " : " << op->shape.toString() << "\n";
  }
  return os.str();
}

} // namespace tensorir
} // namespace matrixdsl
