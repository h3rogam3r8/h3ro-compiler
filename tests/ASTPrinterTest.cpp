#include "hero/ASTPrinter.h"
#include "hero/Parser.h"
#include <gtest/gtest.h>
#include <sstream>
#include <string>

using namespace hero;

namespace {

std::string printSource(const std::string &source) {
  SourceFile file("test.hero", source);
  Parser parser(file);
  auto program = parser.parse();
  if (!program)
    return "<parse failed>";

  std::ostringstream out;
  printProgram(*program, out);
  return out.str();
}

}  // namespace

TEST(ASTPrinter, ScalarTypeString) {
  Type t;
  t.dtype = TokenKind::KwF32;
  EXPECT_EQ(typeToString(t), "f32");
}

TEST(ASTPrinter, TensorTypeString) {
  Type t;
  t.dtype = TokenKind::KwF16;

  Dim symbolic;
  symbolic.isSymbol = true;
  symbolic.symbol = "B";
  t.dims.push_back(symbolic);

  Dim fixed;
  fixed.size = 768;
  t.dims.push_back(fixed);

  EXPECT_EQ(typeToString(t), "tensor<[B, 768], f16>");
}

TEST(ASTPrinter, SimplestFunction) {
  EXPECT_EQ(printSource("fn one() -> f32 { 1.0 }"),
            "fn one\n"
            "  returns f32\n"
            "  result\n"
            "    float 1.0\n");
}

TEST(ASTPrinter, ParamsAndResult) {
  EXPECT_EQ(printSource("fn f(x: tensor<[B, 4], f32>) -> f32 { x }"),
            "fn f\n"
            "  param x: tensor<[B, 4], f32>\n"
            "  returns f32\n"
            "  result\n"
            "    name x\n");
}

TEST(ASTPrinter, IndentationShowsPrecedence) {
  EXPECT_EQ(printSource("fn f() -> f32 { let a = 1.0; a + 2.0 * 3.0 }"),
            "fn f\n"
            "  returns f32\n"
            "  let a\n"
            "    float 1.0\n"
            "  result\n"
            "    binary +\n"
            "      name a\n"
            "      binary *\n"
            "        float 2.0\n"
            "        float 3.0\n");
}

TEST(ASTPrinter, UnaryAndCall) {
  EXPECT_EQ(printSource("fn f(x: f32, w: f32) -> f32 { -matmul(x, w) }"),
            "fn f\n"
            "  param x: f32\n"
            "  param w: f32\n"
            "  returns f32\n"
            "  result\n"
            "    unary -\n"
            "      call matmul\n"
            "        name x\n"
            "        name w\n");
}

TEST(ASTPrinter, BoolLiteral) {
  EXPECT_EQ(printSource("fn f() -> bool { true }"),
            "fn f\n"
            "  returns bool\n"
            "  result\n"
            "    bool true\n");
}

TEST(ASTPrinter, BlankLineBetweenFunctions) {
  EXPECT_EQ(printSource("fn a() -> i32 { 1 } fn b() -> i32 { 2 }"),
            "fn a\n"
            "  returns i32\n"
            "  result\n"
            "    int 1\n"
            "\n"
            "fn b\n"
            "  returns i32\n"
            "  result\n"
            "    int 2\n");
}

TEST(ASTPrinter, CastShowsWhatItConvertsTo) {
  EXPECT_EQ(printSource("fn f(x: f16) -> f32 { cast(x, f32) }"),
            "fn f\n"
            "  param x: f16\n"
            "  returns f32\n"
            "  result\n"
            "    cast to f32\n"
            "      name x\n");
}