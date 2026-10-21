//===----------------------------------------------------------------------===//
// MatrixDSL - Tensor IR optimisation passes implementation
// Owner: Kandi Jeevitesh Reddy (24BCE0350)
//===----------------------------------------------------------------------===//

#include "Passes.h"

#include <algorithm>
#include <unordered_set>

namespace matrixdsl {
namespace tensorir {

int runDeadCodeElimination(TensorFunction &fn) {
  // Mark: Prints are the roots; walk operands backwards. Ops are in SSA
  // order, so a single reverse sweep reaches a fixed point.
  std::unordered_set<const TensorOp *> live;
  const auto &ops = fn.ops();
  for (auto it = ops.rbegin(); it != ops.rend(); ++it) {
    const TensorOp *op = it->get();
    if (op->kind == TensorOpKind::Print || live.count(op))
      for (const TensorOp *operand : op->operands)
        live.insert(operand);
    if (op->kind == TensorOpKind::Print)
      live.insert(op);
  }
  return static_cast<int>(
      fn.eraseIf([&](const TensorOp *op) { return !live.count(op); }));
}

namespace {

struct Candidate {
  TensorOp *consumer;
  int operandIndex;
  int tiles;
  int uses;
  long long saved; // DRAM bytes no longer re-read
};

} // namespace

TilingReport runTilingPass(TensorFunction &fn, int scratchpadBytes) {
  TilingReport report;
  std::vector<const TensorOp *> owners; // consumer of report.staged[i]

  // Snapshot the consumers first: inserting staging ops reallocates the list.
  std::vector<TensorOp *> consumers;
  for (const std::unique_ptr<TensorOp> &op : fn.ops())
    if (isMatMulFamily(op->kind))
      consumers.push_back(op.get());

  for (TensorOp *op : consumers) {
    const TensorShape out = op->shape;
    const TensorShape a = op->operands[0]->shape;
    const TensorShape b = op->operands[1]->shape;
    const int mt = tilesOf(out.rows), nt = tilesOf(out.cols), kt = tilesOf(a.cols);

    if (op->kind == TensorOpKind::MatMul && mt * nt * kt > 1) {
      op->kind = TensorOpKind::TiledMatMul;
      ++report.tiledMatMuls;
    }

    // Each A tile (row-block i, k) is read once per output column block j,
    // and once per batch when A is broadcast (batch 1). Symmetric for B.
    std::vector<Candidate> cands;
    const int aTiles = a.batch * mt * kt;
    const int aUses = nt * (a.batch == 1 ? out.batch : 1);
    const int bTiles = b.batch * kt * nt;
    const int bUses = mt * (b.batch == 1 ? out.batch : 1);
    if (aUses > 1)
      cands.push_back({op, 0, aTiles, aUses, 1LL * aTiles * kTileBytes * (aUses - 1)});
    if (bUses > 1)
      cands.push_back({op, 1, bTiles, bUses, 1LL * bTiles * kTileBytes * (bUses - 1)});
    std::sort(cands.begin(), cands.end(),
              [](const Candidate &x, const Candidate &y) { return x.saved > y.saved; });

    // The scratchpad is free again once this op finishes, so each consumer
    // packs its own staging area from offset 0.
    int used = 0;
    for (const Candidate &c : cands) {
      const int bytes = c.tiles * kTileBytes;
      if (used + bytes > scratchpadBytes) {
        report.notes.push_back("%" + std::to_string(op->id) + " operand " +
                               (c.operandIndex == 0 ? "A" : "B") + " (" +
                               std::to_string(bytes / 1024) +
                               " KiB) does not fit the remaining scratchpad");
        continue;
      }
      TensorOp *source = op->operands[c.operandIndex];
      TensorOp *store = fn.insertOpBefore(op, TensorOpKind::ScratchStore,
                                          source->shape, {source});
      store->scratchOffset = used;
      TensorOp *load = fn.insertOpBefore(op, TensorOpKind::ScratchLoad,
                                         source->shape, {store});
      op->operands[c.operandIndex] = load;
      used += bytes;
      owners.push_back(op);
      report.staged.push_back({-1, c.operandIndex, c.tiles, c.uses, c.saved});
    }
  }
  // Ids are final only now: every insertion above renumbered the function.
  for (size_t i = 0; i < owners.size(); ++i)
    report.staged[i].consumerId = owners[i]->id;
  return report;
}

} // namespace tensorir
} // namespace matrixdsl
