# MDT Memory Hierarchy — Phase 2

Owner: **Shreyas Mandem (24BCE0381)**, tiling pass co-owned with **Kandi Jeevitesh
Reddy (24BCE0350)**. Status: **frozen for Review 2.**

## 1. The hierarchy

MDT has never had an instruction that moves data directly between two memory
locations — Phase 1's `docs/isa.md` §1 states the memory model as "load/store"
precisely because only `LOAD` and `STORE` touch memory at all, always through a
register. Phase 2 does not introduce an exception to that: there is no
DRAM-to-scratchpad memcpy instruction. Every hop below is register-mediated,
exactly like Phase 1's DRAM access was.

```
DRAM   (flat array — Phase 1's only memory, unchanged in size/role)
  |
  v   LOAD (AS=0) into a register, then SCRATCHSTORE (AS=1) that register out
  |       - two ordinary instructions, register-mediated, no new operand shape
  |
Scratchpad / SRAM   (NEW — small, fixed-size, separate address space)
  |
  v   SCRATCHLOAD (AS=1) into a register — the compute-facing load
  |
Vector / Matrix registers  (unchanged)
```

`SCRATCHLOAD`/`SCRATCHSTORE` are not bulk block-copy instructions — see §4.
Each is exactly one `LOAD`/`STORE` with the address-space bit set (§3),
carrying a register operand exactly as Phase 1's `LOAD`/`STORE` always did.
Staging a tile from DRAM into the scratchpad is therefore two instructions
(an ordinary DRAM `LOAD` into an `M`/`V`/`R` register, then a `SCRATCHSTORE` of
that same register into scratchpad), not one.

## 2. Scratchpad sizing

**64 KiB**, fixed. At 64 bytes per 4x4 FP32 tile, that is 1024 tiles of headroom —
proportionate to a teaching accelerator, not a claim about real hardware. For
scale contrast: TPU v5e's VMEM scratchpad is 128 MiB (`docs/review2/Review2_Report.md`
§5.1, citing the TPU scaling-book source). MDT's scratchpad exists to make blocking
and reuse *observable* in the performance model, not to be realistic in absolute
size.

## 3. Addressing — reusing headroom already left in Phase 1's encoding

`docs/isa.md` §6 defined the I-type format with `reserved(16)` bits split across
the opcode/register fields. Phase 2 claims exactly **one** previously-reserved bit
in the I-type format as the address-space selector:

```
 31        26 25     21 20     16 15 14                 0
+------------+---------+---------+--+--------------------+
|   opcode   |   Rd    |   Rs    |AS|    immediate (15)   |
+------------+---------+---------+--+--------------------+

AS = 0  ->  DRAM        (Phase 1 behaviour, unchanged)
AS = 1  ->  Scratchpad  (NEW)
```

This is a deliberate callback: no new opcode is spent on addressing, and the
15-bit immediate remains large enough for every offset either memory needs (the
64 KiB scratchpad only needs 16 bits of address space; DRAM offsets in every test
and benchmark program stay well under 2^15 bytes).

`SCRATCHLOAD`/`SCRATCHSTORE` (`docs/isa-extensions.md` §3) are exactly `LOAD`/
`STORE` assembled with `AS=1`. `MATMUL`, `MATMULACC`, `MATMULRELU`, `TRANSPOSE`
and `RELU` operate only on matrix registers and never touch memory directly, so
they carry no address-space bit — data must already be in `M0`–`M7` via a prior
`LOAD`/`SCRATCHLOAD`.

## 4. "DMA" here means a register-mediated instruction pair, modelled synchronously

There is no dedicated DMA engine and no memory-to-memory instruction (§1). What
Phase 1 accelerator literature calls "DMA" — moving a block between two memory
tiers without going through the compute datapath — is approximated here by a
`LOAD`/`SCRATCHSTORE` (or `SCRATCHLOAD`/`STORE`) pair that happens to route
through a register the compiler never otherwise uses, i.e. the register is
acting as a transfer buffer rather than a compute operand. Every instruction
in that pair executes as an ordinary, blocking instruction in the simulator —
one register's worth of data moves per instruction, in full, before the next
instruction runs. No concurrency is modelled at execution time.

Real double buffering (Pallas' "ping/pong" scratchpad pair overlapping transfer
and compute — Review 2 report §5.3) is real hardware/software-pipelining
behaviour this project does not build, for the reasons in the report's §1.4
scoping table. Its *benefit* is instead estimated analytically by the
performance model (`docs/isa-extensions.md` companion tooling, implemented in
`tools/mdtsim`): for a tiled loop where consecutive iterations' transfer
instructions and compute instructions do not depend on each other, the model
reports `max(compute_cycles, transfer_cycles)` for that iteration instead of
their sum, and `compute_cycles + transfer_cycles` when they do depend on each
other (e.g. the very first iteration, which must load before it can compute).

## 5. Who decides what

| Decision | Owner |
|---|---|
| Tile size, tile iteration order | Compiler (Kandi's tiling pass) |
| What data is scratchpad-resident, and when | Compiler (tiling pass inserts `SCRATCHLOAD`/`SCRATCHSTORE`) |
| Whether a given `LOAD`/`STORE` targets DRAM or scratchpad | Compiler (sets the `AS` bit at emission time) |
| Actually copying bytes and executing instructions | Hardware / simulator — no hardware-side prefetch heuristics |
| Whether an iteration's transfer and compute overlap in the cost estimate | Performance model, from a static dependence check on the instruction trace — not real concurrency |

This mirrors the real division of labour in accelerator compiler stacks (Pallas'
`BlockSpec` + `grid`, cited in the Phase 2 report) — the compiler describes tiling
and staging, the runtime/hardware executes what it is told.

## 6. Tiling strategy — extended with one nesting level

Phase 1's tiling (decompose a large 2-D matrix into a grid of 4x4 register tiles)
gains a batch loop at the outermost level and a scratchpad-staging step before the
register-level loop:

```
for b in 0 .. batch:
    for i in 0 .. M/4:
        for j in 0 .. N/4:
            if A-tile(b,i,k) not scratchpad-resident:
                LOAD tile from DRAM into Mtmp ; SCRATCHSTORE Mtmp to scratchpad
            if B-tile(b,k,j) not scratchpad-resident:
                LOAD tile from DRAM into Mtmp ; SCRATCHSTORE Mtmp to scratchpad
            for k in 0 .. K/4:
                SCRATCHLOAD the A/B tiles from scratchpad into M-registers
                MATMUL / MATMULACC accumulate
            SCRATCHSTORE the result tile, then LOAD it back and STORE to DRAM
```

Register-level tiling (at most 3 tiles live in `M0`–`M7` at once, per Phase 1's
risk register R6) is unchanged. The new step is purely the DRAM<->scratchpad
staging (two register-mediated instructions per tile, §1), which is what lets
the ablation study (Review 2 report §8) measure a "tiling only" configuration
distinct from "no tiling."

## 7. Correctness discipline

Tiling changes *where* data lives, never *what* is computed. The differential
test for this layer is therefore stricter than a tolerance comparison: a tiled
execution of a program and a non-tiled (direct DRAM) execution of the *same*
program must match **bit-for-bit**, not just within a numerical tolerance —
unlike the tiled-matmul-accumulation-order tolerance check inherited from Phase 1
(which exists because FP32 addition is not associative), staging data through a
scratchpad performs no arithmetic and so introduces no legitimate source of
numerical difference at all. Any difference here is a bug, full stop.
