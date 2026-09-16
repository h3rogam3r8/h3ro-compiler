#include "hero/Parser.h"
#include "hero/SourceFile.h"
#include <gtest/gtest.h>
#include <string>

using namespace hero;

namespace {

// Every test here expects a failure so yeah
void expectRejected(const std::string &source) {
  SourceFile file("bad.hero", source);
  Parser parser(file);
  auto program = parser.parse();
  EXPECT_EQ(program, nullptr) << "source: " << source;
  EXPECT_TRUE(parser.diags().hasErrors()) << "source: " << source;
}

}  // namespace

//Copied from Alpha 
TEST(ParserErrors, JunkAtTopLevel)          { expectRejected("$"); }
TEST(ParserErrors, BareFn)                  { expectRejected("fn"); }
TEST(ParserErrors, FnWithNoName)            { expectRejected("fn () -> f32 { 1.0 }"); }
TEST(ParserErrors, MissingArrow)            { expectRejected("fn f() f32 { 1.0 }"); }
TEST(ParserErrors, MissingReturnType)       { expectRejected("fn f() -> { 1.0 }"); }
TEST(ParserErrors, MissingOpenBrace)        { expectRejected("fn f() -> f32 1.0 }"); }
TEST(ParserErrors, MissingCloseBrace)       { expectRejected("fn f() -> f32 { 1.0"); }
TEST(ParserErrors, MissingCloseParen)       { expectRejected("fn f( -> f32 { 1.0 }"); }
TEST(ParserErrors, ParamWithNoType)         { expectRejected("fn f(x) -> f32 { x }"); }
TEST(ParserErrors, ParamWithNoColon)        { expectRejected("fn f(x f32) -> f32 { x }"); }
TEST(ParserErrors, TrailingCommaInParams)   { expectRejected("fn f(x: f32,) -> f32 { x }"); }
TEST(ParserErrors, LetWithNoEquals)         { expectRejected("fn f() -> f32 { let a 1.0; a }"); }
TEST(ParserErrors, LetWithNoName)           { expectRejected("fn f() -> f32 { let = 1.0; 1.0 }"); }
TEST(ParserErrors, LetWithNoValue)          { expectRejected("fn f() -> f32 { let a = ; 1.0 }"); }
TEST(ParserErrors, EmptyBody)               { expectRejected("fn f() -> f32 { }"); }
TEST(ParserErrors, UnclosedCall)            { expectRejected("fn f() -> f32 { matmul(x, w }"); }
TEST(ParserErrors, CallWithLeadingComma)    { expectRejected("fn f() -> f32 { g(, x) }"); }
TEST(ParserErrors, CallWithTrailingComma)   { expectRejected("fn f() -> f32 { g(x,) }"); }
TEST(ParserErrors, UnclosedParenInExpr)     { expectRejected("fn f() -> f32 { (1.0 }"); }
TEST(ParserErrors, BinaryWithNoRightSide)   { expectRejected("fn f() -> f32 { 1.0 + }"); }
TEST(ParserErrors, TensorMissingCloseAngle) { expectRejected("fn f(x: tensor<[4], f32) -> f32 { x }"); }
TEST(ParserErrors, TensorMissingBracket)    { expectRejected("fn f(x: tensor<4], f32>) -> f32 { x }"); }
TEST(ParserErrors, TensorWithNoDims)        { expectRejected("fn f(x: tensor<[], f32>) -> f32 { x }"); }
TEST(ParserErrors, TensorWithNoDtype)       { expectRejected("fn f(x: tensor<[4]>) -> f32 { x }"); }
TEST(ParserErrors, FloatDimension)          { expectRejected("fn f(x: tensor<[1.5], f32>) -> f32 { x }"); }
TEST(ParserErrors, DtypeWhereADimGoes)      { expectRejected("fn f(x: tensor<[f32], f32>) -> f32 { x }"); }

// The spec says a fixed dimension is an integer literal of at least 1.
TEST(ParserErrors, ZeroDimensionIsRejected) {
  expectRejected("fn f(x: tensor<[0], f32>) -> f32 { x }");
}

// stoll throws on something this big. An uncaught exception out of the
// parser would take down heroc entirely.
TEST(ParserErrors, AbsurdlyLargeDimensionDoesNotThrow) {
  expectRejected("fn f(x: tensor<[999999999999999999999999], f32>) -> f32 { x }");
}

// Dtype names are keywords, so they can't be function names either. Found
// this by accident writing the test below with functions called f16.
TEST(ParserErrors, FunctionNamedAfterADtypeIsRejected) {
  expectRejected("fn f16() -> i32 { 1 }");
  expectRejected("fn bool() -> i32 { 1 }");
}

// Two missing semicolons, two errors. Before recovery this only reported
// the first one.
TEST(ParserErrors, ReportsEveryMissingSemicolon) {
  SourceFile file("bad.hero", "fn f() -> f32 { let a = 1.0 let b = 2.0 a }");
  Parser parser(file);
  parser.parse();
  EXPECT_EQ(parser.diags().count(), 2u);
}

TEST(ParserErrors, KeepsGoingIntoTheNextFunction) {
  SourceFile file("bad.hero",
                  "fn a() -> { 1.0 } fn b() -> { 2.0 } fn c() -> { 3.0 }");
  Parser parser(file);
  parser.parse();
  EXPECT_EQ(parser.diags().count(), 3u);
}

TEST(ParserErrors, AGoodFunctionAfterABrokenOneStillParses) {
  SourceFile file("bad.hero", "fn a() -> { 1.0 } fn b() -> f32 { 2.0 }");
  Parser parser(file);
  EXPECT_EQ(parser.parse(), nullptr);
  EXPECT_EQ(parser.diags().count(), 1u);
}

// Nothing here should go on for forever. If recovery fails to make progress
// this test hangs instead of failing. Will solve later
TEST(ParserErrors, RecoveryAlwaysMakesProgress) {
  const char *nasty[] = {
      "fn", "fn fn fn", "{{{{", "}}}}", "let let let",
      "fn f() -> f32 { let let let }", "$$$$", "fn f( ( ( (",
  };

  for (const char *source : nasty) {
    SourceFile file("bad.hero", source);
    Parser parser(file);
    EXPECT_EQ(parser.parse(), nullptr) << source;
  }
}

TEST(ParserErrors, StopsAfterTooManyErrors) {
  std::string source;
  for (int i = 0; i < 100; i++)
    source += "fn f" + std::to_string(i) + "() -> { 1.0 } ";

  SourceFile file("bad.hero", source);
  Parser parser(file);
  parser.parse();
  EXPECT_LE(parser.diags().count(), 25u);
}

TEST(ParserErrors, SecondFunctionBeingBrokenStillFails) {
  expectRejected("fn good() -> f32 { 1.0 } fn bad() -> { 2.0 }");
}

TEST(ParserEdges, CommentsOnlyFileIsAnEmptyProgram) {
  SourceFile file("t.hero", "// just a comment\n/* and another */\n");
  Parser parser(file);
  auto program = parser.parse();
  ASSERT_NE(program, nullptr);
  EXPECT_TRUE(program->functions.empty());
}

TEST(ParserEdges, WhitespaceOnlyFileIsAnEmptyProgram) {
  SourceFile file("t.hero", "\n\n   \t\n");
  Parser parser(file);
  auto program = parser.parse();
  ASSERT_NE(program, nullptr);
  EXPECT_TRUE(program->functions.empty());
}

TEST(ParserEdges, CarriageReturnsDoNotBreakParsing) {
  SourceFile file("t.hero", "fn f() -> f32 {\r\n  1.0\r\n}\r\n");
  Parser parser(file);
  auto program = parser.parse();
  ASSERT_NE(program, nullptr);
  EXPECT_EQ(program->functions.size(), 1u);
}

TEST(ParserEdges, ManyFunctionsInOneFile) {
  std::string source;
  for (int i = 0; i < 50; i++)
    source += "fn fun" + std::to_string(i) + "() -> i32 { " + std::to_string(i) + " }\n";

  SourceFile file("t.hero", source);
  Parser parser(file);
  auto program = parser.parse();
  ASSERT_NE(program, nullptr);
  EXPECT_EQ(program->functions.size(), 50u);
}

TEST(ParserEdges, DeeplyNestedParens) {
  std::string inner(100, '(');
  inner += "1.0";
  inner += std::string(100, ')');

  SourceFile file("t.hero", "fn f() -> f32 { " + inner + " }");
  Parser parser(file);
  auto program = parser.parse();
  ASSERT_NE(program, nullptr);
  EXPECT_EQ(program->functions.size(), 1u);
}

TEST(ParserErrors, CastWithNoDtype)       { expectRejected("fn f(x: f16) -> f32 { cast(x) }"); }
TEST(ParserErrors, CastToSomethingElse)   { expectRejected("fn f(x: f16) -> f32 { cast(x, y) }"); }
TEST(ParserErrors, CastMissingOpenParen)  { expectRejected("fn f(x: f16) -> f32 { cast x, f32) }"); }
TEST(ParserErrors, CastMissingCloseParen) { expectRejected("fn f(x: f16) -> f32 { cast(x, f32 }"); }

// cast is a reserved word so this is the price of keeping it a keyword.
TEST(ParserErrors, CannotNameAVariableCast) {
  expectRejected("fn f() -> f32 { let cast = 1.0; cast }");
}

