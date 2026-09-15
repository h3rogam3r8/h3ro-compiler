#include "hero/Sema.h"

#include <algorithm>
#include <stdexcept>
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

// Can we prove these two dimensions are the same number?
bool dimsMatch(const Dim &a, const Dim &b) {
  if (a.isSymbol != b.isSymbol)
    return false;
  if (a.isSymbol)
    return a.symbol == b.symbol;
  return a.size == b.size;
}

bool dimIsOne(const Dim &d) { return !d.isSymbol && d.size == 1; }

// Line the shapes up from the right, a missing
// leading dimension counts as 1, and a 1 stretches to meet anything.
bool broadcastShapes(const std::vector<Dim> &a, const std::vector<Dim> &b,
                     std::vector<Dim> &out) {
  const size_t rank = std::max(a.size(), b.size());
  out.assign(rank, Dim{});

  for (size_t i = 0; i < rank; i++) {
    Dim one;
    one.size = 1;

    const Dim &da = i < a.size() ? a[a.size() - 1 - i] : one;
    const Dim &db = i < b.size() ? b[b.size() - 1 - i] : one;

    if (dimsMatch(da, db))
      out[rank - 1 - i] = da;
    else if (dimIsOne(da))
      out[rank - 1 - i] = db;
    else if (dimIsOne(db))
      out[rank - 1 - i] = da;
    else
      return false;
  }

  return true;
}

std::string shapeToString(const std::vector<Dim> &dims) {
  if (dims.empty())
    return "a scalar";

  std::string out = "[";
  for (size_t i = 0; i < dims.size(); i++) {
    if (i > 0)
      out += ", ";
    out += dims[i].isSymbol ? dims[i].symbol : std::to_string(dims[i].size);
  }
  return out + "]";
}

// A name, an expression or a negative one all fail here.
bool axisValue(const Expr &e, long long &out) {
  if (e.kind != ExprKind::IntLit)
    return false;
  try {
    out = std::stoll(e.text);
  } catch (const std::out_of_range &) {
    return false;
  }
  return true;
}


// Literals and anything rank 0. Known shape, no dims.
TypeInfo scalar(bool flexible, TokenKind dtype) {
  TypeInfo t;
  t.known = true;
  t.flexible = flexible;
  t.dtype = dtype;
  t.shapeKnown = true;
  return t;
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

  for (Param &p : fn.params) {
    TypeInfo t;
    t.known = true;
    t.dtype = p.type.dtype;
    t.shapeKnown = true;
    t.dims = p.type.dims;
    declare(p.name, p.loc, t);
  }

  TypeInfo body = checkBlock(fn.body);
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
    return;
  }

  // Shape too, when we actually worked one out. Plenty of bodies still
  // come back unknown because matmul has no shape rule yet.
  if (!body.shapeKnown)
    return;

  if (body.dims.size() != fn.returnType.dims.size()) {
    diags_.error(fn.body.result->loc, "E006",
                 "function says it returns " +
                     shapeToString(fn.returnType.dims) +
                     " but the body produces " + shapeToString(body.dims));
    return;
  }

  for (size_t i = 0; i < body.dims.size(); i++) {
    if (!dimsMatch(body.dims[i], fn.returnType.dims[i])) {
      diags_.error(fn.body.result->loc, "E006",
                   "function says it returns " +
                       shapeToString(fn.returnType.dims) +
                       " but the body produces " + shapeToString(body.dims));
      return;
    }
  }
}

TypeInfo Sema::checkBlock(Block &block) {
  for (LetStmt &stmt : block.lets) {
    TypeInfo value;

    // Value first so that let a = a; is an error instead of quietly
    // resolving to itself.
    if (stmt.value)
      value = checkExpr(*stmt.value);

    value.flexible = false;
    declare(stmt.name, stmt.loc, value);
  }

  if (block.result)
    return checkExpr(*block.result);

  return {};
}

TypeInfo Sema::checkExpr(Expr &expr) {
  TypeInfo out = computeExpr(expr);
  expr.dtype = out.dtype;
  expr.shapeKnown = out.shapeKnown;
  expr.dims = out.dims;
  return out;
}

TypeInfo Sema::computeExpr(Expr &expr) {
  switch (expr.kind) {
  case ExprKind::IntLit:
    return scalar(true, TokenKind::KwI32);

  case ExprKind::FloatLit:
    return scalar(true, TokenKind::KwF32);

  case ExprKind::BoolLit:
    return scalar(false, TokenKind::KwBool);

  case ExprKind::Name:{
    auto it = scope_.find(expr.text);
    if (it == scope_.end()) {
      diags_.error(expr.loc, "E001", "nothing named " + expr.text + " here");
      return {};
    }
    return it->second.type;
  }

  case ExprKind::Unary: {
    TypeInfo operand = expr.lhs ? checkExpr(*expr.lhs) : TypeInfo{};
    if (!operand.known)
      return {};

    if (operand.dtype == TokenKind::KwBool) {
      diags_.error(expr.loc, "E005", "cannot negate a bool");
      return {};
    }
    return operand;
  }

  case ExprKind::Cast: {
    TypeInfo operand = expr.lhs ? checkExpr(*expr.lhs) : TypeInfo{};
    if (!operand.known)
      return {};

    TypeInfo out = operand;
    out.flexible = false;
    out.dtype = expr.castTo;
    return out;
  }

  case ExprKind::Binary:
    return checkBinary(expr);

  case ExprKind::Call:
    return checkCall(expr);
  }

  return {};
}

TypeInfo Sema::checkBinary(Expr &expr) {
  TypeInfo l = expr.lhs ? checkExpr(*expr.lhs) : TypeInfo{};
  TypeInfo r = expr.rhs ? checkExpr(*expr.rhs) : TypeInfo{};
  if (!l.known || !r.known)
    return {};

  if (l.dtype == TokenKind::KwBool || r.dtype == TokenKind::KwBool) {
    diags_.error(expr.loc, "E005",
                 std::string("cannot do arithmetic on bool"));
    return {};
  }

  TypeInfo out;
  out.known = true;

  // Two bare numbers with nothing to take a dtype from. Stay flexible in
  // case the whole thing gets combined with something typed later.
  if (l.flexible && r.flexible) {
    const bool anyFloat = isFloatDtype(l.dtype) || isFloatDtype(r.dtype);
    out.flexible = true;
    out.dtype = anyFloat ? TokenKind::KwF32 : TokenKind::KwI32;
  } else if (l.flexible || r.flexible) {
    const TypeInfo &lit = l.flexible ? l : r;
    const TypeInfo &fixed = l.flexible ? r : l;

    if (!literalFits(lit.dtype, fixed.dtype)) {
      diags_.error(expr.loc, "E005",
                   std::string("a number with a decimal point can't be used "
                               "as ") +
                       tokenKindName(fixed.dtype));
      return {};
    }
    out.dtype = fixed.dtype;
  } else if (l.dtype != r.dtype) {
    diags_.error(expr.loc, "E005",
                 std::string("these are ") + tokenKindName(l.dtype) + " and " +
                     tokenKindName(r.dtype) +
                     ", the spec has no implicit conversion so one of them "
                     "needs a cast");
    return {};
  } else {
    out.dtype = l.dtype;
  }

  // Shapes are independent of all that.
  if (!l.shapeKnown || !r.shapeKnown)
    return out;

  if (!broadcastShapes(l.dims, r.dims, out.dims)) {
    diags_.error(expr.loc, "E006",
                 shapeToString(l.dims) + " and " + shapeToString(r.dims) +
                     " don't broadcast together");
    return {};
  }

  out.shapeKnown = true;
  return out;
}

TypeInfo Sema::checkCall(Expr &expr) {
  const Builtin *builtin = findBuiltin(expr.text);
  auto userFn = functions_.find(expr.text);
  const bool isUser = userFn != functions_.end();

  // Arguments get checked either way so their names still get resolved.
  std::vector<TypeInfo> args;
  for (ExprPtr &arg : expr.args)
    args.push_back(arg ? checkExpr(*arg) : TypeInfo{});

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

  for (const TypeInfo &a : args) {
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
    TypeInfo out;
    out.known = true;
    out.dtype = userFn->second.returns;
    return out;
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

  TypeInfo out;
  out.known = true;
  out.dtype = result;

  // Shape rules per builtin.
  if (expr.text == "matmul") {
    if (args[0].shapeKnown && args[1].shapeKnown) {
      if (args[0].dims.size() != 2 || args[1].dims.size() != 2) {
        diags_.error(expr.loc, "E007",
                     "matmul needs two rank 2 tensors, these are " +
                         shapeToString(args[0].dims) + " and " +
                         shapeToString(args[1].dims));
        return {};
      }

      // [M, K] times [K, N]. The two Ks have to be provably the same.
      if (!dimsMatch(args[0].dims[1], args[1].dims[0])) {
        diags_.error(expr.loc, "E008",
                     "matmul inner dimensions don't line up, " +
                         shapeToString(args[0].dims) + " times " +
                         shapeToString(args[1].dims));
        return {};
      }

      out.shapeKnown = true;
      out.dims = {args[0].dims[0], args[1].dims[1]};
    }
  } else if (expr.text == "sum" || expr.text == "max") {
    long long axis = 0;
    if (!axisValue(*expr.args[1], axis)) {
      diags_.error(expr.args[1]->loc, "E010",
                   "the axis has to be a plain integer, and the spec says "
                   "no negative ones");
      return {};
    }

    if (args[0].shapeKnown) {
      if (axis < 0 || axis >= (long long)args[0].dims.size()) {
        diags_.error(expr.args[1]->loc, "E010",
                     "axis " + std::to_string(axis) +
                         " is out of range for " +
                         shapeToString(args[0].dims));
        return {};
      }

      // Reductions keep the axis at size 1 instead of dropping it, which
      // is what lets the result broadcast back against the input.
      out.shapeKnown = true;
      out.dims = args[0].dims;
      out.dims[axis] = Dim{};
      out.dims[axis].size = 1;
    }
  } else if (expr.text == "transpose") {
    if (args[0].shapeKnown) {
      if (args[0].dims.size() != 2) {
        diags_.error(expr.loc, "E007",
                     "transpose needs a rank 2 tensor, this is " +
                         shapeToString(args[0].dims));
        return {};
      }
      out.shapeKnown = true;
      out.dims = {args[0].dims[1], args[0].dims[0]};
    }
  } else if (builtin->arity == 1) {
    out.shapeKnown = args[0].shapeKnown;
    out.dims = args[0].dims;
  }

  return out;
}

bool Sema::declare(const std::string &name, SourceLoc loc,
                   const TypeInfo &type) {
  auto it = scope_.find(name);
  if (it != scope_.end()) {
    diags_.error(loc, "E002",
                 name + " is already defined on line " +
                     std::to_string(it->second.loc.line));
    return false;
  }

  Binding b;
  b.loc = loc;
  b.type = type;
  scope_.emplace(name, b);
  return true;
}

}  // namespace hero