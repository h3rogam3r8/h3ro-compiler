// Most of the Parser is imported from Alpha, at least in an ideological sense.

#include "hero/Parser.h"
#include <stdexcept>

#include <memory>
#include <string>
#include <utility>

namespace hero {
namespace {

// The dtype keywords. parseType checks against this.
bool isDtype(TokenKind kind) {
  return kind == TokenKind::KwF32 || kind == TokenKind::KwF16 ||
         kind == TokenKind::KwBf16 || kind == TokenKind::KwI32 ||
         kind == TokenKind::KwI8 || kind == TokenKind::KwBool;
}

ExprPtr makeExpr(ExprKind kind, SourceLoc loc) {
  auto e = std::make_unique<Expr>();
  e->kind = kind;
  e->loc = loc;
  return e;
}

}  // namespace

Parser::Parser(const SourceFile &file) : diags_(file) {
  tokens_ = Lexer(file.text()).tokenize();
}

const Token &Parser::peek(size_t ahead) const {
  size_t i = index_ + ahead;
  // tokenize() always ends with an Eof token, so clamping here means
  // peeking past the end just keeps handing back Eof.
  if (i >= tokens_.size())
    i = tokens_.size() - 1;
  return tokens_[i];
}

const Token &Parser::advance() {
  const Token &tok = peek();
  if (index_ + 1 < tokens_.size())
    index_++;
  return tok;
}

bool Parser::check(TokenKind kind) const { return peek().kind == kind; }

bool Parser::match(TokenKind kind) {
  if (!check(kind))
    return false;
  advance();
  return true;
}

void Parser::error(const Token &tok, const std::string &message) {
  diags_.error(tok.loc, message);
}

bool Parser::expect(TokenKind kind, const char *what) {
  if (match(kind))
    return true;
  error(peek(), std::string("expected ") + what + " but found " +
                    tokenKindName(peek().kind));
  return false;
}

// No error recovery yet. The first problem stops everything, so a file
// with two mistakes only reports one. Fine for now, needs fixing when
// there's a real diagnostics system. Pretty easy to import from Alpha.
std::unique_ptr<Program> Parser::parse() {
  auto program = std::make_unique<Program>();

  while (!check(TokenKind::Eof)) {
    Function fn;
    if (!parseFunction(fn))
      return nullptr;
    program->functions.push_back(std::move(fn));
  }

  if (diags_.hasErrors())
    return nullptr;
  return program;
}

bool Parser::parseFunction(Function &out) {
  out.loc = peek().loc;

  if (!expect(TokenKind::KwFn, "fn"))
    return false;

  if (!check(TokenKind::Identifier)) {
    error(peek(), "expected a function name");
    return false;
  }
  out.name = advance().text;

  if (!expect(TokenKind::LParen, "("))
    return false;

  if (!check(TokenKind::RParen)) {
    for (;;) {
      Param param;
      if (!parseParam(param))
        return false;
      out.params.push_back(std::move(param));
      if (!match(TokenKind::Comma))
        break;
    }
  }

  if (!expect(TokenKind::RParen, ")"))
    return false;
  if (!expect(TokenKind::Arrow, "->"))
    return false;
  if (!parseType(out.returnType))
    return false;

  return parseBlock(out.body);
}

bool Parser::parseParam(Param &out) {
  out.loc = peek().loc;

  if (!check(TokenKind::Identifier)) {
    error(peek(), "expected a parameter name");
    return false;
  }
  out.name = advance().text;

  if (!expect(TokenKind::Colon, ":"))
    return false;

  return parseType(out.type);
}

bool Parser::parseType(Type &out) {
  // A bare dtype name is a scalar, no dims.
  if (isDtype(peek().kind)) {
    out.dtype = advance().kind;
    out.dims.clear();
    return true;
  }

  if (!expect(TokenKind::KwTensor, "a type"))
    return false;
  if (!expect(TokenKind::Less, "<"))
    return false;
  if (!expect(TokenKind::LBracket, "["))
    return false;

  for (;;) {
    Dim dim;
    if (check(TokenKind::IntLiteral)) {
      const Token &tok = advance();

      long long value = 0;
      try {
        value = std::stoll(tok.text);
      } catch (const std::out_of_range &) {
        // Only out_of_range can happen here, the lexer already made sure
        error(tok, "dimension is too big");
        return false;
      }
      if (value < 1) {
        error(tok, "dimension has to be at least 1");
        return false;
      }

      dim.isSymbol = false;
      dim.size = value;
    } else if (check(TokenKind::Identifier)) {
      dim.isSymbol = true;
      dim.symbol = advance().text;
    } else {
      error(peek(), "expected a dimension");
      return false;
    }
    out.dims.push_back(dim);

    if (!match(TokenKind::Comma))
      break;
  }

  if (!expect(TokenKind::RBracket, "]"))
    return false;
  if (!expect(TokenKind::Comma, ","))
    return false;

  if (!isDtype(peek().kind)) {
    error(peek(), "expected a dtype");
    return false;
  }
  out.dtype = advance().kind;

  return expect(TokenKind::Greater, ">");
}

bool Parser::parseBlock(Block &out) {
  if (!expect(TokenKind::LBrace, "{"))
    return false;

  while (check(TokenKind::KwLet)) {
    LetStmt stmt;
    stmt.loc = peek().loc;
    advance();  // let

    if (!check(TokenKind::Identifier)) {
      error(peek(), "expected a name after let");
      return false;
    }
    stmt.name = advance().text;

    if (!expect(TokenKind::Equals, "="))
      return false;

    stmt.value = parseExpr();
    if (!stmt.value)
      return false;

    if (!expect(TokenKind::Semicolon, ";"))
      return false;

    out.lets.push_back(std::move(stmt));
  }

  if (check(TokenKind::RBrace)) {
    error(peek(), "block needs a result expression at the end");
    return false;
  }

  out.result = parseExpr();
  if (!out.result)
    return false;

  return expect(TokenKind::RBrace, "}");
}

ExprPtr Parser::parseExpr() { return parseAdd(); }

// parseAdd and parseMul loop instead of calling themselves again. That
// loop is the whole reason a - b - c comes out as (a - b) - c. Recursing
// on the right would flip it and nothing else would notice.
ExprPtr Parser::parseAdd() {
  ExprPtr left = parseMul();
  if (!left)
    return nullptr;

  while (check(TokenKind::Plus) || check(TokenKind::Minus)) {
    const Token &op = advance();
    ExprPtr right = parseMul();
    if (!right)
      return nullptr;

    ExprPtr node = makeExpr(ExprKind::Binary, op.loc);
    node->op = op.kind == TokenKind::Plus ? '+' : '-';
    node->lhs = std::move(left);
    node->rhs = std::move(right);
    left = std::move(node);
  }

  return left;
}

ExprPtr Parser::parseMul() {
  ExprPtr left = parseUnary();
  if (!left)
    return nullptr;

  while (check(TokenKind::Star) || check(TokenKind::Slash)) {
    const Token &op = advance();
    ExprPtr right = parseUnary();
    if (!right)
      return nullptr;

    ExprPtr node = makeExpr(ExprKind::Binary, op.loc);
    node->op = op.kind == TokenKind::Star ? '*' : '/';
    node->lhs = std::move(left);
    node->rhs = std::move(right);
    left = std::move(node);
  }

  return left;
}

ExprPtr Parser::parseUnary(){
  if (check(TokenKind::Minus)) {
    const Token &op = advance();

    // Calls itself so --x parses. Nobody writes that but it costs nothing.
    ExprPtr operand = parseUnary();
    if (!operand)
      return nullptr;

    ExprPtr node = makeExpr(ExprKind::Unary, op.loc);
    node->op = '-';
    node->lhs = std::move(operand);
    return node;
  }
  return parsePrimary();
}

ExprPtr Parser::parsePrimary() {
  SourceLoc loc = peek().loc;

  if (check(TokenKind::IntLiteral)) {
    ExprPtr e = makeExpr(ExprKind::IntLit, loc);
    e->text = advance().text;
    return e;
  }

  if (check(TokenKind::FloatLiteral)) {
    ExprPtr e = makeExpr(ExprKind::FloatLit, loc);
    e->text = advance().text;
    return e;
  }

  if (check(TokenKind::KwTrue) || check(TokenKind::KwFalse)) {
    ExprPtr e = makeExpr(ExprKind::BoolLit, loc);
    e->boolValue = advance().kind == TokenKind::KwTrue;
    return e;
  }

    // cast has to be handled here because the lexer makes it a keyword, so
  // the identifier branch below never sees it. It's also the only call in
  // the language whose second argument is a type instead of a value.
  if (check(TokenKind::KwCast)) {
    advance();

    if (!expect(TokenKind::LParen, "("))
      return nullptr;

    ExprPtr value = parseExpr();
    if (!value)
      return nullptr;

    if (!expect(TokenKind::Comma, ","))
      return nullptr;

    if (!isDtype(peek().kind)) {
      error(peek(), "expected a dtype to cast to");
      return nullptr;
    }
    TokenKind target = advance().kind;

    if (!expect(TokenKind::RParen, ")"))
      return nullptr;

    ExprPtr e = makeExpr(ExprKind::Cast, loc);
    e->lhs = std::move(value);
    e->castTo = target;
    return e;
  }

  if (check(TokenKind::LParen)) {
    advance();
    ExprPtr inner = parseExpr();
    if (!inner)
      return nullptr;
    if (!expect(TokenKind::RParen, ")"))
      return nullptr;
    // Parens don't get a node of their own, the nesting already says it.
    return inner;
  }

  if (check(TokenKind::Identifier)){
    std::string name = advance().text;

    // An identifier with a ( right after it is a call. Otherwise it's
    // just a name. One token of lookahead is all this needs.
    if (check(TokenKind::LParen)) {
      advance();
      ExprPtr call = makeExpr(ExprKind::Call, loc);
      call->text = name;

      if (!check(TokenKind::RParen)) {
        for (;;) {
          ExprPtr arg = parseExpr();
          if (!arg)
            return nullptr;
          call->args.push_back(std::move(arg));
          if (!match(TokenKind::Comma))
            break;
        }
      }

      if (!expect(TokenKind::RParen, ")"))
        return nullptr;
      return call;
    }

    ExprPtr e = makeExpr(ExprKind::Name, loc);
    e->text = name;
    return e;
  }

  error(peek(), std::string("expected an expression but found ") +
                    tokenKindName(peek().kind));
  return nullptr;
}

}  // namespace hero