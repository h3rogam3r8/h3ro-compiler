// The compiler driver. Reads a .hero file and prints the tree. Argument
// handling is the same shape as the one in Alpha, just smaller.

#include "hero/ASTPrinter.h"
#include "hero/Lexer.h"
#include "hero/Parser.h"
#include "hero/Sema.h"
#include "hero/SourceFile.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

void usage() {
  std::cerr << "usage: heroc <file.hero> [--emit=ast|tokens]\n";
}

}  // namespace

int main(int argc, char **argv) {
  std::string path;
  std::string emit = "ast";  // the only thing it can do right now

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];

    // No starts_with until C++20 so this is the rfind trick. Not gonna use C++20 cause of LLVM issues.
    if (arg.rfind("--emit=", 0) == 0) {
      emit = arg.substr(7);
    } else if (arg == "-h" || arg == "--help") {
      usage();
      return 0;
    } else if (!arg.empty() && arg[0] == '-') {
      std::cerr << "heroc: unknown option " << arg << "\n";
      return 1;
    } else {
      path = arg;
    }
  }

  if (path.empty()) {
    usage();
    return 1;
  }

  if (emit != "ast" && emit != "tokens") {
    std::cerr << "heroc: --emit has to be ast or tokens\n";
    return 1;
  }

  std::ifstream in(path);
  if (!in) {
    std::cerr << "heroc: could not open " << path << "\n";
    return 1;
  }

  std::stringstream buffer;
  buffer << in.rdbuf();
  std::string source = buffer.str();

  hero::SourceFile file(path, source);

  // Stops before the parser. Useful when the parser is complaining about parser..
  if (emit == "tokens") {
    hero::printTokens(hero::Lexer(file.text()).tokenize(), std::cout);
    return 0;
  }

  hero::Parser parser(file);

  auto program = parser.parse();
  if (!program) {
    parser.diags().print(std::cerr);
    return 1;
  }

  // Names get resolved before anything gets printed. A tree full of
  // undefined names isn't worth emitting.
  hero::Sema sema(file);
  if (!sema.check(*program)) {
    sema.diags().print(std::cerr);
    return 1;
  }

  hero::printProgram(*program, std::cout);
  return 0;
}