//===-- MDTRegisterInfo.td - MDT Register Definitions ------*- tablegen -*-===//
//
// MatrixDSL Target (MDT) register file description.
// Owner: Shreyas Mandem (24BCE0381)
//
// Three disjoint register classes. Keeping them disjoint means the register
// allocator never has to reason about overlap or sub-register relationships,
// which is a deliberate simplification for a first target.
//
//===----------------------------------------------------------------------===//

let Namespace = "MDT" in {

//===----------------------------------------------------------------------===//
// Register classes for the encoder
//===----------------------------------------------------------------------===//

// Scalar registers use a 5-bit field (R0-R15 needs 4, one spare).
class MDTScalarReg<bits<5> Enc, string n> : Register<n> {
  let HWEncoding{4-0} = Enc;
}

// Vector registers use a 3-bit field (V0-V7).
class MDTVectorReg<bits<3> Enc, string n> : Register<n> {
  let HWEncoding{2-0} = Enc;
}

// Matrix registers use a 3-bit field (M0-M7).
class MDTMatrixReg<bits<3> Enc, string n> : Register<n> {
  let HWEncoding{2-0} = Enc;
}

//===----------------------------------------------------------------------===//
// Scalar registers: R0-R15, 32-bit
//===----------------------------------------------------------------------===//

def R0  : MDTScalarReg<0,  "R0">;
def R1  : MDTScalarReg<1,  "R1">;
def R2  : MDTScalarReg<2,  "R2">;
def R3  : MDTScalarReg<3,  "R3">;
def R4  : MDTScalarReg<4,  "R4">;
def R5  : MDTScalarReg<5,  "R5">;
def R6  : MDTScalarReg<6,  "R6">;
def R7  : MDTScalarReg<7,  "R7">;
def R8  : MDTScalarReg<8,  "R8">;
def R9  : MDTScalarReg<9,  "R9">;
def R10 : MDTScalarReg<10, "R10">;
def R11 : MDTScalarReg<11, "R11">;
def R12 : MDTScalarReg<12, "R12">;
def R13 : MDTScalarReg<13, "R13">;   // SP
def R14 : MDTScalarReg<14, "R14">;   // FP
def R15 : MDTScalarReg<15, "R15">;   // RA

//===----------------------------------------------------------------------===//
// Vector registers: V0-V7, 128-bit (4 x FP32)
//===----------------------------------------------------------------------===//

def V0 : MDTVectorReg<0, "V0">;
def V1 : MDTVectorReg<1, "V1">;
def V2 : MDTVectorReg<2, "V2">;
def V3 : MDTVectorReg<3, "V3">;
def V4 : MDTVectorReg<4, "V4">;
def V5 : MDTVectorReg<5, "V5">;
def V6 : MDTVectorReg<6, "V6">;
def V7 : MDTVectorReg<7, "V7">;

//===----------------------------------------------------------------------===//
// Matrix registers: M0-M7, 4x4 FP32 = 512 bits
//===----------------------------------------------------------------------===//

def M0 : MDTMatrixReg<0, "M0">;
def M1 : MDTMatrixReg<1, "M1">;
def M2 : MDTMatrixReg<2, "M2">;
def M3 : MDTMatrixReg<3, "M3">;
def M4 : MDTMatrixReg<4, "M4">;
def M5 : MDTMatrixReg<5, "M5">;
def M6 : MDTMatrixReg<6, "M6">;
def M7 : MDTMatrixReg<7, "M7">;

} // let Namespace = "MDT"

//===----------------------------------------------------------------------===//
// Register classes
//===----------------------------------------------------------------------===//

// General-purpose scalar registers. R13-R15 are reserved (SP/FP/RA) and are
// listed last so the allocator prefers R0-R12.
def ScalarRegs : RegisterClass<"MDT", [i32, f32], 32,
                               (add (sequence "R%u", 0, 12),
                                    R13, R14, R15)>;

// Allocatable subset - excludes the reserved registers entirely.
def ScalarRegsAlloc : RegisterClass<"MDT", [i32, f32], 32,
                                    (add (sequence "R%u", 0, 12))>;

// 4 x FP32 lanes.
def VectorRegs : RegisterClass<"MDT", [v4f32], 128,
                               (add (sequence "V%u", 0, 7))>;

// A 4x4 FP32 tile is modelled as v16f32. Reusing LLVM's existing vector type
// machinery avoids having to patch LLVM core with a new MVT, which would make
// the target impossible to build against a stock LLVM release.
def MatrixRegs : RegisterClass<"MDT", [v16f32], 512,
                               (add (sequence "M%u", 0, 7))>;
