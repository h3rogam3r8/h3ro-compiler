#include "hero/ASTPrinter.h"
#include <ostream>
#include <string>

namespace hero {
namespace {

void indent(std::ostream &os, int depth) {
  for (int i = 0; i < depth; i++)
    os << "  ";
}

void printExpr(const Expr &e, std::ostream &os, int depth) {
  indent(os, depth);

  switch (e.kind) {
  case ExprKind::IntLit:
    os << "int " << e.text << "\n";
    break;
  case ExprKind::FloatLit:
    os << "float " << e.text << "\n";
    break;
  case ExprKind::BoolLit:
    os << "bool " << (e.boolValue ? "true" : "false") << "\n";
    break;
  case ExprKind::Name:
    os << "name " << e.text << "\n";
    break;
  case ExprKind::Unary:
    os << "unary " << e.op << "\n";
    printExpr(*e.lhs, os, depth + 1);
    break;
  case ExprKind::Binary:
    os << "binary " << e.op << "\n";
    printExpr(*e.lhs, os, depth + 1);
    printExpr(*e.rhs, os, depth + 1);
    break;
  case ExprKind::Cast:
    os << "cast to " << tokenKindName(e.castTo) << "\n";
    printExpr(*e.lhs, os, depth + 1);
    break;
  case ExprKind::Call:
    os << "call " << e.text << "\n";
    for (const ExprPtr &arg : e.args)
      printExpr(*arg, os, depth + 1);
    break;
  }
}

void printFunction(const Function &fn, std::ostream &os) {
  os << "fn " << fn.name << "\n";

  for (const Param &p : fn.params) {
    indent(os, 1);
    os << "param " << p.name << ": " << typeToString(p.type) << "\n";
  }

  indent(os, 1);
  os << "returns " << typeToString(fn.returnType) << "\n";

  for (const LetStmt &stmt : fn.body.lets) {
    indent(os, 1);
    os << "let " << stmt.name << "\n";
    printExpr(*stmt.value, os, 2);
  }

  indent(os, 1);
  os << "result\n";
  printExpr(*fn.body.result, os, 2);
}

}  // namespace

std::string typeToString(const Type &type) {
  if (!type.isTensor())
    return tokenKindName(type.dtype);

  std::string out = "tensor<[";
  for (size_t i = 0; i < type.dims.size(); i++) {
    if (i > 0)
      out += ", ";
    const Dim &dim = type.dims[i];
    out += dim.isSymbol ? dim.symbol : std::to_string(dim.size);
  }
  out += "], ";
  out += tokenKindName(type.dtype);
  out += ">";
  return out;
}

void printProgram(const Program &program, std::ostream &os) {
  for (size_t i = 0; i < program.functions.size(); i++) {
    // Blank line between functions, nothing before the first one.
    if (i > 0)
      os << "\n";
    printFunction(program.functions[i], os);
  }
}

}  // namespace hero