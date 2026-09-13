#ifndef HERO_DIAGNOSTICS_H
#define HERO_DIAGNOSTICS_H

#include "hero/SourceFile.h"

#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

namespace hero {

enum class Severity {
  Error,
  Warning,
  Note,
};

struct Diagnostic {
  Severity severity = Severity::Error;
  SourceLoc loc;
  std::string message;

  // Codes like E001 come from section 8 of the spec. Empty for the
  // parser, its errors are syntax and the spec codes are about names and
  // types.
  std::string code;
};

// Diagnostics can carry one of the spec's error codes. The parser doesn't
// use them, sema does.
class Diagnostics {
public:
  explicit Diagnostics(const SourceFile &file) : file_(&file) {}

  void error(SourceLoc loc, std::string message);
  void error(SourceLoc loc, std::string code, std::string message);
  void warning(SourceLoc loc, std::string message);

  bool hasErrors() const;
  size_t count() const { return diags_.size(); }
  const std::vector<Diagnostic> &all() const { return diags_; }

  void print(std::ostream &os) const;

  // One diagnostic on its own. Exposed so tests can check the formatting
  // without going through a stream.
  std::string render(const Diagnostic &diag) const;

private:
  const SourceFile *file_;
  std::vector<Diagnostic> diags_;
};

}  // namespace hero

#endif