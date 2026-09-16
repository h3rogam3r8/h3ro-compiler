#ifndef HERO_PARSER_H
#define HERO_PARSER_H

#include "hero/AST.h"
#include "hero/Diagnostics.h"
#include "hero/Lexer.h"
#include "hero/SourceFile.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace hero {

class Parser {
public:
  // The file has to outlive the parser, diagnostics keeps pointing at it.
  explicit Parser(const SourceFile &file);

  // Null if it didn't parse. diags() says why.
  std::unique_ptr<Program> parse();

  const Diagnostics &diags() const { return diags_; }

private:
  // Token helpers.
  const Token &peek(size_t ahead = 0) const;
  const Token &advance();
  bool check(TokenKind kind) const;
  bool match(TokenKind kind);
  bool expect(TokenKind kind, const char *what);
  void error(const Token &tok, const std::string &message);

  // One function per grammar rule, same names as the spec uses.
  bool parseFunction(Function &out);
  bool parseLet(Block &out);
  bool parseParam(Param &out);
  bool parseType(Type &out);
  bool parseBlock(Block &out);

  void syncToFunction();
  void syncToStatement();

  ExprPtr parseExpr();
  ExprPtr parseAdd();
  ExprPtr parseMul();
  ExprPtr parseUnary();
  ExprPtr parsePrimary();

  // Lexing the whole file up front instead of pulling one token at a time.
  std::vector<Token> tokens_;
  size_t index_ = 0;
  Diagnostics diags_;
};

}  // namespace hero

#endif  // HERO_PARSER_H