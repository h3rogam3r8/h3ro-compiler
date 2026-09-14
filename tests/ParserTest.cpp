#include "hero/Parser.h"

#include <gtest/gtest.h>

#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>

namespace hero {

// Same trick as the lexer tests. Without it a failure just prints a number.
void PrintTo(const ExprKind &kind, std::ostream *os) {
  switch (kind) {
  case ExprKind::IntLit:   *os << "int literal";   break;
  case ExprKind::FloatLit: *os << "float literal"; break;
  case ExprKind::BoolLit:  *os << "bool literal";  break;
  case ExprKind::Name:     *os << "name";          break;
  case ExprKind::Unary:    *os << "unary";         break;
  case ExprKind::Binary:   *os << "binary";        break;
  case ExprKind::Call:     *os << "call";          break;
  case ExprKind::Cast:     *os << "cast";          break;
  }
}

}  // namespace hero

using namespace hero;

namespace {

std::unique_ptr<Program> parseProgram(const std::string &source) {
  SourceFile file("test.hero", source);
  Parser parser(file);
  return parser.parse();
}

// Wraps an expression in the smallest legal function so the expression
// tests don't repeat the boilerplate every time.
ExprPtr parseExpr(const std::string &text) {
  std::string source = "fn wrapper() -> f32 { " + text + " }";
  SourceFile file("test.hero", source);
  Parser parser(file);
  auto program = parser.parse();
  if (!program || program->functions.size() != 1)
    return nullptr;
  return std::move(program->functions[0].body.result);
}

}  // namespace

TEST(Parser, EmptySourceGivesNoFunctions) {
  auto program = parseProgram("");
  ASSERT_NE(program, nullptr);
  EXPECT_TRUE(program->functions.empty());
}

TEST(Parser, SimplestFunction) {
  auto program = parseProgram("fn one() -> f32 { 1.0 }");
  ASSERT_NE(program, nullptr);
  ASSERT_EQ(program->functions.size(), 1u);

  const Function &fn = program->functions[0];
  EXPECT_EQ(fn.name, "one");
  EXPECT_TRUE(fn.params.empty());
  EXPECT_EQ(fn.returnType.dtype, TokenKind::KwF32);
  EXPECT_FALSE(fn.returnType.isTensor());
  ASSERT_NE(fn.body.result, nullptr);
  EXPECT_EQ(fn.body.result->kind, ExprKind::FloatLit);
}

TEST(Parser, TensorAndScalarParameterTypes) {
  auto program = parseProgram(
      "fn f(x: tensor<[B, 128], f16>, s: i32) -> tensor<[B, 64], f16> { x }");
  ASSERT_NE(program, nullptr);
  ASSERT_EQ(program->functions.size(), 1u);

  const Function &fn = program->functions[0];
  ASSERT_EQ(fn.params.size(), 2u);

  EXPECT_EQ(fn.params[0].name, "x");
  EXPECT_EQ(fn.params[0].type.dtype, TokenKind::KwF16);
  ASSERT_EQ(fn.params[0].type.dims.size(), 2u);
  EXPECT_TRUE(fn.params[0].type.dims[0].isSymbol);
  EXPECT_EQ(fn.params[0].type.dims[0].symbol, "B");
  EXPECT_FALSE(fn.params[0].type.dims[1].isSymbol);
  EXPECT_EQ(fn.params[0].type.dims[1].size, 128);

  EXPECT_EQ(fn.params[1].name, "s");
  EXPECT_EQ(fn.params[1].type.dtype, TokenKind::KwI32);
  EXPECT_FALSE(fn.params[1].type.isTensor());
}

TEST(Parser, LetStatementsComeOutInOrder) {
  auto program =
      parseProgram("fn f() -> f32 { let a = 1.0; let b = a * 2.0; b }");
  ASSERT_NE(program, nullptr);
  ASSERT_EQ(program->functions.size(), 1u);

  const Block &body = program->functions[0].body;
  ASSERT_EQ(body.lets.size(), 2u);
  EXPECT_EQ(body.lets[0].name, "a");
  EXPECT_EQ(body.lets[1].name, "b");
  ASSERT_NE(body.result, nullptr);
  EXPECT_EQ(body.result->kind, ExprKind::Name);
  EXPECT_EQ(body.result->text, "b");
}

TEST(Parser, MultiplyBindsTighterThanAdd) {
  ExprPtr e = parseExpr("a + b * c");
  ASSERT_NE(e, nullptr);
  ASSERT_EQ(e->kind, ExprKind::Binary);
  EXPECT_EQ(e->op, '+');
  ASSERT_NE(e->rhs, nullptr);
  EXPECT_EQ(e->rhs->kind, ExprKind::Binary);
  EXPECT_EQ(e->rhs->op, '*');
}

// a - b - c has to come out as (a - b) - c, so the nested one is on the
// left. If this passes with the nesting on the right, subtraction is
// broken in a way that's easy to miss.
TEST(Parser, SubtractionIsLeftAssociative) {
  ExprPtr e = parseExpr("a - b - c");
  ASSERT_NE(e, nullptr);
  ASSERT_EQ(e->kind, ExprKind::Binary);
  EXPECT_EQ(e->op, '-');
  ASSERT_NE(e->lhs, nullptr);
  EXPECT_EQ(e->lhs->kind, ExprKind::Binary);
  ASSERT_NE(e->rhs, nullptr);
  EXPECT_EQ(e->rhs->kind, ExprKind::Name);
  EXPECT_EQ(e->rhs->text, "c");
}

TEST(Parser, ParenthesesBeatPrecedence) {
  ExprPtr e = parseExpr("(a + b) * c");
  ASSERT_NE(e, nullptr);
  ASSERT_EQ(e->kind, ExprKind::Binary);
  EXPECT_EQ(e->op, '*');
  ASSERT_NE(e->lhs, nullptr);
  EXPECT_EQ(e->lhs->kind, ExprKind::Binary);
  EXPECT_EQ(e->lhs->op, '+');
}

TEST(Parser, UnaryMinusBindsTighterThanAdd) {
  ExprPtr e = parseExpr("-a + b");
  ASSERT_NE(e, nullptr);
  ASSERT_EQ(e->kind, ExprKind::Binary);
  EXPECT_EQ(e->op, '+');
  ASSERT_NE(e->lhs, nullptr);
  EXPECT_EQ(e->lhs->kind, ExprKind::Unary);
}

TEST(Parser, CallWithTwoArguments) {
  ExprPtr e = parseExpr("matmul(x, w)");
  ASSERT_NE(e, nullptr);
  ASSERT_EQ(e->kind, ExprKind::Call);
  EXPECT_EQ(e->text, "matmul");
  ASSERT_EQ(e->args.size(), 2u);
  EXPECT_EQ(e->args[0]->text, "x");
  EXPECT_EQ(e->args[1]->text, "w");
}

TEST(Parser, CallWithNoArguments) {
  ExprPtr e = parseExpr("nothing()");
  ASSERT_NE(e, nullptr);
  ASSERT_EQ(e->kind, ExprKind::Call);
  EXPECT_TRUE(e->args.empty());
}

TEST(Parser, NestedCalls) {
  ExprPtr e = parseExpr("gelu(matmul(x, w))");
  ASSERT_NE(e, nullptr);
  ASSERT_EQ(e->kind, ExprKind::Call);
  ASSERT_EQ(e->args.size(), 1u);
  EXPECT_EQ(e->args[0]->kind, ExprKind::Call);
  EXPECT_EQ(e->args[0]->text, "matmul");
}

TEST(Parser, MissingSemicolonIsAnError) {
  SourceFile file("test.hero", "fn f() -> f32 { let a = 1.0 a }");
  Parser parser(file);
  auto program = parser.parse();
  EXPECT_EQ(program, nullptr);
  EXPECT_TRUE(parser.diags().hasErrors());
}

TEST(Parser, BodyWithNoResultExpressionIsAnError) {
  SourceFile file("test.hero", "fn f() -> f32 { let a = 1.0; }");
  Parser parser(file);
  auto program = parser.parse();
  EXPECT_EQ(program, nullptr);
  EXPECT_TRUE(parser.diags().hasErrors());
}

TEST(Parser, ParsesTheMlpExample) {
  auto program = parseProgram(
      "fn mlp(x:  tensor<[B, 768],    f16>,\n"
      "       w1: tensor<[768, 3072], f16>, b1: tensor<[3072], f16>,\n"
      "       w2: tensor<[3072, 768], f16>, b2: tensor<[768],  f16>)\n"
      "    -> tensor<[B, 768], f16>\n"
      "{\n"
      "    let h = matmul(x, w1) + b1;\n"
      "    let a = gelu(h);\n"
      "    matmul(a, w2) + b2\n"
      "}\n");
  ASSERT_NE(program, nullptr);
  ASSERT_EQ(program->functions.size(), 1u);

  const Function &fn = program->functions[0];
  EXPECT_EQ(fn.name, "mlp");
  EXPECT_EQ(fn.params.size(), 5u);
  EXPECT_EQ(fn.body.lets.size(), 2u);
  ASSERT_NE(fn.body.result, nullptr);
  EXPECT_EQ(fn.body.result->kind, ExprKind::Binary);
  EXPECT_EQ(fn.body.result->op, '+');
}

// End to end check that the parser's error comes out looking like a real
// compiler error and not just a line number.
TEST(Parser, ErrorPointsAtTheOffendingToken) {
  SourceFile file("bad.hero", "fn f() -> f32 { let a = 1.0 a }");
  Parser parser(file);
  parser.parse();

  std::ostringstream out;
  parser.diags().print(out);

  EXPECT_EQ(out.str(),
            "bad.hero:1:29: error: expected ; but found identifier\n"
            "fn f() -> f32 { let a = 1.0 a }\n"
            "                            ^\n");
}

TEST(Parser, CastTakesADtypeAsItsSecondArgument) {
  ExprPtr e = parseExpr("cast(x, f16)");
  ASSERT_NE(e, nullptr);
  ASSERT_EQ(e->kind, ExprKind::Cast);
  EXPECT_EQ(e->castTo, TokenKind::KwF16);
  ASSERT_NE(e->lhs, nullptr);
  EXPECT_EQ(e->lhs->kind, ExprKind::Name);
  EXPECT_EQ(e->lhs->text, "x");
}

TEST(Parser, CastCanHoldAWholeExpression) {
  ExprPtr e = parseExpr("cast(matmul(x, w), f32)");
  ASSERT_NE(e, nullptr);
  ASSERT_EQ(e->kind, ExprKind::Cast);
  EXPECT_EQ(e->castTo, TokenKind::KwF32);
  ASSERT_NE(e->lhs, nullptr);
  EXPECT_EQ(e->lhs->kind, ExprKind::Call);
}

TEST(Parser, CastNestsInsideOtherExpressions) {
  ExprPtr e = parseExpr("cast(x, f32) + 1.0");
  ASSERT_NE(e, nullptr);
  ASSERT_EQ(e->kind, ExprKind::Binary);
  ASSERT_NE(e->lhs, nullptr);
  EXPECT_EQ(e->lhs->kind, ExprKind::Cast);
}