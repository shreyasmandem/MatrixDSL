# MDT LLVM Backend Design

Owner: **Shreyas Mandem (24BCE0381)**

## 1. Purpose

The MDT backend turns optimized, target-independent LLVM IR into MDT machine
instructions and assembly text. It is the component that makes this project a
compiler-construction contribution rather than a source-to-source translator: the
target does not exist in LLVM, so every piece of target knowledge must be described
from scratch.

## 2. Backend responsibilities

| Responsibility | Component |
|---|---|
| Register and register-class description | `MDTRegisterInfo.td` |
| Instruction description and assembly syntax | `MDTInstrInfo.td` |
| Instruction selection patterns | `MDTInstrInfo.td`, `MDTISelDAGToDAG.cpp` |
| Type legalization and custom lowering | `MDTISelLowering.cpp` |
| Calling convention | `MDTCallingConv.td` |
| Stack frame layout | `MDTFrameLowering.cpp` |
| Assembly emission | `MDTAsmPrinter.cpp`, `MCTargetDesc/MDTInstPrinter.cpp` |
| Target registration | `MDTTargetMachine.cpp`, `MCTargetDesc/MDTMCTargetDesc.cpp` |

## 3. File organisation

```
llvm-backend/MDT/
├── CMakeLists.txt
├── MDT.td                    Top-level target definition
├── MDTRegisterInfo.td        R0-R15, V0-V7, M0-M7 and register classes
├── MDTInstrInfo.td           17 instructions, operands, patterns, asm syntax
├── MDTCallingConv.td         Argument and return-value assignment
├── MDTSchedule.td            Instruction latencies (stub for v1)
├── MDTTargetMachine.cpp/.h   Target machine, pass pipeline registration
├── MDTSubtarget.cpp/.h       Subtarget feature description
├── MDTInstrInfo.cpp/.h       Instruction info implementation
├── MDTRegisterInfo.cpp/.h    Register info implementation
├── MDTISelLowering.cpp/.h    SelectionDAG lowering, custom operations
├── MDTISelDAGToDAG.cpp/.h    DAG-to-DAG instruction selection
├── MDTFrameLowering.cpp/.h   Prologue/epilogue, stack layout
├── MDTAsmPrinter.cpp/.h      MachineInstr to MCInst, assembly output
└── MCTargetDesc/
    ├── MDTMCTargetDesc.cpp/.h    MC layer registration
    ├── MDTMCAsmInfo.cpp/.h       Assembly syntax properties
    └── MDTInstPrinter.cpp/.h     Textual instruction printing
```

The MDT target builds against an existing LLVM source or install tree. The LLVM
source tree is **not** vendored into this repository.

## 4. TableGen description

TableGen carries the declarative target information. Three files matter most.

### 4.1 Register description

```tablegen
// Scalar: R0-R15, 32-bit
class MDTScalarReg<bits<5> num, string n> : Register<n> {
  let HWEncoding{4-0} = num;
  let Namespace = "MDT";
}

def R0 : MDTScalarReg<0, "R0">;
// ... R1 through R15

def ScalarRegs : RegisterClass<"MDT", [i32, f32], 32,
                               (add (sequence "R%u", 0, 15))>;

// Vector: V0-V7, 128-bit = 4 x FP32
def VectorRegs : RegisterClass<"MDT", [v4f32], 128,
                               (add (sequence "V%u", 0, 7))>;

// Matrix: M0-M7, 4x4 FP32 = 512 bits
def MatrixRegs : RegisterClass<"MDT", [v16f32], 512,
                               (add (sequence "M%u", 0, 7))>;
```

Representing a 4 by 4 tile as `v16f32` reuses LLVM's existing vector type machinery
instead of introducing a new MVT, which avoids patching LLVM core.

### 4.2 Instruction description

```tablegen
class MDT_R<bits<6> opcode, string asmstr, list<dag> pattern>
    : Instruction {
  let Namespace   = "MDT";
  let Size        = 4;
  let AsmString   = asmstr;
  let Pattern     = pattern;
  field bits<32> Inst;
  let Inst{31-26} = opcode;
}

def ADD : MDT_R<0x01, "ADD $dst, $src1, $src2",
                [(set i32:$dst, (add i32:$src1, i32:$src2))]>;

def VADD : MDT_R<0x30, "VADD $dst, $src1, $src2",
                 [(set v4f32:$dst, (fadd v4f32:$src1, v4f32:$src2))]>;

def MATMUL : MDT_M<0x40, "MATMUL $dst, $a, $b",
                   [(set v16f32:$dst, (mdt_matmul v16f32:$a, v16f32:$b))]>;
```

Scalar and vector instructions map onto existing LLVM SDNodes (`add`, `fadd`) and so
are selected automatically. Matrix instructions have no LLVM equivalent and require
custom SDNodes.

### 4.3 Custom SDNodes for matrix operations

```tablegen
def SDT_MDTMatMul : SDTypeProfile<1, 2,
    [SDTCisVT<0, v16f32>, SDTCisVT<1, v16f32>, SDTCisVT<2, v16f32>]>;

def mdt_matmul : SDNode<"MDTISD::MATMUL", SDT_MDTMatMul>;
def mdt_relu   : SDNode<"MDTISD::RELU",   SDT_MDTUnary>;
def mdt_transp : SDNode<"MDTISD::TRANSPOSE", SDT_MDTUnary>;
```

## 5. Instruction selection

| LLVM IR construct | MDT instruction |
|---|---|
| `add i32` | `ADD` |
| `sub i32` | `SUB` |
| `mul i32` | `MUL` |
| `load i32` | `LOAD` |
| `store i32` | `STORE` |
| `br` / `br i1` | `BR` / `BEQ` / `BNE` |
| `call` / `ret` | `CALL` / `RET` |
| `fadd <4 x float>` | `VADD` |
| `fmul <4 x float>` | `VMUL` |
| `call @mdt.matmul.4x4` | `MATMUL` |
| `call @mdt.relu.4x4` | `RELU` |
| `call @mdt.transpose.4x4` | `TRANSPOSE` |

### 5.1 How matrix operations reach the backend

The frontend does not emit a loop nest for `matmul` and hope the backend recognises
it. Loop-idiom recognition is fragile and would be defeated by the optimizer. Instead
the IR generator emits an explicit call:

```llvm
call void @mdt.matmul.4x4(float* %dst, float* %a, float* %b)
```

`MDTISelLowering::LowerCall` intercepts calls to these reserved names and replaces
them with the corresponding custom SDNode, which then selects to a single machine
instruction. The tile size is part of the function name, so the backend never has to
infer tile geometry.

This is the critical interface between Kandi's IR generation and Shreyas's backend,
and is why those two roles must agree on the intrinsic naming before either starts.

## 6. Lowering pipeline

```
optimized LLVM IR
      |
      v
MDTTargetMachine::addIRPasses
      |
      +-- MDTTilingPass          decompose >4x4 matrices into 4x4 tile operations
      |
      v
SelectionDAG construction
      |
      v
MDTISelLowering::LowerOperation  custom lowering for matrix intrinsic calls
      |
      v
MDTDAGToDAGISel::Select          pattern matching to MDT instructions
      |
      v
MachineInstr (virtual registers)
      |
      v
Register allocation              ScalarRegs / VectorRegs / MatrixRegs
      |
      v
MDTFrameLowering                 prologue, epilogue, spill slots
      |
      v
MDTAsmPrinter                    MachineInstr -> MCInst -> text
      |
      v
MDT assembly (.s)
```

## 7. Register allocation notes

Three disjoint register classes with no overlap, which keeps allocation simple. The
constraint worth noting is that **only 8 matrix registers exist**, and a tiled
matrix multiply naturally wants three live tiles at once (A tile, B tile,
accumulator). With M0–M7 there is room for two-deep unrolling; beyond that the
allocator must spill tiles to the stack at 64 bytes each.

Spilling a matrix register is an ordinary `STORET` / `LOADT` pair against a 64-byte
stack slot.

## 8. Implementation order

Deliberately incremental, so that something works end to end early:

| Phase | Deliverable | Verification |
|---|---|---|
| 1 | Target registration, MDT recognised by `llc` | `llc -march=mdt` does not error |
| 2 | `ScalarRegs`, `ADD`/`SUB`/`MUL` | Hand-written IR produces correct scalar asm |
| 3 | `LOAD`/`STORE` | Memory operations round-trip in the simulator |
| 4 | Branches, `CALL`/`RET` | A loop compiles and terminates |
| 5 | `VectorRegs`, `VADD`/`VMUL` | 4-lane operations produce correct results |
| 6 | `MatrixRegs`, `MATMUL`/`RELU`/`TRANSPOSE` | 4 by 4 operations match reference |
| 7 | Tiling pass | 16 by 16 and 32 by 32 multiply correctly |

Each phase is independently demonstrable, so slipping on a later phase still leaves a
working artifact.

## 9. Risk and fallback

**Risk.** A full SelectionDAG and TableGen target is the most technically demanding
component in the project. Public tutorials for building a toy LLVM target run to
several hundred pages, and the calling-convention, frame-lowering and MC-layer
plumbing each carry a real learning curve. If phases 1 to 4 consume more time than
budgeted, the matrix instructions — the actual novelty — may not be reached.

**Mitigation.** A fallback code path is designed in from the start: a direct
IR-to-assembly emitter (`MDTDirectEmitter`) that walks optimized LLVM IR, pattern
matches instructions itself, performs linear-scan register assignment, and prints MDT
assembly text. It bypasses SelectionDAG, TableGen, and the MC layer entirely.

The fallback still satisfies the project abstract — LLVM IR, LLVM's optimizer, and
custom target code generation — and produces byte-identical assembly for the test
suite, so the simulator, tests and benchmarks are unaffected by which path is used.

**Decision point: end of Week 8.** If scalar assembly is not being emitted correctly
through the TableGen path by then, the team switches to the direct emitter for the
remaining phases. This is a planned engineering decision, not a failure mode.

## 10. Verification

The backend is verified at three levels:

1. **Assembly inspection.** Golden-file tests compare emitted assembly against
   expected output for each instruction class (`tests/backend/`).
2. **Simulation.** `tools/mdtsim` executes the generated assembly and checks the
   resulting register and memory state.
3. **Differential.** The same MatrixDSL program is compiled both to the host
   architecture and to MDT; the simulator result must match the native result
   element for element.

Level 3 is the strongest check, and it is the reason the frontend retains the ability
to target the host machine.
