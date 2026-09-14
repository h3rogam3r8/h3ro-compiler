#include "hero/Sema.h"

#include <string>
#include <unordered_set>

namespace hero {
namespace {

// cast isn't here because it isn't a call any more, the parser gives it
// its own node kind.
bool isBuiltin(const std::string &name) {
  static const std::unordered_set<std::string> builtins = {
      "matmul", "transpose", "sum",  "max",  "exp",
      "sqrt",   "tanh",      "relu", "gelu",
  };
  return builtins.count(name) > 0;
}

}  // namespace

Sema::Sema(const SourceFile &file) : diags_(file) {}

bool Sema::check(const Program &program) {
  for (const Function &fn : program.functions) {
    // Checked before the name goes in, so a function can't call itself.
    checkFunction(fn);

    if (!functions_.insert(fn.name).second) {
      diags_.error(fn.loc, "E002",
                   "there is already a function called " + fn.name);
    }
  }

  return !diags_.hasErrors();
}

void Sema::checkFunction(const Function &fn) {
  scope_.clear();

  for (const Param &p : fn.params)
    declare(p.name, p.loc);

  checkBlock(fn.body);
}

void Sema::checkBlock(const Block &block) {
  for (const LetStmt &stmt : block.lets) {
    // Value first so that let a = a; is an error instead of quietly
    // resolving to itself.
    if (stmt.value)
      checkExpr(*stmt.value);

    declare(stmt.name, stmt.loc);
  }

  if (block.result)
    checkExpr(*block.result);
}

void Sema::checkExpr(const Expr &expr) {
  switch (expr.kind) {
  case ExprKind::IntLit:
  case ExprKind::FloatLit:
  case ExprKind::BoolLit:
    break;

  case ExprKind::Name:
    if (scope_.count(expr.text) == 0)
      diags_.error(expr.loc, "E001", "nothing named " + expr.text + " here");
    break;

    case ExprKind::Unary:
    case ExprKind::Cast:
        // Cast keeps its operand in lhs, same as unary minus. 
        if (expr.lhs)
            checkExpr(*expr.lhs);
        break;

  case ExprKind::Binary:
    if (expr.lhs)
      checkExpr(*expr.lhs);
    if (expr.rhs)
      checkExpr(*expr.rhs);
    break;

  case ExprKind::Call:
    if (!isBuiltin(expr.text) && functions_.count(expr.text) == 0) {
      diags_.error(expr.loc, "E003",
                   "there is no function called " + expr.text);
    }

    for (const ExprPtr &arg : expr.args) {
      if (arg)
        checkExpr(*arg);
    }
    break;
  }
}

bool Sema::declare(const std::string &name, SourceLoc loc) {
  auto it = scope_.find(name);
  if (it != scope_.end()) {
    diags_.error(loc, "E002",
                 name + " is already defined on line " +
                     std::to_string(it->second.line));
    return false;
  }

  scope_.emplace(name, loc);
  return true;
}

}  // namespace hero