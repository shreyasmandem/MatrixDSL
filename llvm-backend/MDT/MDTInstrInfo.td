//===-- MDTInstrInfo.td - MDT Instruction Definitions ------*- tablegen -*-===//
//
// MatrixDSL Target (MDT) instruction set: 17 instructions.
// Owner: Shreyas Mandem (24BCE0381)
//
// Scalar and vector instructions map onto existing LLVM SDNodes (add, fadd,
// ...) and are therefore selected automatically by the generated matcher.
//
// Matrix instructions have no LLVM equivalent, so they need custom SDNodes.
// These are produced by MDTISelLowering, which intercepts calls to the
// reserved @mdt.* intrinsic names emitted by the IR generator.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Instruction formats - fixed 32-bit width, see docs/isa.md section 6
//===----------------------------------------------------------------------===//

class MDTInst<dag outs, dag ins, string asmstr, list<dag> pattern>
    : Instruction {
  let Namespace = "MDT";
  let Size = 4;
  let OutOperandList = outs;
  let InOperandList = ins;
  let AsmString = asmstr;
  let Pattern = pattern;

  field bits<32> Inst;
  bits<6> Opcode;
  let Inst{31-26} = Opcode;
}

// R-type: opcode | Rd(5) | Rs1(5) | Rs2(5) | reserved(11)
class MDT_R<bits<6> opcode, dag outs, dag ins, string asmstr,
            list<dag> pattern> : MDTInst<outs, ins, asmstr, pattern> {
  bits<5> dst;
  bits<5> src1;
  bits<5> src2;

  let Opcode = opcode;
  let Inst{25-21} = dst;
  let Inst{20-16} = src1;
  let Inst{15-11} = src2;
  let Inst{10-0}  = 0;
}

// I-type: opcode | Rd(5) | Rs(5) | immediate(16)
class MDT_I<bits<6> opcode, dag outs, dag ins, string asmstr,
            list<dag> pattern> : MDTInst<outs, ins, asmstr, pattern> {
  bits<5>  dst;
  bits<5>  base;
  bits<16> imm;

  let Opcode = opcode;
  let Inst{25-21} = dst;
  let Inst{20-16} = base;
  let Inst{15-0}  = imm;
}

// M-type: opcode | Md(3) | Ma(3) | Mb(3) | reserved(17)
// Matrix register fields are 3 bits because there are only 8 of them.
class MDT_M<bits<6> opcode, dag outs, dag ins, string asmstr,
            list<dag> pattern> : MDTInst<outs, ins, asmstr, pattern> {
  bits<3> dst;
  bits<3> srcA;
  bits<3> srcB;

  let Opcode = opcode;
  let Inst{25-23} = dst;
  let Inst{22-20} = srcA;
  let Inst{19-17} = srcB;
  let Inst{16-0}  = 0;
}

//===----------------------------------------------------------------------===//
// Operand types
//===----------------------------------------------------------------------===//

def simm16 : Operand<i32>, ImmLeaf<i32, [{
  return Imm >= -32768 && Imm <= 32767;
}]>;

def MDTMemOperand : Operand<i32> {
  let PrintMethod = "printMemOperand";
  let MIOperandInfo = (ops ScalarRegs, simm16);
}

def brtarget : Operand<OtherVT>;
def calltarget : Operand<i32>;

//===----------------------------------------------------------------------===//
// Custom SDNodes for matrix operations
//
// LLVM has no notion of a matrix multiply, so these nodes are introduced by
// MDTISelLowering when it sees a call to a reserved @mdt.* name.
//===----------------------------------------------------------------------===//

def SDT_MDTMatBinary : SDTypeProfile<1, 2, [
  SDTCisVT<0, v16f32>, SDTCisVT<1, v16f32>, SDTCisVT<2, v16f32>
]>;

def SDT_MDTMatUnary : SDTypeProfile<1, 1, [
  SDTCisVT<0, v16f32>, SDTCisVT<1, v16f32>
]>;

def mdt_matmul    : SDNode<"MDTISD::MATMUL",    SDT_MDTMatBinary>;
def mdt_transpose : SDNode<"MDTISD::TRANSPOSE", SDT_MDTMatUnary>;
def mdt_relu      : SDNode<"MDTISD::RELU",      SDT_MDTMatUnary>;

def mdt_ret : SDNode<"MDTISD::RET_FLAG", SDTNone,
                     [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;
def mdt_call : SDNode<"MDTISD::CALL", SDTNone,
                      [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue,
                       SDNPVariadic]>;

//===----------------------------------------------------------------------===//
// Scalar arithmetic - opcodes 0x01 to 0x03
//===----------------------------------------------------------------------===//

def ADD : MDT_R<0x01, (outs ScalarRegs:$dst),
                      (ins ScalarRegs:$src1, ScalarRegs:$src2),
                "ADD $dst, $src1, $src2",
                [(set i32:$dst, (add i32:$src1, i32:$src2))]>;

def SUB : MDT_R<0x02, (outs ScalarRegs:$dst),
                      (ins ScalarRegs:$src1, ScalarRegs:$src2),
                "SUB $dst, $src1, $src2",
                [(set i32:$dst, (sub i32:$src1, i32:$src2))]>;

def MUL : MDT_R<0x03, (outs ScalarRegs:$dst),
                      (ins ScalarRegs:$src1, ScalarRegs:$src2),
                "MUL $dst, $src1, $src2",
                [(set i32:$dst, (mul i32:$src1, i32:$src2))]>;

//===----------------------------------------------------------------------===//
// Memory - opcodes 0x10 to 0x11
//
// LOAD and STORE are overloaded on the register class of the transferred
// register: 4 bytes for R, 16 for V, 64 for M.
//===----------------------------------------------------------------------===//

let mayLoad = 1 in {

def LOAD : MDT_I<0x10, (outs ScalarRegs:$dst),
                       (ins ScalarRegs:$base, simm16:$imm),
                 "LOAD $dst, [$base + $imm]",
                 [(set i32:$dst, (load (add i32:$base, simm16:$imm)))]>;

def VLOAD : MDT_I<0x10, (outs VectorRegs:$dst),
                        (ins ScalarRegs:$base, simm16:$imm),
                  "LOAD $dst, [$base + $imm]",
                  [(set v4f32:$dst, (load (add i32:$base, simm16:$imm)))]>;

def MLOAD : MDT_I<0x10, (outs MatrixRegs:$dst),
                        (ins ScalarRegs:$base, simm16:$imm),
                  "LOAD $dst, [$base + $imm]",
                  [(set v16f32:$dst, (load (add i32:$base, simm16:$imm)))]>;

} // mayLoad

let mayStore = 1 in {

def STORE : MDT_I<0x11, (outs),
                        (ins ScalarRegs:$dst, ScalarRegs:$base, simm16:$imm),
                  "STORE $dst, [$base + $imm]",
                  [(store i32:$dst, (add i32:$base, simm16:$imm))]>;

def VSTORE : MDT_I<0x11, (outs),
                         (ins VectorRegs:$dst, ScalarRegs:$base, simm16:$imm),
                   "STORE $dst, [$base + $imm]",
                   [(store v4f32:$dst, (add i32:$base, simm16:$imm))]>;

def MSTORE : MDT_I<0x11, (outs),
                         (ins MatrixRegs:$dst, ScalarRegs:$base, simm16:$imm),
                   "STORE $dst, [$base + $imm]",
                   [(store v16f32:$dst, (add i32:$base, simm16:$imm))]>;

} // mayStore

//===----------------------------------------------------------------------===//
// Control flow - opcodes 0x20 to 0x25
//===----------------------------------------------------------------------===//

let isBranch = 1, isTerminator = 1 in {

let isBarrier = 1 in
def BR : MDT_I<0x20, (outs), (ins brtarget:$target),
               "BR $target", [(br bb:$target)]>;

def BEQ : MDT_I<0x21, (outs),
                      (ins ScalarRegs:$src1, ScalarRegs:$src2,
                           brtarget:$target),
                "BEQ $src1, $src2, $target",
                [(brcond (i32 (seteq i32:$src1, i32:$src2)), bb:$target)]>;

def BNE : MDT_I<0x22, (outs),
                      (ins ScalarRegs:$src1, ScalarRegs:$src2,
                           brtarget:$target),
                "BNE $src1, $src2, $target",
                [(brcond (i32 (setne i32:$src1, i32:$src2)), bb:$target)]>;

let isIndirectBranch = 1, isBarrier = 1 in
def JMP : MDT_R<0x23, (outs), (ins ScalarRegs:$target),
                "JMP $target", [(brind i32:$target)]>;

} // isBranch, isTerminator

let isCall = 1, Defs = [R15] in
def CALL : MDT_I<0x24, (outs), (ins calltarget:$target),
                 "CALL $target", []>;

let isReturn = 1, isTerminator = 1, isBarrier = 1, Uses = [R15] in
def RET : MDT_R<0x25, (outs), (ins), "RET", [(mdt_ret)]>;

//===----------------------------------------------------------------------===//
// Vector - opcodes 0x30 to 0x31
//===----------------------------------------------------------------------===//

def VADD : MDT_R<0x30, (outs VectorRegs:$dst),
                       (ins VectorRegs:$src1, VectorRegs:$src2),
                 "VADD $dst, $src1, $src2",
                 [(set v4f32:$dst, (fadd v4f32:$src1, v4f32:$src2))]>;

def VMUL : MDT_R<0x31, (outs VectorRegs:$dst),
                       (ins VectorRegs:$src1, VectorRegs:$src2),
                 "VMUL $dst, $src1, $src2",
                 [(set v4f32:$dst, (fmul v4f32:$src1, v4f32:$src2))]>;

//===----------------------------------------------------------------------===//
// Matrix - opcodes 0x40 to 0x42
//
// These are the instructions the whole project exists to reach. One MATMUL
// performs 64 multiply-accumulate operations.
//===----------------------------------------------------------------------===//

def MATMUL : MDT_M<0x40, (outs MatrixRegs:$dst),
                         (ins MatrixRegs:$srcA, MatrixRegs:$srcB),
                   "MATMUL $dst, $srcA, $srcB",
                   [(set v16f32:$dst,
                         (mdt_matmul v16f32:$srcA, v16f32:$srcB))]>;

def TRANSPOSE : MDT_M<0x41, (outs MatrixRegs:$dst), (ins MatrixRegs:$srcA),
                      "TRANSPOSE $dst, $srcA",
                      [(set v16f32:$dst, (mdt_transpose v16f32:$srcA))]>;

def RELU : MDT_M<0x42, (outs MatrixRegs:$dst), (ins MatrixRegs:$srcA),
                 "RELU $dst, $srcA",
                 [(set v16f32:$dst, (mdt_relu v16f32:$srcA))]>;

//===----------------------------------------------------------------------===//
// Pseudo-instructions
//===----------------------------------------------------------------------===//

let isPseudo = 1 in {

def NOP : MDT_R<0x00, (outs), (ins), "NOP", []>;

// Expanded by MDTFrameLowering into SP adjustment.
def ADJCALLSTACKDOWN : MDTInst<(outs), (ins i32imm:$amt), "", []>;
def ADJCALLSTACKUP   : MDTInst<(outs), (ins i32imm:$amt), "", []>;

} // isPseudo
