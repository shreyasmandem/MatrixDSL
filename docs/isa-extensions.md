# MDT ISA Extensions — Phase 2

Owner: **Shreyas Mandem (24BCE0381)**. Status: **frozen for Review 2.**

This document is the frozen contract for Phase 2's additions to the MDT instruction
set defined in `docs/isa.md`. Anything here is a promise Kandi's Tensor IR lowering
can rely on and Shreyas's backend and simulator must honour exactly.

## 1. What is and is not a new opcode

Phase 1 closed with 17 instructions. Phase 2 adds **7 real opcodes** — the number
carried through `docs/review2/Review2_Report.md`. Everything else added in Phase 2
is a pseudo-instruction that assembles down to existing or newly-added opcodes,
following the precedent already set in Phase 1: `LOADT`/`STORET` were documented
there as "strided variants... shar[ing] the `LOAD`/`STORE` opcodes with a mode bit,"
not separate opcodes. Phase 2's `SCRATCHLOAD`/`SCRATCHSTORE` (§3) follow the
identical pattern for the new scratchpad address space.

**17 (Phase 1) + 7 (Phase 2) = 24 real opcodes.**

## 2. The 7 new instructions

| Instruction | Type | Semantics | Motivation |
|---|---|---|---|
| `MATMULACC Md, Ma, Mb` | M | `Md = Md + (Ma x Mb)` | Accumulate form of MATMUL — bias/residual half of fusion |
| `MATMULRELU Md, Ma, Mb` | M | `Md = relu(Ma x Mb)` | Fused epilogue — the flagship Phase 2 instruction |
| `CVT.F32.BF16 Rd, Rs` | R | Convert one FP32 scalar in Rs to BF16, zero-extended back into Rd | Mixed-precision *storage* only; compute datapath is untouched |
| `CVT.BF16.F32 Rd, Rs` | R | Convert one BF16 value (held in the low 16 bits of Rs) to FP32 in Rd | Inverse of the above |
| `VREDSUM Rd, Vs` | R | `Rd = Vs[0]+Vs[1]+Vs[2]+Vs[3]`, truncated to i32 view of the sum | 4-lane reduction, feeds `softmax` normalisation |
| `MROWMAX Vd, Ms` | R-like, matrix source | `Vd[i] = max(Ms[i][0..3])` for each of the 4 rows | Numerically-stable softmax needs row-max before `exp` |
| `MROWSUM Vd, Ms` | R-like, matrix source | `Vd[i] = sum(Ms[i][0..3])` for each of the 4 rows | Softmax normalisation denominator |

### 2.1 `MATMULACC` — accumulate

```
MATMULACC Md, Ma, Mb      Md = Md + (Ma x Mb)
```

Identical datapath to `MATMUL`, with the destination pre-loaded rather than
overwritten. Used by the fusion pass to implement `matmul(A,B) + C`: the compiler
loads `C` into `Md` first, then issues `MATMULACC Md, A, B`.

### 2.2 `MATMULRELU` — fused epilogue

```
MATMULRELU Md, Ma, Mb     Md = relu(Ma x Mb)
```

Implements XLA-style epilogue fusion (`docs/review2/Review2_Report.md` §4.2, §6,
citing Snider & Liang, *Operator Fusion in XLA*, arXiv:2301.13062): the product
already sits in the accumulator before it would be written back, so clamping
negative values to zero at that point costs nothing beyond the compare. This is the
instruction the fusion pass targets for the `matmul -> relu` pattern, which the
Phase 2 benchmarks (`ffn_block.mtx`) use directly.

### 2.3 `CVT.F32.BF16` / `CVT.BF16.F32` — mixed-precision storage

Compute inside `M0`–`M7` stays FP32 (§4.3 of the Phase 2 report — the `MATMUL`
datapath is not touched). BF16 exists only as a *storage* format: a tile is
converted to BF16 immediately before a `STORE` to halve the bytes moved, and
converted back to FP32 immediately after a `LOAD`. Conversion is scalar
(element-at-a-time via `R` registers), reflecting that this is a memory-bandwidth
optimisation, not a new compute mode.

Truncation semantics: `CVT.F32.BF16` keeps the top 16 bits of the IEEE-754 FP32
representation (sign, 8-bit exponent, top 7 mantissa bits) — the standard
round-toward-zero BF16 truncation. `CVT.BF16.F32` zero-extends those 16 bits back
into the low half of the mantissa.

### 2.4 `VREDSUM`, `MROWMAX`, `MROWSUM` — reduction primitives for softmax

`softmax` is not a hardware instruction. It is lowered (Tensor IR, `docs/tensor-ir.md`)
into a sequence: row-max (for numerical stability) → subtract → `exp` (a runtime
call, not a hardware instruction — see §4) → row-sum → divide. `MROWMAX`/`MROWSUM`
compute the two reductions across a 4x4 tile's rows in one instruction each;
`VREDSUM` is the general-purpose 4-lane reduction used wherever a vector needs
collapsing to a scalar (including as a building block if a future pass needs a
full-tile sum rather than a row-wise one).

## 3. `SCRATCHLOAD` / `SCRATCHSTORE` — pseudo-instructions, not opcodes

```
SCRATCHLOAD  Md, [Rs + imm]      pseudo-op; expands to LOAD with the scratchpad bit set
SCRATCHSTORE Md, [Rs + imm]      pseudo-op; expands to STORE with the scratchpad bit set
```

These exist at the assembler/compiler level only, exactly like Phase 1's `LOADT`/
`STORET`. They select the scratchpad address space (§4) rather than DRAM; the
underlying opcode is still `0x10`/`0x11` (`LOAD`/`STORE`). No opcode table entry is
needed for them — see `docs/memory-hierarchy.md` for the addressing rule.

## 4. Opcode assignments (extends `docs/isa.md` §7)

| Opcode | Instruction | Type |
|---|---|---|
| `0x50` | `MATMULACC` | M |
| `0x51` | `MATMULRELU` | M |
| `0x60` | `CVT.F32.BF16` | R |
| `0x61` | `CVT.BF16.F32` | R |
| `0x70` | `VREDSUM` | R |
| `0x71` | `MROWMAX` | R-like (matrix source, vector dest) |
| `0x72` | `MROWSUM` | R-like (matrix source, vector dest) |

Chosen in the `0x50`–`0x7F` range specifically so they never collide with Phase 1's
`0x00`–`0x42` assignments, and so a future Phase 3 extension has an obvious next
free block (`0x80` onward) without renumbering anything here.

## 5. `exp` is a runtime call, not an instruction

**Explicit scope decision**, carried from the Phase 2 report's classification
table: a hardware `EXP` instruction is **DO NOT ADD**. Transcendental functions
need either a lookup table or a polynomial approximation to implement in fixed
logic, and neither is worth the complexity for a teaching ISA. `softmax`'s
lowering instead emits a call to a runtime function (`mdt_softmax_exp`, implemented
with `expf` from the host's `<cmath>` in the simulator's runtime support) for the
elementwise exponential step, and uses real MDT instructions (`MROWMAX`, `MROWSUM`,
plus ordinary `SUB`/scalar division) for everything around it. This mirrors how
`docs/llvm-backend.md` already treats matrix operations as intrinsic calls when a
dedicated instruction does not exist.

## 6. Interface contract with Kandi's Tensor IR lowering

| Tensor IR op (see `docs/tensor-ir.md`) | Lowers to |
|---|---|
| `FusedMatMulRelu` | `MATMULRELU` |
| `FusedMatMulAcc` | `MATMULACC` |
| `ConvertPrecision` (FP32 → BF16) | `CVT.F32.BF16` |
| `ConvertPrecision` (BF16 → FP32) | `CVT.F32.BF16` reversed, i.e. `CVT.BF16.F32` |
| `RowMax` | `MROWMAX` |
| `RowSum` | `MROWSUM` |
| `Exp` | runtime call `mdt_softmax_exp` (§5) |
| `ScratchLoad` / `ScratchStore` | `SCRATCHLOAD` / `SCRATCHSTORE` pseudo-ops (§3) |

This table is fixed before either side's implementation begins, for the same
reason the `@mdt.matmul.4x4` intrinsic naming was fixed in Phase 1 before IR
generation and the backend diverged.
