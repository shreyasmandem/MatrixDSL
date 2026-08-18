# MDT Instruction Set Architecture

**MDT — MatrixDSL Target.** A 32-bit, load/store, matrix-oriented architecture defined
by this project as the compilation target for MatrixDSL.

Status: **frozen for Review 1.** Changes to this document require team agreement.

## 1. Architecture summary

| Feature | Value | Rationale |
|---|---|---|
| Target name | MDT (MatrixDSL Target) | Dedicated target for MatrixDSL |
| Data width | 32-bit | Simple, consistent base design |
| General registers | R0–R15 | Enough for scalar code generation |
| Vector registers | V0–V7 | Small SIMD register class |
| Vector width | 128-bit | 4 by FP32 |
| Matrix registers | M0–M7 | Dedicated matrix register class |
| Matrix register size | 4 by 4 FP32 (64 bytes) | Fixed hardware tile granularity |
| Element type | FP32 | Keeps the numerical model simple |
| Memory model | Load/store | Only LOAD and STORE touch memory |
| Instruction width | 32-bit fixed | Simple encoding model |
| Endianness | Little-endian | Simple convention |
| Max MatrixDSL matrix | 256 by 256 | Practical project boundary |
| Matrix tile | 4 by 4 | Maps large matrices onto fixed-size operations |

### Why a 4 by 4 tile

A matrix register holds 4 by 4 = 16 FP32 values. At 4 bytes per element:

```
16 elements x 4 bytes = 64 bytes per matrix register
```

64 bytes is large enough that `MATMUL` performs real work (64 multiply-accumulate
operations per instruction), and small enough that the register file remains
implementable and the simulator stays simple. Fixed-size matrix compute units of this
kind are the same idea used by NVIDIA tensor cores and the systolic array in Google's
TPU, at a scale appropriate to this project.

## 2. Register file

### 2.1 Scalar registers

```
R0-R15    16 x 32-bit registers

R0-R12    General purpose
R13       Stack Pointer      (SP)
R14       Frame Pointer      (FP)
R15       Return Address     (RA)
```

### 2.2 Vector registers

```
V0-V7     8 x 128-bit registers

Each V register contains 4 x FP32:

V0 = [ x0 | x1 | x2 | x3 ]
```

### 2.3 Matrix registers

```
M0-M7     8 x (4x4 FP32) registers

Each M register contains 16 x FP32 = 64 bytes, row-major:

M0 = [ m00 m01 m02 m03 ]
     [ m10 m11 m12 m13 ]
     [ m20 m21 m22 m23 ]
     [ m30 m31 m32 m33 ]
```

The 4 by 4 size is the target's fixed matrix-tile granularity. Larger MatrixDSL
matrices are handled by compiler-level tiling rather than by defining arbitrarily
large hardware registers.

## 3. Instruction set

17 instructions across five classes.

| Class | Instructions | Purpose |
|---|---|---|
| Scalar arithmetic | `ADD` `SUB` `MUL` | Basic scalar computation |
| Memory | `LOAD` `STORE` | Move values between registers and memory |
| Control flow | `BR` `BEQ` `BNE` `JMP` `CALL` `RET` | Program control |
| Vector | `VADD` `VMUL` | Element-wise 4-lane FP32 operations |
| Matrix / AI | `MATMUL` `TRANSPOSE` `RELU` | Core matrix-oriented operations |

The instruction set is intentionally compact. The objective is a complete,
demonstrable LLVM target rather than a large production processor.

## 4. Instruction semantics

### 4.1 Scalar arithmetic

```
ADD  Rd, Rs1, Rs2        Rd = Rs1 + Rs2
SUB  Rd, Rs1, Rs2        Rd = Rs1 - Rs2
MUL  Rd, Rs1, Rs2        Rd = Rs1 * Rs2
```

### 4.2 Memory

```
LOAD  Rd, [Rs + imm]     Rd = Memory[Rs + imm]
STORE Rs, [Rd + imm]     Memory[Rd + imm] = Rs
```

`LOAD` and `STORE` are overloaded on register class. The assembler selects the
transfer width from the destination register:

| Form | Bytes moved |
|---|---|
| `LOAD Rd, [Rs + imm]` | 4 (one 32-bit word) |
| `LOAD Vd, [Rs + imm]` | 16 (four FP32 lanes) |
| `LOAD Md, [Rs + imm]` | 64 (one 4 by 4 tile) |

Matrix loads read 64 contiguous bytes. Because MatrixDSL matrices are stored
row-major and may be wider than four columns, a tile load of a sub-block uses a
row-stride form:

```
LOADT Md, [Rs + imm], stride    ; load 4x4 tile, rows separated by `stride` bytes
STORET Ms, [Rd + imm], stride   ; store 4x4 tile with row stride
```

`LOADT` / `STORET` are strided variants rather than separate instructions in the
17-instruction count; they share the `LOAD` / `STORE` opcodes with a mode bit.

### 4.3 Control flow

```
BR   label               Unconditional branch (PC-relative)
BEQ  Rs1, Rs2, label     Branch if Rs1 == Rs2
BNE  Rs1, Rs2, label     Branch if Rs1 != Rs2
JMP  Rs                  Jump to address in register (indirect)
CALL label               Push return address into R15, jump to label
RET                      Jump to address in R15
```

Control flow is not exposed in the MatrixDSL surface language, but the compiler needs
it to emit tiling loops for matrices larger than 4 by 4.

### 4.4 Vector

```
VADD Vd, Vs1, Vs2        Vd[i] = Vs1[i] + Vs2[i]    for i = 0..3
VMUL Vd, Vs1, Vs2        Vd[i] = Vs1[i] * Vs2[i]    for i = 0..3
```

### 4.5 Matrix

```
MATMUL    Md, Ma, Mb     Md = Ma x Mb            (4x4 by 4x4, 64 MACs)
TRANSPOSE Md, Ma         Md[i][j] = Ma[j][i]
RELU      Md, Ma         Md[i][j] = max(0, Ma[i][j])
```

`MATMUL` computes a full 4 by 4 matrix product in one instruction:

```
Md[i][j] = sum over k=0..3 of Ma[i][k] * Mb[k][j]
```

## 5. Assembly syntax

Register names are written bare. Memory operands use bracket notation. Comments start
with `;`.

```asm
; scalar
ADD       R1, R2, R3
SUB       R4, R5, R6
MUL       R7, R8, R9

; memory
LOAD      R1, [R2 + 0]
STORE     R3, [R4 + 16]

; vector
VADD      V0, V1, V2
VMUL      V3, V4, V5

; matrix
MATMUL    M0, M1, M2
RELU      M3, M0
TRANSPOSE M4, M3

; control flow
loop:
    BEQ   R1, R2, done
    BR    loop
done:
    RET
```

Labels are an identifier followed by `:` at the start of a line.

Directives:

| Directive | Meaning |
|---|---|
| `.text` | begin code section |
| `.data` | begin data section |
| `.global name` | export a symbol |
| `.word value` | emit a 32-bit word |
| `.float value` | emit an FP32 value |
| `.space n` | reserve n zero bytes |

## 6. Instruction encoding

MDT uses a fixed 32-bit instruction width. Encoding is an extension goal — readable
assembly is the primary backend output and a binary encoder must not be allowed to
delay the backend.

### R-type (scalar register-register)

```
 31        26 25     21 20     16 15     11 10          0
+------------+---------+---------+---------+-------------+
|   opcode   |   Rd    |   Rs1   |   Rs2   |  reserved   |
|   6 bits   | 5 bits  | 5 bits  | 5 bits  |   11 bits   |
+------------+---------+---------+---------+-------------+
```

### M-type (matrix register-register)

```
 31        26 25     23 22     20 19     17 16          0
+------------+---------+---------+---------+-------------+
|   opcode   |   Md    |   Ma    |   Mb    |  reserved   |
|   6 bits   | 3 bits  | 3 bits  | 3 bits  |   17 bits   |
+------------+---------+---------+---------+-------------+
```

Matrix register fields are 3 bits because there are only 8 matrix registers
(M0–M7), whereas scalar fields are 5 bits to address R0–R15 with room to spare.

### I-type (immediate / memory)

```
 31        26 25     21 20     16 15                    0
+------------+---------+---------+------------------------+
|   opcode   |   Rd    |   Rs    |     immediate (16)     |
+------------+---------+---------+------------------------+
```

The exact bit allocation is finalised once the opcode list and operand requirements
are fixed. The field sizes above are illustrative, not hardware constraints.

## 7. Opcode assignments

| Opcode | Instruction | Type |
|---|---|---|
| `0x00` | `NOP` | R |
| `0x01` | `ADD` | R |
| `0x02` | `SUB` | R |
| `0x03` | `MUL` | R |
| `0x10` | `LOAD` | I |
| `0x11` | `STORE` | I |
| `0x20` | `BR` | I |
| `0x21` | `BEQ` | I |
| `0x22` | `BNE` | I |
| `0x23` | `JMP` | R |
| `0x24` | `CALL` | I |
| `0x25` | `RET` | R |
| `0x30` | `VADD` | R |
| `0x31` | `VMUL` | R |
| `0x40` | `MATMUL` | M |
| `0x41` | `TRANSPOSE` | M |
| `0x42` | `RELU` | M |

## 8. Calling convention

| Register | Role | Saved by |
|---|---|---|
| R0–R3 | Argument / return values | Caller |
| R4–R12 | General purpose | Callee |
| R13 | Stack pointer (SP) | Callee |
| R14 | Frame pointer (FP) | Callee |
| R15 | Return address (RA) | Caller |
| V0–V7 | Vector temporaries | Caller |
| M0–M7 | Matrix temporaries | Caller |

The stack grows downward. Matrix and vector registers are all caller-saved to keep
prologue and epilogue generation simple.

## 9. Out of scope

The following are explicitly outside the MDT project boundary:

- A physical processor or FPGA implementation
- Cache hierarchy or memory-system modelling
- Pipelining, hazard detection, out-of-order execution
- A GPU-style execution model or thread hierarchy
- A full TPU-class systolic array implementation
- Floating-point exception handling and rounding-mode control
- Virtual memory, privilege levels, interrupts
- A large production-scale instruction set

MDT is a compilation target, not a processor design project.

## 10. Reference

The `tools/mdtsim` simulator is the executable reference for these semantics. Where
this document and the simulator disagree, that is a bug in one of them and must be
resolved before the affected instruction is considered implemented.
