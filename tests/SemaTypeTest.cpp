// dtype and shape checking. The other future sema file is about names.

#include "hero/Parser.h"
#include "hero/Sema.h"
#include "hero/SourceFile.h"
#include <gtest/gtest.h>

#include <sstream>
#include <string>

using namespace hero;

namespace {

struct Result {
  bool ok = false;
  std::string output;
};

Result check(const std::string &source) {
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

void expectOk(const std::string &source) {
  Result r = check(source);
  EXPECT_TRUE(r.ok) << source << "\n" << r.output;
}

void expectCode(const std::string &source, const std::string &code) {
  Result r = check(source);
  EXPECT_FALSE(r.ok) << source;
  EXPECT_NE(r.output.find(code), std::string::npos) << source << "\n" << r.output;
}

}  // namespace

// ---- dtypes ----

TEST(SemaTypes, MatchingDtypesAreFine) {
  expectOk("fn f(a: f32, b: f32) -> f32 { a + b }");
}

TEST(SemaTypes, MixedDtypesAreE005) {
  expectCode("fn f(a: f16, b: f32) -> f32 { a + b }", "E005");
}

TEST(SemaTypes, CastFixesAMixedExpression) {
  expectOk("fn f(a: f16, b: f32) -> f32 { cast(a, f32) + b }");
}

TEST(SemaTypes, IntLiteralTakesTheOtherSidesDtype) {
  expectOk("fn f(a: i8) -> i8 { a + 1 }");
  expectOk("fn f(a: f16) -> f16 { a + 2 }");
}

TEST(SemaTypes, FloatLiteralCannotBecomeAnInteger) {
  expectCode("fn f(a: i32) -> i32 { a + 1.5 }", "E005");
}

TEST(SemaTypes, TwoBareLiteralsFallBackToTheDefault) {
  expectOk("fn f() -> i32 { 1 + 2 }");
  expectOk("fn f() -> f32 { 1 + 2.0 }");
}

TEST(SemaTypes, BoolDoesNotDoArithmetic) {
  expectCode("fn f(a: bool) -> bool { a + a }", "E005");
}

TEST(SemaTypes, ReturnDtypeHasToMatchTheBody) {
  expectCode("fn f(a: f32) -> i32 { a }", "E005");
}

TEST(SemaTypes, LetRemembersItsDtype) {
  expectCode("fn f(a: f16, b: f32) -> f32 { let h = a; h + b }", "E005");
}

// ---- builtins ----

TEST(SemaTypes, WrongArgumentCountIsE004) {
  expectCode("fn f(a: f32) -> f32 { relu(a, a) }", "E004");
  expectCode("fn f(a: f32) -> f32 { matmul(a) }", "E004");
}

TEST(SemaTypes, FloatOnlyBuiltinsRejectIntegers) {
  expectCode("fn f(a: i32) -> i32 { relu(a) }", "E005");
  expectCode("fn f(a: i32) -> i32 { exp(a) }", "E005");
}

TEST(SemaTypes, MatmulWantsBothSidesTheSameDtype) {
  expectCode("fn f(a: tensor<[2, 2], f16>, b: tensor<[2, 2], f32>) -> f32 "
             "{ matmul(a, b) }",
             "E005");
}

TEST(SemaTypes, UserFunctionArgumentsAreChecked) {
  expectCode("fn g(x: f32) -> f32 { x } fn f(y: i32) -> f32 { g(y) }", "E005");
  expectCode("fn g(x: f32) -> f32 { x } fn f(y: f32) -> f32 { g(y, y) }",
             "E004");
  expectOk("fn g(x: f32) -> f32 { x } fn f(y: f32) -> f32 { g(y) }");
}