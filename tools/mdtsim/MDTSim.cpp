//===----------------------------------------------------------------------===//
//
// MDT Simulator - fetch, decode, execute
//
// Owner: Shreyas Mandem (24BCE0381)
//
//===----------------------------------------------------------------------===//

#include "MDTSim.h"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>

namespace mdt {

//===----------------------------------------------------------------------===//
// MachineState
//===----------------------------------------------------------------------===//

void MachineState::reset() {
  std::memset(R, 0, sizeof(R));
  std::memset(V, 0, sizeof(V));
  std::memset(M, 0, sizeof(M));
  std::fill(memory.begin(), memory.end(), static_cast<uint8_t>(0));
  std::fill(scratchpad.begin(), scratchpad.end(), static_cast<uint8_t>(0));
  pc = 0;
  halted = false;
}

//===----------------------------------------------------------------------===//
// Simulator setup
//===----------------------------------------------------------------------===//

Simulator::Simulator(SimulatorOptions options)
    : state_(options.memoryBytes, options.scratchpadBytes), options_(options) {}

void Simulator::load(const std::vector<Instruction> &code,
                     const std::vector<uint8_t> &data) {
  code_ = code;
  state_.reset();

  if (!data.empty()) {
    const size_t n =
        data.size() < state_.memory.size() ? data.size() : state_.memory.size();
    std::memcpy(state_.memory.data(), data.data(), n);
  }

  // Stack pointer starts at the top of memory and grows downward.
  state_.R[kRegSP] = static_cast<int32_t>(state_.memory.size());
  state_.R[kRegFP] = state_.R[kRegSP];
}

void Simulator::runtimeError(const Instruction &inst,
                             const std::string &message) {
  errors_.push_back({state_.pc, inst.sourceLine, message});
  state_.halted = true;
}

//===----------------------------------------------------------------------===//
// Memory access
//
// Every access is bounds-checked. An out-of-range access becomes a diagnostic
// rather than undefined behaviour, so a backend bug shows up as a clear error
// instead of corrupting simulator state.
//===----------------------------------------------------------------------===//

/// Phase 2: every access now resolves against one of two arrays depending on
/// the memory operand's `.scratchpad` flag (docs/memory-hierarchy.md section
/// 3's AS bit) - DRAM (state_.memory, Phase 1's only memory, unchanged in
/// role and size) or the new scratchpad (state_.scratchpad). Bounds are
/// checked against whichever array was selected.
bool Simulator::readWord(size_t address, bool scratchpad, int32_t &out) {
  std::vector<uint8_t> &mem = scratchpad ? state_.scratchpad : state_.memory;
  if (address + 4 > mem.size())
    return false;
  std::memcpy(&out, mem.data() + address, 4);
  return true;
}

bool Simulator::writeWord(size_t address, bool scratchpad, int32_t value) {
  std::vector<uint8_t> &mem = scratchpad ? state_.scratchpad : state_.memory;
  if (address + 4 > mem.size())
    return false;
  std::memcpy(mem.data() + address, &value, 4);
  return true;
}

/// Read `count` floats. With stride == 0 the data is contiguous; otherwise
/// each group of `rowLength` floats starts `stride` bytes after the previous,
/// which is how a 4x4 tile is extracted from a wider matrix.
bool Simulator::readFloats(size_t address, bool scratchpad, float *out,
                           int count, int32_t stride, int rowLength) {
  std::vector<uint8_t> &mem = scratchpad ? state_.scratchpad : state_.memory;

  if (stride == 0) {
    if (address + static_cast<size_t>(count) * 4 > mem.size())
      return false;
    std::memcpy(out, mem.data() + address, static_cast<size_t>(count) * 4);
    return true;
  }

  const int rows = count / rowLength;
  for (int r = 0; r < rows; ++r) {
    const size_t rowAddr = address + static_cast<size_t>(r) * stride;
    if (rowAddr + static_cast<size_t>(rowLength) * 4 > mem.size())
      return false;
    std::memcpy(out + r * rowLength, mem.data() + rowAddr,
                static_cast<size_t>(rowLength) * 4);
  }
  return true;
}

bool Simulator::writeFloats(size_t address, bool scratchpad, const float *in,
                            int count, int32_t stride, int rowLength) {
  std::vector<uint8_t> &mem = scratchpad ? state_.scratchpad : state_.memory;

  if (stride == 0) {
    if (address + static_cast<size_t>(count) * 4 > mem.size())
      return false;
    std::memcpy(mem.data() + address, in, static_cast<size_t>(count) * 4);
    return true;
  }

  const int rows = count / rowLength;
  for (int r = 0; r < rows; ++r) {
    const size_t rowAddr = address + static_cast<size_t>(r) * stride;
    if (rowAddr + static_cast<size_t>(rowLength) * 4 > mem.size())
      return false;
    std::memcpy(mem.data() + rowAddr, in + r * rowLength,
                static_cast<size_t>(rowLength) * 4);
  }
  return true;
}

bool Simulator::checkRegister(const Operand &operand, RegClass expected,
                              const Instruction &inst) {
  if (!operand.isRegister() || operand.regClass != expected) {
    runtimeError(inst, std::string(opcodeName(inst.op)) +
                           ": operand has the wrong register class");
    return false;
  }
  return true;
}

//===----------------------------------------------------------------------===//
// Execution
//===----------------------------------------------------------------------===//

bool Simulator::execScalarArith(const Instruction &inst) {
  if (!checkRegister(inst.dst, RegClass::Scalar, inst) ||
      !checkRegister(inst.src1, RegClass::Scalar, inst))
    return false;

  const int32_t a = state_.R[inst.src1.regIndex];

  // The second source may be a register or an immediate; immediates keep the
  // hand-written test assembly readable.
  int32_t b = 0;
  if (inst.src2.isRegister()) {
    if (!checkRegister(inst.src2, RegClass::Scalar, inst))
      return false;
    b = state_.R[inst.src2.regIndex];
  } else if (inst.src2.kind == Operand::Kind::Immediate) {
    b = static_cast<int32_t>(inst.src2.immediate);
  } else {
    runtimeError(inst, "second source must be a register or an immediate");
    return false;
  }

  int32_t result = 0;
  switch (inst.op) {
  case Opcode::ADD: result = a + b; break;
  case Opcode::SUB: result = a - b; break;
  case Opcode::MUL: result = a * b; break;
  default:
    runtimeError(inst, "not a scalar arithmetic instruction");
    return false;
  }

  state_.R[inst.dst.regIndex] = result;
  return true;
}

bool Simulator::execLoad(const Instruction &inst) {
  if (!inst.src1.isMemory()) {
    runtimeError(inst, "LOAD source must be a memory operand");
    return false;
  }

  const size_t address =
      static_cast<size_t>(state_.R[inst.src1.regIndex] + inst.src1.immediate);
  const bool scratch = inst.src1.scratchpad;

  // The destination register class selects the transfer width:
  //   R -> 4 bytes, V -> 16 bytes (4 lanes), M -> 64 bytes (one 4x4 tile)
  switch (inst.dst.regClass) {
  case RegClass::Scalar: {
    int32_t value = 0;
    if (!readWord(address, scratch, value)) {
      runtimeError(inst, "LOAD out of bounds");
      return false;
    }
    state_.R[inst.dst.regIndex] = value;
    lastTransferBytes_ = 4;
    return true;
  }
  case RegClass::Vector:
    if (!readFloats(address, scratch, state_.V[inst.dst.regIndex],
                    kVectorLanes, 0, 0)) {
      runtimeError(inst, "vector LOAD out of bounds");
      return false;
    }
    lastTransferBytes_ = kVectorLanes * 4;
    return true;
  case RegClass::Matrix:
    if (!readFloats(address, scratch, state_.M[inst.dst.regIndex], kTileElems,
                    inst.src1.stride, kTileDim)) {
      runtimeError(inst, "matrix LOAD out of bounds");
      return false;
    }
    lastTransferBytes_ = kTileBytes;
    return true;
  default:
    runtimeError(inst, "LOAD destination must be a register");
    return false;
  }
}

bool Simulator::execStore(const Instruction &inst) {
  if (!inst.src1.isMemory()) {
    runtimeError(inst, "STORE destination must be a memory operand");
    return false;
  }

  const size_t address =
      static_cast<size_t>(state_.R[inst.src1.regIndex] + inst.src1.immediate);
  const bool scratch = inst.src1.scratchpad;

  switch (inst.dst.regClass) {
  case RegClass::Scalar:
    if (!writeWord(address, scratch, state_.R[inst.dst.regIndex])) {
      runtimeError(inst, "STORE out of bounds");
      return false;
    }
    lastTransferBytes_ = 4;
    return true;
  case RegClass::Vector:
    if (!writeFloats(address, scratch, state_.V[inst.dst.regIndex],
                     kVectorLanes, 0, 0)) {
      runtimeError(inst, "vector STORE out of bounds");
      return false;
    }
    lastTransferBytes_ = kVectorLanes * 4;
    return true;
  case RegClass::Matrix:
    if (!writeFloats(address, scratch, state_.M[inst.dst.regIndex], kTileElems,
                     inst.src1.stride, kTileDim)) {
      runtimeError(inst, "matrix STORE out of bounds");
      return false;
    }
    lastTransferBytes_ = kTileBytes;
    return true;
  default:
    runtimeError(inst, "STORE source must be a register");
    return false;
  }
}

bool Simulator::execBranch(const Instruction &inst, bool &branched) {
  branched = false;

  switch (inst.op) {
  case Opcode::BR: {
    const size_t target = static_cast<size_t>(inst.dst.immediate);
    if (target >= code_.size()) {
      runtimeError(inst, "branch target out of range");
      return false;
    }
    state_.pc = target;
    branched = true;
    return true;
  }

  case Opcode::BEQ:
  case Opcode::BNE: {
    if (!checkRegister(inst.dst, RegClass::Scalar, inst) ||
        !checkRegister(inst.src1, RegClass::Scalar, inst))
      return false;

    const int32_t a = state_.R[inst.dst.regIndex];
    const int32_t b = state_.R[inst.src1.regIndex];
    const bool take = (inst.op == Opcode::BEQ) ? (a == b) : (a != b);

    if (take) {
      const size_t target = static_cast<size_t>(inst.src2.immediate);
      if (target >= code_.size()) {
        runtimeError(inst, "branch target out of range");
        return false;
      }
      state_.pc = target;
      branched = true;
    }
    return true;
  }

  case Opcode::JMP: {
    if (!checkRegister(inst.dst, RegClass::Scalar, inst))
      return false;
    const size_t target = static_cast<size_t>(state_.R[inst.dst.regIndex]);
    if (target >= code_.size()) {
      runtimeError(inst, "indirect jump target out of range");
      return false;
    }
    state_.pc = target;
    branched = true;
    return true;
  }

  case Opcode::CALL: {
    const size_t target = static_cast<size_t>(inst.dst.immediate);
    if (target >= code_.size()) {
      runtimeError(inst, "call target out of range");
      return false;
    }
    state_.R[kRegRA] = static_cast<int32_t>(state_.pc + 1);
    state_.pc = target;
    branched = true;
    return true;
  }

  case Opcode::RET: {
    const int32_t target = state_.R[kRegRA];
    // A RET with no caller ends the program, which is how a top-level
    // routine terminates.
    if (target <= 0 || static_cast<size_t>(target) >= code_.size()) {
      state_.halted = true;
      branched = true;
      return true;
    }
    state_.pc = static_cast<size_t>(target);
    branched = true;
    return true;
  }

  default:
    runtimeError(inst, "not a control-flow instruction");
    return false;
  }
}

bool Simulator::execVector(const Instruction &inst) {
  if (!checkRegister(inst.dst, RegClass::Vector, inst) ||
      !checkRegister(inst.src1, RegClass::Vector, inst) ||
      !checkRegister(inst.src2, RegClass::Vector, inst))
    return false;

  const float *a = state_.V[inst.src1.regIndex];
  const float *b = state_.V[inst.src2.regIndex];
  float *d = state_.V[inst.dst.regIndex];

  for (int i = 0; i < kVectorLanes; ++i)
    d[i] = (inst.op == Opcode::VADD) ? (a[i] + b[i]) : (a[i] * b[i]);

  return true;
}

/// Shared by MATMUL, MATMULACC and MATMULRELU - all three compute the same
/// 4x4 product; they differ only in what happens to the result afterwards
/// (overwrite, accumulate into the destination, or clamp negatives to zero).
static void matmulProduct(const float *a, const float *b, float *result) {
  for (int i = 0; i < kTileDim; ++i)
    for (int j = 0; j < kTileDim; ++j) {
      float sum = 0.0f;
      for (int k = 0; k < kTileDim; ++k)
        sum += a[i * kTileDim + k] * b[k * kTileDim + j];
      result[i * kTileDim + j] = sum;
    }
}

bool Simulator::execMatrix(const Instruction &inst) {
  if (!checkRegister(inst.dst, RegClass::Matrix, inst) ||
      !checkRegister(inst.src1, RegClass::Matrix, inst))
    return false;

  float *d = state_.M[inst.dst.regIndex];
  const float *a = state_.M[inst.src1.regIndex];

  switch (inst.op) {
  case Opcode::MATMUL: {
    if (!checkRegister(inst.src2, RegClass::Matrix, inst))
      return false;
    const float *b = state_.M[inst.src2.regIndex];

    // Compute into a temporary: the destination may alias a source.
    float result[kTileElems];
    matmulProduct(a, b, result);
    std::memcpy(d, result, sizeof(result));
    return true;
  }

  case Opcode::MATMULACC: {
    // docs/isa-extensions.md 2.1: Md = Md + (Ma x Mb). The destination's
    // PRIOR value is read before it is overwritten, so MATMULACC Md, Md, Mb
    // (accumulating in place with Md also as a source) is well-defined.
    if (!checkRegister(inst.src2, RegClass::Matrix, inst))
      return false;
    const float *b = state_.M[inst.src2.regIndex];

    float product[kTileElems];
    matmulProduct(a, b, product);

    float result[kTileElems];
    for (int i = 0; i < kTileElems; ++i)
      result[i] = d[i] + product[i];
    std::memcpy(d, result, sizeof(result));
    return true;
  }

  case Opcode::MATMULRELU: {
    // docs/isa-extensions.md 2.2: Md = relu(Ma x Mb) - the fused epilogue
    // instruction the fusion pass targets for the matmul->relu pattern.
    if (!checkRegister(inst.src2, RegClass::Matrix, inst))
      return false;
    const float *b = state_.M[inst.src2.regIndex];

    float product[kTileElems];
    matmulProduct(a, b, product);

    float result[kTileElems];
    for (int i = 0; i < kTileElems; ++i)
      result[i] = product[i] > 0.0f ? product[i] : 0.0f;
    std::memcpy(d, result, sizeof(result));
    return true;
  }

  case Opcode::TRANSPOSE: {
    float result[kTileElems];
    for (int i = 0; i < kTileDim; ++i)
      for (int j = 0; j < kTileDim; ++j)
        result[i * kTileDim + j] = a[j * kTileDim + i];
    std::memcpy(d, result, sizeof(result));
    return true;
  }

  case Opcode::RELU:
    for (int i = 0; i < kTileElems; ++i)
      d[i] = a[i] > 0.0f ? a[i] : 0.0f;
    return true;

  default:
    runtimeError(inst, "not a matrix instruction");
    return false;
  }
}

//===----------------------------------------------------------------------===//
// Phase 2: mixed-precision storage and reduction primitives
//===----------------------------------------------------------------------===//

namespace {

/// docs/isa-extensions.md 2.3: the BF16 payload lives in the top 16 bits of
/// a 32-bit register, bottom 16 bits zero - a value in that form is already
/// a valid (reduced-precision) FP32 bit pattern, no reinterpretation needed.
int32_t roundToBF16(int32_t fp32Bits) { return fp32Bits & 0xFFFF0000; }

} // namespace

bool Simulator::execConvert(const Instruction &inst) {
  if (!checkRegister(inst.dst, RegClass::Scalar, inst) ||
      !checkRegister(inst.src1, RegClass::Scalar, inst))
    return false;

  const int32_t bits = state_.R[inst.src1.regIndex];

  switch (inst.op) {
  case Opcode::CVT_F32_BF16:
    // The numerically-observable rounding step - docs/isa-extensions.md 2.3.
    state_.R[inst.dst.regIndex] = roundToBF16(bits);
    return true;
  case Opcode::CVT_BF16_F32:
    // An exact copy under the bit convention above - see 2.3 for why this
    // is still a distinct instruction rather than being folded away.
    state_.R[inst.dst.regIndex] = bits;
    return true;
  default:
    runtimeError(inst, "not a conversion instruction");
    return false;
  }
}

bool Simulator::execReduce(const Instruction &inst) {
  switch (inst.op) {
  case Opcode::VREDSUM: {
    if (!checkRegister(inst.dst, RegClass::Scalar, inst) ||
        !checkRegister(inst.src1, RegClass::Vector, inst))
      return false;
    const float *v = state_.V[inst.src1.regIndex];
    const float sum = v[0] + v[1] + v[2] + v[3];
    // docs/isa-extensions.md 2.4: "truncated to i32 view of the sum" - VREDSUM
    // is defined over the scalar register file, so the FP32 result is
    // truncated toward zero into the destination the same way a compiler
    // would for any float-to-int scalar move on this target.
    state_.R[inst.dst.regIndex] = static_cast<int32_t>(sum);
    return true;
  }

  case Opcode::MROWMAX:
  case Opcode::MROWSUM: {
    if (!checkRegister(inst.dst, RegClass::Vector, inst) ||
        !checkRegister(inst.src1, RegClass::Matrix, inst))
      return false;
    const float *m = state_.M[inst.src1.regIndex];
    float *v = state_.V[inst.dst.regIndex];
    for (int row = 0; row < kTileDim; ++row) {
      const float *rowData = m + row * kTileDim;
      if (inst.op == Opcode::MROWMAX) {
        float best = rowData[0];
        for (int col = 1; col < kTileDim; ++col)
          if (rowData[col] > best)
            best = rowData[col];
        v[row] = best;
      } else {
        float sum = 0.0f;
        for (int col = 0; col < kTileDim; ++col)
          sum += rowData[col];
        v[row] = sum;
      }
    }
    return true;
  }

  default:
    runtimeError(inst, "not a reduction instruction");
    return false;
  }
}

//===----------------------------------------------------------------------===//
// Performance model - docs/memory-hierarchy.md section 4, docs/
// isa-extensions.md. See MDTSim.h for what this is and is not: a static
// latency table plus a simple run-pairing heuristic, explicitly not
// cycle-accurate.
//===----------------------------------------------------------------------===//

PerfClass classifyForPerf(Opcode op) {
  switch (op) {
  case Opcode::LOAD:
  case Opcode::STORE:
    return PerfClass::Transfer;

  case Opcode::ADD:
  case Opcode::SUB:
  case Opcode::MUL:
  case Opcode::VADD:
  case Opcode::VMUL:
  case Opcode::MATMUL:
  case Opcode::MATMULACC:
  case Opcode::MATMULRELU:
  case Opcode::TRANSPOSE:
  case Opcode::RELU:
  case Opcode::CVT_F32_BF16:
  case Opcode::CVT_BF16_F32:
  case Opcode::VREDSUM:
  case Opcode::MROWMAX:
  case Opcode::MROWSUM:
    return PerfClass::Compute;

  default:
    // NOP, control flow, CALL/RET, HALT: never part of a transfer/compute
    // overlap pair - see the class comment on PerfClass in MDTSim.h.
    return PerfClass::Other;
  }
}

int instructionLatency(Opcode op, bool scratchpadAccess) {
  // Deliberately simple, and deliberately NOT equal for DRAM vs scratchpad:
  // if the two cost the same, tiling would show zero benefit in the model
  // no matter how well it staged data, which would defeat the entire point
  // of having a memory hierarchy at all. 8 vs 1 is illustrative, not a
  // measurement of anything - it exists to make the ablation study in
  // docs/review2/Review2_Report.md section 8 produce a non-trivial number
  // when scratchpad tiling is toggled on.
  if (op == Opcode::LOAD || op == Opcode::STORE)
    return scratchpadAccess ? 1 : 8;

  if (isMatMulFamily(op))
    return 4; // 64 multiply-accumulates per instruction - the costliest op

  switch (op) {
  case Opcode::VADD:
  case Opcode::VMUL:
  case Opcode::TRANSPOSE:
  case Opcode::RELU:
  case Opcode::VREDSUM:
  case Opcode::MROWMAX:
  case Opcode::MROWSUM:
    return 2;

  case Opcode::ADD:
  case Opcode::SUB:
  case Opcode::MUL:
  case Opcode::CVT_F32_BF16:
  case Opcode::CVT_BF16_F32:
  case Opcode::BR:
  case Opcode::BEQ:
  case Opcode::BNE:
  case Opcode::JMP:
  case Opcode::CALL:
  case Opcode::RET:
    return 1;

  case Opcode::NOP:
  case Opcode::HALT:
    return 0;

  default:
    return 1;
  }
}

std::string PerformanceModel::summary() const {
  std::ostringstream os;
  os << "Performance model (not cycle-accurate - see docs/memory-hierarchy.md"
        " section 4)\n";
  os << "------------------------------------------------------------------"
        "----\n";
  os << "  Instructions executed        : " << instructionCount << "\n";
  os << "  Bytes moved (DRAM)           : " << bytesMovedDRAM << "\n";
  os << "  Bytes moved (scratchpad)     : " << bytesMovedScratchpad << "\n";
  os << "  Estimated cycles, sequential : " << sequentialCycles << "\n";
  os << "  Estimated cycles, overlap-aware: " << overlapAwareCycles << "\n";
  if (sequentialCycles > 0) {
    const double reduction =
        100.0 * static_cast<double>(sequentialCycles - overlapAwareCycles) /
        static_cast<double>(sequentialCycles);
    os << "  Overlap reduction            : ";
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f%%", reduction);
    os << buf << "\n";
  }
  return os.str();
}

void Simulator::recordPerfInstruction(Opcode op, bool scratchpadAccess,
                                      int bytesMoved) {
  ++perf_.instructionCount;

  if (bytesMoved > 0) {
    if (scratchpadAccess)
      perf_.bytesMovedScratchpad += bytesMoved;
    else
      perf_.bytesMovedDRAM += bytesMoved;
  }

  const int latency = instructionLatency(op, scratchpadAccess);
  perf_.sequentialCycles += latency;

  // Build the run-length-encoded trace incrementally: extend the last run if
  // this instruction shares its class, otherwise start a new run. No pairing
  // happens here - see finalizePerformanceModel for why that has to be a
  // separate pass over the finished list rather than done inline.
  const PerfClass cls = classifyForPerf(op);
  if (!perfRuns_.empty() && perfRuns_.back().first == cls)
    perfRuns_.back().second += latency;
  else
    perfRuns_.emplace_back(cls, latency);
}

void Simulator::finalizePerformanceModel() {
  // Non-overlapping pairwise reduction over the run-length-encoded trace.
  // Each run is consumed by AT MOST ONE pairing, which is what makes this
  // correct where the single-pass "pair with whatever is pending" design it
  // replaced was not (see the comment on perfRuns_ in MDTSim.h): run i pairs
  // with run i+1 only, never with i-1 as well.
  //
  // `Other` runs (branches, NOP, HALT, CALL/RET) always cost their cycles
  // directly and are never a pairing partner - they mark a point where the
  // straight-line "transfer then compute" adjacency the heuristic
  // approximates cannot be assumed to hold.
  long long total = 0;
  size_t i = 0;
  while (i < perfRuns_.size()) {
    const PerfClass cls = perfRuns_[i].first;
    const long long cycles = perfRuns_[i].second;

    if (cls == PerfClass::Other) {
      total += cycles;
      ++i;
      continue;
    }

    const bool hasPartner = (i + 1 < perfRuns_.size()) &&
                            (perfRuns_[i + 1].first != PerfClass::Other) &&
                            (perfRuns_[i + 1].first != cls);
    if (hasPartner) {
      const long long partnerCycles = perfRuns_[i + 1].second;
      total += (cycles > partnerCycles ? cycles : partnerCycles);
      i += 2;
    } else {
      total += cycles;
      i += 1;
    }
  }

  perf_.overlapAwareCycles = total;
}

bool Simulator::step() {
  if (state_.pc >= code_.size()) {
    state_.halted = true;
    return true;
  }

  const Instruction &inst = code_[state_.pc];

  if (options_.trace)
    std::printf("  [%4zu] %s\n", state_.pc, inst.text.c_str());

  bool branched = false;
  bool ok = true;

  switch (inst.op) {
  case Opcode::NOP:
    break;

  case Opcode::ADD:
  case Opcode::SUB:
  case Opcode::MUL:
    ok = execScalarArith(inst);
    break;

  case Opcode::LOAD:
    ok = execLoad(inst);
    break;
  case Opcode::STORE:
    ok = execStore(inst);
    break;

  case Opcode::BR:
  case Opcode::BEQ:
  case Opcode::BNE:
  case Opcode::JMP:
  case Opcode::CALL:
  case Opcode::RET:
    ok = execBranch(inst, branched);
    break;

  case Opcode::VADD:
  case Opcode::VMUL:
    ok = execVector(inst);
    break;

  case Opcode::MATMUL:
  case Opcode::MATMULACC:
  case Opcode::MATMULRELU:
  case Opcode::TRANSPOSE:
  case Opcode::RELU:
    ok = execMatrix(inst);
    break;

  case Opcode::CVT_F32_BF16:
  case Opcode::CVT_BF16_F32:
    ok = execConvert(inst);
    break;

  case Opcode::VREDSUM:
  case Opcode::MROWMAX:
  case Opcode::MROWSUM:
    ok = execReduce(inst);
    break;

  case Opcode::HALT:
    state_.halted = true;
    return true;

  default:
    runtimeError(inst, "unimplemented instruction");
    return false;
  }

  ++executed_;

  if (ok) {
    // Phase 2: fold this instruction into the running performance totals.
    // lastTransferBytes_ was set by execLoad/execStore just above for LOAD/
    // STORE (including their LOADT/STORET/SCRATCHLOAD/SCRATCHSTORE forms);
    // every other instruction moves no memory traffic.
    const bool isMemoryOp = (inst.op == Opcode::LOAD || inst.op == Opcode::STORE);
    const int bytesMoved = isMemoryOp ? lastTransferBytes_ : 0;
    recordPerfInstruction(inst.op, isMemoryOp && inst.src1.scratchpad,
                          bytesMoved);
    lastTransferBytes_ = 0;
  }

  if (!branched && !state_.halted)
    ++state_.pc;

  return ok;
}

bool Simulator::run() {
  executed_ = 0;
  errors_.clear();
  perf_ = PerformanceModel();
  perfRuns_.clear();

  while (!state_.halted && state_.pc < code_.size()) {
    if (executed_ >= options_.maxInstructions) {
      errors_.push_back(
          {state_.pc, 0, "instruction limit reached - possible infinite loop"});
      return false;
    }
    if (!step())
      return false;
  }

  finalizePerformanceModel();

  return errors_.empty();
}

//===----------------------------------------------------------------------===//
// Output
//===----------------------------------------------------------------------===//

std::string formatMatrix(const float *data) {
  std::ostringstream os;
  char buffer[32];
  for (int i = 0; i < kTileDim; ++i) {
    os << "  [";
    for (int j = 0; j < kTileDim; ++j) {
      std::snprintf(buffer, sizeof(buffer), "%9.2f", data[i * kTileDim + j]);
      os << buffer;
    }
    os << " ]\n";
  }
  return os.str();
}

void Simulator::dumpScalarRegisters() const {
  std::printf("Scalar registers:\n");
  for (int i = 0; i < kNumScalarRegs; ++i) {
    std::printf("  R%-2d = %11d", i, state_.R[i]);
    if (i == kRegSP)      std::printf("   (SP)");
    else if (i == kRegFP) std::printf("   (FP)");
    else if (i == kRegRA) std::printf("   (RA)");
    std::printf("\n");
  }
}

void Simulator::dumpVectorRegisters() const {
  std::printf("Vector registers:\n");
  for (int i = 0; i < kNumVectorRegs; ++i) {
    std::printf("  V%d = [", i);
    for (int lane = 0; lane < kVectorLanes; ++lane)
      std::printf("%9.2f", state_.V[i][lane]);
    std::printf(" ]\n");
  }
}

void Simulator::dumpMatrixRegister(int index) const {
  if (index < 0 || index >= kNumMatrixRegs) {
    std::printf("  (no such matrix register: M%d)\n", index);
    return;
  }
  std::printf("M%d =\n%s", index, formatMatrix(state_.M[index]).c_str());
}

void Simulator::dumpAllMatrixRegisters() const {
  for (int i = 0; i < kNumMatrixRegs; ++i)
    dumpMatrixRegister(i);
}

} // namespace mdt
