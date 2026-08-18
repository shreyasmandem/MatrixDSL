# Risk Register

**Project:** MatrixDSL | **Team 13**

Severity is Likelihood x Impact. Risks are ordered by severity.

| Level | Meaning |
|---|---|
| High | Threatens project completion |
| Medium | Threatens a deliverable or the schedule |
| Low | Manageable within existing slack |

---

## R1 — LLVM backend complexity exceeds available time

| Field | Value |
|---|---|
| **Likelihood** | High |
| **Impact** | High |
| **Severity** | **Critical** |
| **Owner** | Shreyas Mandem |

**Description.** A complete SelectionDAG and TableGen target is the most technically
demanding component in the project. Public tutorials for constructing a toy LLVM
backend run to several hundred pages, and the calling-convention, frame-lowering and
MC-layer plumbing each carry a substantial learning curve. If the scalar phases
consume more time than budgeted, the matrix instructions — the actual novelty — may
never be reached.

**Mitigation.**

1. **Phased implementation.** Seven phases (registration, scalar, memory, control
   flow, vector, matrix, tiling), each independently demonstrable. Partial completion
   still yields a working artifact.
2. **Designed fallback.** `MDTDirectEmitter` walks optimized LLVM IR, pattern matches
   instructions directly, performs linear-scan register assignment, and prints MDT
   assembly — bypassing SelectionDAG, TableGen and the MC layer entirely. It satisfies
   the project abstract (LLVM IR, LLVM optimizer, custom target code generation) and
   produces identical assembly, so simulator, tests and benchmarks are unaffected.
3. **Decision point at Week 8.** If scalar assembly is not emitting correctly through
   the TableGen path by then, the team switches. This is a planned engineering
   decision with stated criteria, not an improvised retreat.

**Residual risk.** Low. The fallback is well understood and bounded in scope.

---

## R3 — Serial dependency blocks three members behind the parser

| Field | Value |
|---|---|
| **Likelihood** | High |
| **Impact** | High |
| **Severity** | **Critical** |
| **Owner** | All |

**Description.** The natural pipeline order means semantic analysis, IR generation and
the backend all wait on a finished parser. Three of four members idle for weeks, and
all the work compresses into the final third of the schedule.

**Mitigation.** Two interfaces frozen and committed to `main` **before** implementation
began:

- Token enumeration and AST node hierarchy (`compiler/frontend/ast/AST.h`) — Parth and
  Kandi develop against a fixed contract and test with hand-constructed AST fixtures.
- Matrix intrinsic naming (`@mdt.matmul.4x4`) — Shreyas develops instruction selection
  against hand-written LLVM IR without waiting for the IR generator.

**Status.** **Mitigated as of Review 1.** Both interfaces are committed. All four
members can work in parallel from Week 3.

**Residual risk.** Low.

---

## R4 — Interface drift between IR generation and backend

| Field | Value |
|---|---|
| **Likelihood** | Medium |
| **Impact** | High |
| **Severity** | **High** |
| **Owner** | Kandi Jeevitesh Reddy, Shreyas Mandem |

**Description.** The IR-to-backend boundary crosses the two most complex modules. If
the IR generator emits matrix operations in a form the backend does not expect,
integration fails at Week 8 with little schedule left.

**Mitigation.** The exact intrinsic signatures are fixed and documented in
`docs/llvm-backend.md` §5.1 before either side begins:

```llvm
declare void @mdt.matmul.4x4(float* %dst, float* %a, float* %b)
declare void @mdt.relu.4x4(float* %dst, float* %src)
declare void @mdt.transpose.4x4(float* %dst, float* %src)
```

Backend tests are written against hand-authored IR matching these signatures, so any
drift is caught by a failing test rather than at integration.

**Residual risk.** Low.

---

## R2 — LLVM build and setup difficulty on Windows

| Field | Value |
|---|---|
| **Likelihood** | Medium |
| **Impact** | Medium |
| **Severity** | **Medium** |
| **Owner** | All |

**Description.** Building LLVM from source on Windows is slow and error-prone and can
easily consume a week. Version differences between members cause API mismatches that
appear as unexplained compile errors.

**Mitigation.**

- Use prebuilt LLVM via `vcpkg install llvm[core]` or WSL2 rather than building from
  source.
- Pin a single LLVM version (17 or 18) across the whole team, recorded in the README.
- The frontend has no LLVM dependency, so Vinay and Parth are productive before LLVM
  is installed at all.
- The simulator is standalone, so Shreyas can develop and verify the ISA without LLVM.

**Residual risk.** Low.

---

## R5 — Tiling correctness for non-multiples of four

| Field | Value |
|---|---|
| **Likelihood** | Medium |
| **Impact** | Medium |
| **Severity** | **Medium** |
| **Owner** | Shreyas Mandem |

**Description.** A 5x7 matrix does not divide evenly into 4x4 tiles. Incorrect padding
produces silently wrong numerical results rather than a crash, which is the hardest
class of bug to detect.

**Mitigation.** Zero-padding to the next multiple of four is specified up front; the
logical dimensions are preserved in the matrix descriptor so `print` and shape checking
still observe the true size. Dedicated boundary tests cover 5x7, 1x1, 1x4 and 4x1.
Differential testing against native execution catches any numerical divergence.

**Residual risk.** Low.

---

## R6 — Register pressure with only eight matrix registers

| Field | Value |
|---|---|
| **Likelihood** | Low |
| **Impact** | Medium |
| **Severity** | **Low** |
| **Owner** | Shreyas Mandem |

**Description.** A tiled matrix multiply wants three live tiles (A tile, B tile,
accumulator). With M0–M7 there is room for modest unrolling, but aggressive unrolling
would force spills at 64 bytes per tile.

**Mitigation.** The tiling loop is written to keep at most three tiles live. A spill
path is specified (`STORET` / `LOADT` against a 64-byte stack slot) and tested
explicitly.

**Residual risk.** Low.

---

## R7 — Member unavailability

| Field | Value |
|---|---|
| **Likelihood** | Low |
| **Impact** | Medium |
| **Severity** | **Low** |
| **Owner** | All |

**Description.** Illness or other commitments could remove a member during a critical
phase.

**Mitigation.** All module interfaces are documented, so work is transferable rather
than locked in one person's head. Every module has a named secondary in the
responsibility matrix. The contribution log records progress continuously, so a
handover starts from a known state.

**Residual risk.** Low.

---

## Summary

| Risk | Severity | Status |
|---|---|---|
| R1 — Backend complexity | Critical | Mitigated by phasing and designed fallback |
| R3 — Serial dependency | Critical | **Resolved** — interfaces frozen at Review 1 |
| R4 — Interface drift | High | Mitigated by documented intrinsic contract |
| R2 — LLVM setup | Medium | Mitigated by prebuilt LLVM and version pinning |
| R5 — Tiling correctness | Medium | Mitigated by specified padding and boundary tests |
| R6 — Register pressure | Low | Mitigated by tiling loop design and spill path |
| R7 — Member unavailability | Low | Mitigated by documented interfaces and secondaries |

The two critical risks both have concrete mitigations already executed rather than
merely planned. R3 is fully resolved as of this review.
