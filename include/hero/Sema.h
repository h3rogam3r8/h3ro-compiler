#ifndef HERO_SEMA_H
#define HERO_SEMA_H

#include "hero/AST.h"
#include "hero/Diagnostics.h"
#include "hero/SourceFile.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace hero {

// What sema works out about one expression.
struct DtypeInfo {
  // False means something already went wrong and got reported. Callers
  // should go quiet instead of piling more errors on top.
  bool known = false;

  // A bare number literal that hasn't committed to a dtype yet. 
  bool flexible = false;

  TokenKind dtype = TokenKind::Unknown;
};

// Semantic analysis. Resolves names and works out dtypes. Shapes are
// still to come.
class Sema {
public:
  explicit Sema(const SourceFile &file);

  // Annotates the tree as it goes, so this takes a non const program.
  // False if anything was wrong, diags() says what.
  bool check(Program &program);

  const Diagnostics &diags() const { return diags_; }

private:
  void checkFunction(Function &fn);
  DtypeInfo checkBlock(Block &block);
  DtypeInfo checkExpr(Expr &expr);
  DtypeInfo computeExpr(Expr &expr);
  DtypeInfo checkBinary(Expr &expr);
  DtypeInfo checkCall(Expr &expr);

  bool declare(const std::string &name, SourceLoc loc, TokenKind dtype);

  struct Binding {
    SourceLoc loc;
    TokenKind dtype = TokenKind::Unknown;
  };

  // What a user defined function looks like from the outside.
  struct FnSig {
    std::vector<TokenKind> params;
    TokenKind returns = TokenKind::Unknown;
  };

  Diagnostics diags_;

  //Params and lets share one scope.
  std::unordered_map<std::string, Binding> scope_;

  // Functions seen so far. Filled in as we walk, so a function can only
  // call ones defined above it. That's also what rules out recursion.
  std::unordered_map<std::string, FnSig> functions_;
};

}  // namespace hero

#endif  // HERO_SEMA_H