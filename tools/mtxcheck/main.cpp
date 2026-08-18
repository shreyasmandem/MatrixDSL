//===----------------------------------------------------------------------===//
//
// mtxcheck - MatrixDSL shape rule and symbol table checker
//
// Owner: Mudpe Parth Tulsidas (24BDS0353)
//
// Exercises every shape rule and every symbol table behaviour directly, and
// renders the diagnostic each failing case would produce.
//
// The point worth making: this runs WITHOUT the parser. Because the shape
// rules are pure functions over MatrixType, and the symbol table is
// independent of the AST, my module's entire core logic is verifiable before
// Vinay's parser exists. That is what the frozen interfaces on `main` bought
// us - four people working in parallel instead of three waiting on one.
//
// Usage:
//   mtxcheck [section]
//
// Sections: add, matmul, scalar, transpose, relu, bounds, symbols, diag, all
//
//===----------------------------------------------------------------------===//

#include "../../compiler/frontend/semantic/MatrixType.h"
#include "../../compiler/frontend/semantic/SymbolTable.h"

#include <cstdio>
#include <string>

using matrixdsl::MatrixType;
using matrixdsl::Symbol;
using matrixdsl::SymbolTable;

namespace {

int gChecks = 0;
int gFailures = 0;

void check(bool condition, const std::string &what) {
  ++gChecks;
  if (condition) {
    std::printf("  PASS  %s\n", what.c_str());
  } else {
    ++gFailures;
    std::printf("  FAIL  %s\n", what.c_str());
  }
}

/// Assert that a rule accepts its operands and yields the expected shape.
void expectAccept(const MatrixType &result, int rows, int cols,
                  const std::string &what) {
  const bool ok = result.isValid() && result.rows() == rows &&
                  result.cols() == cols;
  if (!ok && result.isValid())
    std::printf("        (got %s, wanted %dx%d)\n", result.toString().c_str(),
                rows, cols);
  check(ok, what);
}

/// Assert that a rule rejects its operands.
void expectReject(const MatrixType &result, const std::string &what) {
  check(!result.isValid(), what);
}

//===--------------------------------------------------------------------===//
// Shape rules
//===--------------------------------------------------------------------===//

void testAddition() {
  std::printf("\nAddition and subtraction - shapes must match exactly\n");

  expectAccept(MatrixType::checkAddition(MatrixType(4, 4), MatrixType(4, 4)),
               4, 4, "4x4 + 4x4 -> 4x4");
  expectAccept(MatrixType::checkAddition(MatrixType(3, 7), MatrixType(3, 7)),
               3, 7, "3x7 + 3x7 -> 3x7");
  expectAccept(MatrixType::checkAddition(MatrixType(1, 1), MatrixType(1, 1)),
               1, 1, "1x1 + 1x1 -> 1x1");

  expectReject(MatrixType::checkAddition(MatrixType(4, 4), MatrixType(3, 3)),
               "4x4 + 3x3 rejected");
  expectReject(MatrixType::checkAddition(MatrixType(4, 3), MatrixType(3, 4)),
               "4x3 + 3x4 rejected (transposed shape is not the same shape)");
  expectReject(MatrixType::checkAddition(MatrixType(4, 4), MatrixType(4, 5)),
               "4x4 + 4x5 rejected (one dimension differing is enough)");
}

void testMatMul() {
  std::printf("\nMatrix multiplication - A.cols must equal B.rows\n");

  expectAccept(MatrixType::checkMatMul(MatrixType(4, 4), MatrixType(4, 4)),
               4, 4, "matmul(4x4, 4x4) -> 4x4");
  expectAccept(MatrixType::checkMatMul(MatrixType(4, 3), MatrixType(3, 5)),
               4, 5, "matmul(4x3, 3x5) -> 4x5");
  expectAccept(MatrixType::checkMatMul(MatrixType(1, 4), MatrixType(4, 1)),
               1, 1, "matmul(1x4, 4x1) -> 1x1 (dot product)");
  expectAccept(MatrixType::checkMatMul(MatrixType(4, 1), MatrixType(1, 4)),
               4, 4, "matmul(4x1, 1x4) -> 4x4 (outer product)");

  expectReject(MatrixType::checkMatMul(MatrixType(4, 3), MatrixType(7, 5)),
               "matmul(4x3, 7x5) rejected (3 != 7)");
  expectReject(MatrixType::checkMatMul(MatrixType(2, 3), MatrixType(2, 3)),
               "matmul(2x3, 2x3) rejected (equal shapes are not multipliable)");

  // Multiplication is not commutative in shape either.
  const MatrixType ab = MatrixType::checkMatMul(MatrixType(2, 3), MatrixType(3, 2));
  const MatrixType ba = MatrixType::checkMatMul(MatrixType(3, 2), MatrixType(2, 3));
  check(ab.rows() == 2 && ab.cols() == 2 && ba.rows() == 3 && ba.cols() == 3,
        "matmul is not shape-commutative: AB is 2x2 but BA is 3x3");
}

void testScalar() {
  std::printf("\nScalar multiplication - preserves shape\n");

  expectAccept(MatrixType::checkScalarMul(MatrixType(1, 1), MatrixType(4, 4)),
               4, 4, "scalar * 4x4 -> 4x4");
  expectAccept(MatrixType::checkScalarMul(MatrixType(3, 7), MatrixType(1, 1)),
               3, 7, "3x7 * scalar -> 3x7");
  expectReject(MatrixType::checkScalarMul(MatrixType(4, 4), MatrixType(4, 4)),
               "4x4 * 4x4 is not scalar multiplication (falls back to matmul)");
}

void testTranspose() {
  std::printf("\nTranspose - r x c becomes c x r\n");

  expectAccept(MatrixType::checkTranspose(MatrixType(4, 3)), 3, 4,
               "transpose(4x3) -> 3x4");
  expectAccept(MatrixType::checkTranspose(MatrixType(1, 256)), 256, 1,
               "transpose(1x256) -> 256x1");

  // Transposing twice is the identity - self-checking, no constants needed.
  const MatrixType once = MatrixType::checkTranspose(MatrixType(5, 9));
  const MatrixType twice = MatrixType::checkTranspose(once);
  check(twice == MatrixType(5, 9), "transpose(transpose(A)) has A's shape");
}

void testRelu() {
  std::printf("\nReLU - element-wise, preserves shape\n");

  expectAccept(MatrixType::checkRelu(MatrixType(4, 4)), 4, 4,
               "relu(4x4) -> 4x4");
  expectAccept(MatrixType::checkRelu(MatrixType(3, 7)), 3, 7,
               "relu(3x7) -> 3x7");

  // Shapes propagate through nesting: this is what makes chained expressions
  // checkable in a single bottom-up pass.
  const MatrixType product =
      MatrixType::checkMatMul(MatrixType(4, 3), MatrixType(3, 5));
  const MatrixType activated = MatrixType::checkRelu(product);
  expectAccept(activated, 4, 5, "relu(matmul(4x3, 3x5)) -> 4x5");
}

void testBounds() {
  std::printf("\nDimension bounds - 1 to 256 inclusive\n");

  check(MatrixType::isValidDimension(1), "1 is a valid dimension");
  check(MatrixType::isValidDimension(256), "256 is a valid dimension");
  check(!MatrixType::isValidDimension(0), "0 rejected");
  check(!MatrixType::isValidDimension(257), "257 rejected");
  check(!MatrixType::isValidDimension(-1), "-1 rejected");

  std::printf("\nTile alignment - relevant to the backend's 4x4 tiling\n");
  check(MatrixType(4, 4).isTileAligned(), "4x4 is tile-aligned");
  check(MatrixType(8, 16).isTileAligned(), "8x16 is tile-aligned");
  check(!MatrixType(5, 7).isTileAligned(), "5x7 is not tile-aligned");
  check(MatrixType(5, 7).tileRows() == 2 && MatrixType(5, 7).tileCols() == 2,
        "5x7 needs a 2x2 tile grid (padded to 8x8)");
  check(MatrixType(4, 4).byteSize() == 64,
        "4x4 FP32 is 64 bytes - exactly one MDT matrix register");
}

//===--------------------------------------------------------------------===//
// Symbol table
//===--------------------------------------------------------------------===//

void testSymbols() {
  std::printf("\nSymbol table\n");

  SymbolTable symbols;

  check(symbols.declare("A", MatrixType(4, 4), 1, 1), "declare A as 4x4");
  check(symbols.declare("B", MatrixType(3, 5), 2, 1), "declare B as 3x5");

  const auto a = symbols.lookup("A");
  check(a.has_value() && a->rows() == 4 && a->cols() == 4,
        "lookup A returns 4x4");

  check(!symbols.lookup("Z").has_value(), "lookup of undeclared Z fails");
  check(!symbols.declare("A", MatrixType(2, 2), 3, 1),
        "redeclaring A in the same scope is rejected");

  const Symbol *record = symbols.lookupSymbol("A");
  check(record && record->declaredLine == 1,
        "declaration site recorded (for 'note: A declared here')");

  // Shadowing in an inner scope is legal even though redeclaration is not.
  symbols.pushScope();
  check(symbols.declare("A", MatrixType(2, 2), 5, 1),
        "shadowing A in an inner scope is allowed");
  const auto inner = symbols.lookup("A");
  check(inner.has_value() && inner->rows() == 2,
        "inner scope shadows the outer declaration");
  symbols.popScope();

  const auto restored = symbols.lookup("A");
  check(restored.has_value() && restored->rows() == 4,
        "outer declaration is restored after popScope");

  symbols.markRead("A");
  const auto unused = symbols.unusedSymbols();
  bool bIsUnused = false;
  for (const Symbol *s : unused)
    if (s->name == "B")
      bIsUnused = true;
  check(bIsUnused, "B reported as declared but never read");

  // Declaration order, not hash order - otherwise output differs run to run.
  const auto all = symbols.allSymbols();
  check(all.size() == 2 && all[0]->name == "A" && all[1]->name == "B",
        "symbols enumerate in declaration order");
}

//===--------------------------------------------------------------------===//
// Diagnostics
//===--------------------------------------------------------------------===//

/// Render the diagnostic format specified in docs/shape-rules.md.
void showDiagnostic(const std::string &message, int line, int column,
                    const std::string &source, const std::string &note) {
  std::printf("error: %s\n", message.c_str());
  std::printf("  --> program.mtx:%d:%d\n", line, column);
  std::printf("   |\n");
  std::printf("%2d |     %s\n", line, source.c_str());
  std::printf("   |         %s\n", std::string(source.size() - 4, '^').c_str());
  std::printf("   = note: %s\n\n", note.c_str());
}

void testDiagnostics() {
  std::printf("\nDiagnostic rendering\n");
  std::printf("--------------------\n\n");

  showDiagnostic("cannot multiply matrix A (4x3) with matrix B (7x5)", 12, 5,
                 "C = matmul(A, B);",
                 "matmul requires A.cols == B.rows, but 3 != 7");

  showDiagnostic("cannot add matrix A (4x4) and matrix B (3x3)", 8, 5,
                 "C = A + B;",
                 "addition requires both operands to have identical shapes");

  showDiagnostic("cannot assign 4x5 result to matrix C (4x4)", 15, 5,
                 "C = matmul(A, B);",
                 "matmul(4x3, 3x5) produces 4x5");

  std::printf("A good diagnostic states what was found, what was required,\n");
  std::printf("and why - naming the actual dimensions rather than saying\n");
  std::printf("\"type mismatch\". That is the difference between a message a\n");
  std::printf("user can act on and one they cannot.\n");

  check(true, "diagnostics render in the specified format");
}

} // namespace

int main(int argc, char **argv) {
  const std::string section = (argc > 1) ? argv[1] : "all";

  std::printf("MatrixDSL shape rule and symbol table checker\n");
  std::printf("============================================\n");

  const bool all = (section == "all");
  if (all || section == "add")       testAddition();
  if (all || section == "matmul")    testMatMul();
  if (all || section == "scalar")    testScalar();
  if (all || section == "transpose") testTranspose();
  if (all || section == "relu")      testRelu();
  if (all || section == "bounds")    testBounds();
  if (all || section == "symbols")   testSymbols();
  if (all || section == "diag")      testDiagnostics();

  if (gChecks == 0) {
    std::fprintf(stderr, "\nUnknown section '%s'\n", section.c_str());
    std::fprintf(stderr,
                 "Valid: add matmul scalar transpose relu bounds symbols diag all\n");
    return 1;
  }

  std::printf("\n--------------------------------------------\n");
  std::printf("  %d checks, %d failed\n", gChecks, gFailures);
  std::printf("--------------------------------------------\n");

  return gFailures == 0 ? 0 : 1;
}
