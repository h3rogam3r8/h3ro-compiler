#ifndef HERO_AST_H
#define HERO_AST_H

#include "hero/Lexer.h"

#include <memory>
#include <string>
#include <vector>

namespace hero {

// One dimension out of a tensor type. Either a number or a name like B.
struct Dim {
  bool isSymbol = false;
  long long size = 0;
  std::string symbol;
};

// Reusing TokenKind for the dtype since the lexer already has KwF32 and
// friends sitting there. Might regret this once there's a real type
// system but it saves a second enum for now.
struct Type {
  TokenKind dtype = TokenKind::KwF32;
  std::vector<Dim> dims;
  bool isTensor() const { return !dims.empty(); }
};

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;
enum class ExprKind {
  IntLit,
  FloatLit,
  BoolLit,
  Name,
  Unary,
  Binary,
  Call,
  Cast,
};

// One struct for every kind of expression instead of a class per kind.
// Means some fields sit unused depending on what kind it is, which is a
// bit wasteful, but it keeps the parser short and I don't have to write
// a visitor. Can split it up later if it gets in the way.
struct Expr {
  ExprKind kind = ExprKind::Name;
  SourceLoc loc;

  // IntLit and FloatLit put the literal text here. Name puts the
  // identifier. Call puts the callee name.
  std::string text;

  bool boolValue = false;     // BoolLit only
  char op = 0;                // Unary and Binary, one of + - * /

  // Cast only. The dtype being converted to, as the keyword token the
  // lexer produced.
  TokenKind castTo = TokenKind::KwF32;


  ExprPtr lhs;                // Unary (and now Cast) uses this as its only operand
  ExprPtr rhs;
  std::vector<ExprPtr> args;  // Call only

  // Worked out by sema, not the parser. Unknown means either sema hasn't
  // run or it gave up on this subtree.
  TokenKind dtype = TokenKind::Unknown;

  // Also from sema
  bool shapeKnown = false;
  std::vector<Dim> dims;
};

struct LetStmt {
  std::string name;
  ExprPtr value;
  SourceLoc loc;
};

// Zero or more lets, then the one expression the block evaluates to.
struct Block {
  std::vector<LetStmt> lets;
  ExprPtr result;
};

struct Param {
  std::string name;
  Type type;
  SourceLoc loc;
};

struct Function {
  std::string name;
  std::vector<Param> params;
  Type returnType;
  Block body;
  SourceLoc loc;
};

struct Program {
  std::vector<Function> functions;
};

}  // namespace hero

#endif  // HERO_AST_H