//===-- MDTISelLowering.h - MDT DAG Lowering Interface ----------*- C++ -*-===//
//
// MatrixDSL Target (MDT) SelectionDAG lowering.
// Owner: Shreyas Mandem (24BCE0381)
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MDT_MDTISELLOWERING_H
#define LLVM_LIB_TARGET_MDT_MDTISELLOWERING_H

#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/TargetLowering.h"

namespace llvm {

class MDTSubtarget;

namespace MDTISD {

/// Target-specific DAG nodes.
///
/// MATMUL, TRANSPOSE and RELU exist because LLVM has no notion of a matrix
/// operation. They are introduced by LowerCall when it encounters a call to
/// one of the reserved @mdt.* intrinsic names emitted by the IR generator,
/// and each selects to exactly one MDT instruction.
enum NodeType : unsigned {
  FIRST_NUMBER = ISD::BUILTIN_OP_END,

  RET_FLAG,
  CALL,

  MATMUL,     // v16f32 = MATMUL v16f32, v16f32
  TRANSPOSE,  // v16f32 = TRANSPOSE v16f32
  RELU        // v16f32 = RELU v16f32
};

} // namespace MDTISD

class MDTTargetLowering : public TargetLowering {
public:
  explicit MDTTargetLowering(const TargetMachine &TM,
                             const MDTSubtarget &STI);

  const char *getTargetNodeName(unsigned Opcode) const override;

  SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;

  SDValue LowerFormalArguments(SDValue Chain, CallingConv::ID CallConv,
                               bool IsVarArg,
                               const SmallVectorImpl<ISD::InputArg> &Ins,
                               const SDLoc &DL, SelectionDAG &DAG,
                               SmallVectorImpl<SDValue> &InVals) const override;

  SDValue LowerCall(CallLoweringInfo &CLI,
                    SmallVectorImpl<SDValue> &InVals) const override;

  SDValue LowerReturn(SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
                      SelectionDAG &DAG) const override;

private:
  /// Recognise a call to a reserved matrix intrinsic and replace it with the
  /// corresponding MDTISD node.
  ///
  /// This is the hinge of the whole design. The IR generator deliberately
  /// emits an opaque call rather than a loop nest, because a loop nest would
  /// have to be re-recognised here by idiom matching - and loop-idiom
  /// recognition is routinely defeated by the optimizer's own transformations.
  /// An opaque call survives -O2 untouched.
  SDValue lowerMatrixIntrinsic(CallLoweringInfo &CLI, SelectionDAG &DAG,
                               bool &Handled) const;

  /// True if `Name` is one of mdt.matmul.4x4, mdt.relu.4x4,
  /// mdt.transpose.4x4.
  static bool isMatrixIntrinsic(StringRef Name);

  /// Map an intrinsic name onto its MDTISD node type.
  static unsigned matrixNodeForIntrinsic(StringRef Name);

  const MDTSubtarget &Subtarget;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_MDT_MDTISELLOWERING_H
