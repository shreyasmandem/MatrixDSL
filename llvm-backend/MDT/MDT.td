//===-- MDT.td - Top-level MDT target description ----------*- tablegen -*-===//
//
// MatrixDSL Target (MDT)
// Owner: Shreyas Mandem (24BCE0381)
//
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

include "MDTRegisterInfo.td"
include "MDTCallingConv.td"
include "MDTInstrInfo.td"

//===----------------------------------------------------------------------===//
// Subtarget features
//
// The matrix unit is modelled as an optional feature so that a scalar-only
// configuration remains testable while the matrix instructions are still
// being brought up.
//===----------------------------------------------------------------------===//

def FeatureVector : SubtargetFeature<"vector", "HasVector", "true",
                                     "Enable V0-V7 and vector instructions">;

def FeatureMatrix : SubtargetFeature<"matrix", "HasMatrix", "true",
                                     "Enable M0-M7 and matrix instructions">;

//===----------------------------------------------------------------------===//
// Processor model
//===----------------------------------------------------------------------===//

def : ProcessorModel<"mdt", NoSchedModel, [FeatureVector, FeatureMatrix]>;
def : ProcessorModel<"mdt-scalar", NoSchedModel, []>;

//===----------------------------------------------------------------------===//
// Assembly output
//===----------------------------------------------------------------------===//

def MDTAsmWriter : AsmWriter {
  let AsmWriterClassName = "InstPrinter";
  let PassSubtarget = 1;
  let Variant = 0;
}

def MDTInstrInfo : InstrInfo;

def MDT : Target {
  let InstructionSet = MDTInstrInfo;
  let AssemblyWriters = [MDTAsmWriter];
}
