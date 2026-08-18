//===----------------------------------------------------------------------===//
//
// MatrixDSL - Symbol Table
//
// SHARED INTERFACE - FROZEN FOR REVIEW 1
// Owner: Mudpe Parth Tulsidas (24BDS0353)
//
//===----------------------------------------------------------------------===//

#ifndef MATRIXDSL_SYMBOLTABLE_H
#define MATRIXDSL_SYMBOLTABLE_H

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "MatrixType.h"

namespace matrixdsl {

struct Symbol {
  std::string name;
  MatrixType type;
  int declaredLine = 0;
  int declaredColumn = 0;
  bool everAssigned = false;
  bool everRead = false;
};

/// Scoped name-to-shape binding.
///
/// MatrixDSL v1.0 has no block scoping - there is exactly one global scope -
/// but the table is written with a scope stack so that adding functions or
/// blocks later does not require reworking every caller.
class SymbolTable {
public:
  SymbolTable() { pushScope(); }

  void pushScope();
  void popScope();
  size_t scopeDepth() const { return scopes_.size(); }

  /// Declare a name in the innermost scope.
  /// Returns false if the name is already declared in that same scope.
  bool declare(const std::string &name, MatrixType type, int line = 0,
               int column = 0);

  /// Look up a name, innermost scope outward.
  std::optional<MatrixType> lookup(const std::string &name) const;

  /// Look up the full symbol record, for diagnostics that need the
  /// declaration site ("note: `A` declared here").
  const Symbol *lookupSymbol(const std::string &name) const;

  bool isDeclared(const std::string &name) const {
    return lookup(name).has_value();
  }

  void markAssigned(const std::string &name);
  void markRead(const std::string &name);

  /// Symbols declared but never read. Not an error, but worth warning about.
  std::vector<const Symbol *> unusedSymbols() const;

  /// All symbols in declaration order, for `--dump-symbols`.
  std::vector<const Symbol *> allSymbols() const;

  void clear();

private:
  std::vector<std::unordered_map<std::string, Symbol>> scopes_;
  std::vector<std::string> declarationOrder_;
};

} // namespace matrixdsl

#endif // MATRIXDSL_SYMBOLTABLE_H
