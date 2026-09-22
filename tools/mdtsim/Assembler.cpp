//===----------------------------------------------------------------------===//
//
// MDT Assembler - text to Instruction
//
// Owner: Shreyas Mandem (24BCE0381)
//
//===----------------------------------------------------------------------===//

#include "MDTSim.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace mdt {

//===----------------------------------------------------------------------===//
// Text helpers
//===----------------------------------------------------------------------===//

std::string trim(const std::string &s) {
  const size_t first = s.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return "";
  const size_t last = s.find_last_not_of(" \t\r\n");
  return s.substr(first, last - first + 1);
}

/// Split a line into an opcode token followed by operand tokens.
///
/// Commas separate operands; whitespace separates the mnemonic from the first
/// operand. Memory operands keep their brackets intact so that "[R2 + 8]"
/// arrives as a single token.
std::vector<std::string> tokenizeLine(const std::string &line) {
  std::vector<std::string> tokens;

  // Strip comments: ';' or '//' to end of line.
  std::string work = line;
  const size_t semi = work.find(';');
  if (semi != std::string::npos)
    work = work.substr(0, semi);
  const size_t slashes = work.find("//");
  if (slashes != std::string::npos)
    work = work.substr(0, slashes);

  work = trim(work);
  if (work.empty())
    return tokens;

  std::string current;
  int bracketDepth = 0;

  for (const char c : work) {
    if (c == '[') {
      ++bracketDepth;
      current += c;
    } else if (c == ']') {
      --bracketDepth;
      current += c;
    } else if (c == ',' && bracketDepth == 0) {
      const std::string t = trim(current);
      if (!t.empty())
        tokens.push_back(t);
      current.clear();
    } else if (std::isspace(static_cast<unsigned char>(c)) &&
               bracketDepth == 0 && tokens.empty() && !trim(current).empty()) {
      // The first whitespace run separates the mnemonic from the operands.
      tokens.push_back(trim(current));
      current.clear();
    } else {
      current += c;
    }
  }

  const std::string t = trim(current);
  if (!t.empty())
    tokens.push_back(t);

  return tokens;
}

//===----------------------------------------------------------------------===//
// Opcodes
//===----------------------------------------------------------------------===//

namespace {

struct OpcodeEntry {
  const char *name;
  Opcode op;
};

const OpcodeEntry kOpcodes[] = {
    {"NOP", Opcode::NOP},         {"ADD", Opcode::ADD},
    {"SUB", Opcode::SUB},         {"MUL", Opcode::MUL},
    {"LOAD", Opcode::LOAD},       {"STORE", Opcode::STORE},
    {"LOADT", Opcode::LOAD},      {"STORET", Opcode::STORE},
    // Phase 2: SCRATCHLOAD/SCRATCHSTORE are LOAD/STORE with the scratchpad
    // address-space bit set (docs/isa-extensions.md section 3) - same
    // "share the opcode, distinguish by mnemonic" pattern as LOADT/STORET
    // above, not a new opcode.
    {"SCRATCHLOAD", Opcode::LOAD},  {"SCRATCHSTORE", Opcode::STORE},
    {"BR", Opcode::BR},           {"BEQ", Opcode::BEQ},
    {"BNE", Opcode::BNE},         {"JMP", Opcode::JMP},
    {"CALL", Opcode::CALL},       {"RET", Opcode::RET},
    {"VADD", Opcode::VADD},       {"VMUL", Opcode::VMUL},
    {"MATMUL", Opcode::MATMUL},   {"TRANSPOSE", Opcode::TRANSPOSE},
    {"RELU", Opcode::RELU},       {"HALT", Opcode::HALT},
    // Phase 2 real opcodes (docs/isa-extensions.md section 4: 0x50-0x72)
    {"MATMULACC", Opcode::MATMULACC},   {"MATMULRELU", Opcode::MATMULRELU},
    {"CVT.F32.BF16", Opcode::CVT_F32_BF16},
    {"CVT.BF16.F32", Opcode::CVT_BF16_F32},
    {"VREDSUM", Opcode::VREDSUM},
    {"MROWMAX", Opcode::MROWMAX}, {"MROWSUM", Opcode::MROWSUM},
};

std::string toUpper(const std::string &s) {
  std::string out = s;
  for (char &c : out)
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return out;
}

} // namespace

const char *opcodeName(Opcode op) {
  for (const OpcodeEntry &e : kOpcodes)
    if (e.op == op)
      return e.name;
  return "INVALID";
}

Opcode parseOpcode(const std::string &mnemonic) {
  const std::string upper = toUpper(mnemonic);
  for (const OpcodeEntry &e : kOpcodes)
    if (upper == e.name)
      return e.op;
  return Opcode::INVALID;
}

bool isMatMulFamily(Opcode op) {
  return op == Opcode::MATMUL || op == Opcode::MATMULACC ||
         op == Opcode::MATMULRELU;
}

//===----------------------------------------------------------------------===//
// Operand parsing
//===----------------------------------------------------------------------===//

namespace {

/// Recognise R0-R15, V0-V7, M0-M7. Returns false if `text` is not a register.
bool parseRegister(const std::string &text, RegClass &cls, int &index) {
  if (text.size() < 2)
    return false;

  const char prefix =
      static_cast<char>(std::toupper(static_cast<unsigned char>(text[0])));

  int limit = 0;
  switch (prefix) {
  case 'R': cls = RegClass::Scalar; limit = kNumScalarRegs; break;
  case 'V': cls = RegClass::Vector; limit = kNumVectorRegs; break;
  case 'M': cls = RegClass::Matrix; limit = kNumMatrixRegs; break;
  default:  return false;
  }

  for (size_t i = 1; i < text.size(); ++i)
    if (!std::isdigit(static_cast<unsigned char>(text[i])))
      return false;

  index = std::atoi(text.c_str() + 1);
  return index >= 0 && index < limit;
}

bool parseInteger(const std::string &text, int64_t &out) {
  if (text.empty())
    return false;

  size_t i = 0;
  bool negative = false;
  if (text[0] == '-' || text[0] == '+') {
    negative = (text[0] == '-');
    i = 1;
    // A memory operand's offset is tokenized as "+ 16" (space preserved
    // between the sign and the digits - see tokenizeLine, which keeps
    // everything inside brackets verbatim), so the sign and the digits are
    // not necessarily adjacent. Skip past that whitespace explicitly rather
    // than requiring the caller to have already stripped it.
    while (i < text.size() &&
           std::isspace(static_cast<unsigned char>(text[i])))
      ++i;
  }
  if (i >= text.size())
    return false;

  // Hexadecimal
  if (i + 1 < text.size() && text[i] == '0' &&
      (text[i + 1] == 'x' || text[i + 1] == 'X')) {
    char *end = nullptr;
    const long long v = std::strtoll(text.c_str() + i + 2, &end, 16);
    if (end == text.c_str() + i + 2)
      return false;
    out = negative ? -v : v;
    return true;
  }

  for (size_t j = i; j < text.size(); ++j)
    if (!std::isdigit(static_cast<unsigned char>(text[j])))
      return false;

  // Parse from the validated digit substring directly rather than handing
  // the original (possibly sign-space-digit) text to atoll: atoll's
  // whitespace-skipping is only specified before the sign, not between the
  // sign and the first digit, so trusting it here would be relying on
  // unspecified behaviour rather than a documented guarantee.
  const long long magnitude = std::atoll(text.c_str() + i);
  out = negative ? -magnitude : magnitude;
  return true;
}

} // namespace

bool Assembler::parseOperand(const std::string &text, int line, Operand &out) {
  const std::string t = trim(text);
  if (t.empty())
    return false;

  // Memory: [Rbase + offset]  or  [Rbase]  or  [Rbase + offset, stride]
  if (t.front() == '[' && t.back() == ']') {
    std::string inner = trim(t.substr(1, t.size() - 2));

    // Optional stride after a comma inside the brackets.
    int32_t stride = 0;
    const size_t comma = inner.find(',');
    if (comma != std::string::npos) {
      int64_t s = 0;
      if (!parseInteger(trim(inner.substr(comma + 1)), s)) {
        error(line, "invalid stride in memory operand", text);
        return false;
      }
      stride = static_cast<int32_t>(s);
      inner = trim(inner.substr(0, comma));
    }

    std::string baseText = inner;
    int64_t offset = 0;

    const size_t plus = inner.find_first_of("+-");
    if (plus != std::string::npos && plus > 0) {
      baseText = trim(inner.substr(0, plus));
      const std::string offsetText = trim(inner.substr(plus));
      if (!parseInteger(offsetText, offset)) {
        error(line, "invalid offset in memory operand", text);
        return false;
      }
    }

    RegClass cls = RegClass::None;
    int index = -1;
    if (!parseRegister(baseText, cls, index) || cls != RegClass::Scalar) {
      error(line, "memory base must be a scalar register R0-R15", text);
      return false;
    }

    out.kind = Operand::Kind::Memory;
    out.regClass = RegClass::Scalar;
    out.regIndex = index;
    out.immediate = offset;
    out.stride = stride;
    return true;
  }

  // Register
  RegClass cls = RegClass::None;
  int index = -1;
  if (parseRegister(t, cls, index)) {
    out.kind = Operand::Kind::Register;
    out.regClass = cls;
    out.regIndex = index;
    return true;
  }

  // Immediate
  int64_t value = 0;
  if (parseInteger(t, value)) {
    out.kind = Operand::Kind::Immediate;
    out.immediate = value;
    return true;
  }

  // Otherwise a label reference.
  out.kind = Operand::Kind::Label;
  out.label = t;
  return true;
}

//===----------------------------------------------------------------------===//
// Directives
//===----------------------------------------------------------------------===//

bool Assembler::handleDirective(const std::vector<std::string> &tokens,
                                int line) {
  const std::string directive = tokens[0];

  if (directive == ".text") {
    inDataSection_ = false;
    return true;
  }
  if (directive == ".data") {
    inDataSection_ = true;
    return true;
  }
  if (directive == ".global" || directive == ".globl") {
    return true; // No linker; exports are a no-op.
  }

  if (directive == ".word") {
    for (size_t i = 1; i < tokens.size(); ++i) {
      int64_t v = 0;
      if (!parseInteger(trim(tokens[i]), v)) {
        error(line, "invalid .word value", tokens[i]);
        return false;
      }
      const int32_t w = static_cast<int32_t>(v);
      uint8_t bytes[4];
      std::memcpy(bytes, &w, 4);
      data_.insert(data_.end(), bytes, bytes + 4);
    }
    return true;
  }

  if (directive == ".float") {
    for (size_t i = 1; i < tokens.size(); ++i) {
      const float f = static_cast<float>(std::atof(trim(tokens[i]).c_str()));
      uint8_t bytes[4];
      std::memcpy(bytes, &f, 4);
      data_.insert(data_.end(), bytes, bytes + 4);
    }
    return true;
  }

  if (directive == ".space") {
    if (tokens.size() < 2) {
      error(line, ".space requires a byte count", "");
      return false;
    }
    int64_t n = 0;
    if (!parseInteger(trim(tokens[1]), n) || n < 0) {
      error(line, "invalid .space size", tokens[1]);
      return false;
    }
    data_.insert(data_.end(), static_cast<size_t>(n), 0);
    return true;
  }

  error(line, "unknown directive", directive);
  return false;
}

//===----------------------------------------------------------------------===//
// Instruction decoding
//===----------------------------------------------------------------------===//

bool Assembler::decodeInstruction(const std::vector<std::string> &tokens,
                                  int line, const std::string &raw,
                                  Instruction &out) {
  out.op = parseOpcode(tokens[0]);
  out.sourceLine = line;
  out.text = raw;

  if (out.op == Opcode::INVALID) {
    error(line, "unknown instruction", tokens[0]);
    return false;
  }

  // LOADT/STORET both assemble to the LOAD/STORE opcode (see kOpcodes), but
  // take one extra trailing operand - the row stride in bytes, per
  // docs/isa.md section 4.2's "strided variants... share the LOAD/STORE
  // opcodes with a mode bit". The mnemonic itself, not the resulting Opcode
  // (which is identical for LOAD and LOADT), is what distinguishes the two
  // arities, so it has to be checked here rather than in the switch below.
  const std::string upperMnemonic = toUpper(tokens[0]);
  const bool isStrided = (upperMnemonic == "LOADT" || upperMnemonic == "STORET");

  const size_t operandCount = tokens.size() - 1;

  // Arity check up front, so a malformed instruction produces one clear
  // diagnostic rather than a confusing downstream failure.
  size_t expected = 0;
  if (isStrided) {
    expected = 3; // Md/Ms, [Rbase + imm], stride
  } else {
    switch (out.op) {
    case Opcode::NOP:
    case Opcode::RET:
    case Opcode::HALT:
      expected = 0;
      break;
    case Opcode::BR:
    case Opcode::JMP:
    case Opcode::CALL:
      expected = 1;
      break;
    case Opcode::LOAD:
    case Opcode::STORE:
    case Opcode::TRANSPOSE:
    case Opcode::RELU:
    // Phase 2: all four take (dst, src) - same shape as TRANSPOSE/RELU.
    case Opcode::CVT_F32_BF16:
    case Opcode::CVT_BF16_F32:
    case Opcode::VREDSUM:
    case Opcode::MROWMAX:
    case Opcode::MROWSUM:
      expected = 2;
      break;
    default:
      // ADD SUB MUL BEQ BNE VADD VMUL MATMUL, plus Phase 2's MATMULACC and
      // MATMULRELU - all five/seven take (dst, src1, src2).
      expected = 3;
      break;
    }
  }

  if (operandCount != expected) {
    std::ostringstream os;
    os << (isStrided ? upperMnemonic : std::string(opcodeName(out.op)))
       << " expects " << expected << " operand(s), got " << operandCount;
    error(line, os.str(), raw);
    return false;
  }

  Operand *slots[3] = {&out.dst, &out.src1, &out.src2};
  for (size_t i = 0; i < operandCount && i < 3; ++i)
    if (!parseOperand(tokens[i + 1], line, *slots[i]))
      return false;

  if (isStrided) {
    // The trailing stride operand is folded into the memory operand's own
    // `.stride` field, which is where the simulator's execLoad/execStore
    // actually look for it (MDTSim.cpp readFloats/writeFloats). src2 itself
    // plays no further role once this transfer happens.
    if (!out.src1.isMemory()) {
      error(line, std::string(upperMnemonic) +
                     ": second operand must be a memory operand", raw);
      return false;
    }
    if (out.src2.kind != Operand::Kind::Immediate) {
      error(line, std::string(upperMnemonic) +
                     ": stride operand must be an immediate", raw);
      return false;
    }
    out.src1.stride = static_cast<int32_t>(out.src2.immediate);
  }

  // Phase 2: SCRATCHLOAD/SCRATCHSTORE select the scratchpad address space by
  // setting the memory operand's `.scratchpad` flag - docs/memory-hierarchy.md
  // section 3's AS bit. Same "mnemonic implies a flag on the memory operand"
  // shape as the stride transfer above, but simpler: no extra operand to
  // fold in, the address-space choice is entirely determined by which
  // mnemonic was written.
  if (upperMnemonic == "SCRATCHLOAD" || upperMnemonic == "SCRATCHSTORE") {
    if (!out.src1.isMemory()) {
      error(line, upperMnemonic + ": second operand must be a memory operand",
            raw);
      return false;
    }
    out.src1.scratchpad = true;
  }

  return true;
}

//===----------------------------------------------------------------------===//
// Passes
//===----------------------------------------------------------------------===//

void Assembler::error(int line, const std::string &message,
                      const std::string &text) {
  errors_.push_back({line, message, text});
}

bool Assembler::firstPass(const std::string &source) {
  std::istringstream in(source);
  std::string line;
  size_t instructionIndex = 0;
  bool dataSection = false;

  // Labels recorded here carry no line number - firstPass only needs to know
  // WHERE each label points (an instruction index or a data offset), not
  // where it was written; that is why unlike secondPass, no lineNo counter
  // is tracked or threaded through error() here.
  while (std::getline(in, line)) {
    std::string work = trim(line);

    // Strip comments before looking for labels.
    const size_t semi = work.find(';');
    if (semi != std::string::npos)
      work = trim(work.substr(0, semi));
    const size_t slashes = work.find("//");
    if (slashes != std::string::npos)
      work = trim(work.substr(0, slashes));
    if (work.empty())
      continue;

    // Label definition: identifier followed by ':'
    const size_t colon = work.find(':');
    if (colon != std::string::npos) {
      const std::string name = trim(work.substr(0, colon));
      if (!name.empty()) {
        if (dataSection)
          dataLabels_[name] = 0; // resolved during the second pass
        else
          labels_[name] = instructionIndex;
      }
      work = trim(work.substr(colon + 1));
      if (work.empty())
        continue;
    }

    if (work[0] == '.') {
      const std::vector<std::string> tokens = tokenizeLine(work);
      if (!tokens.empty()) {
        if (tokens[0] == ".data")
          dataSection = true;
        else if (tokens[0] == ".text")
          dataSection = false;
      }
      continue;
    }

    if (!dataSection)
      ++instructionIndex;
  }

  return true;
}

bool Assembler::secondPass(const std::string &source) {
  std::istringstream in(source);
  std::string line;
  int lineNo = 0;
  inDataSection_ = false;

  while (std::getline(in, line)) {
    ++lineNo;
    std::string work = trim(line);

    const size_t semi = work.find(';');
    if (semi != std::string::npos)
      work = trim(work.substr(0, semi));
    const size_t slashes = work.find("//");
    if (slashes != std::string::npos)
      work = trim(work.substr(0, slashes));
    if (work.empty())
      continue;

    const size_t colon = work.find(':');
    if (colon != std::string::npos && work.find('[') == std::string::npos) {
      const std::string name = trim(work.substr(0, colon));
      if (inDataSection_ && !name.empty())
        dataLabels_[name] = data_.size();
      work = trim(work.substr(colon + 1));
      if (work.empty())
        continue;
    }

    const std::vector<std::string> tokens = tokenizeLine(work);
    if (tokens.empty())
      continue;

    if (tokens[0][0] == '.') {
      if (!handleDirective(tokens, lineNo))
        return false;
      continue;
    }

    if (inDataSection_)
      continue;

    Instruction inst;
    if (decodeInstruction(tokens, lineNo, work, inst))
      code_.push_back(inst);
  }

  // Resolve label references to instruction indices.
  for (Instruction &inst : code_) {
    Operand *operands[3] = {&inst.dst, &inst.src1, &inst.src2};
    for (Operand *operand : operands) {
      if (operand->kind != Operand::Kind::Label)
        continue;

      const auto codeIt = labels_.find(operand->label);
      if (codeIt != labels_.end()) {
        operand->immediate = static_cast<int64_t>(codeIt->second);
        continue;
      }

      const auto dataIt = dataLabels_.find(operand->label);
      if (dataIt != dataLabels_.end()) {
        operand->immediate = static_cast<int64_t>(dataIt->second);
        continue;
      }

      error(inst.sourceLine, "undefined label '" + operand->label + "'",
            inst.text);
    }
  }

  return errors_.empty();
}

bool Assembler::assemble(const std::string &source) {
  code_.clear();
  data_.clear();
  labels_.clear();
  dataLabels_.clear();
  errors_.clear();

  firstPass(source);
  secondPass(source);

  return errors_.empty();
}

} // namespace mdt
