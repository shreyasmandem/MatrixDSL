//===----------------------------------------------------------------------===//
// MatrixDSL - Tensor IR -> MDT assembly code generator implementation
// Owner: Shreyas Mandem (24BCE0381)
//===----------------------------------------------------------------------===//

#include "MDTCodeGen.h"

#include <cstdio>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace matrixdsl {
namespace backend {

using namespace tensorir;

namespace {

int pad4(int n) { return (n + 3) / 4 * 4; }
int tiles(int n) { return (n + 3) / 4; }

std::string fmtFloat(double v) {
  char buf[40];
  std::snprintf(buf, sizeof(buf), "%.9g", static_cast<float>(v));
  return buf;
}

/// One DRAM-resident tensor.
struct Buffer {
  std::string label;
  long long addr = 0;
  TensorShape shape;
  int rp = 0, cp = 0; // padded rows / cols
  long long batchBytes() const { return 1LL * rp * cp * 4; }
  long long bytes() const { return batchBytes() * shape.batch; }
  int stride() const { return cp * 4; }
};

class CodeGen {
public:
  CodeGen(const TensorFunction &fn, const InputMap &inputs)
      : fn_(fn), inputs_(inputs) {}

  CodeGenResult run(const std::string &header) {
    CodeGenResult result;
    layout();
    prologue();
    for (const std::unique_ptr<TensorOp> &op : fn_.ops()) {
      if (!lower(*op)) {
        result.error = error_;
        return result;
      }
    }

    std::ostringstream out;
    out << header << "\n.data\n" << data_.str();
    for (const auto &p : printNames_)
      out << ".printname " << p.first << ", " << p.second << "\n";
    out << "\n.text\n.global _start\n\n_start:\n" << text_.str() << "    HALT\n";
    result.ok = true;
    result.assembly = out.str();
    result.stats = {count_, dataEnd_, static_cast<int>(buffers_.size())};
    return result;
  }

private:
  const TensorFunction &fn_;
  const InputMap &inputs_;
  std::ostringstream data_, text_;
  std::unordered_map<const TensorOp *, Buffer> buffers_;
  std::vector<std::pair<int, std::string>> printNames_;
  long long dataEnd_ = 0;
  long long count_ = 0;
  long long identityAddr_ = -1, negIdentityAddr_ = -1;
  std::unordered_map<const TensorOp *, long long> scaleAddr_;
  std::string error_;

  // ---------------------------------------------------------------- data --

  long long allocate(const std::string &label, long long bytes) {
    const long long aligned = (dataEnd_ + 63) / 64 * 64;
    if (aligned > dataEnd_)
      data_ << "    .space " << (aligned - dataEnd_) << "\n";
    data_ << label << ":\n";
    dataEnd_ = aligned + bytes;
    return aligned;
  }

  void emitFloats(const std::vector<float> &values) {
    for (size_t k = 0; k < values.size(); k += 8) {
      data_ << "    .float ";
      for (size_t i = k; i < k + 8 && i < values.size(); ++i)
        data_ << (i > k ? ", " : "") << fmtFloat(values[i]);
      data_ << "\n";
    }
  }

  long long constantTile(const std::string &label, float diagonal) {
    std::vector<float> tile(16, 0.0f);
    for (int i = 0; i < 4; ++i)
      tile[i * 5] = diagonal;
    const long long addr = allocate(label, 64);
    emitFloats(tile);
    return addr;
  }

  static bool needsBuffer(TensorOpKind k) {
    return k != TensorOpKind::Print && k != TensorOpKind::ScratchStore &&
           k != TensorOpKind::ScratchLoad;
  }

  void layout() {
    bool add = false, sub = false;
    for (const std::unique_ptr<TensorOp> &op : fn_.ops()) {
      add |= op->kind == TensorOpKind::Add;
      sub |= op->kind == TensorOpKind::Sub;
    }
    if (add || sub)
      identityAddr_ = constantTile("__mdt_identity", 1.0f);
    if (sub)
      negIdentityAddr_ = constantTile("__mdt_neg_identity", -1.0f);

    for (const std::unique_ptr<TensorOp> &opPtr : fn_.ops()) {
      const TensorOp *op = opPtr.get();
      if (op->kind == TensorOpKind::ScalarMul)
        scaleAddr_[op] = constantTile("__mdt_scale_" + std::to_string(op->id),
                                      static_cast<float>(op->constValue));
      if (!needsBuffer(op->kind))
        continue;

      Buffer buf;
      buf.shape = op->shape;
      buf.rp = pad4(op->shape.rows);
      buf.cp = pad4(op->shape.cols);
      buf.label = op->kind == TensorOpKind::Load    ? "in_" + op->refName
                  : op->kind == TensorOpKind::Const ? "c" + std::to_string(op->id)
                                                    : "t" + std::to_string(op->id);
      buf.addr = allocate(buf.label, buf.bytes());

      std::vector<float> image;
      if (op->kind == TensorOpKind::Const || op->kind == TensorOpKind::Load) {
        image.assign(static_cast<size_t>(buf.bytes() / 4), 0.0f);
        const std::vector<float> *src = nullptr;
        std::vector<float> lit;
        if (op->kind == TensorOpKind::Const) {
          lit.assign(op->constData.begin(), op->constData.end());
          src = &lit;
        } else {
          auto it = inputs_.find(op->refName);
          if (it != inputs_.end())
            src = &it->second;
        }
        const TensorShape &s = op->shape;
        for (int b = 0; src && b < s.batch; ++b)
          for (int i = 0; i < s.rows; ++i)
            for (int j = 0; j < s.cols; ++j) {
              const size_t from = (static_cast<size_t>(b) * s.rows + i) * s.cols + j;
              if (from < src->size())
                image[(static_cast<size_t>(b) * buf.rp + i) * buf.cp + j] = (*src)[from];
            }
        emitFloats(image);
      } else {
        data_ << "    .space " << buf.bytes() << "\n";
      }
      buffers_[op] = buf;
    }
  }

  // ---------------------------------------------------------------- text --

  void emit(const std::string &line) {
    text_ << "    " << line << "\n";
    ++count_;
  }
  void comment(const std::string &line) { text_ << "    ; " << line << "\n"; }

  void prologue() {
    if (identityAddr_ >= 0)
      emit("LOAD   M7, [R0 + " + std::to_string(identityAddr_) + "]   ; +I for tile add");
    if (negIdentityAddr_ >= 0)
      emit("LOAD   M6, [R0 + " + std::to_string(negIdentityAddr_) + "]   ; -I for tile sub");
  }

  /// Register holding the base address of each DRAM operand of one op.
  std::unordered_map<const TensorOp *, int> base_;

  void setBase(const TensorOp *op, int reg) {
    if (base_.count(op))
      return;
    base_[op] = reg;
    const Buffer &b = buffers_.at(op);
    emit("ADD    R" + std::to_string(reg) + ", R0, " + std::to_string(b.addr) +
         "   ; &" + b.label);
  }

  /// Load tile (b, ti, tj) of `op` into register `m`. Batch broadcasts; a
  /// ScratchLoad operand is read from the scratchpad instead of DRAM.
  void loadTile(const TensorOp *op, int b, int ti, int tj, int m) {
    const std::string reg = "M" + std::to_string(m);
    if (op->kind == TensorOpKind::ScratchLoad) {
      const TensorOp *store = op->operands[0];
      const TensorShape &s = store->shape;
      const int bb = s.batch == 1 ? 0 : b;
      const long long idx = (1LL * bb * tiles(s.rows) + ti) * tiles(s.cols) + tj;
      emit("SCRATCHLOAD  " + reg + ", [R0 + " +
           std::to_string(store->scratchOffset + idx * 64) + "]");
      return;
    }
    const Buffer &buf = buffers_.at(op);
    const int bb = buf.shape.batch == 1 ? 0 : b;
    const long long off = bb * buf.batchBytes() + (4LL * ti * buf.cp + 4LL * tj) * 4;
    const std::string mem = "[R" + std::to_string(base_.at(op)) + " + " +
                            std::to_string(off) + "]";
    if (buf.cp == 4)
      emit("LOAD   " + reg + ", " + mem);
    else
      emit("LOADT  " + reg + ", " + mem + ", " + std::to_string(buf.stride()));
  }

  void storeTile(const TensorOp *op, int b, int ti, int tj, int m) {
    const Buffer &buf = buffers_.at(op);
    const long long off = b * buf.batchBytes() + (4LL * ti * buf.cp + 4LL * tj) * 4;
    const std::string mem = "[R" + std::to_string(base_.at(op)) + " + " +
                            std::to_string(off) + "]";
    const std::string reg = "M" + std::to_string(m);
    if (buf.cp == 4)
      emit("STORE  " + reg + ", " + mem);
    else
      emit("STORET " + reg + ", " + mem + ", " + std::to_string(buf.stride()));
  }

  /// Bind R1.. to every DRAM operand and R4 to the result.
  void bindBases(const TensorOp &op) {
    base_.clear();
    int reg = 1;
    for (const TensorOp *operand : op.operands)
      if (operand->kind != TensorOpKind::ScratchLoad && buffers_.count(operand) &&
          !base_.count(operand))
        setBase(operand, reg++);
    if (buffers_.count(&op)) {
      base_.erase(&op);
      setBase(&op, 4);
    }
  }

  void runtimeCall(const char *fn, const std::vector<long long> &args) {
    for (size_t i = 0; i < args.size(); ++i)
      emit("ADD    R" + std::to_string(i + 1) + ", R0, " + std::to_string(args[i]));
    emit(std::string("CALL   ") + fn);
  }

  bool lower(const TensorOp &op) {
    const TensorShape &s = op.shape;
    const int mt = tiles(s.rows), nt = tiles(s.cols);

    std::ostringstream ir;
    ir << "%" << op.id << " = " << tensorOpKindName(op.kind);
    for (const TensorOp *operand : op.operands)
      ir << " %" << operand->id;
    if (!op.refName.empty())
      ir << " @" << op.refName;
    ir << " : " << s.toString();

    switch (op.kind) {
    case TensorOpKind::Const:
    case TensorOpKind::Load:
    case TensorOpKind::ScratchLoad:
      return true; // data only, or read lazily by the consumer

    case TensorOpKind::ScratchStore: {
      text_ << "\n";
      comment(ir.str() + "  -> scratchpad +" + std::to_string(op.scratchOffset));
      bindBases(op);
      const TensorOp *src = op.operands[0];
      long long idx = 0;
      for (int b = 0; b < s.batch; ++b)
        for (int i = 0; i < mt; ++i)
          for (int j = 0; j < nt; ++j, ++idx) {
            loadTile(src, b, i, j, 3);
            emit("SCRATCHSTORE M3, [R0 + " +
                 std::to_string(op.scratchOffset + idx * 64) + "]");
          }
      return true;
    }

    case TensorOpKind::Print: {
      text_ << "\n";
      comment(ir.str());
      const TensorOp *src = op.operands[0];
      while (src->kind == TensorOpKind::ScratchLoad || src->kind == TensorOpKind::ScratchStore)
        src = src->operands[0];
      const Buffer &buf = buffers_.at(src);
      const int id = static_cast<int>(printNames_.size());
      printNames_.push_back({id, op.refName});
      runtimeCall("mdt_print", {buf.addr, s.batch, s.rows, s.cols, buf.stride(), id});
      return true;
    }

    case TensorOpKind::Softmax: {
      text_ << "\n";
      comment(ir.str());
      const Buffer &in = buffers_.at(op.operands[0]);
      const Buffer &out = buffers_.at(&op);
      runtimeCall("mdt_softmax", {in.addr, out.addr, s.batch, s.rows, s.cols, out.stride()});
      return true;
    }

    default:
      break;
    }

    text_ << "\n";
    comment(ir.str());
    bindBases(op);

    switch (op.kind) {
    case TensorOpKind::MatMul:
    case TensorOpKind::TiledMatMul:
    case TensorOpKind::FusedMatMulAcc:
    case TensorOpKind::FusedMatMulRelu: {
      const TensorOp *a = op.operands[0], *b = op.operands[1];
      const int kt = tiles(a->shape.cols);
      // FusedMatMulAcc, or the Phase 3 three-operand FusedMatMulRelu.
      const bool acc = op.operands.size() == 3;
      const bool relu = op.kind == TensorOpKind::FusedMatMulRelu;
      for (int bb = 0; bb < s.batch; ++bb)
        for (int i = 0; i < mt; ++i)
          for (int j = 0; j < nt; ++j) {
            if (acc)
              loadTile(op.operands[2], bb, i, j, 0);
            for (int k = 0; k < kt; ++k) {
              loadTile(a, bb, i, k, 1);
              loadTile(b, bb, k, j, 2);
              if (relu && kt == 1 && !acc)
                emit("MATMULRELU M0, M1, M2");
              else if (k == 0 && !acc)
                emit("MATMUL M0, M1, M2");
              else
                emit("MATMULACC M0, M1, M2");
            }
            if (relu && (kt > 1 || acc))
              emit("RELU   M0, M0");
            storeTile(&op, bb, i, j, 0);
          }
      return true;
    }

    case TensorOpKind::Add:
    case TensorOpKind::Sub:
      for (int bb = 0; bb < s.batch; ++bb)
        for (int i = 0; i < mt; ++i)
          for (int j = 0; j < nt; ++j) {
            loadTile(op.operands[0], bb, i, j, 0);
            loadTile(op.operands[1], bb, i, j, 1);
            emit(op.kind == TensorOpKind::Add ? "MATMULACC M0, M1, M7   ; M0 += M1"
                                              : "MATMULACC M0, M1, M6   ; M0 -= M1");
            storeTile(&op, bb, i, j, 0);
          }
      return true;

    case TensorOpKind::ScalarMul:
      emit("LOAD   M5, [R0 + " + std::to_string(scaleAddr_.at(&op)) + "]   ; " +
           fmtFloat(op.constValue) + " * I");
      for (int bb = 0; bb < s.batch; ++bb)
        for (int i = 0; i < mt; ++i)
          for (int j = 0; j < nt; ++j) {
            loadTile(op.operands[0], bb, i, j, 1);
            emit("MATMUL M0, M1, M5");
            storeTile(&op, bb, i, j, 0);
          }
      return true;

    case TensorOpKind::Transpose:
    case TensorOpKind::Relu: {
      const bool tr = op.kind == TensorOpKind::Transpose;
      for (int bb = 0; bb < s.batch; ++bb)
        for (int i = 0; i < mt; ++i)
          for (int j = 0; j < nt; ++j) {
            if (tr)
              loadTile(op.operands[0], bb, j, i, 1);
            else
              loadTile(op.operands[0], bb, i, j, 1);
            emit(tr ? "TRANSPOSE M0, M1" : "RELU   M0, M1");
            storeTile(&op, bb, i, j, 0);
          }
      return true;
    }

    default:
      error_ = std::string("MDT code generation: unsupported op ") +
               tensorOpKindName(op.kind);
      return false;
    }
  }
};

} // namespace

CodeGenResult generateMDT(const TensorFunction &fn, const InputMap &inputs,
                          const std::string &header) {
  return CodeGen(fn, inputs).run(header);
}

} // namespace backend
} // namespace matrixdsl
