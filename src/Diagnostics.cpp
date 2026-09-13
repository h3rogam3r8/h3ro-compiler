#include "hero/Diagnostics.h"

#include <ostream>
#include <utility>

namespace hero {
namespace {

const char *severityName(Severity s) {
  switch (s) {
  case Severity::Error:   return "error";
  case Severity::Warning: return "warning";
  case Severity::Note:    return "note";
  }
  return "error";
}

}  // namespace

void Diagnostics::error(SourceLoc loc, std::string message) {
  Diagnostic d;
  d.severity = Severity::Error;
  d.loc = loc;
  d.message = std::move(message);
  diags_.push_back(std::move(d));
}

// Modified function with code arg
void Diagnostics::error(SourceLoc loc, std::string code, std::string message) {
  Diagnostic d;
  d.severity = Severity::Error;
  d.loc = loc;
  d.message = std::move(message);
  d.code = std::move(code);
  diags_.push_back(std::move(d));
}

void Diagnostics::warning(SourceLoc loc, std::string message) {
  Diagnostic d;
  d.severity = Severity::Warning;
  d.loc = loc;
  d.message = std::move(message);
  diags_.push_back(std::move(d));
}

bool Diagnostics::hasErrors() const {
  for (const Diagnostic &d : diags_) {
    if (d.severity == Severity::Error)
      return true;
  }
  return false;
}

std::string Diagnostics::render(const Diagnostic &diag) const {
  std::string out = file_->name() + ":" + std::to_string(diag.loc.line) + ":" +
                    std::to_string(diag.loc.column) + ": " +
                    severityName(diag.severity);

  // error[E001]: ... when there's a code, plain error: ... when there isn't.
  if (!diag.code.empty())
    out += "[" + diag.code + "]";

  out += ": " + diag.message + "\n";

  std::string_view text = file_->line(diag.loc.line);
  if (text.empty())
    return out;

  out += std::string(text);
  out += "\n"; // Edge case

  // Build the run before the caret out of the line itself, so a line
  // indented with tabs still lines up.
  for (int i = 0; i + 1 < diag.loc.column; i++) {
    if (i < (int)text.size() && text[i] == '\t')
      out += '\t';
    else
      out += ' ';
  }
  out += "^\n";

  return out;
}

void Diagnostics::print(std::ostream &os) const {
  for (const Diagnostic &d : diags_)
    os << render(d);
}

}  // namespace hero
