#include "hero/Parser.h"
#include "hero/Sema.h"
#include "hero/SourceFile.h"
#include <gtest/gtest.h>

#include <sstream>
#include <string>

using namespace hero;

namespace {

// Parses then runs sema. Anything that fails to parse is a broken test
// rather than a sema result, so that gets its own message.
struct Checked {
  bool ok = false;
  std::string output;
};

Checked check(const std::string &source) {
  SourceFile file("t.hero", source);

  Parser parser(file);
  auto program = parser.parse();
  if (!program) {
    std::ostringstream why;
    parser.diags().print(why);
    return {false, "PARSE FAILED: " + why.str()};
  }

  Sema sema(file);
  bool ok = sema.check(*program);

  std::ostringstream out;
  sema.diags().print(out);
  return {ok, out.str()};
}

}  // namespace

TEST(Sema, ParametersAreInScope) {
  Checked r = check("fn f(x: f32) -> f32 { x }");
  EXPECT_TRUE(r.ok) << r.output;
}

TEST(Sema, LetIsInScopeAfterItIsBound) {
  Checked r = check("fn f() -> f32 { let a = 1.0; a }");
  EXPECT_TRUE(r.ok) << r.output;
}

TEST(Sema, UnknownNameIsE001) {
  Checked r = check("fn f() -> f32 { nope }");
  EXPECT_FALSE(r.ok);
  EXPECT_NE(r.output.find("E001"), std::string::npos) << r.output;
}

// let a = a; should not resolve to the thing being defined.
TEST(Sema, LetCannotSeeItself) {
  Checked r = check("fn f() -> f32 { let a = a; a }");
  EXPECT_FALSE(r.ok);
  EXPECT_NE(r.output.find("E001"), std::string::npos) << r.output;
}

TEST(Sema, LetCannotSeeALaterLet) {
  Checked r = check("fn f() -> f32 { let a = b; let b = 1.0; a }");
  EXPECT_FALSE(r.ok);
  EXPECT_NE(r.output.find("E001"), std::string::npos) << r.output;
}

TEST(Sema, RebindingALetIsE002) {
  Checked r = check("fn f() -> f32 { let a = 1.0; let a = 2.0; a }");
  EXPECT_FALSE(r.ok);
  EXPECT_NE(r.output.find("E002"), std::string::npos) << r.output;
}

TEST(Sema, LetCannotShadowAParameter) {
  Checked r = check("fn f(x: f32) -> f32 { let x = 1.0; x }");
  EXPECT_FALSE(r.ok);
  EXPECT_NE(r.output.find("E002"), std::string::npos) << r.output;
}

TEST(Sema, DuplicateParameterIsE002) {
  Checked r = check("fn f(x: f32, x: f32) -> f32 { x }");
  EXPECT_FALSE(r.ok);
  EXPECT_NE(r.output.find("E002"), std::string::npos) << r.output;
}

TEST(Sema, TwoFunctionsWithTheSameNameIsE002) {
  Checked r = check("fn f() -> f32 { 1.0 } fn f() -> f32 { 2.0 }");
  EXPECT_FALSE(r.ok);
  EXPECT_NE(r.output.find("E002"), std::string::npos) << r.output;
}

TEST(Sema, NamesInOneFunctionAreNotVisibleInAnother) {
  Checked r = check("fn a(x: f32) -> f32 { x } fn b() -> f32 { x }");
  EXPECT_FALSE(r.ok);
  EXPECT_NE(r.output.find("E001"), std::string::npos) << r.output;
}

TEST(Sema, BuiltinsResolve) {
  Checked r = check("fn f(x: f32, w: f32) -> f32 { matmul(x, w) }");
  EXPECT_TRUE(r.ok) << r.output;
}

TEST(Sema, UnknownFunctionIsE003) {
  Checked r = check("fn f(x: f32) -> f32 { conv2d(x) }");
  EXPECT_FALSE(r.ok);
  EXPECT_NE(r.output.find("E003"), std::string::npos) << r.output;
}

TEST(Sema, CanCallAFunctionDefinedEarlier) {
  Checked r = check("fn a() -> f32 { 1.0 } fn b() -> f32 { a() }");
  EXPECT_TRUE(r.ok) << r.output;
}

TEST(Sema, CannotCallAFunctionDefinedLater) {
  Checked r = check("fn b() -> f32 { a() } fn a() -> f32 { 1.0 }");
  EXPECT_FALSE(r.ok);
  EXPECT_NE(r.output.find("E003"), std::string::npos) << r.output;
}

// The spec says no recursion, and building the function table as we go is
// what enforces it.
TEST(Sema, FunctionCannotCallItself) {
  Checked r = check("fn loop_forever() -> f32 { loop_forever() }");
  EXPECT_FALSE(r.ok);
  EXPECT_NE(r.output.find("E003"), std::string::npos) << r.output;
}

TEST(Sema, NamesInsideCallArgumentsAreChecked) {
  Checked r = check("fn f(x: f32) -> f32 { matmul(x, missing) }");
  EXPECT_FALSE(r.ok);
  EXPECT_NE(r.output.find("E001"), std::string::npos) << r.output;
}

TEST(Sema, NamesUnderOperatorsAreChecked) {
  Checked r = check("fn f() -> f32 { -a + 1.0 }");
  EXPECT_FALSE(r.ok);
  EXPECT_NE(r.output.find("E001"), std::string::npos) << r.output;
}

TEST(Sema, DuplicateErrorPointsAtTheFirstBinding) {
  Checked r = check("fn f() -> f32 {\n  let a = 1.0;\n  let a = 2.0;\n  a\n}");
  EXPECT_FALSE(r.ok);
  EXPECT_NE(r.output.find("already defined on line 2"), std::string::npos)
      << r.output;
}

TEST(Sema, TheWholeMlpExamplePasses) {
  Checked r = check(
      "fn mlp(x:  tensor<[B, 768],    f16>,\n"
      "       w1: tensor<[768, 3072], f16>, b1: tensor<[3072], f16>,\n"
      "       w2: tensor<[3072, 768], f16>, b2: tensor<[768],  f16>)\n"
      "    -> tensor<[B, 768], f16>\n"
      "{\n"
      "    let h = matmul(x, w1) + b1;\n"
      "    let a = gelu(h);\n"
      "    matmul(a, w2) + b2\n"
      "}\n");
  EXPECT_TRUE(r.ok) << r.output;
}

TEST(Sema, RenderedErrorLooksLikeACompilerError) {
  SourceFile file("bad.hero", "fn f() -> f32 { nope }");
  Parser parser(file);
  auto program = parser.parse();
  ASSERT_NE(program, nullptr);

  Sema sema(file);
  EXPECT_FALSE(sema.check(*program));

  std::ostringstream out;
  sema.diags().print(out);

  EXPECT_EQ(out.str(),
            "bad.hero:1:17: error[E001]: nothing named nope here\n"
            "fn f() -> f32 { nope }\n"
            "                ^\n");
}