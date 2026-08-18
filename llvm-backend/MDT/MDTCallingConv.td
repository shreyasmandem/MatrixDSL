//===-- MDTCallingConv.td - MDT Calling Convention ---------*- tablegen -*-===//
//
// Owner: Shreyas Mandem (24BCE0381)
// See docs/isa.md section 8.
//
// R0-R3    arguments and return values  (caller-saved)
// R4-R12   general purpose              (callee-saved)
// R13/14/15 SP / FP / RA
//
// Vector and matrix registers are all caller-saved. This keeps prologue and
// epilogue generation simple: the callee never has to spill a 64-byte tile
// just because it touched M0.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Return values
//===----------------------------------------------------------------------===//

def RetCC_MDT : CallingConv<[
  CCIfType<[i32, f32],  CCAssignToReg<[R0, R1, R2, R3]>>,
  CCIfType<[v4f32],     CCAssignToReg<[V0, V1, V2, V3]>>,
  CCIfType<[v16f32],    CCAssignToReg<[M0, M1]>>,

  // Anything that does not fit goes on the stack.
  CCIfType<[i32, f32],  CCAssignToStack<4, 4>>,
  CCIfType<[v4f32],     CCAssignToStack<16, 16>>,
  CCIfType<[v16f32],    CCAssignToStack<64, 64>>
]>;

//===----------------------------------------------------------------------===//
// Arguments
//===----------------------------------------------------------------------===//

def CC_MDT : CallingConv<[
  CCIfType<[i8, i16], CCPromoteToType<i32>>,

  CCIfType<[i32, f32],  CCAssignToReg<[R0, R1, R2, R3]>>,
  CCIfType<[v4f32],     CCAssignToReg<[V0, V1, V2, V3]>>,
  CCIfType<[v16f32],    CCAssignToReg<[M0, M1, M2, M3]>>,

  CCIfType<[i32, f32],  CCAssignToStack<4, 4>>,
  CCIfType<[v4f32],     CCAssignToStack<16, 16>>,

  // A matrix tile is 64 bytes and 64-byte aligned, so a spilled tile is a
  // single aligned transfer.
  CCIfType<[v16f32],    CCAssignToStack<64, 64>>
]>;

//===----------------------------------------------------------------------===//
// Callee-saved registers
//===----------------------------------------------------------------------===//

def CSR_MDT : CalleeSavedRegs<(add R4, R5, R6, R7, R8, R9, R10, R11, R12,
                                   R13, R14)>;
