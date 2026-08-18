//===----------------------------------------------------------------------===//
//
// MDT Instruction Set Simulator
//
// Owner: Shreyas Mandem (24BCE0381)
//
// A functional simulator for the MatrixDSL Target. MDT has no silicon, so this
// is the executable reference for the ISA and the mechanism by which generated
// backend code is verified.
//
// Functional, not cycle-accurate: it models architectural state (registers and
// memory) and instruction semantics, not pipelines, caches or timing.
//
// Where this simulator and docs/isa.md disagree, one of them is wrong and the
// discrepancy must be resolved before the affected instruction is considered
// implemented.
//
//===----------------------------------------------------------------------===//

#ifndef MDTSIM_H
#define MDTSIM_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace mdt {

//===----------------------------------------------------------------------===//
// Architectural constants - see docs/isa.md section 1
//===----------------------------------------------------------------------===//

constexpr int kNumScalarRegs = 16; // R0-R15
constexpr int kNumVectorRegs = 8;  // V0-V7
constexpr int kNumMatrixRegs = 8;  // M0-M7

constexpr int kVectorLanes = 4;             // 128-bit = 4 x FP32
constexpr int kTileDim = 4;                 // 4x4 matrix tile
constexpr int kTileElems = kTileDim * kTileDim; // 16 FP32
constexpr int kTileBytes = kTileElems * 4;  // 64 bytes

constexpr int kRegSP = 13;
constexpr int kRegFP = 14;
constexpr int kRegRA = 15;

constexpr size_t kDefaultMemoryBytes = 1u << 20; // 1 MiB

//===----------------------------------------------------------------------===//
// Instruction representation
//===----------------------------------------------------------------------===//

enum class Opcode {
  NOP,
  // Scalar
  ADD, SUB, MUL,
  // Memory
  LOAD, STORE,
  // Control flow
  BR, BEQ, BNE, JMP, CALL, RET,
  // Vector
  VADD, VMUL,
  // Matrix
  MATMUL, TRANSPOSE, RELU,
  // Simulator control (not part of the 17-instruction ISA)
  HALT,
  INVALID
};

const char *opcodeName(Opcode op);
Opcode parseOpcode(const std::string &mnemonic);

enum class RegClass { None, Scalar, Vector, Matrix };

struct Operand {
  enum class Kind { None, Register, Immediate, Label, Memory };

  Kind kind = Kind::None;
  RegClass regClass = RegClass::None;
  int regIndex = -1;       // Register / Memory base
  int64_t immediate = 0;   // Immediate / Memory offset
  int32_t stride = 0;      // Memory: row stride in bytes (0 = contiguous)
  std::string label;       // Label

  bool isRegister() const { return kind == Kind::Register; }
  bool isMemory() const { return kind == Kind::Memory; }
};

struct Instruction {
  Opcode op = Opcode::INVALID;
  Operand dst, src1, src2;
  int sourceLine = 0;
  std::string text; // original assembly, for tracing
};

//===----------------------------------------------------------------------===//
// Machine state
//===----------------------------------------------------------------------===//

struct MachineState {
  int32_t R[kNumScalarRegs] = {};
  float V[kNumVectorRegs][kVectorLanes] = {};
  float M[kNumMatrixRegs][kTileElems] = {};

  std::vector<uint8_t> memory;
  size_t pc = 0;
  bool halted = false;

  explicit MachineState(size_t memoryBytes = kDefaultMemoryBytes)
      : memory(memoryBytes, 0) {}

  void reset();
};

//===----------------------------------------------------------------------===//
// Assembler
//
// Two passes: the first records label addresses, the second decodes each
// instruction. Labels must be resolvable in one pass over a known symbol
// table, so forward branches work.
//===----------------------------------------------------------------------===//

struct AssemblyError {
  int line = 0;
  std::string message;
  std::string text;
};

class Assembler {
public:
  /// Assemble source text. Returns false if any error was recorded.
  bool assemble(const std::string &source);

  const std::vector<Instruction> &instructions() const { return code_; }
  const std::vector<AssemblyError> &errors() const { return errors_; }

  /// Static data emitted by .word / .float / .space, loaded at address 0.
  const std::vector<uint8_t> &dataSection() const { return data_; }

  const std::unordered_map<std::string, size_t> &labels() const {
    return labels_;
  }

private:
  bool firstPass(const std::string &source);
  bool secondPass(const std::string &source);

  bool decodeInstruction(const std::vector<std::string> &tokens, int line,
                         const std::string &raw, Instruction &out);
  bool parseOperand(const std::string &text, int line, Operand &out);
  bool handleDirective(const std::vector<std::string> &tokens, int line);

  void error(int line, const std::string &message, const std::string &text);

  std::vector<Instruction> code_;
  std::vector<uint8_t> data_;
  std::unordered_map<std::string, size_t> labels_; // label -> instruction index
  std::unordered_map<std::string, size_t> dataLabels_; // label -> byte address
  std::vector<AssemblyError> errors_;
  bool inDataSection_ = false;
};

//===----------------------------------------------------------------------===//
// Simulator
//===----------------------------------------------------------------------===//

struct SimulatorOptions {
  bool trace = false;
  bool dumpScalar = false;
  bool dumpVector = false;
  bool dumpMatrix = false;
  int dumpMatrixIndex = -1; // -1 = all
  size_t maxInstructions = 10000000;
  size_t memoryBytes = kDefaultMemoryBytes;
};

struct RuntimeError {
  size_t pc = 0;
  int sourceLine = 0;
  std::string message;
};

class Simulator {
public:
  explicit Simulator(SimulatorOptions options = {});

  /// Load assembled code and static data.
  void load(const std::vector<Instruction> &code,
            const std::vector<uint8_t> &data);

  /// Run to HALT, RET at top level, or the end of the instruction stream.
  bool run();

  const MachineState &state() const { return state_; }
  MachineState &state() { return state_; }

  size_t executedCount() const { return executed_; }
  const std::vector<RuntimeError> &errors() const { return errors_; }

  void dumpScalarRegisters() const;
  void dumpVectorRegisters() const;
  void dumpMatrixRegister(int index) const;
  void dumpAllMatrixRegisters() const;

private:
  bool step();

  bool execScalarArith(const Instruction &inst);
  bool execLoad(const Instruction &inst);
  bool execStore(const Instruction &inst);
  bool execBranch(const Instruction &inst, bool &branched);
  bool execVector(const Instruction &inst);
  bool execMatrix(const Instruction &inst);

  // Memory access, bounds-checked. Out-of-range access is a runtime error
  // rather than undefined behaviour, so backend bugs surface as diagnostics.
  bool readWord(size_t address, int32_t &out);
  bool writeWord(size_t address, int32_t value);
  bool readFloats(size_t address, float *out, int count, int32_t stride,
                  int rowLength);
  bool writeFloats(size_t address, const float *in, int count, int32_t stride,
                   int rowLength);

  bool checkRegister(const Operand &operand, RegClass expected,
                     const Instruction &inst);

  void runtimeError(const Instruction &inst, const std::string &message);

  MachineState state_;
  std::vector<Instruction> code_;
  SimulatorOptions options_;
  std::vector<RuntimeError> errors_;
  size_t executed_ = 0;
};

//===----------------------------------------------------------------------===//
// Helpers
//===----------------------------------------------------------------------===//

std::string formatMatrix(const float *data);
std::string trim(const std::string &s);
std::vector<std::string> tokenizeLine(const std::string &line);

} // namespace mdt

#endif // MDTSIM_H
