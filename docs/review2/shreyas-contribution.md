# Individual Contribution — Shreyas Mandem (Phase 2 / Review 2)

| | |
|---|---|
| **Name** | Shreyas Mandem |
| **Register Number** | 24BCE0381 |
| **Role** | MDT ISA extensions + memory hierarchy + performance model |
| **Branch** | `shreyas` |
| **Review** | Review 2 |

---

## 1. Scope of my Phase 2 component

Same module boundary as Review 1, extended: everything below optimized LLVM IR.
Phase 2 adds 7 new instructions, a second memory tier (scratchpad), and a
lightweight, explicitly non-cycle-accurate performance model — the tooling the
`docs/review2/Review2_Report.md` §8 ablation study is measured with.

---

## 2. What changed since Review 1 — verification, not just new work

A C++ toolchain became available on the authoring machine for the first time
since this branch was started. Before writing a single line of Phase 2 code, I
built and ran everything already on this branch — and found two real bugs that
had shipped, unverified, at Review 1:

1. **`parseInteger` rejected any offset with whitespace between the sign and
   the digits** — e.g. `"+ 0"` from `[R1 + 0]`. `tokenizeLine` keeps bracket
   contents verbatim by design, so that whitespace is the *normal* case for
   every memory operand, not an edge case. **7 of 10** Review 1 backend tests
   failed to assemble because of this.
2. **`LOADT`/`STORET` shared the `LOAD`/`STORE` `Opcode` enum value**, so the
   arity check — keyed on `Opcode`, not the original mnemonic — always
   expected 2 operands and rejected `LOADT`'s documented 3-operand form.

Both are fixed, and all 10 Review 1 tests now produce their exact documented
expected values (not just exit code 0) — see the `shreyas` branch commit
history for the full before/after. This matters for Review 2 because every
Phase 2 addition below builds directly on top of that now-verified code.

I also caught and fixed **two inconsistencies inside my own frozen Phase 2
docs** before implementing against them: `docs/memory-hierarchy.md` described
`SCRATCHLOAD`/`SCRATCHSTORE` as if they moved data directly between DRAM and
scratchpad, while `docs/isa-extensions.md` gave them a register-destination
form — the two cannot both be true. Resolved in favour of the form consistent
with MDT's design since Phase 1 (load/store only, always through a register,
never memory-to-memory). Separately, the two `CVT` instructions disagreed
about which half of a register holds the BF16 payload. Both fixes are commits
on `main`, made *before* implementation, not discovered by the implementation
and patched around.

---

## 3. What I completed for Review 2

### 3.1 Seven new MDT instructions — implemented and tested

`docs/isa-extensions.md`, `tools/mdtsim/MDTSim.h`/`.cpp`, `Assembler.cpp`

| Instruction | Purpose | Test |
|---|---|---|
| `MATMULACC` | Accumulate form: `Md = Md + (Ma x Mb)` | `matmulacc.s` |
| `MATMULRELU` | Fused epilogue: `Md = relu(Ma x Mb)` | `matmulrelu.s` |
| `CVT.F32.BF16` / `CVT.BF16.F32` | Mixed-precision storage (rounding, not a new compute mode) | `bf16_convert.s` |
| `VREDSUM` | 4-lane reduction | `reductions.s` |
| `MROWMAX` / `MROWSUM` | Row-wise reductions, softmax primitives | `reductions.s` |

**Design decision — `MATMULACC`/`MATMULRELU` share one product routine.**
`matmulProduct()` computes the 4x4 product once; `MATMUL`, `MATMULACC` and
`MATMULRELU` each apply a different one-line epilogue to it (overwrite,
accumulate, clamp). This is the simulator's own small demonstration of the
epilogue-fusion idea the instructions themselves implement in the ISA.

**Design decision — the BF16 bit convention.** The payload lives in the top 16
bits of a 32-bit register, bottom 16 zero — which falls directly out of what
BF16 *is* (the top half of an FP32 bit pattern), and means `CVT.BF16.F32` is
an exact copy under that convention rather than requiring real bit-shuffling.
Documented in full in `docs/isa-extensions.md` §2.3, including why the
instruction still exists as a distinct opcode.

### 3.2 Scratchpad memory hierarchy — implemented and tested

`docs/memory-hierarchy.md`, `MDTSim.h`/`.cpp`

A second 64 KiB array (`state_.scratchpad`), addressed via one bit reused from
Phase 1's I-type encoding reserved space (`docs/isa.md` §6) — no opcode was
spent on it. `SCRATCHLOAD`/`SCRATCHSTORE` are `LOAD`/`STORE` assembled with
that bit set, exactly like Phase 1's `LOADT`/`STORET` shared the `LOAD`/`STORE`
opcode via a mode bit.

**Design decision — no memory-to-memory instruction.** MDT has never had one;
`docs/isa.md` states the memory model as "load/store" specifically because
only those two opcodes ever touch memory, always through a register. Staging a
tile from DRAM into the scratchpad is therefore two register-mediated
instructions (an ordinary `LOAD`, then a `SCRATCHSTORE` of the same register),
not a bulk copy — verified end to end by `scratchpad_roundtrip.s`, which moves
both a scalar and a full 4x4 tile through DRAM → register → scratchpad →
register → DRAM and checks the value survives every hop unchanged.

### 3.3 Lightweight performance model — implemented and tested

`MDTSim.h`/`.cpp` (`PerformanceModel`, `classifyForPerf`, `instructionLatency`,
`recordPerfInstruction`, `finalizePerformanceModel`), `--perf` flag in `main.cpp`

Explicitly not cycle-accurate: a static per-instruction latency table (DRAM
access costs more than scratchpad access — 8 vs 1 — specifically so that
moving traffic off DRAM is visible in the estimate at all), plus a
**non-overlapping pairwise reduction** over a run-length-encoded instruction
trace as the analytical stand-in for double buffering (`docs/memory-hierarchy.md`
§4). This produces the three columns the Review 2 report's §8 ablation study
needs from one program: instruction count, bytes moved (split DRAM vs
scratchpad), and two cycle estimates.

**A real bug I found and fixed before it shipped: my first pairing design
double-counted.** The initial implementation paired each instruction against
"whatever run is currently pending" and immediately made that instruction the
new pending run — which means an interior run gets compared against *both* of
its neighbours, charged into two overlapping `max()`s instead of one. Hand-
tracing a 6-instruction example produced an "overlap-aware" estimate (44)
*larger* than the sequential total (36) — a self-evidently wrong result for a
value that is supposed to be an upper bound reduction. Caught before ever
running it, by working through the arithmetic by hand. Fixed by splitting
into two phases: build the run-length list incrementally (cheap, no pairing
decisions made yet), then do a single non-overlapping left-to-right pass over
the *finished* list, where each run is consumed by at most one pairing.
Re-verified against the same 6-run trace and against the two ablation-demo
files below by hand before trusting the code.

**`tiling_naive.s` / `tiling_scratchpad.s`** are a matched pair: identical
computation (the same 4x4 tile multiplied against itself three times), only
the memory path differs — one reloads from DRAM on every use, the other loads
once and stages through the scratchpad. Both files carry a fully worked,
independently-verified hand-derivation of their expected `--perf` output in
their header comments (including the two rounds of correcting my own
arithmetic once the traced instruction sequence was checked against the
actual code — my first draft of the comments forgot that the address-setup
`ADD` instructions merge into the adjacent run under RLE, and that `HALT` is
never counted at all since `step()` returns before recording it). Verified
result: naive costs 37 sequential / 28 overlap-aware cycles and moves 192
bytes from DRAM; the scratchpad version costs 26 / 22 and moves only 64 bytes
from DRAM (256 from the cheaper scratchpad instead) — a concrete, runnable
demonstration of exactly the effect the Phase 2 report's §8 ablation study is
built around.

---

## 4. Evidence

| Evidence | Location |
|---|---|
| ISA extension spec | `docs/isa-extensions.md` |
| Memory hierarchy spec | `docs/memory-hierarchy.md` |
| Simulator interface + implementation | `tools/mdtsim/MDTSim.h`, `MDTSim.cpp` |
| Assembler extensions | `tools/mdtsim/Assembler.cpp` |
| CLI `--perf` flag | `tools/mdtsim/main.cpp` |
| New instruction tests | `tests/backend/matmulacc.s`, `matmulrelu.s`, `bf16_convert.s`, `reductions.s` |
| Memory hierarchy test | `tests/backend/scratchpad_roundtrip.s` |
| Ablation demo pair | `tests/backend/tiling_naive.s`, `tiling_scratchpad.s` |
| Phase 1 bug fixes | `shreyas` branch commit history |
| Commits | branch `shreyas` |

---

## 5. Build and test status — actually verified this time

```bash
cmake -S . -B build && cmake --build build -j
```

```bash
for f in tests/backend/*.s; do ./build/bin/mdtsim "$f" --quiet; echo "$f: $?"; done
```

**All 17 backend tests pass** (10 from Review 1, re-verified after the bug
fixes above, plus 7 new for Phase 2), every one checked against a value
computed independently of the simulator, not merely exit code 0. This is a
change from Review 1, where the equivalent note read "not yet compiled."

---

## 6. Review 3 targets

| # | Target |
|---|---|
| 1 | MDT registered as a real LLVM target; scalar instructions selecting from IR |
| 2 | Wire Kandi's Tensor IR fusion output to the new `MATMULACC`/`MATMULRELU` intrinsics |
| 3 | Wire Kandi's tiling pass output to `SCRATCHLOAD`/`SCRATCHSTORE` emission |
| 4 | Run the full 4-configuration ablation study end to end on both Phase 2 benchmarks, not just the hand-written demo pair |
| 5 | `exp` runtime call (`docs/isa-extensions.md` §5) implemented in the runtime library for `softmax` |

---

## 7. What I can be questioned on

- Why `SCRATCHLOAD`/`SCRATCHSTORE` are register-destination, not
  memory-to-memory, and how that follows from MDT's design since Phase 1
- Why DRAM and scratchpad have different latencies in the performance model,
  and why that specific choice matters for the ablation study to mean anything
- Why the first pairing algorithm for `overlapAwareCycles` was wrong, and what
  "double-counting an interior run" actually means concretely
- Why `MATMULACC`/`MATMULRELU`/`MATMUL` share one product routine
- Where the BF16 payload lives in a register, and why `CVT.BF16.F32` is a
  distinct instruction despite being a plain copy today
- Which reserved encoding bit the scratchpad address space uses, and why that
  was possible without a new opcode
- The two Review 1 assembler bugs, precisely: what broke, why, and how the fix
  works
