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
  pc = 0;
  halted = false;
}

//===----------------------------------------------------------------------===//
// Simulator setup
//===----------------------------------------------------------------------===//

Simulator::Simulator(SimulatorOptions options)
    : state_(options.memoryBytes), options_(options) {}

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

bool Simulator::readWord(size_t address, int32_t &out) {
  if (address + 4 > state_.memory.size())
    return false;
  std::memcpy(&out, state_.memory.data() + address, 4);
  return true;
}

bool Simulator::writeWord(size_t address, int32_t value) {
  if (address + 4 > state_.memory.size())
    return false;
  std::memcpy(state_.memory.data() + address, &value, 4);
  return true;
}

/// Read `count` floats. With stride == 0 the data is contiguous; otherwise
/// each group of `rowLength` floats starts `stride` bytes after the previous,
/// which is how a 4x4 tile is extracted from a wider matrix.
bool Simulator::readFloats(size_t address, float *out, int count,
                           int32_t stride, int rowLength) {
  if (stride == 0) {
    if (address + static_cast<size_t>(count) * 4 > state_.memory.size())
      return false;
    std::memcpy(out, state_.memory.data() + address,
                static_cast<size_t>(count) * 4);
    return true;
  }

  const int rows = count / rowLength;
  for (int r = 0; r < rows; ++r) {
    const size_t rowAddr = address + static_cast<size_t>(r) * stride;
    if (rowAddr + static_cast<size_t>(rowLength) * 4 > state_.memory.size())
      return false;
    std::memcpy(out + r * rowLength, state_.memory.data() + rowAddr,
                static_cast<size_t>(rowLength) * 4);
  }
  return true;
}

bool Simulator::writeFloats(size_t address, const float *in, int count,
                            int32_t stride, int rowLength) {
  if (stride == 0) {
    if (address + static_cast<size_t>(count) * 4 > state_.memory.size())
      return false;
    std::memcpy(state_.memory.data() + address, in,
                static_cast<size_t>(count) * 4);
    return true;
  }

  const int rows = count / rowLength;
  for (int r = 0; r < rows; ++r) {
    const size_t rowAddr = address + static_cast<size_t>(r) * stride;
    if (rowAddr + static_cast<size_t>(rowLength) * 4 > state_.memory.size())
      return false;
    std::memcpy(state_.memory.data() + rowAddr, in + r * rowLength,
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

  // The destination register class selects the transfer width:
  //   R -> 4 bytes, V -> 16 bytes (4 lanes), M -> 64 bytes (one 4x4 tile)
  switch (inst.dst.regClass) {
  case RegClass::Scalar: {
    int32_t value = 0;
    if (!readWord(address, value)) {
      runtimeError(inst, "LOAD out of bounds");
      return false;
    }
    state_.R[inst.dst.regIndex] = value;
    return true;
  }
  case RegClass::Vector:
    if (!readFloats(address, state_.V[inst.dst.regIndex], kVectorLanes, 0, 0)) {
      runtimeError(inst, "vector LOAD out of bounds");
      return false;
    }
    return true;
  case RegClass::Matrix:
    if (!readFloats(address, state_.M[inst.dst.regIndex], kTileElems,
                    inst.src1.stride, kTileDim)) {
      runtimeError(inst, "matrix LOAD out of bounds");
      return false;
    }
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

  switch (inst.dst.regClass) {
  case RegClass::Scalar:
    if (!writeWord(address, state_.R[inst.dst.regIndex])) {
      runtimeError(inst, "STORE out of bounds");
      return false;
    }
    return true;
  case RegClass::Vector:
    if (!writeFloats(address, state_.V[inst.dst.regIndex], kVectorLanes, 0,
                     0)) {
      runtimeError(inst, "vector STORE out of bounds");
      return false;
    }
    return true;
  case RegClass::Matrix:
    if (!writeFloats(address, state_.M[inst.dst.regIndex], kTileElems,
                     inst.src1.stride, kTileDim)) {
      runtimeError(inst, "matrix STORE out of bounds");
      return false;
    }
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

    // Accumulate into a temporary: the destination may alias a source.
    float result[kTileElems];
    for (int i = 0; i < kTileDim; ++i)
      for (int j = 0; j < kTileDim; ++j) {
        float sum = 0.0f;
        for (int k = 0; k < kTileDim; ++k)
          sum += a[i * kTileDim + k] * b[k * kTileDim + j];
        result[i * kTileDim + j] = sum;
      }
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
  case Opcode::TRANSPOSE:
  case Opcode::RELU:
    ok = execMatrix(inst);
    break;

  case Opcode::HALT:
    state_.halted = true;
    return true;

  default:
    runtimeError(inst, "unimplemented instruction");
    return false;
  }

  ++executed_;

  if (!branched && !state_.halted)
    ++state_.pc;

  return ok;
}

bool Simulator::run() {
  executed_ = 0;
  errors_.clear();

  while (!state_.halted && state_.pc < code_.size()) {
    if (executed_ >= options_.maxInstructions) {
      errors_.push_back(
          {state_.pc, 0, "instruction limit reached - possible infinite loop"});
      return false;
    }
    if (!step())
      return false;
  }

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
