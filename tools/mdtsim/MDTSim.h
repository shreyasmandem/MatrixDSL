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
// implemented. The same rule now applies to docs/isa-extensions.md and
// docs/memory-hierarchy.md for everything added below.
//
// Phase 2 additions: 7 new instructions (docs/isa-extensions.md section 2), a
// scratchpad address space reached via one address-space bit on LOAD/STORE
// (docs/memory-hierarchy.md section 3), and a lightweight, explicitly
// non-cycle-accurate performance model (section "Performance model" below).
//
//===----------------------------------------------------------------------===//

#ifndef MDTSIM_H
#define MDTSIM_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
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

/// Phase 2: scratchpad size - docs/memory-hierarchy.md section 2. 64 KiB is
/// 1024 tiles of headroom, proportionate to a teaching accelerator; it is not
/// a claim about real hardware (TPU v5e's VMEM is 128 MiB).
constexpr size_t kScratchpadBytes = 64 * 1024;

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
  // Phase 2: fused matrix (docs/isa-extensions.md section 2.1-2.2)
  MATMULACC, MATMULRELU,
  // Phase 2: mixed-precision storage (section 2.3)
  CVT_F32_BF16, CVT_BF16_F32,
  // Phase 2: reduction primitives for softmax (section 2.4)
  VREDSUM, MROWMAX, MROWSUM,
  // Simulator control (not part of the 24-instruction ISA)
  HALT,
  INVALID
};

const char *opcodeName(Opcode op);
Opcode parseOpcode(const std::string &mnemonic);

/// True for MATMUL and its Phase 2 fused variants - the three opcodes that
/// perform a full 4x4 matrix product. Used by the performance model (below)
/// to apply one latency figure to all three without triplicating the table.
bool isMatMulFamily(Opcode op);

enum class RegClass { None, Scalar, Vector, Matrix };

struct Operand {
  enum class Kind { None, Register, Immediate, Label, Memory };

  Kind kind = Kind::None;
  RegClass regClass = RegClass::None;
  int regIndex = -1;       // Register / Memory base
  int64_t immediate = 0;   // Immediate / Memory offset
  int32_t stride = 0;      // Memory: row stride in bytes (0 = contiguous)
  bool scratchpad = false; // Phase 2: Memory - true selects the scratchpad
                           // address space (AS=1), false selects DRAM (AS=0),
                           // docs/memory-hierarchy.md section 3.
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

  std::vector<uint8_t> memory;     // DRAM - Phase 1, unchanged in role
  std::vector<uint8_t> scratchpad; // Phase 2 - docs/memory-hierarchy.md
  size_t pc = 0;
  bool halted = false;

  explicit MachineState(size_t memoryBytes = kDefaultMemoryBytes,
                        size_t scratchpadBytes = kScratchpadBytes)
      : memory(memoryBytes, 0), scratchpad(scratchpadBytes, 0) {}

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
// Performance model - docs/memory-hierarchy.md section 4, docs/
// isa-extensions.md. Explicitly NOT cycle-accurate: a static per-instruction
// latency table plus a simple run-length heuristic for transfer/compute
// overlap, documented in full in MDTSim.cpp next to finalizePerformanceModel.
//===----------------------------------------------------------------------===//

/// Which of the two categories the overlap heuristic reasons about an
/// executed instruction belongs to. Control flow, NOP and HALT are `Other`:
/// they always cost their latency directly and never participate in the
/// max()-pairing the heuristic applies to adjacent Transfer/Compute runs.
enum class PerfClass { Transfer, Compute, Other };

PerfClass classifyForPerf(Opcode op);

/// Per-instruction latency, in the model's abstract "cycles". Deliberately
/// simple and stated as such: DRAM access costs more than scratchpad access
/// (8 vs 1) specifically so that moving traffic off DRAM is visible in the
/// estimate at all - the whole point of the memory hierarchy story. See
/// MDTSim.cpp for the full table and the reasoning behind each figure.
int instructionLatency(Opcode op, bool scratchpadAccess);

struct PerformanceModel {
  long long instructionCount = 0;
  long long bytesMovedDRAM = 0;
  long long bytesMovedScratchpad = 0;

  /// Sum of every executed instruction's latency - what execution would cost
  /// with no overlap at all.
  long long sequentialCycles = 0;

  /// sequentialCycles, but with each maximal adjacent (Transfer-run,
  /// Compute-run) pair collapsed to max(transferCycles, computeCycles)
  /// instead of their sum - the analytical stand-in for double buffering
  /// docs/memory-hierarchy.md section 4 commits to instead of real
  /// concurrency.
  long long overlapAwareCycles = 0;

  /// Human-readable report: instruction count, bytes moved (split DRAM vs
  /// scratchpad), and both cycle estimates side by side. This is the
  /// direct source of the three columns the Review 2 report's section 8
  /// ablation study asks for.
  std::string summary() const;
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
  bool perf = false;        // Phase 2: report the performance model on exit
  size_t maxInstructions = 10000000;
  size_t memoryBytes = kDefaultMemoryBytes;
  size_t scratchpadBytes = kScratchpadBytes;
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

  /// Phase 2: valid after run() returns. See PerformanceModel above.
  const PerformanceModel &performanceModel() const { return perf_; }

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

  /// Phase 2: CVT.F32.BF16 / CVT.BF16.F32 - docs/isa-extensions.md 2.3.
  bool execConvert(const Instruction &inst);

  /// Phase 2: VREDSUM / MROWMAX / MROWSUM - docs/isa-extensions.md 2.4.
  bool execReduce(const Instruction &inst);

  // Memory access, bounds-checked. Out-of-range access is a runtime error
  // rather than undefined behaviour, so backend bugs surface as diagnostics.
  // Phase 2: `scratchpad` selects which array (state_.memory vs
  // state_.scratchpad) the address is resolved against - docs/
  // memory-hierarchy.md section 3's AS bit.
  bool readWord(size_t address, bool scratchpad, int32_t &out);
  bool writeWord(size_t address, bool scratchpad, int32_t value);
  bool readFloats(size_t address, bool scratchpad, float *out, int count,
                  int32_t stride, int rowLength);
  bool writeFloats(size_t address, bool scratchpad, const float *in,
                   int count, int32_t stride, int rowLength);

  bool checkRegister(const Operand &operand, RegClass expected,
                     const Instruction &inst);

  void runtimeError(const Instruction &inst, const std::string &message);

  /// Phase 2: called once per successfully executed instruction from step(),
  /// after latency is known, to fold it into the running performance
  /// totals. `bytesMoved` is 0 for non-memory instructions.
  void recordPerfInstruction(Opcode op, bool scratchpadAccess,
                             int bytesMoved);

  /// Phase 2: called once, at the end of run(), to flush the last pending
  /// run into perf_.overlapAwareCycles. See MDTSim.cpp for why a pending run
  /// can be left over and what "flushing" means.
  void finalizePerformanceModel();

  MachineState state_;
  std::vector<Instruction> code_;
  SimulatorOptions options_;
  std::vector<RuntimeError> errors_;
  size_t executed_ = 0;

  /// Phase 2: set by execLoad/execStore just before they return success, so
  /// step() can fold the transfer size into the performance model without
  /// every memory-instruction call site having to compute it a second time.
  int lastTransferBytes_ = 0;

  // Phase 2 performance model bookkeeping. recordPerfInstruction builds
  // perfRuns_ incrementally as a run-length-encoded list (consecutive
  // instructions of the same PerfClass collapse into one run); the actual
  // pairing happens once, in finalizePerformanceModel, over the finished
  // list. This two-phase split exists because pairing must consume each run
  // exactly once - an earlier single-pass "pair with whatever is currently
  // pending, then let this instruction become the new pending run" design
  // was tried and rejected: an interior run ends up compared against BOTH
  // of its neighbours that way, and gets charged into two overlapping
  // max()s instead of one, which can make the "overlap-aware" estimate
  // exceed the sequential one - a self-evidently wrong result for a value
  // that is supposed to be an upper-bound-reducing estimate.
  PerformanceModel perf_;
  std::vector<std::pair<PerfClass, long long>> perfRuns_;
};

//===----------------------------------------------------------------------===//
// Helpers
//===----------------------------------------------------------------------===//

std::string formatMatrix(const float *data);
std::string trim(const std::string &s);
std::vector<std::string> tokenizeLine(const std::string &line);

} // namespace mdt

#endif // MDTSIM_H
