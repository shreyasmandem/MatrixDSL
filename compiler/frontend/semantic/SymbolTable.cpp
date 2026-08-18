//===----------------------------------------------------------------------===//
//
// MatrixDSL - Symbol Table implementation
//
// Owner: Mudpe Parth Tulsidas (24BDS0353)
//
// Scoped name-to-shape binding. MatrixDSL v1.0 has exactly one global scope,
// but the table is built on a scope stack so that adding functions or blocks
// later does not require reworking every caller.
//
//===----------------------------------------------------------------------===//

#include "SymbolTable.h"

namespace matrixdsl {

void SymbolTable::pushScope() { scopes_.emplace_back(); }

void SymbolTable::popScope() {
  // Never pop the global scope - doing so would leave the table in a state
  // where declare() has nowhere to write.
  if (scopes_.size() > 1)
    scopes_.pop_back();
}

bool SymbolTable::declare(const std::string &name, MatrixType type, int line,
                          int column) {
  if (scopes_.empty())
    pushScope();

  auto &scope = scopes_.back();

  // Redeclaration is only an error within the SAME scope. A name declared in
  // an outer scope may legitimately be shadowed by an inner one, which is why
  // this checks the innermost map rather than calling lookup().
  if (scope.find(name) != scope.end())
    return false;

  Symbol symbol;
  symbol.name = name;
  symbol.type = type;
  symbol.declaredLine = line;
  symbol.declaredColumn = column;

  scope.emplace(name, std::move(symbol));
  declarationOrder_.push_back(name);
  return true;
}

std::optional<MatrixType> SymbolTable::lookup(const std::string &name) const {
  // Innermost scope outward, so an inner declaration shadows an outer one.
  for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
    const auto found = it->find(name);
    if (found != it->end())
      return found->second.type;
  }
  return std::nullopt;
}

const Symbol *SymbolTable::lookupSymbol(const std::string &name) const {
  for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
    const auto found = it->find(name);
    if (found != it->end())
      return &found->second;
  }
  return nullptr;
}

void SymbolTable::markAssigned(const std::string &name) {
  for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
    const auto found = it->find(name);
    if (found != it->end()) {
      found->second.everAssigned = true;
      return;
    }
  }
}

void SymbolTable::markRead(const std::string &name) {
  for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
    const auto found = it->find(name);
    if (found != it->end()) {
      found->second.everRead = true;
      return;
    }
  }
}

std::vector<const Symbol *> SymbolTable::unusedSymbols() const {
  std::vector<const Symbol *> unused;
  for (const std::string &name : declarationOrder_) {
    const Symbol *symbol = lookupSymbol(name);
    if (symbol && !symbol->everRead)
      unused.push_back(symbol);
  }
  return unused;
}

/// All symbols in declaration order.
///
/// Declaration order, not hash order: iterating an unordered_map would produce
/// output that differs between runs and makes golden-file tests flaky.
std::vector<const Symbol *> SymbolTable::allSymbols() const {
  std::vector<const Symbol *> symbols;
  for (const std::string &name : declarationOrder_) {
    if (const Symbol *symbol = lookupSymbol(name))
      symbols.push_back(symbol);
  }
  return symbols;
}

void SymbolTable::clear() {
  scopes_.clear();
  declarationOrder_.clear();
  pushScope();
}

} // namespace matrixdsl
