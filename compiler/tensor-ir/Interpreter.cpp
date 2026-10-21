//===----------------------------------------------------------------------===//
// MatrixDSL - Tensor IR reference interpreter implementation
// Owner: Kandi Jeevitesh Reddy (24BCE0350)
//===----------------------------------------------------------------------===//

#include "Interpreter.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>

namespace matrixdsl {
namespace tensorir {

namespace {

struct Value {
  TensorShape shape;
  std::vector<float> data; // batch * rows * cols, row-major

  float at(int b, int i, int j) const {
    const int bb = shape.batch == 1 ? 0 : b; // batch broadcast
    return data[(static_cast<size_t>(bb) * shape.rows + i) * shape.cols + j];
  }
};

Value make(const TensorShape &s) {
  Value v;
  v.shape = s;
  v.data.assign(static_cast<size_t>(s.batch) * s.rows * s.cols, 0.0f);
  return v;
}

float &ref(Value &v, int b, int i, int j) {
  return v.data[(static_cast<size_t>(b) * v.shape.rows + i) * v.shape.cols + j];
}

/// Row-wise softmax with the exact float operation order of the simulator's
/// mdt_softmax runtime, so the two agree bit for bit.
void softmaxRow(const float *in, float *out, int cols) {
  float mx = in[0];
  for (int j = 1; j < cols; ++j)
    mx = std::max(mx, in[j]);
  float sum = 0.0f;
  for (int j = 0; j < cols; ++j) {
    out[j] = std::exp(in[j] - mx);
    sum += out[j];
  }
  for (int j = 0; j < cols; ++j)
    out[j] = out[j] / sum;
}

} // namespace

InputMap makeInputs(const TensorFunction &fn, unsigned seed) {
  InputMap inputs;
  for (const std::unique_ptr<TensorOp> &op : fn.ops()) {
    if (op->kind != TensorOpKind::Load || inputs.count(op->refName))
      continue;
    unsigned state = seed;
    for (char c : op->refName)
      state = state * 131u + static_cast<unsigned char>(c);
    const size_t n = static_cast<size_t>(op->shape.batch) * op->shape.rows *
                     op->shape.cols;
    std::vector<float> values(n);
    for (float &v : values) {
      state = state * 1664525u + 1013904223u; // Numerical Recipes LCG
      v = static_cast<float>(static_cast<int>((state >> 16) % 33u) - 16) / 8.0f;
    }
    inputs[op->refName] = std::move(values);
  }
  return inputs;
}

bool interpret(const TensorFunction &fn, const InputMap &inputs,
               std::vector<PrintValue> &prints, std::string &error) {
  std::unordered_map<const TensorOp *, Value> values;
  const auto in = [&](const TensorOp *op, size_t i) -> const Value & {
    return values.at(op->operands[i]);
  };

  for (const std::unique_ptr<TensorOp> &opPtr : fn.ops()) {
    const TensorOp *op = opPtr.get();
    const TensorShape s = op->shape;
    Value out = make(s);

    switch (op->kind) {
    case TensorOpKind::Const:
      for (size_t k = 0; k < out.data.size() && k < op->constData.size(); ++k)
        out.data[k] = static_cast<float>(op->constData[k]);
      break;

    case TensorOpKind::Load: {
      auto it = inputs.find(op->refName);
      if (it == inputs.end() || it->second.size() != out.data.size()) {
        error = "no input values for '" + op->refName + "'";
        return false;
      }
      out.data = it->second;
      break;
    }

    case TensorOpKind::Add:
    case TensorOpKind::Sub: {
      const Value &a = in(op, 0), &b = in(op, 1);
      for (int bb = 0; bb < s.batch; ++bb)
        for (int i = 0; i < s.rows; ++i)
          for (int j = 0; j < s.cols; ++j)
            ref(out, bb, i, j) = op->kind == TensorOpKind::Add
                                     ? a.at(bb, i, j) + b.at(bb, i, j)
                                     : a.at(bb, i, j) - b.at(bb, i, j);
      break;
    }

    case TensorOpKind::ScalarMul: {
      const Value &a = in(op, 0);
      const float k = static_cast<float>(op->constValue);
      for (int bb = 0; bb < s.batch; ++bb)
        for (int i = 0; i < s.rows; ++i)
          for (int j = 0; j < s.cols; ++j)
            ref(out, bb, i, j) = a.at(bb, i, j) * k;
      break;
    }

    case TensorOpKind::MatMul:
    case TensorOpKind::TiledMatMul:
    case TensorOpKind::FusedMatMulRelu:
    case TensorOpKind::FusedMatMulAcc: {
      // Same association as the hardware: 4-term tile partials, accumulated
      // tile by tile (onto the bias first, for FusedMatMulAcc).
      const Value &a = in(op, 0), &b = in(op, 1);
      // FusedMatMulAcc, or the Phase 3 three-operand FusedMatMulRelu.
      const bool acc = op->operands.size() == 3;
      const int depth = a.shape.cols;
      for (int bb = 0; bb < s.batch; ++bb)
        for (int i = 0; i < s.rows; ++i)
          for (int j = 0; j < s.cols; ++j) {
            float sum = acc ? in(op, 2).at(bb, i, j) : 0.0f;
            for (int k0 = 0; k0 < depth; k0 += 4) {
              float partial = 0.0f;
              for (int k = k0; k < k0 + 4 && k < depth; ++k)
                partial += a.at(bb, i, k) * b.at(bb, k, j);
              sum = (k0 == 0 && !acc) ? partial : sum + partial;
            }
            if (op->kind == TensorOpKind::FusedMatMulRelu && sum < 0.0f)
              sum = 0.0f;
            ref(out, bb, i, j) = sum;
          }
      break;
    }

    case TensorOpKind::Transpose: {
      const Value &a = in(op, 0);
      for (int bb = 0; bb < s.batch; ++bb)
        for (int i = 0; i < s.rows; ++i)
          for (int j = 0; j < s.cols; ++j)
            ref(out, bb, i, j) = a.at(bb, j, i);
      break;
    }

    case TensorOpKind::Relu: {
      const Value &a = in(op, 0);
      for (int bb = 0; bb < s.batch; ++bb)
        for (int i = 0; i < s.rows; ++i)
          for (int j = 0; j < s.cols; ++j)
            ref(out, bb, i, j) = std::max(a.at(bb, i, j), 0.0f);
      break;
    }

    case TensorOpKind::Softmax: {
      const Value &a = in(op, 0);
      std::vector<float> row(s.cols);
      for (int bb = 0; bb < s.batch; ++bb)
        for (int i = 0; i < s.rows; ++i) {
          for (int j = 0; j < s.cols; ++j)
            row[j] = a.at(bb, i, j);
          softmaxRow(row.data(), &ref(out, bb, i, 0), s.cols);
        }
      break;
    }

    case TensorOpKind::ScratchStore:
    case TensorOpKind::ScratchLoad:
      out = in(op, 0); // placement only - the value is unchanged
      break;

    case TensorOpKind::Print: {
      const Value &a = in(op, 0);
      prints.push_back({op->refName, s.batch, s.rows, s.cols, a.data});
      continue;
    }

    default:
      error = std::string("interpreter: unsupported op ") +
              tensorOpKindName(op->kind);
      return false;
    }
    values[op] = std::move(out);
  }
  return true;
}

bool samePrints(const std::vector<PrintValue> &expected,
                const std::vector<PrintValue> &actual, double tolerance,
                std::string &report) {
  std::ostringstream os;
  if (expected.size() != actual.size()) {
    os << "expected " << expected.size() << " print(s), got " << actual.size();
    report = os.str();
    return false;
  }
  for (size_t p = 0; p < expected.size(); ++p) {
    const PrintValue &e = expected[p], &a = actual[p];
    if (e.batch != a.batch || e.rows != a.rows || e.cols != a.cols ||
        e.values.size() != a.values.size()) {
      os << "print #" << p << " (" << e.name << "): shape differs";
      report = os.str();
      return false;
    }
    for (size_t k = 0; k < e.values.size(); ++k) {
      const double diff = std::fabs(static_cast<double>(e.values[k]) - a.values[k]);
      const double scale = std::max(1.0, std::fabs(static_cast<double>(e.values[k])));
      if (!(diff <= tolerance * scale)) {
        os << "print #" << p << " (" << e.name << ") element " << k
           << ": expected " << e.values[k] << ", got " << a.values[k];
        report = os.str();
        return false;
      }
    }
  }
  return true;
}

} // namespace tensorir
} // namespace matrixdsl
