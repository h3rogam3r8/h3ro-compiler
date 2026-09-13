#ifndef HERO_SEMA_H
#define HERO_SEMA_H

#include "hero/AST.h"
#include "hero/Diagnostics.h"
#include "hero/SourceFile.h"

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace hero {

// First half of semantic analysis: does every name actually refer to
// something. Types and shapes are a separate pass, this one only cares
// about names.
class Sema {
public:
  explicit Sema(const SourceFile &file);

  // False if anything was wrong. diags() says what.
  bool check(const Program &program);

  const Diagnostics &diags() const { return diags_; }

private:
  void checkFunction(const Function &fn);
  void checkBlock(const Block &block);
  void checkExpr(const Expr &expr);

  // Reports E002 and returns false if the name is taken.
  bool declare(const std::string &name, SourceLoc loc);

  Diagnostics diags_;

  // Params and lets share one scope, since the spec says rebinding a name
  // that's already in scope is an error either way. 
  std::unordered_map<std::string, SourceLoc> scope_;

  // Functions seen so far. Filled in as we traverse, so a function can only
  // call ones defined above it. That's also what rules out recursion.
  std::unordered_set<std::string> functions_;
};

}  // namespace hero

#endif  // HERO_SEMA_H