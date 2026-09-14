#include "hero/Sema.h"

#include <string>
#include <unordered_map>

namespace hero {
namespace {

bool isFloatDtype(TokenKind k) {
  return k == TokenKind::KwF32 || k == TokenKind::KwF16 ||
         k == TokenKind::KwBf16;
}

bool isIntDtype(TokenKind k) {
  return k == TokenKind::KwI32 || k == TokenKind::KwI8;
}

bool isNumericDtype(TokenKind k) {
  return isFloatDtype(k) || isIntDtype(k);
}

// What sema needs to know about a builtin. No shape rules yet.


// cast isn't here because it isn't a call any more, the parser gives it
// its own node kind.
struct Builtin {
  int arity = 1;
  bool floatOnly = false;

  // sum and max take an axis as their second argument, which isn't a
  // tensor and doesn't have to match the first one's dtype.
  bool axisArg = false;
};

const Builtin *findBuiltin(const std::string &name) {
  static const std::unordered_map<std::string, Builtin> builtins = {
      {"matmul",    {2, false, false}},
      {"transpose", {1, false, false}},
      {"sum",       {2, false, true}},
      {"max",       {2, false, true}},
      {"exp",       {1, true,  false}},
      {"sqrt",      {1, true,  false}},
      {"tanh",      {1, true,  false}},
      {"relu",      {1, true,  false}},
      {"gelu",      {1, true,  false}},
  };

  auto it = builtins.find(name);
  return it == builtins.end() ? nullptr : &it->second;
}

// A bare 2 can become an f16 or an i8 or whatever. A 2.5 can't become an int.
bool literalFits(TokenKind literalDefault, TokenKind target) {
  if (isFloatDtype(literalDefault))
    return isFloatDtype(target);
  return isNumericDtype(target);
}

std::string plural(int n, const char *word) {
  return std::to_string(n) + " " + word + (n == 1 ? "" : "s");
}

}  // namespace

Sema::Sema(const SourceFile &file) : diags_(file) {}

bool Sema::check(Program &program) {
  for (Function &fn : program.functions) {
    // Checked before the name goes in, so a function can't call itself.
    checkFunction(fn);

    FnSig sig;
    for (const Param &p : fn.params)
      sig.params.push_back(p.type.dtype);
    sig.returns = fn.returnType.dtype;

    if (!functions_.emplace(fn.name, sig).second) {
      diags_.error(fn.loc, "E002",
                   "there is already a function called " + fn.name);
    }
  }

  return !diags_.hasErrors();
}

void Sema::checkFunction(Function &fn) {
  scope_.clear();

  for (Param &p : fn.params)
    declare(p.name, p.loc, p.type.dtype);

  DtypeInfo body = checkBlock(fn.body);
  if (!body.known || !fn.body.result)
    return;

  const TokenKind want = fn.returnType.dtype;

  if (body.flexible) {
    if (!literalFits(body.dtype, want)) {
      diags_.error(fn.body.result->loc, "E005",
                   std::string("this returns a number with a decimal point "
                               "but the function says it returns ") +
                       tokenKindName(want));
    }
    return;
  }

  if (body.dtype != want) {
    diags_.error(fn.body.result->loc, "E005",
                 std::string("function says it returns ") +
                     tokenKindName(want) + " but the body produces " +
                     tokenKindName(body.dtype));
  }
}

DtypeInfo Sema::checkBlock(Block &block) {
  for (LetStmt &stmt : block.lets) {
    DtypeInfo value;

    // Value first so that let a = a; is an error instead of quietly
    // resolving to itself.
    if (stmt.value)
      value = checkExpr(*stmt.value);

    // A let that's just a bare number settles on the literal's default,
    // since there's nothing else around to take a dtype from.
    declare(stmt.name, stmt.loc, value.known ? value.dtype : TokenKind::Unknown);
  }

  if (block.result)
    return checkExpr(*block.result);

  return {};
}

DtypeInfo Sema::checkExpr(Expr &expr) {
  DtypeInfo out = computeExpr(expr);
  expr.dtype = out.dtype;
  return out;
}

DtypeInfo Sema::computeExpr(Expr &expr) {
  switch (expr.kind) {
  case ExprKind::IntLit:
    return {true, true, TokenKind::KwI32};

  case ExprKind::FloatLit:
    return {true, true, TokenKind::KwF32};

  case ExprKind::BoolLit:
    return {true, false, TokenKind::KwBool};

  case ExprKind::Name: {
    auto it = scope_.find(expr.text);
    if (it == scope_.end()) {
      diags_.error(expr.loc, "E001", "nothing named " + expr.text + " here");
      return {};
    }
    if (it->second.dtype == TokenKind::Unknown)
      return {};
    return {true, false, it->second.dtype};
  }

  case ExprKind::Unary: {
    DtypeInfo operand = expr.lhs ? checkExpr(*expr.lhs) : DtypeInfo{};
    if (!operand.known)
      return {};

    if (operand.dtype == TokenKind::KwBool) {
      diags_.error(expr.loc, "E005", "cannot negate a bool");
      return {};
    }
    return operand;
  }

  case ExprKind::Cast: {
    DtypeInfo operand = expr.lhs ? checkExpr(*expr.lhs) : DtypeInfo{};
    if (!operand.known)
      return {};
    return {true, false, expr.castTo};
  }

  case ExprKind::Binary:
    return checkBinary(expr);

  case ExprKind::Call:
    return checkCall(expr);
  }

  return {};
}

DtypeInfo Sema::checkBinary(Expr &expr) {
  DtypeInfo l = expr.lhs ? checkExpr(*expr.lhs) : DtypeInfo{};
  DtypeInfo r = expr.rhs ? checkExpr(*expr.rhs) : DtypeInfo{};
  if (!l.known || !r.known)
    return {};

  if (l.dtype == TokenKind::KwBool || r.dtype == TokenKind::KwBool) {
    diags_.error(expr.loc, "E005",
                 std::string("cannot do arithmetic on bool"));
    return {};
  }

  // Two bare numbers with nothing to take a dtype from. Stay flexible in
  // case the whole thing gets combined with something typed later.
  if (l.flexible && r.flexible) {
    const bool anyFloat = isFloatDtype(l.dtype) || isFloatDtype(r.dtype);
    return {true, true, anyFloat ? TokenKind::KwF32 : TokenKind::KwI32};
  }

  if (l.flexible || r.flexible) {
    const DtypeInfo &lit = l.flexible ? l : r;
    const DtypeInfo &fixed = l.flexible ? r : l;

    if (!literalFits(lit.dtype, fixed.dtype)) {
      diags_.error(expr.loc, "E005",
                   std::string("a number with a decimal point can't be used "
                               "as ") +
                       tokenKindName(fixed.dtype));
      return {};
    }
    return {true, false, fixed.dtype};
  }

  if (l.dtype != r.dtype) {
    diags_.error(expr.loc, "E005",
                 std::string("these are ") + tokenKindName(l.dtype) + " and " +
                     tokenKindName(r.dtype) +
                     ", the spec has no implicit conversion so one of them "
                     "needs a cast");
    return {};
  }

  return {true, false, l.dtype};
}

DtypeInfo Sema::checkCall(Expr &expr) {
  const Builtin *builtin = findBuiltin(expr.text);
  auto userFn = functions_.find(expr.text);
  const bool isUser = userFn != functions_.end();

  // Arguments get checked either way so their names still get resolved.
  std::vector<DtypeInfo> args;
  for (ExprPtr &arg : expr.args)
    args.push_back(arg ? checkExpr(*arg) : DtypeInfo{});

  if (!builtin && !isUser) {
    diags_.error(expr.loc, "E003",
                 "there is no function called " + expr.text);
    return {};
  }

  const int arity =
      builtin ? builtin->arity : (int)userFn->second.params.size();

  if ((int)args.size() != arity) {
    diags_.error(expr.loc, "E004",
                 expr.text + " takes " + plural(arity, "argument") +
                     " but got " + std::to_string(args.size()));
    return {};
  }

  for (const DtypeInfo &a : args) {
    if (!a.known)
      return {};
  }

  if (isUser) {
    for (size_t i = 0; i < args.size(); i++) {
      const TokenKind want = userFn->second.params[i];
      if (args[i].flexible) {
        if (!literalFits(args[i].dtype, want)) {
          diags_.error(expr.args[i]->loc, "E005",
                       std::string("argument ") + std::to_string(i + 1) +
                           " of " + expr.text + " wants " +
                           tokenKindName(want));
          return {};
        }
        continue;
      }
      if (args[i].dtype != want) {
        diags_.error(expr.args[i]->loc, "E005",
                     std::string("argument ") + std::to_string(i + 1) +
                         " of " + expr.text + " wants " +
                         tokenKindName(want) + " but this is " +
                         tokenKindName(args[i].dtype));
        return {};
      }
    }
    return {true, false, userFn->second.returns};
  }

  // A builtin's result takes the dtype of its first argument. A bare
  // literal there has nothing to take a dtype from, so it settles.
  TokenKind result = args[0].dtype;

  if (builtin->floatOnly && !isFloatDtype(result)) {
    diags_.error(expr.loc, "E005",
                 expr.text + " only works on float dtypes, this is " +
                     tokenKindName(result));
    return {};
  }

  if (!isNumericDtype(result)) {
    diags_.error(expr.loc, "E005",
                 expr.text + " needs a number, this is " +
                     tokenKindName(result));
    return {};
  }

  // matmul wants both sides to be the same dtype. sum and max don't,
  // their second argument is an axis.
  if (arity == 2 && !builtin->axisArg && !args[1].flexible &&
      args[1].dtype != result) {
    diags_.error(expr.loc, "E005",
                 expr.text + " got " + tokenKindName(result) + " and " +
                     tokenKindName(args[1].dtype) +
                     ", both sides have to match");
    return {};
  }

  return {true, false, result};
}

bool Sema::declare(const std::string &name, SourceLoc loc, TokenKind dtype) {
  auto it = scope_.find(name);
  if (it != scope_.end()) {
    diags_.error(loc, "E002",
                 name + " is already defined on line " +
                     std::to_string(it->second.loc.line));
    return false;
  }

  Binding b;
  b.loc = loc;
  b.dtype = dtype;
  scope_.emplace(name, b);
  return true;
}

}  // namespace hero