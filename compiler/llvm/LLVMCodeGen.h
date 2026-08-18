//===----------------------------------------------------------------------===//
//
// MatrixDSL - LLVM IR Generation
//
// SHARED INTERFACE - FROZEN FOR REVIEW 1
// Owner: Kandi Jeevitesh Reddy (24BCE0350)
//
// Lowers the shape-annotated AST to LLVM IR. This module and the MDT backend
// meet at the matrix intrinsic contract below; both sides were fixed before
// either began, so that integration is a link step rather than a negotiation.
//
//===----------------------------------------------------------------------===//

#ifndef MATRIXDSL_LLVMCODEGEN_H
#define MATRIXDSL_LLVMCODEGEN_H

#include <memory>
#include <string>
#include <unordered_map>

#include "../frontend/ast/AST.h"
#include "../frontend/semantic/SymbolTable.h"

namespace llvm {
class LLVMContext;
class Module;
class Value;
class Function;
class StructType;
template <typename, typename> class IRBuilder;
} // namespace llvm

namespace matrixdsl {

//===----------------------------------------------------------------------===//
// Matrix intrinsic contract
//
// Matrix operations are emitted as calls to these reserved names rather than
// as loop nests. Rationale: a loop nest would have to be re-recognised by the
// backend as a matrix multiply, and loop-idiom recognition is defeated by the
// optimizer's own transformations. An opaque call survives -O2 intact and
// lowers to exactly one MDT instruction.
//
// The tile geometry is part of the name, so the backend never has to infer it;
// the tiling pass has already decomposed anything larger than 4x4.
//===----------------------------------------------------------------------===//

namespace intrinsics {
constexpr const char *kMatMul4x4 = "mdt.matmul.4x4";
constexpr const char *kRelu4x4 = "mdt.relu.4x4";
constexpr const char *kTranspose4x4 = "mdt.transpose.4x4";

// void @mdt.matmul.4x4(float* %dst, float* %a, float* %b)
// void @mdt.relu.4x4(float* %dst, float* %src)
// void @mdt.transpose.4x4(float* %dst, float* %src)
} // namespace intrinsics

/// Runtime matrix descriptor, mirrored in LLVM IR as:
///
///     %struct.Matrix = type { float*, i32, i32 }
///
/// Data is stored row-major and contiguously: A[i][j] == data[i * cols + j].
struct MatrixDescriptor {
  llvm::Value *data = nullptr;
  int rows = 0;
  int cols = 0;
};

class LLVMCodeGen : public ASTVisitor {
public:
  explicit LLVMCodeGen(const std::string &moduleName);
  ~LLVMCodeGen();

  /// Generate IR for a fully analyzed program. Returns false if generation
  /// fails or if the resulting module does not verify.
  bool generate(Program &program, const SymbolTable &symbols);

  /// Run llvm::verifyModule. Any failure here is a compiler bug.
  bool verify(std::string &errorOut) const;

  std::string emitIR() const;
  bool writeIRToFile(const std::string &path) const;

  llvm::Module *module() const;

  void visit(NumberExpr &node) override;
  void visit(VariableExpr &node) override;
  void visit(BinaryExpr &node) override;
  void visit(MatrixLiteralExpr &node) override;
  void visit(CallExpr &node) override;
  void visit(MatrixDecl &node) override;
  void visit(Assignment &node) override;
  void visit(PrintStmt &node) override;
  void visit(Program &node) override;

private:
  void declareIntrinsics();
  void declareRuntimeFunctions();

  /// Emit element-wise binary operations as a loop over the flat data array.
  void emitElementwiseLoop(const MatrixDescriptor &dst,
                           const MatrixDescriptor &lhs,
                           const MatrixDescriptor &rhs, BinaryOp op);

  /// Emit a tiled matrix multiply. For 4x4 operands this is a single
  /// intrinsic call; larger shapes produce a tile loop nest.
  void emitMatMul(const MatrixDescriptor &dst, const MatrixDescriptor &lhs,
                  const MatrixDescriptor &rhs);

  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace matrixdsl

#endif // MATRIXDSL_LLVMCODEGEN_H
